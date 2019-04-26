#pragma once

#include <string>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <libgen.h>
#include <cstring>

#include "jail.h"

using namespace std;

void Jail::init(char* const name, char* const args) {
	this->setName(string(name));
	this->setContRoot(string(this->getRoot()).append("/containers/").append(this->getName()));
	int cpid;

	// stack for container
	this->cont_stack = allocate_stack(this->cont_stack_size);

	// small bashrc for colorful terminal
	this->setup_bashrc();

	// flags to clone process tree and unix time sharing
	if ((cpid = clone(Jail::start, this->cont_stack, CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD, (void*) this)) <= 0) {
		errno = EIO;
		panic("Error cloning process");
	} else {
		cout << "INFO container PID: " << cpid << endl;
	}

	// wait for container shell to exit
	wait(nullptr);
}

void Jail::init(char* const cont_name, char* const* args) {
	if (args == nullptr) {
		errno = EINVAL;
		panic("ERROR -c specified with no command");
	}
	this->setName(string(cont_name));
	this->setContRoot(string(this->getRoot()).append("/containers/").append(this->getName()));

	char buf[128];
	int cpid;

	this->cmd_args = (char**) args;

	// allocate stack for container
	this->cont_stack = allocate_stack(this->cont_stack_size);

	this->setup_bashrc();

	this->setup_src();

	getcwd(buf, 128);
	cout << "INFO mounted " << buf << " to /src\n";

	// flags to clone process tree and unix time sharing
	int flags = CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD;
	if ((cpid = clone(Jail::start_cmd, this->cont_stack, flags, (void*) this)) <=
		0) {
		panic("Error cloning process");
	} else {
		cout << "INFO container PID: " << cpid << endl;
	}

	// wait for container shell to exit
	wait(nullptr);
}

int Jail::start(void* args) {
	auto* self = (Jail*) args;

	self->setup_variables();

	self->setup_root();

	Jail::setup_dev();

	cout << "INFO container PID: " << getpid() << endl;

	// setup nameservers for internet access
	Jail::setup_resolvconf();

	if (self->name == "alpine") {
		system("apk add bash");
	}

	if (self->run("/bin/bash") != 0) {
		perror("ERROR");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

int Jail::start_cmd(void* args) {
	auto* self = (Jail*) args;

	self->setup_variables();

	self->setup_root();

	Jail::setup_dev();

	cout << "INFO container PID: " << getpid() << endl;

	// setup nameservers for internet access
	Jail::setup_resolvconf();

	if (self->name == "alpine") {
		system("apk add bash");
	}

	// go back to src after doing installtls
	chdir("/src");

	// BUFFER OVERFLOW EXPLOIT RIGHT THERE WOHOO
	// Managed to hack process into executing inside an
	// invoked shell by starting the shell with temp rc file
	FILE* startup = fopen("/.startup", "w");
	fprintf(startup, "%s\n", "source $HOME/.bashrc");
	while (*self->cmd_args != nullptr) {
		fprintf(startup, "%s", *self->cmd_args);
		fprintf(startup, "%s", " ");
		self->cmd_args++;
	}
	fclose(startup);

	if (system("/bin/bash --init-file /.startup") <= 0) {
		panic("ERROR");
		return EXIT_FAILURE;
	}
	remove("/.startup");
	return EXIT_SUCCESS;
}


template<typename ... Params>
int Jail::run(Params ...params) {
	char* args[] = {(char*) params..., nullptr};
	return execvp(args[0], args);
}


void Jail::setup_src() {
	system(("mkdir -p " + this->getContRoot() + "/src").c_str());
	if (mount("./", (this->getContRoot() + "/src").c_str(), "tmpfs", MS_BIND, nullptr) < 0) {
		panic("Unable to mount /src");
	}
}


void Jail::setup_root() {
	chroot(this->getContRoot().c_str());
	chdir("/");
	if (mount("proc", "/proc", "proc", 0, nullptr) < 0) {
		panic("Unable to mount /proc");
	}

}


void Jail::setup_variables() {
	clearenv();
	system(((string) "hostname " + this->name).c_str());
	setenv("HOME", "/", 0);
	setenv("DISPLAY", ":0.0", 0);
	setenv("TERM", "xterm-256color", 0);
	setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/src:/usr/local/bin:/usr/local/sbin", 0);
}

void Jail::setup_resolvconf() {
	system("echo \"nameserver 8.8.8.8\n nameserver 8.4.4.2\n\" > /etc/resolv.conf");
}

void Jail::setup_bashrc() {
	char buf[256];
	memset(buf, 0, 256);
	sprintf(buf, "cp %s/config/.bashrc %s", this->root.c_str(), this->cont_root.c_str());
	system(buf);
}


void Jail::setup_dev() {
	system("mount -n -vt tmpfs none /dev > /dev/null");

	system("mknod -m 666 /dev/null c 1 3");
	system("mknod -m 622 /dev/console c 5 1 > /dev/null");
	system("mknod -m 666 /dev/zero c 1 5 > /dev/null");
	system("mknod -m 666 /dev/ptmx c 5 2 > /dev/null");
	system("mknod -m 666 /dev/tty c 5 0 > /dev/null");
	system("mknod -m 444 /dev/random c 1 8 > /dev/null");
	system("mknod -m 444 /dev/urandom c 1 9 > /dev/null");

	system("chown -v root:tty /dev/console > /dev/null");
	system("chown -v root:tty /dev/ptmx > /dev/null");
	system("chown -v root:tty /dev/tty > /dev/null");

	system("ln -sv /proc/self/fd /dev/fd > /dev/null");
	system("ln -sv /proc/self/fd/0 /dev/stdin > /dev/null");
	system("ln -sv /proc/self/fd/1 /dev/stdout > /dev/null");
	system("ln -sv /proc/self/fd/2 /dev/stderr > /dev/null");
	system("ln -sv /proc/kcore /dev/core > /dev/null");

	system("mkdir -v /dev/pts > /dev/null");
	system("mkdir -v /dev/shm > /dev/null");

	// system("mount -vt devpts -o gid=4,mode=620 none /dev/pts");
	// system("mount -vt tmpfs none /dev/shm");
}

string Jail::readcmd(string cmd) {

	FILE* stream;
	string out;
	const int max_buffer = 1024;
	char buffer[max_buffer];
	cmd.append(" 2>&1");

	stream = popen(cmd.c_str(), "r");
	if (stream) {
		while (!feof(stream))
			if (fgets(buffer, max_buffer, stream) != nullptr) out.append(buffer);
		pclose(stream);
	}
	return out;
}

char* Jail::allocate_stack(int size) {
	char* stack = (char*) malloc(size);
	if (stack == nullptr) {
		errno = ENOMEM;
		panic("Cannot allocate stack memory");
	}

	// stack grows downwards so move ptr to the end
	return stack + size;
}

void Jail::panic(const char* message) {
	std::perror(message);
	exit(-1);
}

string Jail::get_location(char* const cmd) {
	string dir_cmd(cmd);
	dir_cmd.insert(0, "which ");
	dir_cmd = readcmd(dir_cmd);

	dir_cmd.insert(0, "readlink ");
	return readcmd(dir_cmd);

}


Jail::~Jail() {
	// free allocated container stack
	delete[] (this->cont_stack - this->cont_stack_size);

	// unmount container /proc from outside
	cout << "INFO umounting /proc\n";
	if (system(("umount -f " + this->getContRoot() + "/proc").c_str()) < 0) {
		cout << "ERROR umounting /proc\n";
	}

	// unmount container /dev from outside
	cout << "INFO umounting /dev\n";
	if (system(("umount " + this->getContRoot() + "/dev").c_str()) < 0) {
		cout << "ERROR umounting /dev\n";
	}

	// unmount /src
	cout << "INFO umounting /src\n";
	if (system(("umount -f " + this->getContRoot() + "/src &> /dev/null").c_str()) < 0) {
		cout << "ERROR umounting /src\n";
	}

	if (this->isBuildCont()) {
		cout << "INFO building container image" << "\n";
		char buf[256];
		string dir("./");
		dir = dir.append("cache/");
		dir = dir.append(this->getName());
		dir = dir.append(".tar.gz");
		sprintf(buf, "tar -czf %s -C %s .", dir.c_str(), this->getContRoot().c_str());
		system(buf);
	}

	if (this->isRmCont()) {
		cout << "INFO removing root dir" << "\n";
		system(("rm -rf " + this->getContRoot()).c_str());
	}

}


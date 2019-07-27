#pragma once

#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <errno.h>
#include <sched.h>

#include "jail.h"

#define BUFSIZE 256

typedef struct container {
	char root[128];
	char cont_root[128];
	char cont_name[32];
	char cont_id[16];
	char* argv0;
	char** cmd_args;
	size_t* cont_stack;
	long cont_stack_size;
} container_t;

container_t* cont = NULL;


void init(char const* cname, char const* cid, char const* croot) {
	cont = (container_t*) calloc(1, sizeof(struct container));
	if (cont == NULL) {
		errno = ENOMEM;
		perror("Cannot allocate memory for container");
	}
	cont->cont_stack_size = 65536;

	int clone_flags;
	int cpid;

	strcpy(cont->cont_id, cid);
	strcpy(cont->cont_root, croot);

	strcpy(cont->cont_name, cname);
	strcat(cont->cont_name, "-");
	strcat(cont->cont_name, cid);

	strcpy(cont->cont_root, croot);
	strcat(cont->cont_root, "/containers/");
	strcat(cont->cont_root, cont->cont_name);

	// stack for container
	cont->cont_stack = stalloc(cont->cont_stack_size);

	setup_bashrc();

	// flags to clone process tree and unix time sharing
	clone_flags = CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD;
	if ((cpid = clone(start, cont->cont_stack, clone_flags, NULL)) <= 0) {
		perror("Error cloning process");
	} else {
		printf("INFO container PID: %d\n", cpid);
	}

	// wait for container shell to exit
	wait(NULL);
	cleanup();
}

void init2(char const* cname, char const* cid, char const* croot, char* const* args) {
	if (args == NULL) {
		errno = EINVAL;
		perror("Error '-c' specified with no command");
	}

	cont = (container_t*) calloc(1, sizeof(struct container));
	if (cont == NULL) {
		errno = ENOMEM;
		perror("Cannot allocate memory for container");
	}

	cont->cont_stack_size = 65536;

	strcpy(cont->cont_id, cid);
	strcpy(cont->cont_root, croot);

	strcpy(cont->cont_name, cname);
	strcat(cont->cont_name, "-");
	strcat(cont->cont_name, cid);

	strcpy(cont->cont_root, croot);
	strcat(cont->cont_root, "/containers/");
	strcat(cont->cont_root, cont->cont_name);


	char buf[128];
	int cpid;

	// allocate stack for container
	cont->cont_stack = stalloc(cont->cont_stack_size);

	setup_bashrc();

	setup_src();

	getcwd(buf, 128);
	printf("INFO mounted %s to /src\n", buf);

	// flags to clone process tree and unix time sharing
	int flags = CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD;
	if ((cpid = clone(start_cmd, cont->cont_stack, flags, NULL)) <= 0) {
		perror("Error cloning process");
	} else {
		printf("INFO container PID: %d\n", cpid);
	}

	// wait for container shell to exit
	wait(NULL);
	cleanup();
}

int start() {
	setup_variables();

	setup_root();

	setup_dev();

	printf("INFO container PID: %d", getpid());

	// setup nameservers for internet access
	setup_resolvconf();

	if (strncmp(cont->cont_name, "alpine", 6) == 0) {
		system("apk add bash &> /dev/null");
	}
	char* args[] = {NULL};
	if (execvp("/bin/bash", args) != 0) {
		perror("Error running shell");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

int start_cmd() {

	setup_variables();

	setup_root();

	setup_dev();

	printf("INFO container PID: %d", getpid());

	// setup nameservers for internet access
	setup_resolvconf();

	if (strncmp(cont->cont_name, "alpine", 6) == 0) {
		system("apk add bash &> /dev/null");
	}

	// go back to src after doing installtls
	chdir("/src");

	// BUFFER OVERFLOW EXPLOIT RIGHT THERE WOHOO
	// Managed to hack process into executing inside an
	// invoked shell by starting the shell with temp rc file
	FILE* startup = fopen("/.startup", "w");
	fprintf(startup, "%s\n", "source $HOME/.bashrc");
	while (cont->cmd_args != NULL) {
		fprintf(startup, "%s", *(cont->cmd_args));
		fprintf(startup, "%s", " ");
		cont->cmd_args++;
	}
	fclose(startup);

	if (system("/bin/bash --init-file /.startup") <= 0) {
		perror("Error initializing shell");
		return EXIT_FAILURE;
	}
	remove("/.startup");
	return EXIT_SUCCESS;
}

// int Jail::run(Params ...params) {
// 	char* args[] = {(char*) params..., nullptr};
// 	return execvp(args[0], args);
// }

void setup_src() {
	char buf[BUFSIZE];
	memset(buf, 0, BUFSIZE);
	sprintf(buf, "mkdir -p %s/src", cont->cont_root);
	system(buf);
	memset(buf, 0, BUFSIZE);
	sprintf(buf, "%s/src", cont->cont_root);
	if (mount("./", cont->cont_root, "tmpfs", MS_BIND, NULL) < 0) {
		errno = EIO;
		perror("Unable to mount /src");
	}
}


void setup_root() {
	if (chroot(cont->cont_root) == -1) {
		errno = EIO;
		perror("Error creating chroot");
	}
	chdir("/");
	if (mount("proc", "/proc", "proc", 0, NULL) < 0) {
		errno = EIO;
		perror("Unable to mount /proc");
	}

}


void setup_variables() {
	char buf[BUFSIZE];
	memset(buf, 0, BUFSIZE);
	clearenv();
	sprintf(buf, "hostname %s", cont->cont_name);
	system(buf);
	setenv("HOME", "/", 0);
	setenv("DISPLAY", ":0.0", 0);
	setenv("TERM", "xterm-256color", 0);
	setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/src:/usr/local/bin:/usr/local/sbin", 0);
}

void setup_resolvconf() {
	system("echo \"nameserver 8.8.8.8\n nameserver 8.4.4.2\n\" > /etc/resolv.conf");
}

void setup_bashrc() {
	char buf[512];
	memset(buf, 0, 512);
	sprintf(buf, "cp -n %s/config/.bashrc %s", cont->root, cont->cont_root);
	system(buf);
}


void setup_dev() {
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

size_t* stalloc(long size) {
	size_t* stack = (size_t*) malloc(size);
	if (stack == NULL) {
		errno = ENOMEM;
		perror("Cannot allocate stack memory");
	}
	// stack grows downwards so move ptr to the end
	return stack + size;
}

void cleanup() {
	char buf[BUFSIZE];
	memset(buf, 0, BUFSIZE);
	// free allocated container stack
	if (cont->cont_stack != NULL) {
		free(cont->cont_stack + cont->cont_stack_size);
	}

	// unmount container /proc from outside
	printf("INFO umounting /proc\n");
	sprintf(buf, "umount -f %s/proc &> /dev/null", cont->cont_root);
	if (system(buf) < 0) {
		printf("ERROR umounting /proc\n");
	}
	memset(buf, 0, BUFSIZE);

	// unmount container /dev from outside
	printf("INFO umounting /dev\n");
	sprintf(buf, "umount -f %s/dev &> /dev/null", cont->cont_root);
	if (system(buf) < 0) {
		printf("ERROR umounting /dev\n");
	}
	memset(buf, 0, BUFSIZE);

	// unmount /src
	printf("INFO umounting /src\n");
	sprintf(buf, "umount -f %s/src &> /dev/null", cont->cont_root);
	if (system(buf) < 0) {
		printf("ERROR umounting /src\n");
	}
	memset(buf, 0, BUFSIZE);
	//
	// if (this->isBuildCont()) {
	// 	cout << "INFO building container image" << "\n";
	// 	char buf[256];
	// 	string dir(this->getRoot());
	// 	dir.append("/cache/build/");
	// 	system(("mkdir -p " + dir).c_str());
	// 	dir.append(this->getName());
	// 	dir.append(".tar.gz");
	// 	sprintf(buf, "tar -czf %s -C %s .", dir.c_str(), this->getContRoot().c_str());
	// 	system(buf);
	// 	cout << "INFO saved container " << this->getName() << "\n";
	// }
	//
	// if (this->isRmCont()) {
	// 	cout << "INFO removing root dir" << "\n";
	// 	system(("rm -rf " + this->getContRoot()).c_str());
	// }
	// unsigned int flags = S_IRWXU | S_IRWXG | S_IROTH;
	// chmod((const char*) (this->cont_root.c_str()), flags);
}


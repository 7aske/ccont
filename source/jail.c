#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <errno.h>
#include <sched.h>

#include "../headers/jail.h"

#define BUFSIZE 256

static size_t* cont_stack_fptr = NULL;
static container_t* cont = NULL;

static unsigned int FLAGS;

void signal_handler(int sig) {
	printf("INFO received signal %s\n", strsignal(sig));
	if (sig == SIGINT) {
		if (cont->chld_pid != 0) {
			kill(cont->chld_pid, SIGKILL);
		}
	}
}

void* stalloc(long size) {
	void* stack = (void*) malloc(size);
	if (stack == NULL) {
		errno = ENOMEM;
		perror("Cannot allocate stack memory");
	}
	cont_stack_fptr = stack;
	// stack grows downwards so move ptr to the end
	return stack + size;
}

void setup_src() {
	char buf[BUFSIZE];
	memset(buf, 0, BUFSIZE);
	sprintf(buf, "mkdir -p %s/src", cont->cont_root);
	system(buf);
	memset(buf, 0, BUFSIZE);
	sprintf(buf, "%s/src", cont->cont_root);
	if (mount("./", buf, "tmpfs", (FLAGS & CONT_RBIND) == CONT_RBIND ? MS_BIND | MS_REC : MS_BIND, NULL) < 0) {
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


void setup_variables(void* c) {
	char buf[BUFSIZE];
	memset(buf, 0, BUFSIZE);

	clearenv();

	sprintf(buf, "hostname %s", ((container_t*) c)->cont_name);
	system(buf);

	cenv_t* curr = ((container_t*) c)->cont_envp;
	while (curr != NULL) {
		printf("INFO env %s=%s\n", curr->key, curr->val);
		setenv(curr->key, curr->val, 0);
		curr = curr->next;
	}

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

void cleanup() {
	char buf[BUFSIZE];
	memset(buf, 0, BUFSIZE);

	// unmount container /proc from outside
	printf("INFO umounting /proc\n");
	sprintf(buf, "umount -f %s/proc", cont->cont_root);
	if (system(buf) < 0) {
		printf("ERROR umounting /proc\n");
	}
	memset(buf, 0, BUFSIZE);

	// unmount container /dev from outside
	printf("INFO umounting /dev\n");
	sprintf(buf, "umount -f %s/dev", cont->cont_root);
	if (system(buf) < 0) {
		printf("ERROR umounting /dev\n");
	}
	memset(buf, 0, BUFSIZE);

	// unmount /src
	if ((FLAGS & CONT_CMD) == CONT_CMD) {
		printf("INFO umounting /src\n");
		sprintf(buf, "umount -f %s/src", cont->cont_root);
		if (system(buf) < 0) {
			printf("ERROR umounting /src\n");
		}
		memset(buf, 0, BUFSIZE);
	}

	if ((FLAGS & CONT_BUILD) == CONT_BUILD) {
		printf("INFO building container image\n");
		char cache_dir[256];

		strcpy(cache_dir, cont->root);
		strcat(cache_dir, "/cache/build/");

		sprintf(buf, "mkdir -p %s", cache_dir);
		system(buf);
		memset(buf, 0, BUFSIZE);

		strcat(cache_dir, cont->cont_name);
		strcat(cache_dir, ".tar.gz");

		sprintf(buf, "tar -czf %s -C %s .", cache_dir, cont->cont_root);
		system(buf);
		printf("INFO saved container  %s\n", cont->cont_name);
	}


	if ((FLAGS & CONT_RM) == CONT_RM) {
		printf("INFO removing root dir\n");
		sprintf(buf, "rm -rf %s", cont->cont_root);
		system(buf);
		memset(buf, 0, BUFSIZE);
	}

	if (cont_stack_fptr != NULL) {
		free(cont_stack_fptr);
	}

	if (cont != NULL) {
		free(cont);
	}

}

int start(void* arg) {
	void* container = arg;
	setup_root();
	setup_variables(container);
	setup_dev();

	int pid = getpid();
	if (pid == 1){
		printf("INFO container initialized");

	} else {
		printf("INFO container PID: %d\n", getpid());
	}

	// setup nameservers for internet access
	setup_resolvconf();

	if (strncmp(cont->cont_distro, "alpine", 6) == 0) {
		system("apk add bash &> /dev/null");
	}

	int retval;
	if ((FLAGS & CONT_CMD) == CONT_CMD) {
		FILE* startup = fopen("/.startup", "w");
		fprintf(startup, "%s\n", "source $HOME/.bashrc");
		while (*cont->cmd_args != NULL) {
			fprintf(startup, "%s", *(cont->cmd_args));
			fprintf(startup, "%s", " ");
			cont->cmd_args++;
		}
		fclose(startup);
		chdir("/src");
		if ((retval = execl("/bin/bash", "--init-file", "/.startup", (char*) NULL)) != 0) {
			remove("/.startup");
			printf("ERROR shell retval: %d\n", retval);
			perror("Error initializing shell");
			return EXIT_FAILURE;
		}
		remove("/.startup");
	} else {
		if ((retval = execl("/bin/bash", "", (char*) NULL)) != 0) {
			printf("ERROR shell retval: %d\n", retval);
			perror("Error initializing shell");
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}


void init(char const* cdistro, char const* cname, char const* croot, char** cargs, struct cenv* cenvp, unsigned int flags) {
	signal(SIGINT, signal_handler);

	cont = (container_t*) calloc(1, sizeof(container_t));
	if (cont == NULL) {
		errno = ENOMEM;
		perror("Cannot allocate memory for container");
	}
	cont->chld_pid = 0;
	cont->cont_envp = cenvp;

	strcpy(cont->cont_distro, cdistro);
	strcpy(cont->root, croot);

	strcpy(cont->cont_name, cname);

	strcpy(cont->cont_root, croot);
	strcat(cont->cont_root, "/containers/");
	strcat(cont->cont_root, cont->cont_name);

	cont->cont_stack_size = 65536;
	// stack for container
	cont->cont_stack = stalloc(cont->cont_stack_size);

	FLAGS = flags;

	if ((FLAGS & CONT_RBIND) == CONT_RBIND) {
		printf("WARNING recursive bind enabled\n");
	}

	if (cargs != NULL) {
		FLAGS |= CONT_CMD;
		cont->cmd_args = cargs;
		setup_src();
	}

	int clone_flags;
	int cpid;

	setup_bashrc();

	int code;
	// flags to clone process tree and unix time sharing
	clone_flags = CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD;
	if ((cpid = clone(start, cont->cont_stack, clone_flags, (void*) cont)) <= 0) {
		perror("Error cloning process");
	} else {
		printf("INFO container PID: %d\n", cpid);
		cont->chld_pid = cpid;
	}

	// wait for container shell to exit
	sleep(1);
	waitpid(cpid, &code, 0);
	printf("INFO container exists with code: %d\n", code);
	cleanup();
}

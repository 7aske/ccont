#define _GNU_SOURCE

#include <sys/stat.h>
#include <libgen.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <zconf.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>

#include "../headers/ccontutils.h"
#include "../headers/shortid.h"
#include "../headers/jail.h"

#define _HELPFORMAT "%-25s\t%s\n"
#define _ROOTFSFORMAT "%-25s\t%s\n"

void panic(const char*, int);

void setup_cont_image(char const*, char const*);

void setup_cont_image_prebuilt(char const*, char const*);

void print_rootfs_header();

void print_rootfs(char const*);

void print_help();

void setup_ftree(char*);

int main(int argc, char* args[]) {
	printf("%d\n", argc);
	printf("%s\n", args[0]);

	char* rootdir = dirname(abspth(args[0]));
	char bldfld[192];
	char cntfld[192];

	strcpy(bldfld, rootdir);
	strcat(bldfld, "/cache/build");

	strcpy(cntfld, rootdir);
	strcat(cntfld, "/containers");

	setup_ftree(rootdir);

	if (argc == 1) {
		print_help();
		free(rootdir);
		return 0;
	}

	for (int i = 0; i < argc; i++) {
		if (strcmp(args[i], "--help") == 0 || strcmp(args[i], "-h") == 0) {
			print_help();
			free(rootdir);
			return 0;
		} else if (strcmp(args[i], "--list") == 0 || strcmp(args[i], "-l") == 0) {
			print_rootfs_header();
			print_rootfs(bldfld);
			print_rootfs(cntfld);
			free(rootdir);
			return 0;
		}
	}
	if (getuid() != 0) {
		free(rootdir);
		panic("Please run as root", EACCES);
	}
	unsigned int flags = 0;
	int image_selected = 0;
	int prebuilt = 0;
	int cmd_start = 0;
	char image_name[32];
	char udef_id[32];
	memset(udef_id, 0, 32);
	memset(image_name, 0, 32);

	for (int i = 0; i < argc; i++) {
		if (strcmp(args[i], "alpine") == 0) {
			image_selected = 1;
			strncpy(image_name, "alpine", 6);
		} else if (strcmp(args[i], "ubuntu") == 0) {
			image_selected = 1;
			strncpy(image_name, "ubuntu", 6);
		} else if (strcmp(args[i], "-c") == 0) {
			cmd_start = i;
			break;
		} else if (strcmp(args[i], "--rm") == 0) {
			flags |= CONT_RM;
		} else if (strcmp(args[i], "--build") == 0 || strcmp(args[i], "-b") == 0) {
			flags |= CONT_BUILD;
		} else if (contains(bldfld, args[i]) || contains(cntfld, args[i])) {
			image_selected = 1;
			prebuilt = 1;
			strcpy(image_name, args[i]);
		} else if (strncmp(args[i], "--cont-id=", 10) == 0) {
			char* eq = strrchr(args[i], '=');
			if (strlen(args[i]) > 10) {
				if (strlen(args[i]) < 13) {
					free(rootdir);
					panic("Container ID too short", EINVAL);
				}
				strncpy(udef_id, eq + 1, 32);
			} else {
				free(rootdir);
				panic("Invalid container ID", EINVAL);
			}
		}
	}

	if (image_selected) {
		printf("%s\n", image_name);
		char* contid;
		if (prebuilt) {
			setup_cont_image_prebuilt(image_name, rootdir);
			char* p = strrchr(image_name, '-');
			*p = '\0';
			contid = (char*) calloc(strlen(p + 1), sizeof(char));
			strcpy(contid, p + 1);
		} else {
			srand(time(NULL));
			char buf[64];
			memset(buf, 0, 64);
			if (udef_id[0] == '\0') {
				contid = encode(random(), 42);
			} else {
				contid = udef_id;
			}
			strcpy(buf, image_name);
			strcat(buf, "-");
			strcat(buf, contid);
			setup_cont_image(buf, rootdir);
		}
		if (cmd_start != 0) {
			if (cmd_start == argc - 1) {
				free(rootdir);
				panic("Usage: ccont -c <args>...", EINVAL);
			} else {
				init(image_name, contid, rootdir, &args[cmd_start + 1], flags);
			}
		} else {
			init(image_name, contid, rootdir, NULL, flags);
		}
	} else {
		panic("Please specify a valid rootfs", EINVAL);
		free(rootdir);
		return 1;
	}
	free(rootdir);
	return 0;
}

void setup_cont_image_prebuilt(const char* build_name, char const* rootdir) {
	char* cont_folder = (char*) calloc(128, sizeof(char));
	strcpy(cont_folder, rootdir);

	strcat(cont_folder, "/containers/");
	if (!exists_stat(cont_folder)) {
		mkdir(cont_folder, 0755);
	}

	strcat(cont_folder, build_name);

	if (!exists_stat(cont_folder)) {
		char buf[256];
		memset(buf, 0, 256);

		char* cache_fname = (char*) calloc(128, sizeof(char));
		strcpy(cache_fname, rootdir);

		strcat(cache_fname, "/cache");
		if (!exists_stat(cache_fname)) {
			mkdir(cache_fname, 0755);
		}

		strcat(cache_fname, "/build/");
		if (!exists_stat(cache_fname)) {
			mkdir(cache_fname, 0755);
		}

		strcat(cache_fname, build_name);
		strcat(cache_fname, ".tar.gz");

		if (!exists_stat(cache_fname)) {
			free(cont_folder);
			panic("Cached file not found", ENOENT);
		}

		mkdir(cont_folder, 0755);
		sprintf(buf, "tar xf %s -C %s > /dev/null", cache_fname, cont_folder);
		if (system(buf) != 0) {
			free(cont_folder);
			panic("Cannot extract archive", EIO);
		} else {
			printf("INFO extracted %s\n", build_name);
		}
	}
	free(cont_folder);
}

void setup_cont_image(char const* build_name, char const* rootdir) {
	char ubuntu[] = "http://cdimage.ubuntu.com/ubuntu-base/releases/18.10/release/ubuntu-base-18.10-base-amd64.tar.gz";
	char alpine[] = "http://dl-cdn.alpinelinux.org/alpine/v3.9/releases/x86_64/alpine-minirootfs-3.9.3-x86_64.tar.gz";
	char* sel_rootfs = NULL;

	char buf[256];
	memset(buf, 0, 256);

	if (strncmp(build_name, "ubuntu", 6) == 0) {
		sel_rootfs = ubuntu;
	} else if (strncmp(build_name, "alpine", 6) == 0) {
		sel_rootfs = alpine;
	} else {
		panic("Invalid container name", EINVAL);
	}

	const char* url_base = basename(sel_rootfs);

	char* cache_dir = (char*) calloc(128, sizeof(char));
	strcpy(cache_dir, rootdir);

	strcat(cache_dir, "/cache");
	if (!exists_stat(cache_dir)) {
		mkdir(cache_dir, 0755);
	}

	char* cont_folder = (char*) calloc(128, sizeof(char));
	strcpy(cont_folder, rootdir);

	strcat(cont_folder, "/containers/");
	if (!exists_stat(cont_folder)) {
		mkdir(cont_folder, 0755);
	}

	strcat(cont_folder, build_name);


	char* cache_fname = (char*) calloc(128, sizeof(char));
	strcpy(cache_fname, cache_dir);
	strcat(cache_fname, "/");
	strcat(cache_fname, url_base);

	// download rootfs archive if its not present
	if (!exists_stat(cache_fname)) {
		sprintf(buf, "wget -O %s %s -q --show-progress", cache_fname, sel_rootfs);
		system(buf);
		memset(buf, 0, 256);
	}

	if (!exists_stat(cont_folder)) {
		mkdir(cont_folder, 0755);
		sprintf(buf, "tar xf %s -C %s > /dev/null", cache_fname, cont_folder);
		if (system(buf) != 0) {
			panic("Cannot extract archive", EIO);
		} else {
			printf("INFO extracted %s\n", build_name);
		}

	}

	free(cont_folder);
	free(cache_dir);
}

void print_help() {
	printf("Ccont 0.0.2 ==== Nikola Tasic ==== https://github.com/7aske/ccont\n");
	printf("Usage:\n");
	printf(_HELPFORMAT, "ccont <rootfs> [opts]", "start a <rootfs> container");
	printf(_HELPFORMAT, "ccont <rootfs> -c <cmd> [args]", "start a <rootfs> container and execute");
	printf(_HELPFORMAT, "", "command <cmd> with arguments [args]");
	printf(_HELPFORMAT, "Options:", "");
	printf(_HELPFORMAT, "--rm", "removes container after exiting");
	printf(_HELPFORMAT, "--build, -b", "builds an image of the container after exiting");
	printf(_HELPFORMAT, "--cont-id=<id>", "manually give an id name to a container");
	printf(_HELPFORMAT, "", "");
	printf(_HELPFORMAT, "ccont --list, -l", "print a list of available file systems");
	printf(_HELPFORMAT, "ccont --help, -h", "print this message");
}

void print_rootfs_header() {
	printf("Ccont 0.0.2 ==== Nikola Tasic ==== https://github.com/7aske/ccont\n");
	printf("Available root file systems:\n");
	printf(_HELPFORMAT, "ubuntu", "Ubuntu 18.10");
	printf(_HELPFORMAT, "alpine", "Alpine 3.9.3");
	printf(_HELPFORMAT, "", "");
}

void print_rootfs(char const* r) {
	struct dirent* dir;
	DIR* dirp;
	if ((dirp = opendir(r)) != NULL) {
		while ((dir = readdir(dirp)) != NULL) {
			if (strncmp(dir->d_name, ".", 1) != 0) {
				char noext[64];
				char* p;
				strcpy(noext, dir->d_name);
				p = strchr(noext, '.');
				if (p != NULL) {
					*p = '\0';
					printf(_HELPFORMAT, noext, dir->d_name);
				} else {
					printf(_HELPFORMAT, dir->d_name, dir->d_name);
				}
			}
		}
	}
	free(dirp);
}

void panic(char const* msg, int err) {
	errno = err;
	perror(msg);
	exit(err);
}

void setup_ftree(char* root) {
	char buf[256];
	memset(buf, 0, 256);

	strcpy(buf, root);
	strcat(buf, "/container");

	mkdir(buf, 0755);
	memset(buf, 0, 256);

	strcpy(buf, root);
	strcat(buf, "/cache");

	mkdir(buf, 0755);

	strcat(buf, "/build");
	mkdir(buf, 0755);
}

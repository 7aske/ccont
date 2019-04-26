#include <iostream>
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

#include "lib/shortid.h"
#include "path/utils.h"
#include "jail/jail.cpp"

#define _HELPFORMAT "%-25s\t%s\n"
#define _ROOTFSFORMAT "%-25s\t%s\n"

void panic(const char*);

void setup_cont_image(const char*, string);

void print_rootfs();

void print_help();

using namespace std;

int main(int argc, char* args[]) {
	for (int i = 0; i < argc; i++) {

		if (strcmp(args[i], "--help") == 0 || strcmp(args[i], "-h") == 0) {
			print_help();
			exit(0);
		} else if (strcmp(args[i], "--list") == 0 || strcmp(args[i], "-l") == 0) {
			print_rootfs();
			exit(0);
		}
	}
	if (getuid() != 0) {
		errno = EACCES;
		panic("ERROR");
	}
	Jail jail;
	bool image_selected = false;
	char image_name[32];
	int cmd_start = 0;
	string dir = Jail::get_location(args[0]);

	for (int i = 0; i < argc; i++) {
		if (strcmp(args[i], "alpine") == 0) {
			image_selected = true;
			strncpy(image_name, "alpine", 6);
		} else if (strcmp(args[i], "ubuntu") == 0) {
			image_selected = true;
			strncpy(image_name, "ubuntu", 6);
		} else if (strcmp(args[i], "-c") == 0) {
			cmd_start = i;
			break;
		} else if (strcmp(args[i], "--rm") == 0) {
			jail.setRmCont(true);
		} else if (strcmp(args[i], "--build") == 0) {
			jail.setBuildCont(true);
		} else if (strcmp(args[i], "-b") == 0) {
			jail.setBuildCont(true);
		}
	}

	if (image_selected) {
		setup_cont_image(image_name, dir);
		jail.setArgv0(args[0]);
		// srand(time(nullptr));
		// string id = ShortID::Encode(rand());
		// id.insert(0, image_name);
		if (cmd_start != 0) {
			if (cmd_start == argc - 1) {
				cout << "ERROR please specify commands with -c flag\n";
			} else {
				jail.setRoot(dirname((char* const) Jail::get_location(args[0]).c_str()));
				jail.init(image_name, &args[cmd_start + 1]);
			}
		} else {
			jail.setRoot(dirname((char* const) Jail::get_location(args[0]).c_str()));
			jail.init(image_name, args[0]);
		}
	} else {
		cout << "ERROR please specify a rootfs\n";
		print_rootfs();
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

void panic(const char* message) {
	std::perror(message);
	exit(EXIT_FAILURE);
}

void setup_cont_image(const char* bname, string dir) {
	char ubuntu[] = "http://cdimage.ubuntu.com/ubuntu-base/releases/18.10/release/ubuntu-base-18.10-base-amd64.tar.gz";
	char alpine[] = "http://dl-cdn.alpinelinux.org/alpine/v3.9/releases/x86_64/alpine-minirootfs-3.9.3-x86_64.tar.gz";
	string selected;

	if (strncmp(bname, "ubuntu", 6) == 0) {
		selected = ubuntu;
	} else if (strncmp(bname, "alpine", 6) == 0) {
		selected = alpine;
	} else {
		errno = EINVAL;
		panic("Container image not found");
	}

	const char* url_base = basename((char* const) selected.c_str());
	dir = dirname((char*) dir.c_str());
	string cont_folder(dir + "/containers/" + bname);
	dir = dir.append("/cache");
	string cache_fname(dir + "/" + url_base);

	if (!path::exists_stat(dir)) {
		system(("mkdir -p " + dir).c_str());
	}

	if (!path::exists_stat((cont_folder).c_str())) {
		system(("mkdir -p " + cont_folder).c_str());

		// download rootfs archive if its not present
		if (!path::exists(cache_fname)) {
			char buf[64];
			sprintf(buf, "wget -O %s %s -q --show-progress", cache_fname.c_str(), selected.c_str());
			printf("%s\n", buf);
			system(buf);
		}
		char buf[64];
		sprintf(buf, "tar xf %s -C %s > /dev/null", cache_fname.c_str(), cont_folder.c_str());
		system(buf);
	}
}

void print_help() {
	printf("Ccont 0.0.1 ==== Nikola Tasic ==== https://github.com/7aske/ccont\n");
	printf("Usage:\n");
	printf(_HELPFORMAT, "ccont <rootfs> [opts]", "start a <rootfs> container");
	printf(_HELPFORMAT, "ccont <rootfs> -c <cmd> [args]", "start a <rootfs> container and execute");
	printf(_HELPFORMAT, "", "command <cmd> with arguments [args]");
	printf(_HELPFORMAT, "Options:", "");
	printf(_HELPFORMAT, "--rm", "removes container after exiting");
	printf(_HELPFORMAT, "--build, -b", "builds an image of the container after exiting");
	printf(_HELPFORMAT, "", "");
	printf(_HELPFORMAT, "ccont --list, -l", "print a list of available file systems");
	printf(_HELPFORMAT, "ccont --help, -h", "print this message");
}

void print_rootfs() {
	printf("Ccont 0.0.1 ==== Nikola Tasic ==== https://github.com/7aske/ccont\n");
	printf("Available root file systems:\n");
	printf(_HELPFORMAT, "ubuntu", "Ubuntu 18.10");
	printf(_HELPFORMAT, "alpine", "Alpine 3.9.3");
}

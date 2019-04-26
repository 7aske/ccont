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
#include <dirent.h>

#include "lib/shortid.h"
#include "utils/utils.h"
#include "jail/jail.cpp"

#define _HELPFORMAT "%-25s\t%s\n"
#define _ROOTFSFORMAT "%-25s\t%s\n"

void panic(const char*, int = errno);

void setup_cont_image(const char*, string);

void setup_cont_image_prebuilt(const char* bname, string d);

void print_rootfs(string &);

void print_help();

using namespace std;

int main(int argc, char* args[]) {
	string root(dirname((char* const) path::abs(args[0]).c_str()));
	string build(root);
	build.append("/cache/build");
	for (int i = 0; i < argc; i++) {
		if (strcmp(args[i], "--help") == 0 || strcmp(args[i], "-h") == 0) {
			print_help();
			exit(0);
		} else if (strcmp(args[i], "--list") == 0 || strcmp(args[i], "-l") == 0) {
			print_rootfs(build);
			exit(0);
		}
	}
	if (getuid() != 0) {
		panic("ERROR", EACCES);
	}
	Jail jail;
	bool image_selected = false;
	bool prebuilt = false;
	char image_name[64];
	memset(&image_name, 0, 64);
	int cmd_start = 0;


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
		} else if (strcmp(args[i], "--build") == 0 || strcmp(args[i], "-b") == 0) {
			jail.setBuildCont(true);
		} else if (dir::contains(build, args[i])) {
			image_selected = true;
			prebuilt = true;
			strncpy(image_name, args[i], strlen(args[i]));
		}
	}
	cout << image_name << endl;
	if (image_selected) {
		string id;
		if (prebuilt) {
			setup_cont_image_prebuilt(image_name, root);
			char* p = strrchr(image_name, '-');
			*p = '\0';
			id = string(p + 1);
		} else {
			srand(time(nullptr));
			string inpbuf = "";
			if (jail.isBuildCont()){
				cout << "Enter container ID (empty for random): ";
				getline(cin, inpbuf);
			}
			if (inpbuf.empty()) {
				id = ShortID::Encode(random());
			} else {
				id = inpbuf;
			}
			char buf[64];
			memset(buf, 0, 64);
			strcpy(buf, image_name);
			strcat(buf, "-");
			strcat(buf, id.c_str());
			setup_cont_image(buf, root);
		}

		jail.setArgv0(args[0]);
		jail.setRoot(root);

		if (cmd_start != 0) {
			if (cmd_start == argc - 1) {
				cout << "ERROR please specify commands with -c flag\n";
			} else {
				jail.init(image_name, &args[cmd_start + 1], (char* const) id.c_str());
			}
		} else {
			jail.init(image_name, (char* const) id.c_str());
		}
	} else {
		cout << "ERROR please specify a rootfs\n\n";
		print_rootfs(build);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

void setup_cont_image_prebuilt(const char* bname, string d) {
	string dir(d);
	string cont_folder(d + "/containers/" + bname);
	d.append("/cache/build/");
	string cache_fname(d + bname + ".tar.gz");

	cout << cache_fname << endl;
	cout << cont_folder << endl;

	if (!path::exists_stat((cont_folder).c_str())) {
		system(("mkdir -p " + cont_folder).c_str());

		if (!path::exists_stat(cache_fname)) {
			panic("ERROR cache", ENOENT);
		}
		char buf[64];
		sprintf(buf, "tar xf %s -C %s > /dev/null", cache_fname.c_str(), cont_folder.c_str());
		if (system(buf) != 0) {
			panic("ERROR tar", EIO);
		} else {
			cout << "INFO extracted " << bname << "\n";
		}
	}
}

void setup_cont_image(const char* bname, string d) {
	char ubuntu[] = "http://cdimage.ubuntu.com/ubuntu-base/releases/18.10/release/ubuntu-base-18.10-base-amd64.tar.gz";
	char alpine[] = "http://dl-cdn.alpinelinux.org/alpine/v3.9/releases/x86_64/alpine-minirootfs-3.9.3-x86_64.tar.gz";
	string selected;

	if (strncmp(bname, "ubuntu", 6) == 0) {
		selected = ubuntu;
	} else if (strncmp(bname, "alpine", 6) == 0) {
		selected = alpine;
	} else {
		panic("ERROR container", ENOENT);
	}

	const char* url_base = basename((char* const) selected.c_str());

	string dir(d);
	string cont_folder(dir + "/containers/" + bname);
	dir.append("/cache");
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
			system(buf);
		}
		char buf[64];
		sprintf(buf, "tar xf %s -C %s > /dev/null", cache_fname.c_str(), cont_folder.c_str());
		if(system(buf) != 0){
			panic("ERROR tar", EIO);
		} else {
			cout << "INFO extracted " << bname << "\n";
		}
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

void print_rootfs(string &root) {
	printf("Ccont 0.0.1 ==== Nikola Tasic ==== https://github.com/7aske/ccont\n");
	printf("Available root file systems:\n");
	printf(_HELPFORMAT, "ubuntu", "Ubuntu 18.10");
	printf(_HELPFORMAT, "alpine", "Alpine 3.9.3");
	printf(_HELPFORMAT, "", "");

	struct dirent* dir;
	DIR* dirp;
	if ((dirp = opendir(root.c_str())) != nullptr) {
		while ((dir = readdir(dirp)) != nullptr) {
			if (strncmp(dir->d_name, ".", 1) != 0) {
				char noext[64];
				char* p;
				strcpy(noext, dir->d_name);
				p = strchr(noext, '.');
				*p = '\0';
				printf(_HELPFORMAT, noext, dir->d_name);
			}
		}
		closedir(dirp);
	}
}

void panic(const char* message, int err) {
	errno = err;
	std::perror(message);
	exit(EXIT_FAILURE);
}
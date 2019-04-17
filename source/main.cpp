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
#include "jailubuntu.cpp"
#include "jailalpine.cpp"

#define _HELPFORMAT "%-25s\t%s\n"
#define _ROOTFSFORMAT "%-25s\t%s\n"

void panic(const char*);

void setup_ubuntucont(const char*, string);

void setup_alpinecont(const char*, string);

void print_rootfs();

void print_help();

string get_ccont_dir(char* arg0);

bool use_alpine = false;
bool use_ubuntu = false;

struct avail_rootfs {
    char ubuntu[32];
    char alpine[32];
};

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
    int cmd_start = 0;
    struct avail_rootfs availRootfs = {
            "ubuntu1810-base",
            "alpine939"
    };
    string dir = get_ccont_dir(args[0]);

    for (int i = 0; i < argc; i++) {
        if (strcmp(args[i], "--alpine") == 0) {
            use_alpine = true;
        } else if (strcmp(args[i], "--ubuntu") == 0) {
            use_ubuntu = true;
        } else if (strcmp(args[i], "-e") == 0) {
            cmd_start = i;
        }
    }

    if (use_ubuntu) {
        setup_ubuntucont(availRootfs.ubuntu, dir);
        if (cmd_start != 0) {
            if (cmd_start == argc - 1) {
                cout << "ERROR please specify commands with -e flag\n";
            } else {
                UbuntuJail jail(availRootfs.ubuntu, &args[cmd_start + 1]);
            }
        } else {
            UbuntuJail jail(availRootfs.ubuntu, args[0]);
        }
    } else if (use_alpine) {
        setup_alpinecont(availRootfs.alpine, dir);
        if (cmd_start != 0) {
            if (cmd_start == argc - 1) {
                cout << "ERROR please specify commands with -e flag\n";
            } else {
                AlpineJail jail(availRootfs.alpine, &args[cmd_start + 1]);
            }
        } else {
            AlpineJail jail(availRootfs.alpine, args[0]);
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

void setup_ubuntucont(const char* bname, string dir) {
    char url[] = "http://cdimage.ubuntu.com/ubuntu-base/releases/18.10/release/ubuntu-base-18.10-base-amd64.tar.gz";
    const char* url_base = basename(url);

    dir = dirname((char*) dir.c_str());
    dir = dir.append("/cache");

    string folder("./rootfs/" + (string) bname);
    if (!UbuntuJail::exists_stat(dir.c_str())) {
        system(("mkdir -p " + dir).c_str());
    }

    // assume if there is no home folder that the rootfs doesn't exist
    if (!UbuntuJail::exists_stat((folder + "/home").c_str())) {
        system(("mkdir -p " + folder).c_str());

        // download rootfs archive if its not present
        if (!UbuntuJail::exists(dir + "/" + url_base)) {
            system(("wget -O " + dir + "/" + (string) url_base + " " + (string) url +
                    " -q --show-progress").c_str());
        }
        system(("tar xf " + dir + "/" + (string) url_base + " -C " + folder + " > /dev/null").c_str());
    }
}

void setup_alpinecont(const char* bname, string dir) {
    char url[] = "http://dl-cdn.alpinelinux.org/alpine/v3.9/releases/x86_64/alpine-minirootfs-3.9.3-x86_64.tar.gz";
    const char* url_base = basename(url);

    dir = dirname((char*) dir.c_str());
    dir = dir.append("/cache");

    string folder("./rootfs/" + (string) bname);
    // assume if there is no home folder that the rootfs doesn't exist
    if (!AlpineJail::exists_stat((folder + "/home").c_str())) {
        system(("mkdir -p " + folder).c_str());

        // download rootfs archive if its not present
        if (!AlpineJail::exists(dir + "/" + url_base)) {
            system(("wget -O " + dir + "/" + (string) url_base + " " + (string) url +
                    " -q --show-progress").c_str());
        }
        system(("tar xf " + dir + "/" + (string) url_base + " -C " + folder + " > /dev/null").c_str());
    }
}

void print_help() {
    printf("Ccont 0.0.1 ==== Nikola Tasic ==== https://github.com/7aske/ccont\n");
    printf("Usage:\n");
    printf(_HELPFORMAT, "ccont <rootfs>", "start a <rootfs> container");
    printf(_HELPFORMAT, "ccont <rootfs> -e <cmd> [args]", "start a <rootfs> container and execute");
    printf(_HELPFORMAT, "", "command <cmd> with arguments [args]");
    printf(_HELPFORMAT, "ccont --list, -l", "print a list of available file systems");
    printf(_HELPFORMAT, "ccont --help, -h", "print this message");
}

void print_rootfs() {
    printf("Ccont 0.0.1 ==== Nikola Tasic ==== https://github.com/7aske/ccont\n");
    printf("Available root file systems:\n");
    printf(_HELPFORMAT, "--ubuntu", "Ubuntu 18.10");
    printf(_HELPFORMAT, "--alpine", "Alpine 3.9.3");
}

string get_ccont_dir(char* arg0) {
//    cout << "INFO parent PID: " f<< getpid() << endl;
    string dir(UbuntuJail::readcmd("which " + string(arg0)));
    dir = UbuntuJail::readcmd("readlink " + dir);
    dir = dir.substr(0, dir.size() - 1);
    return dir;
}


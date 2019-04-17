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


void panic(const char*);

void setup_ubuntucont(const char*, string);

void setup_alpinecont(const char*, string);

void list_rootfs();

string get_ccont_dir(char* arg0);

bool use_alpine = false;
bool use_ubuntu = false;


using namespace std;

int main(int argc, char* args[]) {
    if (getuid() != 0) {
        errno = EACCES;
        panic("ERROR");
    }
    int cmd_start = 0;
    char rootfs[32];
    string dir = get_ccont_dir(args[0]);

    // TODO: arg rootfs picker
    for (int i = 0; i < argc; i++) {
    // printf("%s\n", args[i]);
        if (strcmp(args[i], "--alpine") == 0) {
            use_alpine = true;
        } else if (strcmp(args[i], "--ubuntu") == 0) {
            use_ubuntu = true;
        } else if (strcmp(args[i], "-e") == 0) {
            cmd_start = i;
        }
    }

    if (use_ubuntu) {
        strncpy(rootfs, "ubuntu1810-base", 32);
        setup_ubuntucont(rootfs, dir);
        if (cmd_start != 0) {
            if (cmd_start == argc - 1) {
                cout << "ERROR please specify commands with -e flag\n";
            } else {
                UbuntuJail jail(rootfs, &args[cmd_start + 1]);
            }
        } else {
            UbuntuJail jail(rootfs, args[0]);
        }
    } else if (use_alpine) {
        strncpy(rootfs, "alpine939", 32);
        setup_alpinecont(rootfs, dir);
        if (cmd_start != 0) {
            if (cmd_start == argc - 1) {
                cout << "ERROR please specify commands with -e flag\n";
            } else {
                AlpineJail jail(rootfs, &args[cmd_start + 1]);
            }
        } else {
            AlpineJail jail(rootfs, args[0]);
        }
    } else {
        cout << "ERROR please specify a rootfs\n";
        list_rootfs();
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
    dir = dir.append("/tmp");

    string folder("./rootfs/" + (string) bname);
    if (!UbuntuJail::exists_stat(dir.c_str())) {
        system(("mkdir -p " + dir).c_str());
    }

    // assume if there is no home folder that the rootfs doesn't exist
    if (!UbuntuJail::exists_stat((folder + "/home").c_str())) {
        system(("mkdir -p " + folder).c_str());

        // download rootfs archive if its not present
        if (!UbuntuJail::exists(dir + "/" + url_base)) {
            system(("wget -O " + dir + "/" + (string) url_base + " " + (string) url + " -q --show-progress").c_str());
        }
        system(("tar xf " + dir + "/" + (string) url_base + " -C " + folder + " > /dev/null").c_str());
    }
}

void setup_alpinecont(const char* bname, string dir) {
    char url[] = "http://dl-cdn.alpinelinux.org/alpine/v3.9/releases/x86_64/alpine-minirootfs-3.9.3-x86_64.tar.gz";
    const char* url_base = basename(url);

    dir = dirname((char*) dir.c_str());
    dir = dir.append("/tmp");

    string folder("./rootfs/" + (string) bname);
    // assume if there is no home folder that the rootfs doesn't exist
    if (!AlpineJail::exists_stat((folder + "/home").c_str())) {
        system(("mkdir -p " + folder).c_str());

        // download rootfs archive if its not present
        if (!AlpineJail::exists(dir + "/" + url_base)) {
            system(("wget -O " + dir + "/" + (string) url_base + " " + (string) url + " -q --show-progress").c_str());
        }
        system(("tar xf " + dir + "/" + (string) url_base + " -C " + folder + " > /dev/null").c_str());
    }
}

void list_rootfs() {
    cout << "--ubuntu Ubuntu 18.10" << endl;
    cout << "--alpine Alpine 3.9.3" << endl;
}

string get_ccont_dir(char* arg0) {
//    cout << "INFO parent PID: " << getpid() << endl;
    string dir(UbuntuJail::readcmd("which " + string(arg0)));
    dir = UbuntuJail::readcmd("readlink " + dir);
    dir = dir.substr(0, dir.size() - 1);
    return dir;
}

// TODO: select different linux distros

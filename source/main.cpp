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
#include "jail.cpp"

void panic(const char*);

void setup_ubuntucont(const char*, char*);

using namespace std;

int main(int argc, char* args[]) {
    // TODO: arg rootfs picker
    const char* rootfs = "ubuntu1810-base";
    cout << "parent pid: " << getpid() << endl;

    // download and extract minimal ubuntu-base rootfs
    cout << args[0] << endl;
    setup_ubuntucont(rootfs, args[0]);
    if (argc >= 2) {
        Jail jail(rootfs, args[1], &args[1]);
    } else {
        // setup and start the container in selected rootfs
        Jail jail(rootfs, &args[0]);
    }
    return EXIT_SUCCESS;
}

void panic(const char* message) {
    std::perror(message);
    exit(EXIT_FAILURE);
}

void setup_ubuntucont(const char* bname, char* ccont_root) {
    char url[] = "http://cdimage.ubuntu.com/ubuntu-base/releases/18.10/release/ubuntu-base-18.10-base-amd64.tar.gz";
    const char* url_base = basename(url);
    string dir(ccont_root);

    dir = dirname((char*)dir.c_str());
    dir = dir.append("/tmp");

    string folder("./rootfs/" + (string) bname);

    if (!Jail::exists_stat(dir.c_str())) {
        system(("mkdir -p " + dir).c_str());
    }

    // assume if there is no home folder that the rootfs doesn't exist
    if (!Jail::exists_stat((folder + "/home").c_str())) {
        system(("mkdir -p " + folder).c_str());

        // download rootfs archive if its not present
        if (!Jail::exists(dir + "/" + url_base)) {
            system(("wget -O " + dir + "/" + (string) url_base + " " + (string) url + " -q --show-progress").c_str());
        }
        system(("tar xf " + dir + "/" + (string) url_base + " -C " + folder + "> /dev/null").c_str());
    }
}

// TODO: select different linux distros

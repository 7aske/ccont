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

void panic(const char *);

void setup_ubuntucont(const char *);

using namespace std;

int main(int argc, char **args) {
    // TODO: arg rootfs picker
    const char *rootfs = "ubuntu1810-base";
    cout << "parent pid: " << getpid() << endl;

    // download and extract minimal ubuntu-base rootfs
    setup_ubuntucont(rootfs);

    // setup and start the container in selected rootfs
    Jail jail(rootfs);
    return EXIT_SUCCESS;
}

void panic(const char *message) {
    std::perror(message);
    exit(EXIT_FAILURE);
}

void setup_ubuntucont(const char *bname) {
    char url[] = "http://cdimage.ubuntu.com/ubuntu-base/releases/18.10/release/ubuntu-base-18.10-base-amd64.tar.gz";
    const char *url_base = basename(url);
    string folder("./rootfs/" + (string) bname);

    if (!Jail::exists_stat("./tmp")) {
        system("mkdir ./tmp");
    }

    // assume if there is no home folder that the rootfs doesn't exist
    if (!Jail::exists_stat((folder + "/home").c_str())) {
        system(("mkdir -p " + folder).c_str());

        // download rootfs archive if its not present
        if (!Jail::exists("./tmp/" + (string) url_base)) {
            system(("wget -O ./tmp/" + (string) url_base + " " + (string) url + " -q --show-progress").c_str());
        }
        system(("tar xf ./tmp/" + (string) url_base + " -C " + folder + "> /dev/null").c_str());
    }
}

// TODO: select different linux distros

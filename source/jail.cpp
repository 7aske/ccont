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

using namespace std;

class Jail {
public:
    explicit Jail(const char *);

    ~Jail();

    inline static bool exists_stat(const char *name) {
        struct stat fileStat{};
        return stat(name, &fileStat) == 0;
    }

    inline static bool exists(const std::string &name) {
        if (FILE *file = fopen(name.c_str(), "r")) {
            fclose(file);
            return true;
        } else {
            return false;
        }
    }

private:
    void setup_variables();

    void setup_root();

    void setup_bashrc();

    void setup_certs();

    void setup_resolvconf();

    void setup_ssldeb();

    void setup_installssl();

    void setup_dev();

    static int start(void *);

    template<typename ... Params>
    int run(Params...);

    char *allocate_stack(int = 65536);

    static void panic(const char *);


    const char *rootfs;
    char *cont_stack;

};

// tar -C ./rootfs/ubuntu1810-base -xf filename
Jail::Jail(const char *rootfs) {
    this->rootfs = rootfs;

    // stack for container
    this->cont_stack = allocate_stack();

    // bind /dev to container /dev
    this->setup_dev();

    // small bashrc for colorful terminal
    this->setup_bashrc();

    // copy default certs to container
    this->setup_certs();

    // pre-download required .deb packages to install ca-certificates
    this->setup_ssldeb();



    // flags to clone process tree and unix time sharing
    int n = clone(Jail::start, this->cont_stack, CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD, (void *) this);
    if (n <= 0) {
        panic("Error cloning process");
    }

    // wait for container shell to exit
    wait(nullptr);
}

void Jail::setup_variables() {
    clearenv();
    system(((string) "hostname " + this->rootfs).c_str());
    setenv("HOME", "/", 0);
    setenv("TERM", "xterm-256color", 0);
    setenv("PATH", "/bin/:/sbin/:/usr/bin/:/usr/sbin/", 0);
}

void Jail::setup_resolvconf() {
    ofstream fp("/etc/resolv.conf");
    cout << "INFO updating resolv.conf" << endl;
    if (fp.is_open()) {
        fp << "nameserver 8.8.8.8\nnameserver 8.4.4.2\n";
    } else {
        cout << "ERROR unable to open resolv.conf" << endl;
    }
    fp.close();
}

void Jail::setup_bashrc() {
    system(("cp ./config/.bashrc ./rootfs/" + (string) this->rootfs + "/").c_str());
}

void Jail::setup_root() {
    chroot(((string) "./rootfs/" + this->rootfs).c_str());
    chdir("/");
    if (mount("proc", "/proc", "proc", 0, nullptr) < 0) {
        panic("Unable to mount /proc");
    }

}

void Jail::setup_dev() {
    if (mount("/dev", ("./rootfs/" + (string) this->rootfs + "/dev").c_str(), "tmpfs", MS_BIND, nullptr) < 0) {
        panic("Unable to mount /dev");
    }
}

char *Jail::allocate_stack(int size) {
    auto *stack = new(nothrow) char[size];
    if (stack == nullptr) {
        panic("Cannot allocate stack memory");
    }
    // stack grows downwards so move ptr to the end
    return stack + size;
}

int Jail::start(void *args) {
    Jail *self = (Jail *) args;
    cout << "INFO root - " << self->rootfs << endl;

    self->setup_variables();
    self->setup_root();

    cout << "INFO mounted proc to /proc\n";
    cout << "INFO container pid: " << getpid() << endl;

    // setup nameservers for internet access
    self->setup_resolvconf();

    // install pre-downloaded ssl packages
    self->setup_installssl();
    self->run("/bin/bash");
    return EXIT_SUCCESS;
}

template<typename ... Params>
int Jail::run(Params ...params) {
    char *args[] = {(char *) params..., nullptr};
    return execvp(args[0], args);
}


void Jail::panic(const char *message) {
    std::perror(message);
    exit(-1);
}


void Jail::setup_certs() {
    system(("cp ./config/ca-certificates.conf ./rootfs/" + (string) this->rootfs + "/etc/").c_str());
}

void Jail::setup_ssldeb() {
    string path("./rootfs/" + (string) this->rootfs + "/tmp/openssl/");

    char url_openssl[] = "http://security.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1-1ubuntu2.1_amd64.deb";
    char url_libssl[] = "http://security.ubuntu.com/ubuntu/pool/main/o/openssl/openssl_1.1.1-1ubuntu2.1_amd64.deb";
    char url_cacert[] = "http://mirrors.kernel.org/ubuntu/pool/main/c/ca-certificates/ca-certificates_20180409_all.deb";

    string base_openssl(basename(url_openssl));
    string base_libssl(basename(url_libssl));
    string base_cacert(basename(url_cacert));

    string file_openssl(path + base_openssl);
    string file_libssl(path + base_libssl);
    string file_cacert(path + base_cacert);

    system(("mkdir -p " + path).c_str());

    if (!Jail::exists(file_libssl)) {
        system(("wget -O " + file_libssl + " " + url_libssl + " -q --show-progress").c_str());
    }
    if (!Jail::exists(file_openssl)) {
        system(("wget -O " + file_openssl + " " + url_openssl + " -q --show-progress").c_str());
    }
    if (!Jail::exists(file_cacert)) {
        system(("wget -O " + file_cacert + " " + url_cacert + " -q --show-progress").c_str());
    }
}

void Jail::setup_installssl() {

    // if the cmd is not available installed pre-downloaded packages
    // and re-run it to update ssl certs to make apt update possible
//    cout << system("update-ca-certificates") << endl;
    if (system("update-ca-certificates > /dev/null")) {
        chdir("/tmp/openssl/");
        char url_libssl[] = "http://security.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1-1ubuntu2.1_amd64.deb";
        char url_openssl[] = "http://security.ubuntu.com/ubuntu/pool/main/o/openssl/openssl_1.1.1-1ubuntu2.1_amd64.deb";
        char url_cacert[] = "http://mirrors.kernel.org/ubuntu/pool/main/c/ca-certificates/ca-certificates_20180409_all.deb";

        string base_libssl(basename(url_libssl));
        string base_openssl(basename(url_openssl));
        string base_cacert(basename(url_cacert));

        std::perror("ERROR");
        if (system(("dpkg -i " + base_libssl + "> /dev/null").c_str()) < 0) {
            panic("ERROR");
        }
        if (system(("dpkg -i " + base_openssl + "> /dev/null").c_str()) < 0) {
            panic("ERROR");
        }
        if (system(("dpkg -i " + base_cacert + "> /dev/null").c_str()) < 0) {
            panic("ERROR");
        }
        system("update-ca-certificates");
        chdir("/");
        system("apt update");
    }

}

Jail::~Jail() {
    // unmount container /proc from outside
    system(("umount ./rootfs/" + (string) this->rootfs + "/proc").c_str());

    // umount container /dev from outside
    system(("umount ./rootfs/" + (string) this->rootfs + "/dev").c_str());

}


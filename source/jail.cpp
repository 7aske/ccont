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
    explicit Jail(const char*, char* const*);

    explicit Jail(const char*, const char*, char* const*);

    ~Jail();

    inline static bool exists_stat(const char* name) {
        struct stat fileStat{};
        return stat(name, &fileStat) == 0;
    }

    inline static bool exists(const std::string &name) {
        if (FILE* file = fopen(name.c_str(), "r")) {
            fclose(file);
            return true;
        } else {
            return false;
        }
    }

    static string readcmd(string cmd);

private:
//    void setup_sigint_handler();

    void setup_variables();

    void setup_root(const char* = "/");

    void setup_bashrc(char*);

    void setup_certs(char*);

    void setup_resolvconf();

    void setup_ssldeb();

    void setup_installssl();

    void setup_dev();

    void setup_src();

    static int start(void*);

    static int start_cmd(void*);

    template<typename ... Params>
    int run(Params...);

    char* allocate_stack(int = 65536);


    static void panic(const char*);

    const char* rootfs;
    const char* cmd = nullptr;
    char* const* cmd_args = nullptr;

    char* cont_stack;
    long cont_stack_size = 65536;

};

// tar -C ./rootfs/ubuntu1810-base -xf filename
Jail::Jail(const char* rootfs, char* const* args) {
    this->rootfs = rootfs;

    // stack for container
    this->cont_stack = allocate_stack();

    // bind /dev to container /dev
    this->setup_dev();

    // small bashrc for colorful terminal
    this->setup_bashrc(args[0]);

    // copy default certs to container
    this->setup_certs(args[0]);

    // pre-download required .deb packages to install ca-certificates
    this->setup_ssldeb();

    // flags to clone process tree and unix time sharing
    int n = clone(Jail::start, this->cont_stack, CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD, (void*) this);
    if (n <= 0) {
        panic("Error cloning process");
    }

    // wait for container shell to exit
    wait(nullptr);
}

Jail::Jail(const char* rootfs, const char* cmd, char* const* args) {
    this->rootfs = rootfs;
    this->cmd = cmd;
    this->cmd_args = args;

    // allocate stack for container
    this->cont_stack = allocate_stack();

    // bind /dev to container /dev
    this->setup_dev();

    // this->setup_bashrc();

    this->setup_src();

    // flags to clone process tree and unix time sharing
    int n = clone(Jail::start_cmd, this->cont_stack, CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD, (void*) this);
    if (n <= 0) {
        panic("Error cloning process");
    }

    // wait for container shell to exit
    wait(nullptr);
}

int Jail::start(void* args) {
    Jail* self = (Jail*) args;

    self->setup_variables();

    self->setup_root();
    cout << "INFO mounted proc to /proc\n";

    cout << "INFO container pid: " << getpid() << endl;

    // setup nameservers for internet access
    self->setup_resolvconf();

    // install pre-downloaded ssl packages
    self->setup_installssl();

    if (self->run("/bin/bash") != 0) {
        perror("ERROR");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int Jail::start_cmd(void* args) {
    Jail* self = (Jail*) args;
    cout << "INFO root - " << self->rootfs << endl;
    cout << "INFO cmd - " << self->cmd << endl;

    self->setup_variables();

    self->setup_root("/src");

    cout << "INFO mounted proc to /proc\n";
    cout << "INFO mounted current dir to /src\n";

    cout << "INFO container pid: " << getpid() << endl;

    // setup nameservers for internet access
    self->setup_resolvconf();

    // install pre-downloaded ssl packages
    // self->setup_installssl();

    if (execvp(((string) self->cmd).c_str(), self->cmd_args) != 0) {
        panic("ERROR");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

template<typename ... Params>
int Jail::run(Params ...params) {
    char* args[] = {(char*) params..., nullptr};
    return execvp(args[0], args);
}


void Jail::panic(const char* message) {
    std::perror(message);
    exit(-1);
}

void Jail::setup_src() {
    system(("mkdir -p " + (string) "./rootfs/" + this->rootfs + "/src").c_str());
    if (mount("./", ((string) "./rootfs/" + this->rootfs + "/src").c_str(), "tmpfs", MS_BIND, nullptr) < 0) {
        panic("Unable to mount /src");
    }
}


void Jail::setup_root(const char* root) {
    chroot(((string) "./rootfs/" + this->rootfs).c_str());
    chdir(root);
    if (mount("proc", "/proc", "proc", 0, nullptr) < 0) {
        panic("Unable to mount /proc");
    }

}


void Jail::setup_variables() {
    clearenv();
    system(((string) "hostname " + this->rootfs).c_str());
    setenv("HOME", "/", 0);
    setenv("TERM", "xterm-256color", 0);
    setenv("PATH", "/bin/:/sbin/:/usr/bin/:/usr/sbin/:/src/", 0);
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

void Jail::setup_bashrc(char* ccont_root) {
    string dir(ccont_root);
    dir = readcmd("which " + dir);
    dir = readcmd("readlink " + dir);
    dir = dirname((char*) dir.c_str());
    system(("cp " + dir + "/config/.bashrc ./rootfs/" + this->rootfs + "/").c_str());
}

char* Jail::allocate_stack(int size) {
    auto* stack = new(nothrow) char[size];
    if (stack == nullptr) {
        panic("Cannot allocate stack memory");
    }
    // stack grows downwards so move ptr to the end
    this->cont_stack_size = size;
    return stack + size;
}

void Jail::setup_dev() {
    if (mount("/dev", ("./rootfs/" + (string) this->rootfs + "/dev").c_str(), "tmpfs", MS_BIND, nullptr) < 0) {
        panic("Unable to mount /dev");
    }
}

void Jail::setup_certs(char* ccont_root) {
    string dir(ccont_root);
    dir = readcmd("which " + dir);
    dir = readcmd("readlink " + dir);
    dir = dirname((char*) dir.c_str());
    system(("cp " + dir + "/config/ca-certificates.conf ./rootfs/" + this->rootfs + "/etc/").c_str());
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
    if (system("update-ca-certificates > /dev/null")) {
        chdir("/tmp/openssl/");
        char url_libssl[] = "http://security.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1-1ubuntu2.1_amd64.deb";
        char url_openssl[] = "http://security.ubuntu.com/ubuntu/pool/main/o/openssl/openssl_1.1.1-1ubuntu2.1_amd64.deb";
        char url_cacert[] = "http://mirrors.kernel.org/ubuntu/pool/main/c/ca-certificates/ca-certificates_20180409_all.deb";

        string base_libssl(basename(url_libssl));
        string base_openssl(basename(url_openssl));
        string base_cacert(basename(url_cacert));

        if (system(("dpkg -i " + base_libssl + " &> /dev/null").c_str()) < 0) {
            cout << "ERROR unable to install libssl\n";
        } else {
            cout << "INFO installed libssl;\n";
        }
        if (system(("dpkg -i " + base_openssl + " &> /dev/null").c_str()) < 0) {
            cout << "ERROR unable to install openssl\n";
        } else {
            cout << "INFO installed openssl\n";
        }
        if (system(("dpkg -i " + base_cacert + " &> /dev/null").c_str()) < 0) {
            cout << "ERROR unable to install ca-certificates\n";
        } else {
            cout << "INFO installed ca-certificates\n";
        }
        system("update-ca-certificates");
        chdir("/");
        system("apt update");
    }

}

Jail::~Jail() {
    // free allocated container stack
    delete[] (this->cont_stack - this->cont_stack_size);

    // unmount container /proc from outside
    cout << "INFO umounting /proc\n";
    if (system(("umount ./rootfs/" + (string) this->rootfs + "/proc").c_str()) < 0) {
        cout << "ERROR umounting /proc\n";
    }

    // umount container /dev from outside
    cout << "INFO umounting /dev\n";
    if (system(("umount ./rootfs/" + (string) this->rootfs + "/dev").c_str()) < 0) {
        cout << "ERROR umounting /dev\n";
    }

    // unmount /src | if cmd is not set then /src is not even mounted
    if (this->cmd != nullptr) {
        cout << "INFO umounting /src\n";
        if (system(("umount ./rootfs/" + (string) this->rootfs + "/src").c_str()) < 0) {
            cout << "ERROR umounting /src\n";
        }
    }
}

string Jail::readcmd(string cmd) {

    string data;
    FILE* stream;
    const int max_buffer = 256;
    char buffer[max_buffer];
    cmd.append(" 2>&1");

    stream = popen(cmd.c_str(), "r");
    if (stream) {
        while (!feof(stream))
            if (fgets(buffer, max_buffer, stream) != nullptr) data.append(buffer);
        pclose(stream);
    }
    return data;
}
//void Jail::setup_sigint_handler() {
//    struct sigaction sigIntHandler{};
//
//    sigIntHandler.sa_handler = Jail::_sigint_handler;
//    sigemptyset(&sigIntHandler.sa_mask);
//    sigIntHandler.sa_flags = 0;
//
//    sigaction(SIGINT, &sigIntHandler, nullptr);
//}
//void _sigint_handler(int) {
//    // unmount container /proc from outside
//    cout << "INFO umounting /proc";
//    if (system(("umount ./rootfs/" + (string) this->rootfs + "/proc").c_str()) < 0) {
//        cout << "ERROR umounting /proc";
//    }
//
//    // umount container /dev from outside
//    cout << "INFO umounting /dev";
//    if (system(("umount ./rootfs/" + (string) this->rootfs + "/dev").c_str()) < 0) {
//        cout << "ERROR umounting /dev";
//    }
//
//    // unmount /src | if cmd is not set then /src is not even mounted
//    if (this->cmd != nullptr) {
//        cout << "INFO umounting /src";
//        if (system(("umount ./rootfs/" + (string) this->rootfs + "/src").c_str()) < 0) {
//            cout << "ERROR umounting /src";
//        }
//    }
//}
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
#include <cstring>


using namespace std;

class AlpineJail {
public:
    explicit AlpineJail(const char*, char* const);

    explicit AlpineJail(const char*, char* const*);

    ~AlpineJail();

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
    void setup_sigint_handler();

    void setup_sigint_handler_cmd();

    static void _sigint_handler(int);

    static void _sigint_handler_cmd(int);

    void setup_variables();

    void setup_root(const char* = "/");

    void setup_bashrc(char*);

    void setup_bash();

    void setup_certs(char*);

    void setup_resolvconf();

    void setup_dev();

    void setup_src();

    static int start(void*);

    static int start_cmd(void*);

    template<typename ... Params>
    int run(Params...);

    char* allocate_stack(int = 65536);


    static void panic(const char*);

    const char* rootfs;
    char** cmd_args = nullptr;

    char* cont_stack;
    long cont_stack_size = 65536;

};

// tar -C ./rootfs/ubuntu1810-base -xf filename
AlpineJail::AlpineJail(const char* rootfs, char* const arg) {
//    char buf[128];
    int cpid;

    this->rootfs = rootfs;

    // stack for container
    this->cont_stack = allocate_stack();

    // bind /dev to container /dev
    this->setup_dev();

    // small bashrc for colorful terminal
    this->setup_bashrc(arg);

    // flags to clone process tree and unix time sharing
    if ((cpid = clone(AlpineJail::start, this->cont_stack, CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD, (void*) this)) <= 0) {
        panic("Error cloning process");
    } else {
        cout << "INFO container PID: " << cpid << endl;
    }

    // wait for container shell to exit
    wait(nullptr);
}

AlpineJail::AlpineJail(const char* rootfs, char* const* args) {
    if (args == nullptr) {
        panic("ERROR -e specified with no command");
    }
    char buf[128];
    char callcmd[] = "ccont";
    int cpid;

    this->rootfs = rootfs;
    this->cmd_args = (char**) args;

    this->setup_sigint_handler();

    // allocate stack for container
    this->cont_stack = allocate_stack();

    // bind /dev to container /dev
    this->setup_dev();

    this->setup_bashrc(callcmd);

    this->setup_src();

    getcwd(buf, 128);
    cout << "INFO mounted " << buf << " to /src\n";

    // flags to clone process tree and unix time sharing
    if ((cpid = clone(AlpineJail::start_cmd, this->cont_stack, CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD, (void*) this)) <=
        0) {
        panic("Error cloning process");
    } else {
        cout << "INFO container PID: " << cpid << endl;
    }

    // wait for container shell to exit
    wait(nullptr);
}

int AlpineJail::start(void* args) {
    auto* self = (AlpineJail*) args;

    self->setup_sigint_handler_cmd();

    self->setup_variables();

    self->setup_root();
    cout << "INFO mounted proc to /proc\n";

    cout << "INFO container PID: " << getpid() << endl;

    // setup nameservers for internet access
    self->setup_resolvconf();

    self->setup_bash();

    if (self->run("/bin/bash") != 0) {
        perror("ERROR");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int AlpineJail::start_cmd(void* args) {
    auto* self = (AlpineJail*) args;

    cout << "INFO root - " << self->rootfs << endl;

    self->setup_sigint_handler();

    self->setup_variables();

    self->setup_root("/src");
    cout << "INFO mounted proc to /proc\n";

    cout << "INFO container PID: " << getpid() << endl;

    // setup nameservers for internet access
    self->setup_resolvconf();

    self->setup_bash();

    // go back to src after doing installtls
    chdir("/src");

    // BUFFER OVERFLOW EXPLOIT RIGHT THERE WOHOO
    // Managed to hack process into executing inside an
    // invoked shell by starting the shell with temp rc file
    FILE* startup = fopen("/.startup", "w");
    fprintf(startup, "%s\n", "source $HOME/.bashrc");
    while (*self->cmd_args != nullptr) {
        fprintf(startup, "%s", *self->cmd_args);
        fprintf(startup, "%s", " ");
        self->cmd_args++;
    }
    fclose(startup);

    if (system("/bin/bash --init-file /.startup") <= 0) {
        panic("ERROR");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


template<typename ... Params>
int AlpineJail::run(Params ...params) {
    char* args[] = {(char*) params..., nullptr};
    return execvp(args[0], args);
}


void AlpineJail::panic(const char* message) {
    std::perror(message);
    exit(-1);
}

void AlpineJail::setup_src() {
    system(("mkdir -p " + (string) "./rootfs/" + this->rootfs + "/src").c_str());
    if (mount("./", ((string) "./rootfs/" + this->rootfs + "/src").c_str(), "tmpfs", MS_BIND, nullptr) < 0) {
        panic("Unable to mount /src");
    }
}


void AlpineJail::setup_root(const char* root) {
    chroot(((string) "./rootfs/" + this->rootfs).c_str());
    chdir(root);
    if (mount("proc", "/proc", "proc", 0, nullptr) < 0) {
        panic("Unable to mount /proc");
    }

}


void AlpineJail::setup_variables() {
    clearenv();
    system(((string) "hostname " + this->rootfs).c_str());
    setenv("HOME", "/", 0);
    setenv("DISPLAY", ":0.0", 0);
    setenv("TERM", "xterm-256color", 0);
    setenv("PATH", "/bin/:/sbin/:/usr/bin/:/usr/sbin/:/src/", 0);
}

void AlpineJail::setup_resolvconf() {
    ofstream fp("/etc/resolv.conf");
    cout << "INFO updating resolv.conf" << endl;
    if (fp.is_open()) {
        fp << "nameserver 8.8.8.8\nnameserver 8.4.4.2\n";
    } else {
        cout << "ERROR unable to open resolv.conf" << endl;
    }
    fp.close();
}

void AlpineJail::setup_bashrc(char* ccont_root) {
    string dir(ccont_root);
    dir = readcmd("which " + dir);
    dir = readcmd("readlink " + dir);
    dir = dirname((char*) dir.c_str());
    system(("cp " + dir + "/config/.bashrc ./rootfs/" + this->rootfs + "/").c_str());
}

char* AlpineJail::allocate_stack(int size) {
    auto* stack = new(nothrow) char[size];
    if (stack == nullptr) {
        panic("Cannot allocate stack memory");
    }
    // stack grows downwards so move ptr to the end
    this->cont_stack_size = size;
    return stack + size;
}

void AlpineJail::setup_dev() {
    if (mount("/dev", ("./rootfs/" + (string) this->rootfs + "/dev").c_str(), "tmpfs", MS_BIND, nullptr) < 0) {
        panic("Unable to mount /dev");
    }
    cout << "INFO mounted dev to /dev\n";
}

void AlpineJail::setup_certs(char* ccont_root) {
    string dir(ccont_root);
    dir = readcmd("which " + dir);
    dir = readcmd("readlink " + dir);
    dir = dirname((char*) dir.c_str());
    system(("cp " + dir + "/config/ca-certificates.conf ./rootfs/" + this->rootfs + "/etc/").c_str());
}

string AlpineJail::readcmd(string cmd) {

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

void AlpineJail::setup_sigint_handler() {
    struct sigaction sigIntHandler{};

    sigIntHandler.sa_handler = AlpineJail::_sigint_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, nullptr);
}

void AlpineJail::setup_sigint_handler_cmd() {
    struct sigaction sigIntHandler{};

    sigIntHandler.sa_handler = AlpineJail::_sigint_handler_cmd;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, nullptr);
}

void AlpineJail::_sigint_handler(int) {
    cout << "Handling SIGINT for " << getpid() << endl;
    sleep(1);
    exit(0);
}

void AlpineJail::_sigint_handler_cmd(int) {
    cout << "Handling SIGINT for " << getpid() << endl;
    exit(0);
}

AlpineJail::~AlpineJail() {
    // free allocated container stack
    delete[] (this->cont_stack - this->cont_stack_size);

    // unmount container /proc from outside
    cout << "INFO umounting /proc\n";
    if (system(("umount -f ./rootfs/" + (string) this->rootfs + "/proc").c_str()) < 0) {
        cout << "ERROR umounting /proc\n";
    }

    // unmount container /dev from outside
    cout << "INFO umounting /dev\n";
    if (system(("umount ./rootfs/" + (string) this->rootfs + "/dev").c_str()) < 0) {
        cout << "ERROR umounting /dev\n";
    }

    // unmount /src
    cout << "INFO umounting /src\n";
    if (system(("umount -f ./rootfs/" + (string) this->rootfs + "/src").c_str()) < 0) {
        cout << "ERROR umounting /src\n";
    }
}

void AlpineJail::setup_bash() {
    system("apk add bash");
}


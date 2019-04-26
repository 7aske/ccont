
#include <ostream>

using namespace std;

class Jail {
public:
	explicit Jail() {
		// this->cont_stack = Jail::allocate_stack(this->cont_stack_size);
	}

	void init(char*, char*);

	void init(char*, char* const*, char*);

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

	const string &getContRoot() const;

	void setContRoot(const string &contRoot);

	const string &getName() const;

	void setName(const string &name);

	long getContStackSize() const;

	void setContStackSize(long contStackSize);

	const string &getRoot() const;

	void setRoot(const string &root);

	bool isRmCont() const;

	void setRmCont(bool rmCont);

	bool isBuildCont() const;

	void setBuildCont(bool buildCont);

	char* getArgv0() const;

	void setArgv0(char* argv0);

	const string &getId() const;

	void setId(const char* id);

	friend ostream &operator<<(ostream &os, const Jail &jail);

private:
	void setup_variables();

	void setup_root();

	void setup_bashrc();

	static void setup_resolvconf();

	static void setup_dev();

	void setup_src();

	static int start(void*);

	static int start_cmd(void*);

	static char* allocate_stack(int);

	static void panic(const char*);

	template<typename ... Params>
	int run(Params...);

	bool rm_cont = false;
	bool build_cont = false;

	string root;
	string cont_root = "";
	string name;
	string id;

	char* argv0;

	char** cmd_args = nullptr;
	char* cont_stack = nullptr;
	long cont_stack_size = 65536;
};

const string &Jail::getId() const {
	return id;
}

void Jail::setId(const char* id) {
	Jail::id = string(id);
}

char* Jail::getArgv0() const {
	return argv0;
}

void Jail::setArgv0(char* argv) {
	Jail::argv0 = argv;
}

bool Jail::isBuildCont() const {
	return build_cont;
}

void Jail::setBuildCont(bool buildCont) {
	build_cont = buildCont;
}

bool Jail::isRmCont() const {
	return rm_cont;
}

void Jail::setRmCont(bool rmCont) {
	rm_cont = rmCont;
}

const string &Jail::getRoot() const {
	return root;
}

void Jail::setRoot(const string &root) {
	Jail::root = root;
}

const string &Jail::getContRoot() const {
	return cont_root;
}

void Jail::setContRoot(const string &contRoot) {
	cont_root = contRoot;
}

const string &Jail::getName() const {
	return name;
}

void Jail::setName(const string &name) {
	Jail::name = name;
}

long Jail::getContStackSize() const {
	return cont_stack_size;
}

void Jail::setContStackSize(long contStackSize) {
	cont_stack_size = contStackSize;
}


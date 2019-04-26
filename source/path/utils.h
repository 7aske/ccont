#include <pthread.h>
namespace path {
	bool exists_stat(const char* name) {
		struct stat fileStat{};
		return stat(name, &fileStat) == 0;
	}

	bool exists_stat(std::string &name) {
		struct stat fileStat{};
		return stat(name.c_str(), &fileStat) == 0;
	}


	bool exists(const std::string &name) {
		if (FILE* file = fopen(name.c_str(), "r")) {
			fclose(file);
			return true;
		} else {
			return false;
		}
	}

	bool exists(char* const name) {
		if (FILE* file = fopen(name, "r")) {
			fclose(file);
			return true;
		} else {
			return false;
		}
	}


}

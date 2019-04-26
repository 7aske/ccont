#pragma once

#include <string.h>
#include <pthread.h>

namespace exec {
	std::string readcmd(std::string cmd) {
		FILE* stream;
		std::string out;
		const int max_buffer = 1024;
		char buffer[max_buffer];
		cmd.append(" 2>&1");

		stream = popen(cmd.c_str(), "r");
		if (stream) {
			while (!feof(stream))
				if (fgets(buffer, max_buffer, stream) != nullptr) out.append(buffer);
			pclose(stream);
		}
		return out;
	}
}

namespace path {
	bool exists_stat(const char* name) {
		struct stat fileStat{};
		return stat(name, &fileStat) == 0;
	}

	bool exists_stat(std::string &name) {
		return exists_stat(name.c_str());
	}


	bool exists(const char* name) {
		if (FILE* file = fopen(name, "r")) {
			fclose(file);
			return true;
		} else {
			return false;
		}
	}

	bool exists(const std::string &name) {
		return exists(name.c_str());
	}

	std::string abs(char* const cmd) {
		std::string dir_cmd(cmd);
		dir_cmd.insert(0, "which ");
		dir_cmd = exec::readcmd(dir_cmd);
		dir_cmd.insert(0, "readlink ");
		return exec::readcmd(dir_cmd);
	}

}

namespace dir {
	bool contains(std::string &path, char* const fname) {
		struct dirent* dir;
		DIR* dirp;
		if ((dirp = opendir(path.c_str())) != nullptr) {
			while ((dir = readdir(dirp)) != nullptr) {
				if (strncmp(dir->d_name, ".", 1) != 0) {
					char noext[64];
					char* p;
					strcpy(noext, dir->d_name);
					p = strchr(noext, '.');
					*p = '\0';
					if (strcmp(noext, fname) == 0) {
						closedir(dirp);
						return true;
					}
				}
			}
			closedir(dirp);
		}
		return false;
	}

	bool contains(std::string &path, std::string &fname) {
		return contains(path, (char* const) fname.c_str());
	}
}

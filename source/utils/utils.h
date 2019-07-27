#pragma once

#include <string.h>
#include <pthread.h>


int exists_stat(char const* pathname) {
	struct stat fileStat;
	return stat(pathname, &fileStat) == 0;
}

int exists(char const* pathname) {
	FILE* file = fopen(pathname, "r");
	if (file) {
		fclose(file);
		return 1;
	} else {
		return 0;
	}
}

char* abspth(char* const cmd) {
	char* out = (char*) calloc(128, sizeof(char));
	char buf[128];
	struct stat statbuf;

	strcpy(buf, "which ");
	strcat(buf, cmd);

	FILE* pid = popen(buf, "r");
	memset(buf, 0, 128);

	if (pid) {
		fread(out, sizeof(char), 128, pid);
		*strrchr(out, '\n') = '\0';
	}

	lstat(out, &statbuf);
	if (S_ISLNK(statbuf.st_mode)){
		char* readlink = "readlink ";

		strcpy(buf, readlink);
		strcat(buf, out);
		memset(out, 0, 128);

		FILE* pid2 = popen(buf, "r");
		if (pid2) {
			fread(out, sizeof(char), 128, pid2);
			*strrchr(out, '\n') = '\0';
		}
	}
	return out;
}


int contains(char const* pth, char* const fname) {
	struct dirent* dir;
	DIR* dirp;
	if ((dirp = opendir(pth)) != NULL) {
		while ((dir = readdir(dirp)) != NULL) {
			if (strncmp(dir->d_name, ".", 1) != 0) {
				char noext[64];
				char* p;
				strcpy(noext, dir->d_name);
				p = strchr(noext, '.');
				*p = '\0';
				if (strcmp(noext, fname) == 0) {
					closedir(dirp);
					free(dirp);
					return 1;
				}
			}
		}
	}
	closedir(dirp);
	free(dirp);
	return 0;
}

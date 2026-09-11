#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>

int pullpkg(const char *target) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "curl -L -sSf -O %s", target);
    int pullstat = system(cmd);

    if (pullstat == -1) {
        printf("failed to run shell\n");
        return 1;
    } else {
        int status = WEXITSTATUS(pullstat);
        if (status == 127) {
            printf("no curl binary\n");
            return 1;
        } else if (status != 0) {
            printf("invalid target!\n");
            return 1;
        } else {
            printf("downloaded successfully\n");
            return 0;
        }
    }
}

void extract(void) {
    int stat = system("tar -xf *.tar*");
    int statu = WEXITSTATUS(stat);
    if (statu == 0) {
        system("rm -f *.tar *.tar.gz *.tgz");
    } else {
        printf("error extracting file!\n");
    }
}

void enter_extracted_dir(char *found_dir, size_t max_len) {
    DIR *d = opendir(".");
    if (!d) return;
    struct dirent *dir;
    
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type == DT_DIR && dir->d_name[0] != '.') {
            if (strcmp(dir->d_name, "test_vm") == 0 || 
                strcmp(dir->d_name, "inigo") == 0 || 
                strcmp(dir->d_name, "linux-7.2.4") == 0 ||
                strcmp(dir->d_name, "inipkgws") == 0) {
                continue;
            }
            
            char path[512];
            snprintf(path, sizeof(path), "%s/Makefile", dir->d_name);
            if (access(path, F_OK) == 0) {
                strncpy(found_dir, dir->d_name, max_len);
                chdir(dir->d_name);
                closedir(d);
                return;
            }
        }
    }
    rewinddir(d);
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type == DT_DIR && dir->d_name[0] != '.') {
            if (strcmp(dir->d_name, "test_vm") == 0 || 
                strcmp(dir->d_name, "inigo") == 0 || 
                strcmp(dir->d_name, "linux-7.2.4") == 0 ||
                strcmp(dir->d_name, "inipkgws") == 0) {
                continue;
            }
            strncpy(found_dir, dir->d_name, max_len);
            chdir(dir->d_name);
            closedir(d);
            return;
        }
    }
    closedir(d);
}

int check_build_config(void) {
    if (access("Makefile", F_OK) == 0 || access("configure", F_OK) == 0 || access("CMakeLists.txt", F_OK) == 0) {
        printf("found build config compiling...\n");
        system("make");
        return 0;
    } else {
        printf("no make config found\n");
        return 2;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: %s <url>\n", argv[0]);
        return 1;
    }

    if (pullpkg(argv[1]) != 0) {
        return 1;
    }

    extract();

    char ext_dir[512] = {0};
    enter_extracted_dir(ext_dir, sizeof(ext_dir));

    int res = check_build_config();

    if (strlen(ext_dir) > 0) {
        chdir("..");
        char clean_cmd[512];
        snprintf(clean_cmd, sizeof(clean_cmd), "rm -rf %s", ext_dir);
        system(clean_cmd);
        printf("cleaned up build directory: %s\n", ext_dir);
    }

    return res;
}

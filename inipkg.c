#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

static int run_command(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }

    if (!WIFEXITED(status)) {
        return -1;
    }

    return WEXITSTATUS(status);
}

static int is_skipped_name(const char *name) {
    return strcmp(name, "test_vm") == 0 ||
           strcmp(name, "inigo") == 0 ||
           strcmp(name, "linux-7.2.4") == 0 ||
           strcmp(name, "inipkgws") == 0;
}

static int is_directory(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_archive_name(const char *name) {
    size_t len = strlen(name);
    return (len > 4 && strcmp(name + len - 4, ".tar") == 0) ||
           (len > 7 && strcmp(name + len - 7, ".tar.gz") == 0) ||
           (len > 4 && strcmp(name + len - 4, ".tgz") == 0) ||
           (len > 7 && strcmp(name + len - 7, ".tar.xz") == 0);
}

static int find_downloaded_archive(char *archive, size_t max_len) {
    DIR *d = opendir(".");
    if (!d) {
        return 1;
    }

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_name[0] == '.') {
            continue;
        }
        if (is_archive_name(dir->d_name)) {
            snprintf(archive, max_len, "%s", dir->d_name);
            closedir(d);
            return 0;
        }
    }

    closedir(d);
    return 1;
}

int pullpkg(const char *target) {
    char *const args[] = {"curl", "-L", "-sSf", "-O", "--", (char *)target, NULL};
    int pullstat = run_command(args);

    if (pullstat < 0) {
        printf("failed to run shell\n");
        return 1;
    }

    if (pullstat == 127) {
        printf("no curl binary\n");
        return 1;
    }
    if (pullstat != 0) {
        printf("invalid target!\n");
        return 1;
    }

    printf("downloaded successfully\n");
    return 0;
}

int extract(char *archive, size_t archive_len) {
    if (find_downloaded_archive(archive, archive_len) != 0) {
        printf("no archive file found\n");
        return 1;
    }

    char *const args[] = {"tar", "-xf", archive, NULL};
    int status = run_command(args);
    if (status != 0) {
        printf("error extracting file!\n");
        return 1;
    }

    unlink(archive);
    return 0;
}

void enter_extracted_dir(char *found_dir, size_t max_len) {
    DIR *d = opendir(".");
    if (!d) return;
    struct dirent *dir;
    
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_name[0] == '.' || is_skipped_name(dir->d_name) || !is_directory(dir->d_name)) {
            continue;
        }

        char path[512];
        snprintf(path, sizeof(path), "%s/Makefile", dir->d_name);
        if (access(path, F_OK) == 0) {
            snprintf(found_dir, max_len, "%s", dir->d_name);
            chdir(dir->d_name);
            closedir(d);
            return;
        }
    }
    rewinddir(d);
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_name[0] == '.' || is_skipped_name(dir->d_name) || !is_directory(dir->d_name)) {
            continue;
        }

        snprintf(found_dir, max_len, "%s", dir->d_name);
        chdir(dir->d_name);
        closedir(d);
        return;
    }
    closedir(d);
}

int check_build_config(void) {
    if (access("Makefile", F_OK) == 0 || access("configure", F_OK) == 0 || access("CMakeLists.txt", F_OK) == 0) {
        printf("found build config compiling...\n");
        char *const args[] = {"make", NULL};
        int status = run_command(args);
        return status == 0 ? 0 : 2;
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

    char archive[512] = {0};
    if (extract(archive, sizeof(archive)) != 0) {
        return 1;
    }

    char ext_dir[512] = {0};
    enter_extracted_dir(ext_dir, sizeof(ext_dir));

    int res = check_build_config();

    if (strlen(ext_dir) > 0) {
        chdir("..");
        char *const args[] = {"rm", "-rf", "--", ext_dir, NULL};
        run_command(args);
        printf("cleaned up build directory: %s\n", ext_dir);
    }

    return res;
}

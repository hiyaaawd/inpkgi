#include <unistd.h>
#include <stdio.h>
#include <mntent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#define MAX_DRIVES 16

struct DriveInfo {
    char name[64];
    char mountpoint[64];
};

struct DriveInfo detected_drives[MAX_DRIVES];
int drive_count = 0;

void spawn_recovery_shell(void){
    int recovery_count = 0;
    
    retryshellspawn:
    recovery_count++;
    pid_t pid = fork();

    if (pid == 0){
        /* detach from inigo's session (it already owns /dev/console as its
           controlling tty), then start a fresh session and claim the
           console as OUR controlling tty so the shell gets real job control */
        setsid();

        int tty_fd = open("/dev/console", O_RDWR);
        if (tty_fd >= 0) {
            ioctl(tty_fd, TIOCSCTTY, 0);
            dup2(tty_fd, 0);
            dup2(tty_fd, 1);
            dup2(tty_fd, 2);
            if (tty_fd > 2) close(tty_fd);
        }

        char *args[] = {
            "/bin/sh", 
            "-c", 
            "echo 'Recovery Mode Active'; exec /bin/sh", 
            NULL
        };
        
        char *env[] = {"PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL};
        execve("/bin/sh", args, env);
        abort();
    } else if (pid > 0){
        printf("Recovery shell spawn count: %d\n", recovery_count);
        waitpid(pid, NULL, 0);
        goto retryshellspawn;
    }
}

void get_drives(void){
    FILE *fp = setmntent("/proc/mounts", "r");
    if (!fp){
        return;
    }

    struct mntent *ent;
    while ((ent = getmntent(fp)) != NULL) {
        if (strncmp(ent->mnt_fsname, "/dev/sd", 7) == 0 && drive_count < MAX_DRIVES) {
            snprintf(detected_drives[drive_count].name, sizeof(detected_drives[drive_count].name), "%s", ent->mnt_fsname);
            snprintf(detected_drives[drive_count].mountpoint, sizeof(detected_drives[drive_count].mountpoint), "%s", ent->mnt_dir);
            drive_count++;
            
            printf("found drive: %s mounted at %s\n", ent->mnt_fsname, ent->mnt_dir);
        }
    }
    endmntent(fp);
}

void create_proc(void){
    mkdir("/proc", 0755);
    mount("proc", "/proc", "proc", 0, NULL);
}

int main(){
    int fd = open("/dev/console", O_RDWR);
    if (fd >= 0) {
        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);
        if (fd > 2) close(fd);
    }

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    create_proc();
    get_drives();
    
    spawn_recovery_shell();
    return 0;
}

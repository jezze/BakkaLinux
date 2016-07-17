#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/wait.h>

struct tty
{

    pid_t pid;
    char *path;
    char **cmd;

};

static void init(void)
{

    mount("rootfs", "/", "rootfs", MS_RDONLY | MS_NOSUID | MS_NODEV | MS_REMOUNT | MS_NOATIME | MS_NODIRATIME, NULL);
    mount("proc", "/proc", "proc", MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_NOATIME | MS_NODIRATIME, NULL);
    mount("sysfs", "/sys", "sysfs", MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_NOATIME | MS_NODIRATIME, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", MS_NOSUID | MS_NOEXEC | MS_NOATIME | MS_NODIRATIME, NULL);
    mount("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV | MS_NOATIME | MS_NODIRATIME, "size=32M");

}

static void destroy(void)
{

    umount("/tmp");
    umount("/dev");
    umount("/sys");
    umount("/proc");
    umount("/");

}

static pid_t mktty(struct tty *tty)
{

    pid_t pid = fork();

    if (!pid)
    {

        int fd = open(tty->path, O_RDWR);

        if (fd < 0)
            exit(EXIT_FAILURE);

        if (!isatty(fd))
            exit(EXIT_FAILURE);

        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
        setsid();
        execvp(tty->cmd[0], tty->cmd);
        exit(EXIT_FAILURE);

    }

    return pid;

}

static void monitor(struct tty *ttys, unsigned int count, pid_t pid)
{

    unsigned int i;

    for (i = 0; i < count; i++)
    {

        if (pid == ttys[i].pid)
            ttys[i].pid = mktty(&ttys[i]);

    }

}

int main(void)
{

    sigset_t set;
    pid_t child;
    char *cmd[] = {"/bin/sh", "-l", NULL};
    struct tty ttys[] = {
        {0, "/dev/tty1", cmd},
        {0, "/dev/ttyS0", cmd}
    };

    if (getpid() != 1)
        return EXIT_FAILURE;

    init();
    monitor(ttys, 2, 0);
    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, NULL);

    for (;;)
    {

        int sig;

        sigwait(&set, &sig);

        switch (sig)
        {

        case SIGINT:
            destroy();
            reboot(RB_AUTOBOOT);

            break;

        case SIGTERM:
            destroy();
            reboot(RB_HALT_SYSTEM);

            break;

        case SIGUSR1:
            destroy();
            reboot(RB_POWER_OFF);

            break;

        case SIGUSR2:
            destroy();
            reboot(RB_KEXEC);

            break;

        case SIGCHLD:
            while ((child = waitpid(-1, NULL, WNOHANG)) > 0)
                monitor(ttys, 2, child);

            break;

        }

    }

    return EXIT_SUCCESS;

}


#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/wait.h>

struct process
{

    pid_t pid;
    char **cmd;
    char *tty;
    int restart;

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

static void mktty(char *tty)
{

    int fd = open(tty, O_RDWR);

    if (fd < 0)
        exit(EXIT_FAILURE);

    if (!isatty(fd))
        exit(EXIT_FAILURE);

    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

}

static pid_t mkproc(struct process *p)
{

    pid_t pid = fork();

    if (!pid)
    {

        if (p->tty)
            mktty(p->tty);

        setsid();
        execvp(p->cmd[0], p->cmd);
        exit(EXIT_FAILURE);

    }

    return pid;

}

static void monitor(struct process *ps, pid_t pid)
{

    struct process *p;

    for (p = ps; p->cmd; p++)
    {

        if (!pid || (p->pid == pid && p->restart))
            p->pid = mkproc(p);

    }

}

int main(void)
{

    sigset_t set;
    pid_t child;
    char *cmd[] = {"/bin/sh", "-l", NULL};
    struct process ps[] = {
        {0, cmd, "/dev/tty1", 1},
        {0, cmd, "/dev/ttyS0", 1},
        {0, NULL}
    };

    if (getpid() != 1)
        return EXIT_FAILURE;

    init();
    monitor(ps, 0);
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
                monitor(ps, child);

            break;

        }

    }

    return EXIT_SUCCESS;

}


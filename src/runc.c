#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mount.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include "helpers.h"
#include "user.h"
#include "runc.h"
#include "../config.h"

       
int bootstrap_container(void *args_par) {

    struct clone_args *args = (struct clone_args *)args_par;

    /* here we can customize the container process */
    fprintf(stdout, "ProcessID: %ld\n", (long) getpid());

    /* setting new hostname */
    set_container_hostname();
    
    /* mounting the new container file system */
    mount_fs();

    /* mounting /proc in order to support commands like `ps` */
    mount_proc();

    if (execvp(args->argv[0], args->argv) != 0)
        printErr("command exec failed");
    
    /* we should never reach here! */
    exit(EXIT_FAILURE);
}


void runc(int n_values, char *command_input[]) {
    void *child_stack_bottom;
    void *child_stack_top;
    pid_t child_pid;
    struct clone_args args;
    char *uid_map = NULL;
    char *gid_map = NULL;


    /* why 2? just skipping <prog_name> <cmd> */
    args.argv = &command_input[2];
    args.n_args = n_values - 2;

    printRunningInfos(&args);

    /* child stack allocation */
    child_stack_bottom = malloc(STACK_SIZE);
    if (child_stack_bottom == NULL)
        printErr("child stack allocation");

    /* stack will grow from top to bottom */
    child_stack_top = child_stack_bottom + STACK_SIZE;
    
    printf("Booting up your container...\n\n");

    /* 
    * Here we can specify the namespace we want by using the appropriate
    * flags
    * - CLONE_NEWNS:   Mount namespace to make the mount point only
    *                  visible inside the container.
    *                  (but we have to take off by the host's mounts and
    *                  rootfs)
    * - CLONE_NEWUTS:  UTS namespace, where UTS stands for Unix Timestamp
    *                  System.
    * - CLONE_NEWIPC:  Create a process in a new IPC namespace
    * - CLONE_NEWPID:  Process IDs namespace. (we need to mount /proc !)
    * - CLONE_NEWNET:  Create a process in a new Network namespace
    *                  (we have to setup interfaces inside the namespace)
    * - CLONE_NEWUSER: User namespace. (we have to provide a UID/GID mapping)
    *                  about UID and GUI:
    * https://medium.com/@gggauravgandhi/uid-user-identifier-and-gid-group-identifier-in-linux-121ea68bf510
    * 
    * The execution context of the cloned process is, in part, defined
    * by the flags passed in.
    * 
    * When requesting a new User namespace alongside other namespaces,
    * the User namespace will be created first. User namespaces can be
    * created without root permissions, which means we can now drop the
    * sudo and run our program as a non-root user!
    */
    int clone_flags =
                CLONE_NEWNS  |
                CLONE_NEWUTS |
                CLONE_NEWIPC |
                CLONE_NEWPID |
                CLONE_NEWNET |
                CLONE_NEWUSER|
                SIGCHLD;
                /*TODO: CLONE_NEWCGROUP support */
    child_pid = clone(bootstrap_container, child_stack_top, clone_flags, &args);
    if (child_pid < 0)
        printErr("Unable to create child process");
    
    /* Update the UID and GUI maps in the child (see user.h) */
    map_uid_gid(child_pid);

    if (waitpid(child_pid, NULL, 0) == -1)
        printErr("waitpid");

    fprintf(stdout, "\nContainer process terminated.\n");

    umount_proc();

    /* then free the memory */
    free(child_stack_bottom);
    
}


void set_container_hostname() {
    char *hostname = strdup(HOSTNAME);
    int ret = sethostname(hostname, sizeof(hostname)+1);
    if (ret < 0)
        printErr("hostname");
}


void mount_fs() {
    if (chroot(FILE_SYSTEM_PATH) != 0)
        printErr("chroot to the new file system");
    if (chdir("/") < 0)
        printErr("chdir to the root (/)");
}


void mount_proc() {
    /*
    * When we execute `ps`, we are able to see the process IDs. It will
    * look inside the directory called `/proc` (ls /proc) to get process
    * informations. From the host machine point of view, we can see all
    * the processes running on it.
    *
    * `/proc` isn't just a regular file system but a pseudo file system.
    * It does not contain real files but rather runtime system information
    * (e.g. system memory, devices mounted, hardware configuration, etc).
    * It is a mechanism the kernel uses to communicate information about
    * running processes from the kernel space into user spaces. From the
    * user space, it looks like a normal file system.
    * For example, informations about the current self process is stored
    * inside /proc/self. You can find the name of the process using
    * [ls -l /proc/self/exe].
    * Using a sequence of [ls -l /proc/self] you can see that your self
    * process has a new processID because /proc/self changes at the start
    * of a new process.
    *
    * Because we change the root inside the container, we don't currently
    * have this `/procs` file system available. Therefore, we need to
    * mount it otherwise the container process will see the same `/proc`
    * directory of the host.
    */

    /*
     * As proposed by docker runc spec (see the link in "Pointers" section
     * of README.md)
     * 
     * MS_NOEXEC -> do not allow programs to be executed from this
     *              filesystem
     * MS_NOSUID -> Do not honor set-user-ID and set-group-ID bits or file
     *              capabilities when executing programs from this filesystem.
     * MS_NODEV  -> Do not allow access to devices (special files) on this
     *              filesystem.
     *
     * for more informations about mount flags:
     *      http://man7.org/linux/man-pages/man2/mount.2.html
     */
    unsigned long mountflags = MS_NOEXEC | MS_NOSUID | MS_NODEV;
    if (mount("proc", "/proc", "proc", mountflags, "") < 0)
        printErr("mounting /proc");
}


//TODO: check why umount fails
void umount_proc() {
    char *buffer = strdup(FILE_SYSTEM_PATH);
    strcat(buffer, "/proc");
    if (umount(buffer) != 0)
        printErr("unmounting /proc");
    fprintf(stdout, "/proc correctly umounted\n");
}
#define STACK_SIZE (1024 * 1024)


/* This structure identifies the runc arguments */
struct runc_args {
    char **child_entrypoint;        /* child entrypoint command */
    size_t child_entrypoint_size;   /* lenght of the child_entrypoint table */
    struct cgroup_args *resources;  /* cgroup support parameters */
    int privileged;                 /* create new USERNS or not */
};

/* This structure identifies the child_fn arguments */
struct clone_args {
   int sync_userns_fd[2];       
   int sync_uid_gid_map_fd[2];
   char **command;                /* The command table that will be executed */
   size_t command_size;           /* lenght of the command table */
   int hasUserNs;	                /* create new USERNS or not */
   struct cgroup_args *resources; /* cgroups resources limitations structure */
};

/* create and run a new containered process */
void runc(struct runc_args *runc_arguments);

/* set your new hostname */
void set_container_hostname();

/* print the container pid and command name */
void print_running_infos(struct clone_args *args);

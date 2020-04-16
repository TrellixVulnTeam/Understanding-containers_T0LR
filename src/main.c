#include <stdio.h>
#include <stdlib.h>

#define _GNU_SOURCE
#include "runc.h"
#include "helpers.h"


int main(int argc, char *argv[]) {
    if (argc < N_PARAMS)
        printUsage();

    switch(dispatcher(argv[1])) {
        case RUN:
            runc(argc, argv);
            break;

        default:
            perror("bad command");
    }

    exit(EXIT_SUCCESS);
}
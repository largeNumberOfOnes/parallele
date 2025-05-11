/**
 * This program demonstrates the capabilities of MPI_Comm_spawn. It starts
 *    two new processes for each of its instances, printing “hi”.
 * It only runs on the cluster. To run it, you must specify the
 *    pmi_comm_spawn_hi target for the make utility.
 */

#include <mpi.h>
#include <stdio.h>

void proc_main(int argc, char **argv) {
    char *args[] = {"7", NULL};
    int maxprocs = 2;
    int errcodes[2];
    MPI_Comm intercomm;
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    for (int root_rank = 0; root_rank < size; ++root_rank) {
        MPI_Comm_spawn(
            argv[0], args, maxprocs,
            MPI_INFO_NULL, root_rank, MPI_COMM_WORLD, &intercomm, errcodes
        );
    }

    printf("Spawn completed\n");
}

void proc_spwn(int argc, char **argv) {
    MPI_Comm parent_comm;
    MPI_Comm_get_parent(&parent_comm);
    printf("hi\n");
    MPI_Comm_free(&parent_comm);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    
    if (argc == 1) {
        proc_main(argc, argv);
    } else {
        proc_spwn(argc, argv);
    }
    fflush(stdout);

    MPI_Finalize();
    return 0;
}

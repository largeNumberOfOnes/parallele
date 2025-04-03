// #include "mpi.h"



// int main(int argc, char *argv[]) {
//     MPI_Init(&argc, &argv);
    
//     int rank;
//     MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
//     if (rank == 0) {
//         // Only rank 0 will spawn new processes
//         MPI_Comm intercomm;
//         int errcodes[3];
        
//         MPI_Comm_spawn("./execs/task_1_hellow_world.out", MPI_ARGV_NULL, 3, MPI_INFO_NULL, 
//                       0, MPI_COMM_SELF, &intercomm, errcodes);
        
//         // Now rank 0 can communicate with the 3 spawned workers
//         // ...
        
//         MPI_Comm_free(&intercomm);
//     } else {
//         // Other ranks continue their work
//         // ...
//     }
    
//     MPI_Finalize();
//     return 0;
// }



#include <mpi.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    
    char *command = "./task_1_hellow_world.out";
    char *args[] = {NULL};
    int maxprocs = 2;
    int errcodes[2];
    MPI_Comm intercomm;
    
    MPI_Comm_spawn(command, args, maxprocs, MPI_INFO_NULL, 0, 
                  MPI_COMM_WORLD, &intercomm, errcodes);
    
    printf("Spawn completed\n");
    MPI_Finalize();
    return 0;
}

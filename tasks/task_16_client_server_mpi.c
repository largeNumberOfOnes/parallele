#include "mpi.h"
#include <stdio.h>

// #include "../slibs/err_proc.h"
#define RET_IF_ERR(x) x

int main(int argc, char **argv) {
    RET_IF_ERR(MPI_Init(&argc, &argv));

    int size, rank;
    RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &size));
    RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));    

    printf("22\n");

    if (rank == 0) {
        char port_arr[MPI_MAX_PORT_NAME]; 
        MPI_Comm intercomm; 
        MPI_Open_port(MPI_INFO_NULL, port_arr); 
        printf("port name is: %s\n", port_arr); 
        MPI_Bcast(port_arr, MPI_MAX_PORT_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);
    
        MPI_Comm_accept(port_arr, MPI_INFO_NULL, 0, MPI_COMM_SELF, &intercomm);
    } else {
        char name[MPI_MAX_PORT_NAME];
        MPI_Bcast(name, MPI_MAX_PORT_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);
        printf("enter port name: %s\n", name);  
        MPI_Comm intercomm; 
        // gets(name); 
        MPI_Comm_connect(name, MPI_INFO_NULL, 0, MPI_COMM_SELF, &intercomm);
    }

    RET_IF_ERR(MPI_Finalize());

    return 0;
}

#include <mpi.h>
#include <iostream>

int main(int argc, char** argv) {
    // Initialize MPI
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);  // Get the rank of the process
    MPI_Comm_size(MPI_COMM_WORLD, &size);  // Get the total number of processes

    std::cout << "Hello from process " << rank << " of " << size << std::endl;

    // Finalize MPI
    MPI_Finalize();
    return 0;
}


//mpic++ -o mpi_hello mpi_hello.cpp
//mpirun -np 4 ./mpi_hello
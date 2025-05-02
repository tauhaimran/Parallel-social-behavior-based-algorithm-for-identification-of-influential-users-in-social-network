#!/bin/bash

# Compile the C++ program with MPI, OpenMP, METIS, and SFML
mpic++ -fopenmp -o mpi_omp_metis_sfml_program your_code.cpp -lmetis -lsfml-graphics -lsfml-window -lsfml-system

# Run the program using MPI (assuming 2 processes)
mpirun -np 2 ./mpi_omp_metis_sfml_program

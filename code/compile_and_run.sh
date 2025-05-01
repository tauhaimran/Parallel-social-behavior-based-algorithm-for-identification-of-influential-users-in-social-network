#!/bin/bash

# Compile command
g++ main.cpp -o parallel_gui -lsfml-graphics -lsfml-window -lsfml-system -lmetis -fopenmp -lmpi

# Run with 2 MPI processes (adjust as needed)
mpirun -np 3 ./parallel_gui
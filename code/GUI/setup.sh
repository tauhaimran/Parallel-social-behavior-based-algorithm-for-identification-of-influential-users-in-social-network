#!/bin/bash

# Update package list
sudo apt update

# Install MPI (OpenMPI), OpenMP, and METIS dependencies
sudo apt install -y build-essential openmpi-bin openmpi-common libopenmpi-dev libmetis-dev

# Install SFML (for graphics)
sudo apt install -y libsfml-dev

# Install any missing dependencies (if necessary)
sudo apt install -y libboost-all-dev

echo "Setup complete. Dependencies installed."

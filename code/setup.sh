#!/bin/bash

echo "🛠 Updating system packages..."
sudo apt update && sudo apt upgrade -y

echo "🧱 Installing build tools..."
sudo apt install -y build-essential g++ cmake

echo "📦 Installing MPICH (MPI implementation)..."
sudo apt install -y mpich

echo "📦 Installing GTK 3 for GUI support..."
sudo apt install -y libgtk-3-dev

echo "📦 Installing SSH server (optional, for MPI over SSH)..."
sudo apt install -y openssh-server

echo "🔍 Verifying OpenMP support (should show _OPENMP)..."
g++ -fopenmp -dM -E - < /dev/null | grep -i openmp

echo "✅ All dependencies installed successfully. Ready to compile your C++ project!"

#!/bin/bash

echo "🔧 Installing MPI and dependencies..."

# Update package list
sudo apt update

# Install MPICH
sudo apt install -y mpich

# (Optional) Install build essentials and SSH server
sudo apt install -y build-essential openssh-server

echo "✅ Setup complete. You can now compile and run MPI programs."

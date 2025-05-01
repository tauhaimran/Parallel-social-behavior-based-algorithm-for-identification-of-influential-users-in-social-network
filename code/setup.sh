#!/bin/bash

echo "🛠 Updating system packages..."
sudo apt update && sudo apt upgrade -y

echo "🧱 Installing build tools..."
sudo apt install -y build-essential g++ cmake pkgconf

echo "📦 Installing MPICH (MPI implementation)..."
sudo apt install -y mpich

echo "📦 Installing SFML for modern GUI..."
sudo apt install -y libsfml-dev

echo "📦 Installing font dependencies..."
sudo apt install -y fonts-dejavu fonts-ubuntu

echo "📦 Installing SSH server (optional, for MPI over SSH)..."
sudo apt install -y openssh-server

echo "🔍 Verifying installations..."
echo "MPI: $(mpicc --version | head -n1)"
echo "SFML: $(pkg-config --modversion sfml-all)"
echo "Fonts: $(fc-list | grep -E 'Ubuntu|DejaVu' | wc -l) fonts available"

echo "🔗 Creating font fallback (for WSL)..."
sudo mkdir -p /usr/share/fonts/truetype/ubuntu/
sudo ln -sf /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf \
            /usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf

echo "✅ All dependencies installed successfully!"
echo "   Build with: ./build.sh"
echo "   Run with: ./run.sh gui"
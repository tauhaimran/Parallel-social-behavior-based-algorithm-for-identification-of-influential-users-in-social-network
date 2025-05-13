# Use Ubuntu 24.04 as the base image
FROM ubuntu:24.04

# Disable prompts from apt
ENV DEBIAN_FRONTEND=noninteractive

# Install all required packages
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    cmake \
    pkgconf \
    libmetis-dev \
    libsfml-dev \
    fonts-dejavu \
    fonts-ubuntu \
    && apt-get clean

# Set working directory
WORKDIR /app

# Copy everything into the container
COPY . .

# Compile the serial version
RUN g++ code/PSAIIM/serial_main.cpp -o p

# Set default command to run the binary
CMD ["./p"]

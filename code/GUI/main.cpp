#include <iostream>
#include <omp.h>
#include <mpi.h>
#include <SFML/Graphics.hpp>

int main(int argc, char** argv) {
    // --- MPI Init
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (rank == 0) std::cout << "MPI: Hello from process 0 of " << size << std::endl;

    // --- OpenMP Parallel Section
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        #pragma omp critical
        std::cout << "OpenMP: Thread " << tid << " says hello\n";
    }

    // --- SFML Window
    if (rank == 0) {
        sf::RenderWindow window(sf::VideoMode(400, 300), "SFML Test");
        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event))
                if (event.type == sf::Event::Closed)
                    window.close();

            window.clear(sf::Color::Black);
            sf::CircleShape shape(50.f);
            shape.setFillColor(sf::Color::Green);
            shape.setPosition(100, 100);
            window.draw(shape);
            window.display();
        }
    }

    MPI_Finalize();
    return 0;
}
// Compile with: mpicxx -fopenmp -o main main.cpp -lsfml-graphics -lsfml-window -lsfml-system
// Run with: mpirun -np 4 ./main

//or
//compile: g++ -fopenmp -o combined_test combined_test.cpp -lsfml-graphics -lsfml-window -lsfml-system -lmpi
//run: mpirun -np 4 ./combined_test
// This code combines MPI, OpenMP, and SFML to demonstrate a simple parallel program.

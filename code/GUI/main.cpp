#include <iostream>
#include <omp.h>
#include <mpi.h>
#include <metis.h>
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

    // --- METIS Test (partition small graph)
    idx_t nVertices = 4, nEdges = 4;
    idx_t xadj[5] = {0, 2, 3, 5, 6}; // adjacency structure
    idx_t adjncy[6] = {1, 2, 3, 0, 3, 2};
    idx_t nparts = 2;
    idx_t part[4];
    idx_t objval;

    int status = METIS_PartGraphKway(&nVertices, nullptr, xadj, adjncy,
                                     nullptr, nullptr, nullptr, &nparts,
                                     nullptr, nullptr, nullptr, &objval, part);
    if (rank == 0)
        std::cout << "METIS: Partitioning done with objval = " << objval << "\n";

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

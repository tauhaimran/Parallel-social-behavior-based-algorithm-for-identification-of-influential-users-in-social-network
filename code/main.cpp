#include <SFML/Graphics.hpp>
#include <mpi.h>
#include <metis.h>
#include <omp.h>
#include <iostream>

int main(int argc, char* argv[]) {
    // Initialize MPI
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Only rank 0 creates the GUI window
    sf::RenderWindow* window = nullptr;
    if (rank == 0) {
        window = new sf::RenderWindow(sf::VideoMode(800, 600), "Parallel Influence GUI");
    }

    // Simple METIS example (dummy call)
    idx_t nvtxs = 10;
    idx_t ncon = 1;
    idx_t xadj[] = {0,2,5,7,9,12,14,16,18,20,22};
    idx_t adjncy[] = {1,5,0,2,6,1,3,7,2,4,8,3,9,0,6,1,5,7,2,6,8,3,7,9,4,8};
    idx_t nparts = 2;
    idx_t objval;
    idx_t part[nvtxs];
    METIS_PartGraphKway(&nvtxs, &ncon, xadj, adjncy, NULL, NULL, NULL, &nparts, 
                       NULL, NULL, NULL, &objval, part);

    // Main loop (only for rank 0)
    if (rank == 0) {
        while (window->isOpen()) {
            sf::Event event;
            while (window->pollEvent(event)) {
                if (event.type == sf::Event::Closed)
                    window->close();
            }

            // Clear with different color based on OpenMP thread
            #pragma omp parallel
            {
                int thread_id = omp_get_thread_num();
                #pragma omp master
                {
                    window->clear(sf::Color(50 + thread_id * 50, 50, 100));
                }
            }
            
            // Draw something simple
            sf::CircleShape shape(100.f);
            shape.setFillColor(sf::Color::Green);
            shape.setPosition(300, 200);
            window->draw(shape);
            
            window->display();
        }
        delete window;
    }

    // Finalize MPI
    MPI_Finalize();
    return 0;
}
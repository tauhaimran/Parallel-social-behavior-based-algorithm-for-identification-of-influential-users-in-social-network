// main.cpp
#include <mpi.h>
#include <omp.h>
#include <vector>
#include <iostream>
#include "graph.h"
#include "pagerank.h"
#include "metis_interface.h"
#include "influence_bfs.h"

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    // Load partitioned graph
    Graph localGraph = load_partition("partition_" + std::to_string(world_rank) + ".txt");

    // Parallel influence power calculation using OpenMP
    std::vector<float> influenceScores = compute_parallel_pagerank(localGraph);

    // Seed candidate selection (local)
    std::vector<int> localSeeds = select_influential_nodes(localGraph, influenceScores);

    // Global reduction of seed candidates
    std::vector<int> finalSeeds = reduce_and_merge_seeds(localSeeds, MPI_COMM_WORLD);

    // Display result (Rank 0)
    if (world_rank == 0) {
        std::cout << "Final selected influential nodes:\n";
        for (auto node : finalSeeds) std::cout << node << " ";
        std::cout << "\n";
    }

    MPI_Finalize();
    return 0;
}

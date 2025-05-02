#include <iostream>
#include <metis.h>

int main() {
    // Graph with 4 vertices and 5 edges (undirected)
    idx_t nVertices = 4;
    idx_t nEdges = 5;
    idx_t xadj[5]   = {0, 2, 3, 5, 6};        // Indexes into adjncy where each vertex starts
    idx_t adjncy[6] = {1, 2, 3, 0, 3, 2};     // Adjacency list
    idx_t nparts = 2;                         // Number of partitions
    idx_t part[4];                            // Output partition vector
    idx_t objval;                             // Edge-cut or communication volume

    int result = METIS_PartGraphKway(
        &nVertices,  // Number of vertices
        NULL,        // Number of balancing constraints
        xadj,        // Indexes in adjncy where neighbors of vertex start
        adjncy,      // Adjacency list
        NULL, NULL, NULL, // Weights (optional)
        &nparts,     // Desired number of parts
        NULL, NULL,  // Imbalance tolerance & options
        NULL,        // METIS options
        &objval,     // Output: objective value
        part         // Output: part assignments
    );

    if (result == METIS_OK) {
        std::cout << "METIS: Partitioning successful. objval = " << objval << "\n";
        for (int i = 0; i < nVertices; ++i)
            std::cout << "Vertex " << i << " -> Part " << part[i] << "\n";
    } else {
        std::cerr << "METIS: Partitioning failed.\n";
    }

    return 0;
}
// Compile with: g++ -o metis_test metis_test.cpp -lmetis
// Run with: ./metis_test
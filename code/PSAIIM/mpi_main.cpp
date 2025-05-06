#include <mpi.h>
#include <iostream>
#include <vector>
#include <string>
#include "mpi_graph.h"

int main(int argc, char* argv[]) {
    // Initialize MPI
    MPI_Init(&argc, &argv);

    // Create Graph object
    Graph g;

    // Define input files
    std::vector<std::string> files = {
        "higgs-social_network.edgelist",
        "higgs-retweet_network.edgelist",
        "higgs-reply_network.edgelist",
        "higgs-mention_network.edgelist",
        "higgs-interests.txt"
    };

    // Determine maximum number of nodes
    long int max_nodes = g.determineMaxNodes(files);

    // Initialize graph with max_nodes
    g.initialize(max_nodes);

    // Load data from files
    g.loadFromFile(files);

    // Calculate influence power
    g.calculateInfluencePower();

    // Display parameters for first five nodes
    g.displayFirstFive();

    // Select top influential nodes
    auto top_nodes = g.getTopInfluentialNodes();
    if (g.getRank() == 0) {
        std::cout << "Top Influential Nodes:\n";
        for (const auto& node : top_nodes) {
            std::cout << "Node " << node.first << ": Influence Power = " << node.second << "\n";
        }
    }

    // Select seeds (e.g., k=5)
    int k = 5;
    auto seeds = g.selectSeeds(k);
    if (g.getRank() == 0) {
        std::cout << "Selected Seeds:\n";
        for (long int seed : seeds) {
            std::cout << "Seed Node " << seed << "\n";
        }
    }

    // Save logs
    g.saveLogsToFile();

    // Finalize MPI
    MPI_Finalize();
    return 0;
}
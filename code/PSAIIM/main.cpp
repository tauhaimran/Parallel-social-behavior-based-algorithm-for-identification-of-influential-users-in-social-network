#include "graph.h"


int main() {
    Graph g;
    std::vector<std::string> files = {
        "higgs-social_network.edgelist",
        "higgs-retweet_network.edgelist",
        "higgs-reply_network.edgelist",
        "higgs-mention_network.edgelist",
        "higgs-interests.txt"
    };

    long int max_nodes = g.determineMaxNodes(files);
    g.initialize(max_nodes);
    g.loadFromFile(files);
    g.calculateInfluencePower();
    std::vector<long int> seeds = g.selectSeeds(10);
    std::cout << "Selected seeds: ";
    for (long int seed : seeds) {
        std::cout << seed << " ";
    }
    for (long int seed : seeds) {
        g.displayNodeParameters(seed);
    }
    std::cout << std::endl;
    return 0;
}
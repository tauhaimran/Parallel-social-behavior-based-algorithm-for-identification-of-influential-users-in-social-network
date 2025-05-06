#include <iostream>
#include <vector>
#include <chrono>
#include "graph.h"

int main()
{
    // start timer

    Graph g;
    std::vector<std::string> files = {
        "higgs-social_network.edgelist",
        "higgs-retweet_network.edgelist",
        "higgs-reply_network.edgelist",
        "higgs-mention_network.edgelist",
        "higgs-interests.txt"};

    auto t_start = std::chrono::high_resolution_clock::now();

    long int max_nodes = g.determineMaxNodes(files);
    g.initialize(max_nodes);
    g.loadFromFile(files);

    //time calculation
    auto t_end = std::chrono::high_resolution_clock::now();
    auto duration_ms1 = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
    
    t_start = std::chrono::high_resolution_clock::now();
    g.calculateInfluencePower();
    std::vector<long int> seeds = g.selectSeeds(10);
    
    std::cout << "Selected seeds: ";
    for (long int seed : seeds)
    {
        std::cout << seed << " ";
    }
    std::cout << "\n\n";
    
    for (long int seed : seeds)
    {
        g.displayNodeParameters(seed);
    }
    
    // stop timer
    t_end = std::chrono::high_resolution_clock::now();
    auto duration_ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
    std::cout << "\nData initialization time: " << duration_ms1 << " ms" << std::endl;
    std::cout << "\nTotal execution time(without data loading time): " << duration_ms2 << " ms" << std::endl;

    return 0;
}

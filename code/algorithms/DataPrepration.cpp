#include "DataPrepration.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_set>

MetisGraph* prepare_metis_graph(const std::string& file_path, InteractionType it) {
    MetisGraph* mg = new MetisGraph();
    std::ifstream file(file_path);
    std::unordered_map<int, std::vector<int>> adj_list;
    std::unordered_set<int> unique_nodes;

    if (!file) {
        std::cerr << "ERROR: Failed to open " << file_path << "\n";
        return mg;
    }

    std::string line;
    if (it == SOCIAL) {
        // Parse edgelist with two columns: userA userB
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            int u, v;
            if (iss >> u >> v) {
                adj_list[u].push_back(v);
                unique_nodes.insert(u);
                unique_nodes.insert(v);
                // No timestamp for SOCIAL, so we don’t add to mg->timestamps
            }
        }
    } else {
        // Parse activity file with four columns: userA userB timestamp interaction
        while (std::getline(file, line)) {
            int u, v, timestamp;
            std::string interaction;
            std::istringstream iss(line);
            if (iss >> u >> v >> timestamp >> interaction) {
                // Filter based on interaction type
                if ((it == RETWEET && interaction == "RT") ||
                    (it == REPLY && interaction == "RE") ||
                    (it == MENTION && interaction == "MT")) {
                    adj_list[u].push_back(v);
                    unique_nodes.insert(u);
                    unique_nodes.insert(v);
                    mg->timestamps.push_back(timestamp);
                }
            }
        }
    }

    // Create ID mapping (sorted)
    std::vector<int> sorted_nodes(unique_nodes.begin(), unique_nodes.end());
    std::sort(sorted_nodes.begin(), sorted_nodes.end());
    idx_t idx = 0;
    for (int node : sorted_nodes) {
        mg->id_map[node] = idx++;
    }
    mg->nvtxs = sorted_nodes.size();

    // Build METIS adjacency arrays
    mg->xadj.push_back(0);
    for (int node : sorted_nodes) {
        if (adj_list.find(node) != adj_list.end()) {
            for (int neighbor : adj_list[node]) {
                mg->adjncy.push_back(mg->id_map[neighbor]);
                mg->nedges++;
            }
        }
        mg->xadj.push_back(mg->adjncy.size());
    }

    std::cout << "Loaded " << file_path << ": " << mg->nvtxs << " nodes, " 
              << mg->nedges << " edges\n";
    return mg;
}

void free_metis_graph(MetisGraph* g) {
    delete g;
}
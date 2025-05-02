#ifndef DATA_PREPARATION_H
#define DATA_PREPARATION_H

#include <string>
#include <vector>
#include <unordered_map>
#include <metis.h>

enum InteractionType { RETWEET, REPLY, MENTION, SOCIAL, ACTIVITY };

struct MetisGraph {
    std::vector<idx_t> xadj;     // METIS adjacency pointers
    std::vector<idx_t> adjncy;   // METIS adjacency list
    std::unordered_map<int, idx_t> id_map; // Original ID → METIS index
    idx_t nvtxs = 0;             // Number of vertices
    idx_t nedges = 0;            // Number of edges
    std::vector<time_t> timestamps; // Timestamps for temporal analysis (optional)
};

MetisGraph* prepare_metis_graph(const std::string& file_path, InteractionType it);
void free_metis_graph(MetisGraph* g);

#endif
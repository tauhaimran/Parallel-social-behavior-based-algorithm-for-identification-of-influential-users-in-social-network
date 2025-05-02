#include "DataPrepration.h"
#include "GraphPartition.h"
#include <iostream>

int main() {
    const std::vector<std::pair<InteractionType, std::string>> files = {
        {RETWEET, "twitter_data/higgs-activity_time.txt"},
        {REPLY, "twitter_data/higgs-activity_time.txt"},
        {MENTION, "twitter_data/higgs-activity_time.txt"},
        {SOCIAL, "twitter_data/higgs-social_network.edgelist"}
    };

    for (const auto& [it, fpath] : files) {
        std::cout << "Processing " << fpath << " (Interaction: " << it << ")...\n";
        MetisGraph* mg = prepare_metis_graph(fpath, it);
        PartitionResult result = metis_partition(mg, 2);
        
        std::cout << "Partitioned into " << result.nparts << " parts\n";
        if (mg->nvtxs > 0) {
            std::cout << "First 10 node partitions: ";
            for (int i = 0; i < 10 && i < mg->nvtxs; i++) 
                std::cout << result.partitions[i] << " ";
            std::cout << "\n\n";
        }
        
        free_metis_graph(mg);
    }

    return 0;
}
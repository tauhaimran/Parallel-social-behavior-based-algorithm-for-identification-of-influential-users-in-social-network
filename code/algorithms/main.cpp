#include "DataPrepration.h"
#include "GraphPartition.h"
#include <omp.h>
#include <iostream>

int main() {
    std::vector<std::string> user_ids = get_user_ids();
    std::cout << "Found " << user_ids.size() << " users\n";

    std::vector<UserData*> users(user_ids.size(), nullptr);
    std::vector<double> friendship_factors = {0.50, 0.35, 0.15};  // retweet, comment, tag

    #pragma omp parallel for num_threads(12)
    for (size_t i = 0; i < user_ids.size(); i++) {
        users[i] = load_user_data(user_ids[i]);
    }

    #pragma omp parallel for num_threads(12)
    for (size_t i = 0; i < users.size(); i++) {
        if (users[i] && users[i]->graph) {
            simulate_actions(users[i]->graph, friendship_factors);
        }
    }

    int valid_users = 0;
    for (size_t i = 0; i < users.size(); i++) {
        if (users[i] && users[i]->graph && users[i]->graph->node_count > 0) {
            std::cout << "Valid data for user " << users[i]->user_id << ": "
                      << users[i]->graph->node_count << " nodes, "
                      << users[i]->graph->circle_count << " circles\n";
            valid_users++;
        }
    }
    std::cout << "Total valid users: " << valid_users << "\n";

    #pragma omp parallel for num_threads(12)
    for (size_t i = 0; i < users.size(); i++) {
        if (users[i] && users[i]->graph && users[i]->graph->node_count > 0) {
            std::cout << "Partitioning graph for user " << users[i]->user_id << "\n";
            partition_graph(users[i]->graph);
        }
    }

    for (size_t i = 0; i < users.size(); i++) {
        delete users[i];
    }

    return 0;
}
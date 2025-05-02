#ifndef DATA_PREPARATION_H
#define DATA_PREPARATION_H

#include <string>
#include <vector>

#define MAX_NODES 81306  // Twitter dataset nodes
#define MAX_FEATURES 1113  // Feature vector length
#define MAX_CIRCLES 100
#define MAX_USERS 1000
#define MAX_LINE 1000

struct Node {
    int id;
    std::vector<int> neighbors;
    std::vector<int> features;
    double action_weight;
    Node() : id(0), action_weight(0.0) {
        features.resize(MAX_FEATURES, 0);
    }
};

struct Graph {
    std::vector<Node> nodes;
    int node_count;
    std::vector<int> circles;
    int circle_count;
    Graph();
};

struct UserData {
    std::string user_id;
    Graph* graph;
    std::vector<int> ego_features;
    std::vector<std::string> featnames;
    UserData();
    ~UserData();
};

std::vector<std::string> get_user_ids();
UserData* load_user_data(const std::string& ego_id);
void simulate_actions(Graph* g, const std::vector<double>& friendship_factors);
void add_edge(Graph* g, int u, int v);
#endif
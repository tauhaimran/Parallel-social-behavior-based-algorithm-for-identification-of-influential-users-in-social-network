#include "DataPrepration.h"
#include <dirent.h>
#include <iostream>
#include <cstdlib>
#include <cstring>

Graph::Graph() : node_count(0), circle_count(0) {
    nodes.resize(MAX_NODES);
    circles.resize(MAX_NODES, 0);
}

UserData::UserData() : graph(nullptr) {
    ego_features.resize(MAX_FEATURES, 0);
    featnames.resize(MAX_FEATURES);
}

UserData::~UserData() {
    if (graph) delete graph;
}

void add_edge(Graph* g, int u, int v) {
    if (!g || u >= MAX_NODES || v >= MAX_NODES) return;
    if (g->nodes[u].id == 0) {
        g->nodes[u].id = u;
        g->node_count++;
    }
    if (g->nodes[v].id == 0) {
        g->nodes[v].id = v;
        g->node_count++;
    }
    g->nodes[u].neighbors.push_back(v);
}

UserData* load_user_data(const std::string& ego_id) {
    UserData* ud = new UserData();
    ud->user_id = ego_id;
    ud->graph = new Graph();

    std::string edges_file = "twitter_data/" + ego_id + ".edges";
    FILE* file = fopen(edges_file.c_str(), "r");
    if (file) {
        char line[MAX_LINE];
        while (fgets(line, MAX_LINE, file)) {
            int u, v;
            if (sscanf(line, "%d %d", &u, &v) == 2) {
                add_edge(ud->graph, u, v);
            }
        }
        fclose(file);
    }

    std::string circles_file = "twitter_data/" + ego_id + ".circles";
    file = fopen(circles_file.c_str(), "r");
    if (file) {
        int circle_idx = 0;
        char line[MAX_LINE];
        while (fgets(line, MAX_LINE, file)) {
            char* token = strtok(line, " \t");
            token = strtok(NULL, " \t");
            while (token) {
                int member = atoi(token);
                if (member < MAX_NODES) ud->graph->circles[member] = circle_idx;
                token = strtok(NULL, " \t");
            }
            circle_idx++;
        }
        ud->graph->circle_count = circle_idx;
        fclose(file);
    }

    std::string egofeat_file = "twitter_data/" + ego_id + ".egofeat";
    file = fopen(egofeat_file.c_str(), "r");
    if (file) {
        char line[MAX_LINE];
        fgets(line, MAX_LINE, file);
        char* token = strtok(line, " \t");
        while (token) {
            int idx = atoi(token);
            if (idx < MAX_FEATURES) ud->ego_features[idx] = 1;
            token = strtok(NULL, " \t");
        }
        fclose(file);
    }

    std::string feat_file = "twitter_data/" + ego_id + ".feat";
    file = fopen(feat_file.c_str(), "r");
    if (file) {
        char line[MAX_LINE];
        while (fgets(line, MAX_LINE, file)) {
            int node_id;
            if (sscanf(line, "%d", &node_id) == 1 && node_id < MAX_NODES) {
                char* token = strtok(line, " \t");
                token = strtok(NULL, " \t");
                while (token) {
                    int idx = atoi(token);
                    if (idx < MAX_FEATURES) ud->graph->nodes[node_id].features[idx] = 1;
                    token = strtok(NULL, " \t");
                }
            }
        }
        fclose(file);
    }

    std::string featnames_file = "twitter_data/" + ego_id + ".featnames";
    file = fopen(featnames_file.c_str(), "r");
    if (file) {
        int feat_idx = 0;
        char line[MAX_LINE];
        while (fgets(line, MAX_LINE, file) && feat_idx < MAX_FEATURES) {
            int idx;
            if (sscanf(line, "%d %s", &idx, line) == 2 && idx < MAX_FEATURES) {
                ud->featnames[idx] = std::string(line);
                feat_idx++;
            }
        }
        fclose(file);
    }

    std::cout << "Prepared data for user " << ego_id << ": " << ud->graph->node_count << " nodes, " << ud->graph->circle_count << " circles\n";
    return ud;
}

std::vector<std::string> get_user_ids() {
    std::vector<std::string> user_ids;
    DIR* dir = opendir("twitter_data");
    if (!dir) {
        std::cout << "Error opening directory\n";
        exit(1);
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".edges")) {
            std::string user_id = entry->d_name;
            user_id = user_id.substr(0, user_id.size() - 6);
            user_ids.push_back(user_id);
        }
    }
    closedir(dir);
    return user_ids;
}

void simulate_actions(Graph* g, const std::vector<double>& friendship_factors) {
    if (!g) return;
    srand(time(NULL));
    for (int u = 0; u < MAX_NODES; u++) {
        if (g->nodes[u].id == 0) continue;
        for (size_t i = 0; i < g->nodes[u].neighbors.size(); i++) {
            float r = static_cast<float>(rand()) / RAND_MAX;
            if (r < friendship_factors[0]) g->nodes[u].action_weight = friendship_factors[0];  // retweet
            else if (r < friendship_factors[0] + friendship_factors[1]) g->nodes[u].action_weight = friendship_factors[1];  // comment
            else g->nodes[u].action_weight = friendship_factors[2];  // tag
        }
    }
}
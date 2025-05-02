#include "GraphPartition.h"
#include <stack>
#include <iostream>
#include <cstring>

int* index_array;
int* lowlink_array;
int* level_array;
int* depth_array;
int current_index = 0;
std::stack<int> node_stack;

void discover(Node& v) {
    v.id = current_index;
    lowlink_array[v.id] = current_index;
    level_array[v.id] = 1;
    depth_array[v.id] = 1;
    current_index++;
    node_stack.push(v.id);
}

void explore(Graph* g, int v) {
    for (size_t i = 0; i < g->nodes[v].neighbors.size(); i++) {
        int w = g->nodes[v].neighbors[i];
        if (index_array[w] == 0) {
            discover(g->nodes[w]);
            explore(g, w);
            lowlink_array[v] = std::min(lowlink_array[v], lowlink_array[w]);
            if (g->circles[w] != 0) {
                level_array[v] = std::max(level_array[w], level_array[v] + 1);
            } else {
                level_array[v] = std::max(level_array[v], level_array[w]);
                lowlink_array[v] = std::min(lowlink_array[v], lowlink_array[w]);
            }
        }
    }
}

void finish(Graph* g, int v) {
    if (lowlink_array[v] == index_array[v]) {
        int size = 0;
        std::vector<int> component;
        int w;
        do {
            w = node_stack.top();
            node_stack.pop();
            lowlink_array[w] = v;
            level_array[w] = level_array[v];
            g->circles[w] = (size == 1) ? -1 : 1;  // -1 for CAC, 1 for SCC
            component.push_back(w);
            size++;
        } while (w != v);
        if (size == 1) {
            // Check for merges (simplified)
        }
    }
}

void partition_graph(Graph* g) {
    if (!g) return;
    index_array = (int*)calloc(MAX_NODES, sizeof(int));
    lowlink_array = (int*)calloc(MAX_NODES, sizeof(int));
    level_array = (int*)calloc(MAX_NODES, sizeof(int));
    depth_array = (int*)calloc(MAX_NODES, sizeof(int));
    current_index = 0;

    for (int v = 0; v < MAX_NODES; v++) {
        if (g->nodes[v].id != 0 && index_array[v] == 0) {
            discover(g->nodes[v]);
            explore(g, v);
            finish(g, v);
        }
    }

    std::cout << "Graph partitioned into SCC/CAC components\n";
    free(index_array);
    free(lowlink_array);
    free(level_array);
    free(depth_array);
}
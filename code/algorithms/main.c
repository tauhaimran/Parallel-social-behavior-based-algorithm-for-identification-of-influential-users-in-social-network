#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>

#define MAX_NODES 81306  // Total nodes in the dataset
#define MAX_FEATURES 1113  // Feature vector length
#define MAX_CIRCLES 100   // Max number of circles per user
#define MAX_LINE 100000   // Increased to handle long lines
#define MAX_USERS 1000    // Adjust based on number of users

// Structure for a graph node
typedef struct Node {
    int id;
    int *neighbors;
    int neighbor_count;
    int neighbor_capacity;  // To manage dynamic neighbor array size
    int *features;  // 1113-dimensional binary vector
    double action_weight;  // Simulated action weight
} Node;

// Structure for the graph
typedef struct Graph {
    Node *nodes;
    int node_count;
    int *circle_assignments;  // Array to store circle assignments for nodes
    int circle_count;
} Graph;

// Structure for user data
typedef struct UserData {
    char *user_id;
    Graph *graph;
    int *ego_features;  // Ego user's features
    char **featnames;  // Feature labels
} UserData;

// Initialize graph
Graph* init_graph() {
    Graph *g = (Graph*)malloc(sizeof(Graph));
    if (!g) {
        fprintf(stderr, "Failed to allocate graph\n");
        return NULL;
    }
    g->nodes = (Node*)calloc(MAX_NODES, sizeof(Node));
    if (!g->nodes) {
        free(g);
        fprintf(stderr, "Failed to allocate nodes\n");
        return NULL;
    }
    g->circle_assignments = (int*)calloc(MAX_NODES, sizeof(int));
    if (!g->circle_assignments) {
        free(g->nodes);
        free(g);
        fprintf(stderr, "Failed to allocate circle assignments\n");
        return NULL;
    }
    g->node_count = 0;
    g->circle_count = 0;
    return g;
}

// Add edge to the graph
void add_edge(Graph *g, int u, int v) {
    if (!g || u >= MAX_NODES || v >= MAX_NODES || u < 0 || v < 0) return;

    // Initialize node u if not already
    if (g->nodes[u].id == 0) {
        g->nodes[u].id = u;
        g->nodes[u].neighbor_capacity = 10;
        g->nodes[u].neighbors = (int*)malloc(sizeof(int) * g->nodes[u].neighbor_capacity);
        if (!g->nodes[u].neighbors) {
            fprintf(stderr, "Failed to allocate neighbors for node %d\n", u);
            return;
        }
        g->nodes[u].neighbor_count = 0;
        g->nodes[u].features = (int*)calloc(MAX_FEATURES, sizeof(int));
        if (!g->nodes[u].features) {
            free(g->nodes[u].neighbors);
            g->nodes[u].neighbors = NULL;
            fprintf(stderr, "Failed to allocate features for node %d\n", u);
            return;
        }
        g->node_count++;
    }

    // Initialize node v if not already
    if (g->nodes[v].id == 0) {
        g->nodes[v].id = v;
        g->nodes[v].neighbor_capacity = 10;
        g->nodes[v].neighbors = (int*)malloc(sizeof(int) * g->nodes[v].neighbor_capacity);
        if (!g->nodes[v].neighbors) {
            fprintf(stderr, "Failed to allocate neighbors for node %d\n", v);
            return;
        }
        g->nodes[v].neighbor_count = 0;
        g->nodes[v].features = (int*)calloc(MAX_FEATURES, sizeof(int));
        if (!g->nodes[v].features) {
            free(g->nodes[v].neighbors);
            g->nodes[v].neighbors = NULL;
            fprintf(stderr, "Failed to allocate features for node %d\n", v);
            return;
        }
        g->node_count++;
    }

    // Add v to u's neighbors
    if (g->nodes[u].neighbor_count >= g->nodes[u].neighbor_capacity) {
        g->nodes[u].neighbor_capacity *= 2;
        int *new_neighbors = (int*)realloc(g->nodes[u].neighbors, sizeof(int) * g->nodes[u].neighbor_capacity);
        if (!new_neighbors) {
            fprintf(stderr, "Failed to reallocate neighbors for node %d\n", u);
            return;
        }
        g->nodes[u].neighbors = new_neighbors;
    }
    g->nodes[u].neighbors[g->nodes[u].neighbor_count++] = v;

    // Add u to v's neighbors (undirected graph)
    if (g->nodes[v].neighbor_count >= g->nodes[v].neighbor_capacity) {
        g->nodes[v].neighbor_capacity *= 2;
        int *new_neighbors = (int*)realloc(g->nodes[v].neighbors, sizeof(int) * g->nodes[v].neighbor_capacity);
        if (!new_neighbors) {
            fprintf(stderr, "Failed to reallocate neighbors for node %d\n", v);
            return;
        }
        g->nodes[v].neighbors = new_neighbors;
    }
    g->nodes[v].neighbors[g->nodes[v].neighbor_count++] = u;
}

// Load a single graph and associated data for a user
UserData* load_user_data(const char *ego_id) {
    if (!ego_id) return NULL;

    UserData *ud = (UserData*)malloc(sizeof(UserData));
    if (!ud) {
        fprintf(stderr, "Failed to allocate UserData\n");
        return NULL;
    }
    ud->user_id = strdup(ego_id);
    if (!ud->user_id) {
        free(ud);
        fprintf(stderr, "Failed to allocate user_id\n");
        return NULL;
    }
    ud->graph = init_graph();
    if (!ud->graph) {
        free(ud->user_id);
        free(ud);
        fprintf(stderr, "Failed to initialize graph for user %s\n", ego_id);
        return NULL;
    }
    ud->ego_features = (int*)calloc(MAX_FEATURES, sizeof(int));
    if (!ud->ego_features) {
        free(ud->graph->circle_assignments);
        free(ud->graph->nodes);
        free(ud->graph);
        free(ud->user_id);
        free(ud);
        fprintf(stderr, "Failed to allocate ego_features\n");
        return NULL;
    }
    ud->featnames = (char**)calloc(MAX_FEATURES, sizeof(char*));
    if (!ud->featnames) {
        free(ud->ego_features);
        free(ud->graph->circle_assignments);
        free(ud->graph->nodes);
        free(ud->graph);
        free(ud->user_id);
        free(ud);
        fprintf(stderr, "Failed to allocate featnames\n");
        return NULL;
    }

    // Load edges
    char edges_file[256];
    snprintf(edges_file, sizeof(edges_file), "twitter/%s.edges", ego_id);
    FILE *file = fopen(edges_file, "r");
    if (!file) {
        fprintf(stderr, "Warning: Could not open edges file %s\n", edges_file);
    } else {
        char line[MAX_LINE];
        while (fgets(line, MAX_LINE, file)) {
            int u, v;
            if (sscanf(line, "%d %d", &u, &v) == 2) {
                add_edge(ud->graph, u, v);
            }
        }
        fclose(file);
    }

    // Load circles
    char circles_file[256];
    snprintf(circles_file, sizeof(circles_file), "twitter/%s.circles", ego_id);
    file = fopen(circles_file, "r");
    if (file) {
        int circle_idx = 0;
        char line[MAX_LINE];
        while (fgets(line, MAX_LINE, file) && circle_idx < MAX_CIRCLES) {
            char *token = strtok(line, "\t\n"); // Split by tab or newline
            if (!token) continue;
            int circle_num;
            if (sscanf(token, "%d", &circle_num) != 1) continue;
            token = strtok(NULL, "\t\n");
            while (token) {
                int member = atoi(token);
                if (member >= 0 && member < MAX_NODES) {
                    ud->graph->circle_assignments[member] = circle_idx + 1;
                }
                token = strtok(NULL, "\t\n");
            }
            circle_idx++;
        }
        ud->graph->circle_count = circle_idx;
        fclose(file);
    }

    // Load ego features
    char egofeat_file[256];
    snprintf(egofeat_file, sizeof(egofeat_file), "twitter/%s.egofeat", ego_id);
    file = fopen(egofeat_file, "r");
    if (file) {
        char line[MAX_LINE];
        if (fgets(line, MAX_LINE, file)) {
            char *token = strtok(line, " \t\n");
            int idx = 0;
            while (token && idx < MAX_FEATURES) {
                int value = atoi(token);
                if (value == 1) {
                    ud->ego_features[idx] = 1;
                }
                token = strtok(NULL, " \t\n");
                idx++;
            }
        }
        fclose(file);
    }

    // Load node features
    char feat_file[256];
    snprintf(feat_file, sizeof(feat_file), "twitter/%s.feat", ego_id);
    file = fopen(feat_file, "r");
    if (file) {
        char line[MAX_LINE];
        while (fgets(line, MAX_LINE, file)) {
            int node_id;
            char *token = strtok(line, " \t\n");
            if (!token) continue;
            node_id = atoi(token);
            if (node_id < 0 || node_id >= MAX_NODES) continue;
            if (ud->graph->nodes[node_id].id == 0) {
                // Initialize node if not already (might not be in edges)
                ud->graph->nodes[node_id].id = node_id;
                ud->graph->nodes[node_id].neighbor_capacity = 10;
                ud->graph->nodes[node_id].neighbors = (int*)malloc(sizeof(int) * ud->graph->nodes[node_id].neighbor_capacity);
                if (!ud->graph->nodes[node_id].neighbors) {
                    fprintf(stderr, "Failed to allocate neighbors for node %d\n", node_id);
                    continue;
                }
                ud->graph->nodes[node_id].neighbor_count = 0;
                ud->graph->nodes[node_id].features = (int*)calloc(MAX_FEATURES, sizeof(int));
                if (!ud->graph->nodes[node_id].features) {
                    free(ud->graph->nodes[node_id].neighbors);
                    ud->graph->nodes[node_id].neighbors = NULL;
                    fprintf(stderr, "Failed to allocate features for node %d\n", node_id);
                    continue;
                }
                ud->graph->node_count++;
            }
            int idx = 0;
            token = strtok(NULL, " \t\n");
            while (token && idx < MAX_FEATURES) {
                int value = atoi(token);
                if (value == 1) {
                    ud->graph->nodes[node_id].features[idx] = 1;
                }
                token = strtok(NULL, " \t\n");
                idx++;
            }
        }
        fclose(file);
    }

    // Load feature names
    char featnames_file[256];
    snprintf(featnames_file, sizeof(featnames_file), "twitter/%s.featnames", ego_id);
    file = fopen(featnames_file, "r");
    if (file) {
        char line[MAX_LINE];
        while (fgets(line, MAX_LINE, file)) {
            int idx;
            char featname[256] = {0};
            if (sscanf(line, "%d %255[^\n]", &idx, featname) == 2) {
                if (idx >= 0 && idx < MAX_FEATURES) {
                    ud->featnames[idx] = strdup(featname);
                    if (!ud->featnames[idx]) {
                        fprintf(stderr, "Failed to allocate featname for index %d\n", idx);
                    }
                }
            }
        }
        fclose(file);
    }

    printf("Prepared data for user %s: %d nodes, %d circles\n", ego_id, ud->graph->node_count, ud->graph->circle_count);
    return ud;
}

// Get list of user IDs from .edges files
int get_user_ids(char **user_ids, int max_users) {
    DIR *dir = opendir("twitter");
    if (!dir) {
        fprintf(stderr, "Error opening directory twitter\n");
        return 0;
    }

    int user_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && user_count < max_users) {
        if (strstr(entry->d_name, ".edges")) {
            char *user_id = strdup(entry->d_name);
            if (user_id) {
                user_id[strcspn(user_id, ".")] = '\0'; // Remove ".edges"
                user_ids[user_count++] = user_id;
            }
        }
    }
    closedir(dir);
    return user_count;
}

// Simulate social actions
void simulate_actions(Graph *g) {
    if (!g) return;
    float action_weights[] = {0.50, 0.35, 0.15};  // retweet, comment, tag
    srand(time(NULL));
    for (int u = 0; u < MAX_NODES; u++) {
        if (g->nodes[u].id == 0) continue;
        for (int i = 0; i < g->nodes[u].neighbor_count; i++) {
            float r = (float)rand() / RAND_MAX;
            if (r < action_weights[0]) {
                g->nodes[u].action_weight = action_weights[0];
            } else if (r < action_weights[0] + action_weights[1]) {
                g->nodes[u].action_weight = action_weights[1];
            } else {
                g->nodes[u].action_weight = action_weights[2];
            }
        }
    }
}

// Print graph summary
void print_graph(UserData *ud) {
    if (!ud || !ud->graph) return;

    printf("Graph Summary for User: %s\n", ud->user_id);
    printf("-----------------------------------------\n");
    printf("Total Nodes: %d\n", ud->graph->node_count);
    printf("Total Circles: %d\n\n", ud->graph->circle_count);
    printf("Node List:\n-----------\n");

    for (int i = 0; i < MAX_NODES; ++i) {
        Node *node = &ud->graph->nodes[i];
        if (node->id == 0) continue;

        printf("Node ID: %d\n", node->id);
        printf("  Circle: %d\n", ud->graph->circle_assignments[node->id]);

        printf("  Features: [");
        int first = 1;
        for (int j = 0; j < MAX_FEATURES; ++j) {
            if (node->features && node->features[j]) {
                if (!first) printf(", ");
                if (ud->featnames[j]) {
                    printf("%s", ud->featnames[j]);
                } else {
                    printf("%d", j);
                }
                first = 0;
            }
        }
        printf("]\n");

        printf("  Neighbors: [");
        for (int j = 0; j < node->neighbor_count; ++j) {
            printf("%d", node->neighbors[j]);
            if (j < node->neighbor_count - 1) printf(", ");
        }
        printf("]\n");
        printf("  Action Weight: %.2f\n\n", node->action_weight);
    }
}

int main() {
    // Initialize user_ids array
    char *user_ids[MAX_USERS] = {0};
    int user_count = get_user_ids(user_ids, MAX_USERS);
    if (user_count == 0) {
        printf("No users found.\n");
        return 1;
    }
    printf("Found %d users\n", user_count);

    // Array to store user data
    UserData **users = (UserData**)calloc(user_count, sizeof(UserData*));
    if (!users) {
        fprintf(stderr, "Memory allocation failed for users array\n");
        for (int i = 0; i < user_count; i++) free(user_ids[i]);
        return 1;
    }

    // Load and prepare data for each user
    for (int i = 0; i < user_count; i++) {
        users[i] = load_user_data(user_ids[i]);
    }

    // Simulate actions
    #pragma omp parallel for num_threads(12)
    for (int i = 0; i < user_count; i++) {
        if (users[i] && users[i]->graph) {
            simulate_actions(users[i]->graph);
        }
    }

    // Print summary
    int valid_users = 0;
    for (int i = 0; i < user_count; i++) {
        if (users[i] && users[i]->graph && users[i]->graph->node_count > 0) {
            valid_users++;
            print_graph(users[i]);
        }
    }
    printf("Total valid users: %d\n", valid_users);

    // Clean up
    for (int i = 0; i < user_count; i++) {
        if (users[i]) {
            if (users[i]->graph) {
                for (int u = 0; u < MAX_NODES; u++) {
                    if (users[i]->graph->nodes[u].id != 0) {
                        free(users[i]->graph->nodes[u].neighbors);
                        free(users[i]->graph->nodes[u].features);
                    }
                }
                free(users[i]->graph->nodes);
                free(users[i]->graph->circle_assignments);
                free(users[i]->graph);
            }
            free(users[i]->ego_features);
            if (users[i]->featnames) {
                for (int j = 0; j < MAX_FEATURES; j++) {
                    free(users[i]->featnames[j]);
                }
                free(users[i]->featnames);
            }
            free(users[i]->user_id);
            free(users[i]);
        }
        free(user_ids[i]);
    }
    free(users);

    return 0;
}


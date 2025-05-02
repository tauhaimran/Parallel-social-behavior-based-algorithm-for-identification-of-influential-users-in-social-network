#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>

#define MAX_NODES 81306  // Total nodes in the dataset
#define MAX_FEATURES 1113  // Feature vector length
#define MAX_CIRCLES 100   // Max number of circles per user
#define MAX_LINE 1024
#define MAX_USERS 1000   // Adjust based on number of users

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
    int *circles;  // Array to store circle assignments
    int circle_count;
} Graph;

// Structure for user data
typedef struct UserData {
    char *user_id;
    Graph *graph;
    int *ego_features;  // Ego user's features
    char **featnames;  // Feature labels
} UserData;

// Structure for SCC/CAC partitioning context
typedef struct SCCContext {
    int *index;        // Vertex index
    int *lowlink;      // Lowest reachable vertex index
    int *level;        // Component level
    int *depth;        // Depth for merging
    int *type;         // Component type (0: undefined, 1: SCC, 2: CAC)
    int *component;    // Component ID
    int *stack;        // Stack for DFS
    int stack_top;     // Stack top
    int global_index;  // Global index counter
    int component_count; // Number of components
} SCCContext;

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
    g->circles = (int*)calloc(MAX_NODES, sizeof(int));
    if (!g->circles) {
        free(g->nodes);
        free(g);
        fprintf(stderr, "Failed to allocate circles\n");
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
    ud->ego_features = NULL;
    ud->featnames = NULL;

    // Load edges
    char edges_file[256];
    snprintf(edges_file, sizeof(edges_file), "twitter_data/%s.edges", ego_id);
    FILE *file = fopen(edges_file, "r");
    if (!file) {
        fprintf(stderr, "Warning: Could not open edges file %s\n", edges_file);
        return ud;
    }
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, file)) {
        int u, v;
        if (sscanf(line, "%d %d", &u, &v) == 2) {
            add_edge(ud->graph, u, v);
        }
    }
    fclose(file);

    // Load circles
    char circles_file[256];
    snprintf(circles_file, sizeof(circles_file), "twitter_data/%s.circles", ego_id);
    file = fopen(circles_file, "r");
    if (file) {
        int circle_idx = 0;
        while (fgets(line, MAX_LINE, file)) {
            char *token = strtok(line, " \t");
            if (!token) continue;
            token = strtok(NULL, " \t");
            while (token) {
                int member = atoi(token);
                if (member >= 0 && member < MAX_NODES) {
                    ud->graph->circles[member] = circle_idx + 1;
                }
                token = strtok(NULL, " \t");
            }
            circle_idx++;
        }
        ud->graph->circle_count = circle_idx;
        fclose(file);
    }

    // Load ego features
    ud->ego_features = (int*)calloc(MAX_FEATURES, sizeof(int));
    if (!ud->ego_features) {
        fprintf(stderr, "Failed to allocate ego_features for user %s\n", ego_id);
        return ud;
    }
    char egofeat_file[256];
    snprintf(egofeat_file, sizeof(egofeat_file), "twitter_data/%s.egofeat", ego_id);
    file = fopen(egofeat_file, "r");
    if (file) {
        if (fgets(line, MAX_LINE, file)) {
            char *token = strtok(line, " \t");
            while (token) {
                int idx = atoi(token);
                if (idx >= 0 && idx < MAX_FEATURES) ud->ego_features[idx] = 1;
                token = strtok(NULL, " \t");
            }
        }
        fclose(file);
    }

    // Load node features
    char feat_file[256];
    snprintf(feat_file, sizeof(feat_file), "twitter_data/%s.feat", ego_id);
    file = fopen(feat_file, "r");
    if (file) {
        while (fgets(line, MAX_LINE, file)) {
            int node_id;
            if (sscanf(line, "%d", &node_id) == 1 && node_id >= 0 && node_id < MAX_NODES) {
                if (ud->graph->nodes[node_id].id != 0 && !ud->graph->nodes[node_id].features) {
                    ud->graph->nodes[node_id].features = (int*)calloc(MAX_FEATURES, sizeof(int));
                    if (!ud->graph->nodes[node_id].features) {
                        fprintf(stderr, "Failed to allocate features for node %d\n", node_id);
                        continue;
                    }
                }
                char *token = strtok(line, " \t");
                token = strtok(NULL, " \t");
                while (token) {
                    int idx = atoi(token);
                    if (idx >= 0 && idx < MAX_FEATURES && ud->graph->nodes[node_id].features) {
                        ud->graph->nodes[node_id].features[idx] = 1;
                    }
                    token = strtok(NULL, " \t");
                }
            }
        }
        fclose(file);
    }

    // Load feature names
    ud->featnames = (char**)calloc(MAX_FEATURES, sizeof(char*));
    if (!ud->featnames) {
        fprintf(stderr, "Failed to allocate featnames for user %s\n", ego_id);
        return ud;
    }
    char featnames_file[256];
    snprintf(featnames_file, sizeof(featnames_file), "twitter_data/%s.featnames", ego_id);
    file = fopen(featnames_file, "r");
    if (file) {
        int feat_idx = 0;
        while (fgets(line, MAX_LINE, file) && feat_idx < MAX_FEATURES) {
            int idx;
            char featname[256];
            if (sscanf(line, "%d %255s", &idx, featname) == 2 && idx >= 0 && idx < MAX_FEATURES) {
                ud->featnames[idx] = strdup(featname);
                if (!ud->featnames[idx]) {
                    fprintf(stderr, "Failed to allocate featname %d\n", idx);
                }
                feat_idx++;
            }
        }
        fclose(file);
    }

    printf("Prepared data for user %s: %d nodes, %d circles\n", ego_id, ud->graph->node_count, ud->graph->circle_count);
    return ud;
}

// Get list of user IDs from .edges files
int get_user_ids(char **user_ids, int max_users) {
    DIR *dir = opendir("twitter_data");
    if (!dir) {
        fprintf(stderr, "Error opening directory twitter_data\n");
        return 0;
    }

    int user_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && user_count < max_users) {
        if (strstr(entry->d_name, ".edges")) {
            char *user_id = strdup(entry->d_name);
            if (user_id) {
                user_id[strlen(user_id) - 6] = '\0';
                user_ids[user_count++] = user_id;
            }
        }
    }
    closedir(dir);
    return user_count;
}

// Initialize SCC/CAC context
void init_scc_context(SCCContext *ctx) {
    ctx->index = (int*)calloc(MAX_NODES, sizeof(int));
    ctx->lowlink = (int*)calloc(MAX_NODES, sizeof(int));
    ctx->level = (int*)calloc(MAX_NODES, sizeof(int));
    ctx->depth = (int*)calloc(MAX_NODES, sizeof(int));
    ctx->type = (int*)calloc(MAX_NODES, sizeof(int));
    ctx->component = (int*)calloc(MAX_NODES, sizeof(int));
    ctx->stack = (int*)malloc(MAX_NODES * sizeof(int));
    if (!ctx->index || !ctx->lowlink || !ctx->level || !ctx->depth || !ctx->type || !ctx->component || !ctx->stack) {
        fprintf(stderr, "Failed to allocate SCC context\n");
        free(ctx->index);
        free(ctx->lowlink);
        free(ctx->level);
        free(ctx->depth);
        free(ctx->type);
        free(ctx->component);
        free(ctx->stack);
        ctx->index = ctx->lowlink = ctx->level = ctx->depth = ctx->type = ctx->component = ctx->stack = NULL;
    }
    ctx->stack_top = 0;
    ctx->global_index = 1;
    ctx->component_count = 0;
}

// Free SCC/CAC context
void free_scc_context(SCCContext *ctx) {
    free(ctx->index);
    free(ctx->lowlink);
    free(ctx->level);
    free(ctx->depth);
    free(ctx->type);
    free(ctx->component);
    free(ctx->stack);
}

// Discover function (Algorithm 2)
void discover(Graph *g, int v, SCCContext *ctx) {
    if (!g || v >= MAX_NODES || !ctx->index) return;
    ctx->index[v] = ctx->lowlink[v] = ctx->global_index++;
    ctx->level[v] = 1;
    ctx->depth[v] = 1;
    ctx->stack[ctx->stack_top++] = v;
}

// Explore function (Algorithm 3)
void explore(Graph *g, int v, SCCContext *ctx) {
    if (!g || v >= MAX_NODES || !ctx->index) return;
    for (int i = 0; i < g->nodes[v].neighbor_count; i++) {
        int w = g->nodes[v].neighbors[i];
        if (ctx->index[w] == 0) {
            // Unvisited node, perform DFS
            discover(g, w, ctx);
            explore(g, w, ctx);
            finish(g, w, ctx);
        }
        if (ctx->type[w] != 0) {
            // w is in a component
            ctx->level[v] = ctx->level[v] > ctx->level[w] + 1 ? ctx->level[v] : ctx->level[w] + 1;
        } else {
            // w is in the same SCC
            ctx->level[v] = ctx->level[v] > ctx->level[w] ? ctx->level[v] : ctx->level[w];
            ctx->lowlink[v] = ctx->lowlink[v] < ctx->lowlink[w] ? ctx->lowlink[v] : ctx->lowlink[w];
        }
    }
}

// Finish function (Algorithm 4)
void finish(Graph *g, int v, SCCContext *ctx) {
    if (!g || v >= MAX_NODES || !ctx->index) return;
    if (ctx->lowlink[v] == ctx->index[v]) {
        int size = 0;
        int w;
        ctx->component_count++;
        do {
            w = ctx->stack[--ctx->stack_top];
            ctx->lowlink[w] = ctx->index[v];
            ctx->level[w] = ctx->level[v];
            ctx->type[w] = 1; // SCC
            ctx->component[w] = ctx->component_count;
            size++;
        } while (w != v);

        if (size == 1) {
            ctx->type[v] = 2; // CAC
            int merge = 0;
            int *merge_list = (int*)calloc(g->nodes[v].neighbor_count, sizeof(int));
            int merge_count = 0;
            for (int i = 0; i < g->nodes[v].neighbor_count; i++) {
                int w = g->nodes[v].neighbors[i];
                if (ctx->type[w] == 1 && ctx->level[w] == ctx->level[v] - 1) {
                    merge = 0;
                    break;
                }
                if (ctx->type[w] == 2 && ctx->level[w] == ctx->level[v] - 1) {
                    merge_list[merge_count++] = w;
                    merge = 1;
                }
            }
            if (merge) {
                ctx->level[v]--;
                for (int i = 0; i < merge_count; i++) {
                    int w = merge_list[i];
                    ctx->component[w] = ctx->component_count;
                    ctx->level[w] = ctx->level[v];
                }
            }
            free(merge_list);
        }
    }
}

// SCC/CAC partitioning (Algorithm 1)
void find_sccs(Graph *g) {
    if (!g) return;
    SCCContext ctx = {0};
    init_scc_context(&ctx);
    if (!ctx.index) {
        free_scc_context(&ctx);
        return;
    }

    // Perform DFS for all unvisited nodes
    for (int u = 0; u < MAX_NODES; u++) {
        if (g->nodes[u].id != 0 && ctx.index[u] == 0) {
            discover(g, u, &ctx);
            explore(g, u, &ctx);
            finish(g, u, &ctx);
        }
    }

    printf("Found %d SCC/CAC components\n", ctx.component_count);
    free_scc_context(&ctx);
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

int main() {
    // Initialize user_ids array
    char *user_ids[MAX_USERS] = {0};
    int user_count = get_user_ids(user_ids, MAX_USERS);
    printf("Found %d users\n", user_count);

    // Array to store user data
    UserData **users = (UserData**)calloc(user_count, sizeof(UserData*));
    if (!users) {
        fprintf(stderr, "Memory allocation failed for users array\n");
        for (int i = 0; i < user_count; i++) free(user_ids[i]);
        return 1;
    }

    // Load and prepare data for each user serially
    for (int i = 0; i < user_count; i++) {
        users[i] = load_user_data(user_ids[i]);
    }

    // Find SCC/CAC components for each user serially
    for (int i = 0; i < user_count; i++) {
        if (users[i] && users[i]->graph && users[i]->graph->node_count > 0) {
            printf("Partitioning graph for user %s\n", users[i]->user_id);
            find_sccs(users[i]->graph);
        }
    }

    // Print summary
    int valid_users = 0;
    for (int i = 0; i < user_count; i++) {
        if (users[i] && users[i]->graph && users[i]->graph->node_count > 0) {
            valid_users++;
            printf("Valid data for user %s: %d nodes, %d circles\n", users[i]->user_id, users[i]->graph->node_count, users[i]->graph->circle_count);
        }
    }
    printf("Total valid users: %d\n", valid_users);



    #pragma omp parallel for num_threads(12)
    for (int i = 0; i < user_count; i++) {
        if (users[i] && users[i]->graph) {
            simulate_actions(users[i]->graph);
        }
    }


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
                free(users[i]->graph->circles);
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

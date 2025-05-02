// Algorithm 1: SCC/CAC Partitioning Algorithm
// Inputs: A weighted graph G=(V, E, W) where each node is labeled with its influence score
// Outputs: A well-partitioned graph of SCC/CAC components with their levels
// Description: Partitions the graph into Strongly Connected Components (SCC) and Connected Acyclic Components (CAC)
// using a DFS-based approach. Assigns levels to components to enable parallel processing. The algorithm initializes
// node properties (index, lowlink, level, depth), explores neighbors to detect communities, and merges nodes into
// communities, assigning component indices and levels.

void scc_cac_partition(Vertex* V, int V_count, Edge* E, int E_count, double* W, Community** communities, int* community_count) {
    int index = 1;
    Stack* stack = (Stack*)malloc(sizeof(Stack));
    stack->items = (Vertex**)malloc(V_count * sizeof(Vertex*));
    stack->top = -1;
    stack->capacity = V_count;
    int* heads = (int*)malloc(V_count * sizeof(int));
    int ind = 1;

    // Initialize vertices
    for (int i = 0; i < V_count; i++) {
        V[i].index = 0;
        V[i].lowlink = 0;
        V[i].level = 0;
        V[i].depth = 0;
        V[i].type = NULL;
        heads[i] = -1;
    }

    // Discover and explore vertices
    for (int i = 0; i < V_count; i++) {
        if (V[i].index == 0) {
            // Discover
            V[i].index = index;
            V[i].lowlink = index;
            V[i].level = 1;
            V[i].depth = 1;
            index++;
            stack->items[++stack->top] = &V[i];

            // Explore
            for (int j = 0; j < V[i].neighbor_count; j++) {
                int w_idx = V[i].neighbors[j];
                Vertex* w = &V[w_idx];
                if (w->index == 0) {
                    w->index = index;
                    w->lowlink = index;
                    w->level = 1;
                    w->depth = 1;
                    index++;
                    stack->items[++stack->top] = w;
                }
                if (w->type != NULL) {
                    V[i].level = (w->level > V[i].level + 1) ? w->level : V[i].level + 1;
                } else {
                    V[i].level = (V[i].level > w->level) ? V[i].level : w->level;
                    V[i].lowlink = (V[i].lowlink < w->lowlink) ? V[i].lowlink : w->lowlink;
                }
            }

            // Finish
            Vertex* v = stack->items[stack->top--];
            if (v->lowlink == v->index) {
                int size = 0;
                Community* new_comm = (Community*)malloc(sizeof(Community));
                new_comm->vertices = (int*)malloc(V_count * sizeof(int));
                new_comm->vertex_count = 0;
                new_comm->level = v->level;

                Vertex* w;
                do {
                    w = stack->items[stack->top--];
                    w->lowlink = v->index;
                    w->level = v->level;
                    w->type = "scc";
                    new_comm->vertices[new_comm->vertex_count++] = w - V;
                    size++;
                } while (w != v);

                if (size == 1) {
                    v->type = "cac";
                    int m = 0;
                    for (int k = 0; k < v->neighbor_count; k++) {
                        Vertex* w = &V[v->neighbors[k]];
                        if (w->type != NULL && strcmp(w->type, "scc") == 0 && w->level == v->level - 1) {
                            m = 0;
                            break;
                        }
                        if (w->type != NULL && strcmp(w->type, "cac") == 0 && w->level == v->level - 1) {
                            m = 1;
                        }
                    }
                    if (m) {
                        v->level--;
                    }
                }
                new_comm->type = v->type;
                communities[*community_count] = new_comm;
                (*community_count)++;
            }
        }
    }

    // Assign community indices
    for (int i = 0; i < V_count; i++) {
        Vertex* h = &V[i]; // Simplified: assumes v is head
        if (heads[h - V] == -1) {
            heads[h - V] = ind++;
        }
        V[i].comp = heads[h - V];
    }

    free(heads);
    free(stack->items);
    free(stack);
}






// Algorithm 5: Parallel Influence Power Measure Method
// Inputs: A graph G=(V, E), a social action set A={a1,...,an}, a set of friendship factors alpha={alpha1,...,alphan},
// a set of interest I={I1,...,Im}
// Outputs: An influence value of each vertex u in V
// Description: Computes the influence power of each node using a parallelized PageRank variant. Incorporates social
// actions (weighted by friendship factors) and user interests (via Jaccard Coefficient). The graph is processed level by
// level, with parallel threads handling independent components to calculate endorsement weights and influence power.

void parallel_influence_power(Vertex* V, int V_count, Edge* E, int E_count, double* A, int A_count, double* alpha, int* I, int I_count, double* influence_powers) {
    int n = 0;
    int max_levels = 100; // Assumption on max levels
    double* followers_count = (double*)calloc(V_count, sizeof(double));
    double* temp_sums = (double*)calloc(V_count, sizeof(double));
    double* weights = (double*)calloc(E_count, sizeof(double));

    while (n < max_levels) {
        int components_per_level = 10; // Placeholder: number of components at level n
        int num_threads = omp_get_max_threads();
        int c = components_per_level / num_threads + (components_per_level % num_threads ? 1 : 0);

        #pragma omp parallel for
        for (int p = 0; p < num_threads; p++) {
            for (int i = 0; i < V_count; i++) {
                for (int j = 0; j < V[i].neighbor_count; j++) {
                    int uj_idx = V[i].neighbors[j];
                    // Calculate common interest (Jaccard Coefficient)
                    double Ci = 0.0;
                    int common = 0, total = 0;
                    for (int k = 0; k < I_count; k++) {
                        if (I[i * I_count + k] && I[uj_idx * I_count + k]) {
                            common++;
                        }
                        if (I[i * I_count + k] || I[uj_idx * I_count + k]) {
                            total++;
                        }
                    }
                    if (total > 0) {
                        Ci = (double)common / total;
                    }

                    // Calculate endorsement weight (psi)
                    double psi = 0.0;
                    for (int a = 0; a < A_count; a++) {
                        double N_a = 1.0; // Placeholder: number of actions a between ui and uj
                        psi += alpha[a] * Ci * N_a;
                    }
                    psi /= 50.0; // Assume average 50 publications per user
                    weights[i * V_count + uj_idx] = psi;
                }
            }

            // Calculate followers fraction
            for (int i = 0; i < V_count; i++) {
                followers_count[i] = (double)V[i].neighbor_count / V_count;
            }

            // Calculate influence power
            for (int i = 0; i < V_count; i++) {
                double sum = 0.0;
                for (int j = 0; j < V[i].neighbor_count; j++) {
                    int uj_idx = V[i].neighbors[j];
                    sum += weights[uj_idx * V_count + i] * influence_powers[uj_idx] / V[uj_idx].neighbor_count;
                }
                temp_sums[i] = (1 - 0.85) * followers_count[i] + 0.85 * sum;
            }

            #pragma omp critical
            for (int i = 0; i < V_count; i++) {
                influence_powers[i] = temp_sums[i];
            }
        }
        n++;
    }

    free(followers_count);
    free(temp_sums);
    free(weights);
}







// Algorithm 6: Seed Candidates Selection Algorithm
// Inputs: A weighted graph G=(V, E) where each node is labeled with its influence score
// Outputs: A set of seed candidates selection I*={v1,...,vp}
// Description: Selects nodes as seed candidates based on their influence power compared to their local influence zone.
// The local influence zone is computed as the average influence over paths of length L, and a greedy approach increases
// L until the influence drops, selecting nodes where influence power exceeds the local average.

void seed_candidates_selection(Vertex* V, int V_count, Edge* E, int E_count, double* influence_powers, int** I_star, int* I_star_count) {
    *I_star = (int*)malloc(V_count * sizeof(int));
    *I_star_count = 0;

    for (int i = 0; i < V_count; i++) {
        int L = 1;
        double I_L = 0.0;
        int path_count = 0;
        double influence_sum = 0.0;
        // Compute I_L (average influence over paths of length L)
        for (int j = 0; j < V[i].neighbor_count; j++) {
            influence_sum += influence_powers[V[i].neighbors[j]];
            path_count++;
        }
        I_L = path_count > 0 ? influence_sum / path_count : 0.0;

        // Greedy increase of L until influence drops
        double I_L_next = I_L;
        while (I_L > I_L_next && influence_powers[i] > I_L) {
            L++;
            I_L_next = I_L * 0.9; // Placeholder: assume decay for next level
        }

        if (influence_powers[i] > I_L) {
            (*I_star)[*I_star_count] = i;
            (*I_star_count)++;
        }
    }
}





// Algorithm 7: Seed Selection Algorithm
// Inputs: A graph G=(V, E), where each vertex is labeled by its influence power IP
// Outputs: A set of seed nodes INF
// Description: Selects the final seed nodes by constructing influence BFS-trees for candidate nodes. Each tree
// represents the influence zone of a candidate. The algorithm selects the tree with the maximum size (number of nodes),
// computes a black path of candidate nodes within it, and chooses the tree with the minimum rank (average vertex
// distance) as the seed, removing overlapping candidates to reduce redundancy.

void seed_selection(Vertex* V, int V_count, Edge* E, int E_count, double* influence_powers, int* I_star, int I_star_count, int** INF, int* INF_count) {
    *INF = (int*)malloc(V_count * sizeof(int));
    *INF_count = 0;

    // Build influence BFS-trees
    typedef struct {
        int* nodes;
        int node_count;
        double rank;
        int root;
    } BFS_Tree;

    BFS_Tree* trees = (BFS_Tree*)malloc(I_star_count * sizeof(BFS_Tree));
    int tree_count = 0;

    for (int i = 0; i < I_star_count; i++) {
        int v = I_star[i];
        int* queue = (int*)malloc(V_count * sizeof(int));
        int front = 0, rear = 0;
        int* visited = (int*)calloc(V_count, sizeof(int));
        int* distances = (int*)calloc(V_count, sizeof(int));
        BFS_Tree tree;
        tree.nodes = (int*)malloc(V_count * sizeof(int));
        tree.node_count = 0;
        tree.root = v;

        queue[rear++] = v;
        visited[v] = 1;
        distances[v] = 0;

        while (front < rear) {
            int u = queue[front++];
            tree.nodes[tree.node_count++] = u;

            for (int j = 0; j < V[u].neighbor_count; j++) {
                int w = V[u].neighbors[j];
                int is_candidate = 0;
                for (int k = 0; k < I_star_count; k++) {
                    if (w == I_star[k]) {
                        is_candidate = 1;
                        break;
                    }
                }
                if (!visited[w] && is_candidate) {
                    queue[rear++] = w;
                    visited[w] = 1;
                    distances[w] = distances[u] + 1;
                }
            }
        }

        // Compute tree rank
        double rank_sum = 0.0;
        for (int j = 0; j < tree.node_count; j++) {
            rank_sum += distances[tree.nodes[j]];
        }
        tree.rank = tree.node_count > 0 ? rank_sum / tree.node_count : 0.0;
        trees[tree_count++] = tree;

        free(queue);
        free(visited);
        free(distances);
    }

    // Select seeds
    int* I_star_remaining = (int*)malloc(I_star_count * sizeof(int));
    memcpy(I_star_remaining, I_star, I_star_count * sizeof(int));
    int remaining_count = I_star_count;

    while (remaining_count > 0) {
        int max_size = -1;
        int max_idx = -1;
        for (int i = 0; i < tree_count; i++) {
            int is_valid = 0;
            for (int j = 0; j < remaining_count; j++) {
                if (trees[i].root == I_star_remaining[j]) {
                    is_valid = 1;
                    break;
                }
            }
            if (is_valid && trees[i].node_count > max_size) {
                max_size = trees[i].node_count;
                max_idx = i;
            }
        }

        if (max_idx == -1) break;

        int* BLACK = (int*)malloc(tree_count * sizeof(int));
        int BLACK_count = 0;
        for (int j = 0; j < trees[max_idx].node_count; j++) {
            for (int k = 0; k < remaining_count; k++) {
                if (trees[max_idx].nodes[j] == I_star_remaining[k]) {
                    BLACK[BLACK_count++] = trees[max_idx].nodes[j];
                    break;
                }
            }
        }

        double min_rank = INFINITY;
        int min_tree_idx = -1;
        for (int j = 0; j < BLACK_count; j++) {
            for (int k = 0; k < tree_count; k++) {
                if (trees[k].root == BLACK[j] && trees[k].rank < min_rank) {
                    min_rank = trees[k].rank;
                    min_tree_idx = k;
                }
            }
        }

        if (min_tree_idx != -1) {
            (*INF)[*INF_count] = trees[min_tree_idx].root;
            (*INF_count)++;

            int* new_I_star = (int*)malloc(V_count * sizeof(int));
            int new_count = 0;
            for (int j = 0; j < remaining_count; j++) {
                int keep = 1;
                for (int k = 0; k < BLACK_count; k++) {
                    if (I_star_remaining[j] == BLACK[k]) {
                        keep = 0;
                        break;
                    }
                }
                if (I_star_remaining[j] == trees[min_tree_idx].root) {
                    keep = 0;
                }
                if (keep) {
                    new_I_star[new_count++] = I_star_remaining[j];
                }
            }
            memcpy(I_star_remaining, new_I_star, new_count * sizeof(int));
            remaining_count = new_count;
            free(new_I_star);
        }

        free(BLACK);
    }

    for (int i = 0; i < tree_count; i++) {
        free(trees[i].nodes);
    }
    free(trees);
    free(I_star_remaining);
}
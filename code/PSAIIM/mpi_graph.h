#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <random>
#include <stack>
#include <set>
#include <cmath>
#include <queue>
#include <limits>
#include <unordered_map>
#include <functional>
#include <iomanip>
#include <mpi.h>

class Graph {
private:
    class Node {
    public:
        long int node_id;
        std::vector<long int> followers;
        std::vector<double> followers_weight;
        std::vector<long int> retweets;
        std::vector<double> retweet_weight;
        std::vector<long int> replies;
        std::vector<double> reply_weight;
        std::vector<long int> mentions;
        std::vector<double> mention_weight;
        double total_followers_weight;
        double total_retweet_weight;
        double total_reply_weight;
        double total_mention_weight;
        int followers_count;
        int retweets_count;
        int replies_count;
        int mentions_count;
        std::vector<std::string> interests;
        double influence_power;
        int community_id;

        Node(long int id = 0)
            : node_id(id), total_followers_weight(0.0), total_retweet_weight(0.0),
              total_reply_weight(0.0), total_mention_weight(0.0),
              followers_count(0), retweets_count(0), replies_count(0), mentions_count(0),
              influence_power(0.0), community_id(-1) {}

        void setInterests(const std::vector<std::string>& new_interests) {
            interests = new_interests;
        }
    };

    std::vector<Node> nodes; // Local nodes for this process
    long int MAX_NODES = 500000;
    long int local_start, local_end, local_size;
    int index;
    std::stack<long int> node_stack;
    std::vector<int> lowlink, level;
    std::vector<bool> on_stack;
    std::vector<std::vector<long int>> communities;
    const double ALPHA_RETWEET = 0.50;
    const double ALPHA_COMMENT = 0.35;
    const double ALPHA_MENTION = 0.15;
    const double DAMPING_FACTOR = 0.85;
    const int MAX_ITERATIONS = 100;
    const double CONVERGENCE_THRESHOLD = 1e-6;
    const double IP_THRESHOLD = 0.015;
    std::ofstream log_file;
    int rank, size;

    struct InfluenceBFSTree {
        long int root;
        std::vector<long int> nodes;
        std::vector<int> distances;
        double rank;
    };

    void log_message(const std::string& message) {
        if (rank == 0) {
            log_file << message;
            std::cout << message;
        } else {
            MPI_Send(message.c_str(), message.size() + 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
        }
    }

    double calculateJaccard(const std::vector<std::string>& interests1, const std::vector<std::string>& interests2) {
        if (interests1.empty() || interests2.empty()) {
            log_message("Jaccard: Empty interest list detected\n");
            return 0.0; // Return 0 for empty interest lists
        }
        std::set<std::string> set1(interests1.begin(), interests1.end());
        std::set<std::string> set2(interests2.begin(), interests2.end());
        std::vector<std::string> intersection, union_set;
        std::set_intersection(set1.begin(), set1.end(), set2.begin(), set2.end(), std::back_inserter(intersection));
        std::set_union(set1.begin(), set1.end(), set2.begin(), set2.end(), std::back_inserter(union_set));
        double ci = union_set.empty() ? 0.0 : static_cast<double>(intersection.size()) / union_set.size();
        std::stringstream ss;
        ss << "Calculating Jaccard similarity: Intersection size = " << intersection.size() 
           << ", Union size = " << union_set.size() << ", Similarity = " << ci << "\n";
        log_message(ss.str());
        return ci;
    }

    double calculateEdgeWeight(long int u_x, long int u_y, std::set<std::pair<long int, long int>>& printed_edges) {
        // Validate node IDs
        if (u_x >= MAX_NODES || u_y >= MAX_NODES || u_x < 0 || u_y < 0) {
            log_message("Invalid edge (" + std::to_string(u_x) + "," + std::to_string(u_y) + "): Out of bounds\n");
            return 0.0;
        }

        std::stringstream ss;
        ss << "Rank " << rank << ": Calculating edge weight for (" << u_x << "," << u_y << ")\n";
        log_message(ss.str());

        // Fetch interests and weights for u_x and u_y
        std::vector<std::string> interests_x, interests_y;
        double retweet_weight_y = 0.0, reply_weight_y = 0.0, mention_weight_y = 0.0;

        // Handle u_x interests
        if (u_x >= local_start && u_x < local_end) {
            interests_x = nodes[u_x - local_start].interests;
        } else {
            int dest_rank = u_x / (MAX_NODES / size);
            if (dest_rank >= size) dest_rank = size - 1;
            ss.str("");
            ss << "Rank " << rank << ": Fetching interests for u_x = " << u_x << " from rank " << dest_rank << "\n";
            log_message(ss.str());

            int interest_count;
            MPI_Recv(&interest_count, 1, MPI_INT, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (interest_count < 0 || interest_count > 1000) { // Arbitrary limit to prevent buffer issues
                log_message("Invalid interest_count for u_x = " + std::to_string(u_x) + ": " + std::to_string(interest_count) + "\n");
                return 0.0;
            }
            // Use a larger buffer to handle long interest strings
            std::vector<char> interest_buffer(interest_count * 256); // Increased to 256 chars per interest
            MPI_Recv(interest_buffer.data(), interest_count * 256, MPI_CHAR, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::stringstream iss(std::string(interest_buffer.begin(), interest_buffer.begin() + strlen(interest_buffer.data())));
            std::string interest;
            while (iss >> interest) {
                if (interest.size() > 255) {
                    log_message("Warning: Truncating long interest string for u_x = " + std::to_string(u_x) + "\n");
                    interest = interest.substr(0, 255);
                }
                interests_x.push_back(interest);
            }
        }

        // Handle u_y interests and weights
        if (u_y >= local_start && u_y < local_end) {
            long int idx = u_y - local_start;
            if (idx < 0 || idx >= static_cast<long int>(nodes.size())) {
                log_message("Invalid index for u_y = " + std::to_string(u_y) + ": idx = " + std::to_string(idx) + "\n");
                return 0.0;
            }
            interests_y = nodes[idx].interests;
            retweet_weight_y = nodes[idx].total_retweet_weight;
            reply_weight_y = nodes[idx].total_reply_weight;
            mention_weight_y = nodes[idx].total_mention_weight;
        } else {
            int dest_rank = u_y / (MAX_NODES / size);
            if (dest_rank >= size) dest_rank = size - 1;
            ss.str("");
            ss << "Rank " << rank << ": Fetching data for u_y = " << u_y << " from rank " << dest_rank << "\n";
            log_message(ss.str());

            int interest_count;
            MPI_Recv(&interest_count, 1, MPI_INT, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (interest_count < 0 || interest_count > 1000) {
                log_message("Invalid interest_count for u_y = " + std::to_string(u_y) + ": " + std::to_string(interest_count) + "\n");
                return 0.0;
            }
            std::vector<char> interest_buffer(interest_count * 256);
            MPI_Recv(interest_buffer.data(), interest_count * 256, MPI_CHAR, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::stringstream iss(std::string(interest_buffer.begin(), interest_buffer.begin() + strlen(interest_buffer.data())));
            std::string interest;
            while (iss >> interest) {
                if (interest.size() > 255) {
                    log_message("Warning: Truncating long interest string for u_y = " + std::to_string(u_y) + "\n");
                    interest = interest.substr(0, 255);
                }
                interests_y.push_back(interest);
            }
            MPI_Recv(&retweet_weight_y, 1, MPI_DOUBLE, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&reply_weight_y, 1, MPI_DOUBLE, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&mention_weight_y, 1, MPI_DOUBLE, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        double ci = interests_x.empty() || interests_y.empty() ? 0.0 : calculateJaccard(interests_x, interests_y);
        double sum = 0.0;
        sum += ALPHA_RETWEET * ci * retweet_weight_y;
        sum += ALPHA_COMMENT * ci * reply_weight_y;
        sum += ALPHA_MENTION * ci * mention_weight_y;
        double total_interactions = retweet_weight_y + reply_weight_y + mention_weight_y;
        double psi = total_interactions > 0 ? sum / total_interactions : 0.0;
        if (u_x < 5 && u_y < 5 && printed_edges.insert({u_x, u_y}).second) {
            std::stringstream ss;
            ss << "Edge (" << u_x << "," << u_y << "): Jaccard Similarity = " << ci << ", Edge Weight = " << psi << "\n";
            log_message(ss.str());
        }
        return psi;
    }

    void DFS_SCC(long int v, std::vector<std::vector<long int>>& components) {
        if (v >= MAX_NODES || v < 0 || lowlink[v] != -1) {
            log_message("Skipping DFS for node " + std::to_string(v) + ": Out of bounds or already visited\n");
            return;
        }

        lowlink[v] = level[v] = index++;
        node_stack.push(v);
        on_stack[v] = true;
        log_message("DFS: Processing node " + std::to_string(v) + ", lowlink = " + std::to_string(lowlink[v]) + ", level = " + std::to_string(level[v]) + "\n");

        for (long int w : nodes[v - local_start].followers) {
            if (w >= MAX_NODES || w < 0) continue;
            if (lowlink[w] == -1) {
                log_message("DFS: Exploring edge from " + std::to_string(v) + " to unvisited node " + std::to_string(w) + "\n");
                DFS_SCC(w, components);
                lowlink[v] = std::min(lowlink[v], lowlink[w]);
                log_message("DFS: Updated lowlink for node " + std::to_string(v) + " to " + std::to_string(lowlink[v]) + "\n");
            } else if (on_stack[w]) {
                lowlink[v] = std::min(lowlink[v], lowlink[w]);
                log_message("DFS: Found back edge to node " + std::to_string(w) + ", updated lowlink for " + std::to_string(v) + " to " + std::to_string(lowlink[v]) + "\n");
            }
        }

        if (lowlink[v] == level[v]) {
            std::vector<long int> component;
            long int w;
            do {
                w = node_stack.top();
                node_stack.pop();
                on_stack[w] = false;
                component.push_back(w);
                if (w >= local_start && w < local_end) {
                    nodes[w - local_start].community_id = components.size();
                }
            } while (w != v);
            components.push_back(component);
            log_message("Found SCC: Component " + std::to_string(components.size() - 1) + " with " + std::to_string(component.size()) + " nodes\n");
        }
    }

    InfluenceBFSTree buildInfluenceBFSTree(long int root, const std::set<long int>& candidates) {
        InfluenceBFSTree tree;
        tree.root = root;
        std::queue<long int> q;
        std::vector<bool> visited(MAX_NODES, false);
        std::vector<int> distance(MAX_NODES, 0);
        q.push(root);
        visited[root] = true;
        tree.nodes.push_back(root);
        tree.distances.push_back(0);
        log_message("Building BFS tree for root node " + std::to_string(root) + "\n");

        while (!q.empty()) {
            long int u = q.front();
            q.pop();
            std::vector<long int> all_neighbors;
            if (u >= local_start && u < local_end) {
                all_neighbors.insert(all_neighbors.end(), nodes[u - local_start].followers.begin(), nodes[u - local_start].followers.end());
                all_neighbors.insert(all_neighbors.end(), nodes[u - local_start].retweets.begin(), nodes[u - local_start].retweets.end());
                all_neighbors.insert(all_neighbors.end(), nodes[u - local_start].replies.begin(), nodes[u - local_start].replies.end());
                all_neighbors.insert(all_neighbors.end(), nodes[u - local_start].mentions.begin(), nodes[u - local_start].mentions.end());
            } else {
                int dest_rank = u / (MAX_NODES / size);
                if (dest_rank >= size) dest_rank = size - 1;
                int neighbor_count;
                MPI_Recv(&neighbor_count, 1, MPI_INT, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                std::vector<long int> neighbor_buffer(neighbor_count);
                MPI_Recv(neighbor_buffer.data(), neighbor_count, MPI_LONG, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                all_neighbors = neighbor_buffer;
            }
            for (long int w : all_neighbors) {
                if (w >= MAX_NODES || w < 0 || visited[w]) continue;
                visited[w] = true;
                distance[w] = distance[u] + 1;
                tree.nodes.push_back(w);
                tree.distances.push_back(distance[w]);
                if (candidates.count(w) && distance[w] <= 2) {
                    q.push(w);
                    log_message("BFS: Added node " + std::to_string(w) + " at distance " + std::to_string(distance[w]) + "\n");
                }
            }
        }

        double sum_rank = 0.0;
        for (int d : tree.distances) {
            sum_rank += d;
        }
        tree.rank = tree.nodes.size() > 0 ? sum_rank / tree.nodes.size() : 0.0;
        std::stringstream ss;
        ss << "BFS tree for node " << root << ": " << tree.nodes.size() << " nodes, average distance: " << tree.rank << "\n";
        log_message(ss.str());
        return tree;
    }

    double computeInfluenceZone(long int v, int L) {
        if (v >= MAX_NODES || v < 0) {
            log_message("Invalid node " + std::to_string(v) + " for influence zone computation\n");
            return 0.0;
        }
        std::queue<std::pair<long int, int>> q;
        std::vector<bool> visited(MAX_NODES, false);
        q.push(std::make_pair(v, 0));
        visited[v] = true;
        double sum_ip = 0.0;
        int count = 0;

        log_message("Computing influence zone for node " + std::to_string(v) + " with L = " + std::to_string(L) + "\n");
        while (!q.empty()) {
            auto current = q.front();
            q.pop();
            long int u = current.first;
            int dist = current.second;
            if (dist <= L) {
                double ip_u;
                if (u >= local_start && u < local_end) {
                    ip_u = nodes[u - local_start].influence_power;
                } else {
                    int dest_rank = u / (MAX_NODES / size);
                    if (dest_rank >= size) dest_rank = size - 1;
                    MPI_Recv(&ip_u, 1, MPI_DOUBLE, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }
                sum_ip += ip_u;
                count++;
                std::vector<long int> all_neighbors;
                if (u >= local_start && u < local_end) {
                    all_neighbors.insert(all_neighbors.end(), nodes[u - local_start].followers.begin(), nodes[u - local_start].followers.end());
                    all_neighbors.insert(all_neighbors.end(), nodes[u - local_start].retweets.begin(), nodes[u - local_start].retweets.end());
                    all_neighbors.insert(all_neighbors.end(), nodes[u - local_start].replies.begin(), nodes[u - local_start].replies.end());
                    all_neighbors.insert(all_neighbors.end(), nodes[u - local_start].mentions.begin(), nodes[u - local_start].mentions.end());
                } else {
                    int dest_rank = u / (MAX_NODES / size);
                    if (dest_rank >= size) dest_rank = size - 1;
                    int neighbor_count;
                    MPI_Recv(&neighbor_count, 1, MPI_INT, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    std::vector<long int> neighbor_buffer(neighbor_count);
                    MPI_Recv(neighbor_buffer.data(), neighbor_count, MPI_LONG, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    all_neighbors = neighbor_buffer;
                }
                for (long int w : all_neighbors) {
                    if (w >= MAX_NODES || w < 0 || visited[w]) continue;
                    visited[w] = true;
                    q.push(std::make_pair(w, dist + 1));
                    log_message("Influence zone: Added node " + std::to_string(w) + " at distance " + std::to_string(dist + 1) + "\n");
                }
            }
        }
        double I_L = count > 0 ? sum_ip / count : 0.0;
        if (v < 5) {
            std::stringstream ss;
            ss << "Node " << v << ": Influence Zone (L=" << L << ") = " << I_L << ", Nodes reached = " << count << "\n";
            log_message(ss.str());
        }
        return I_L;
    }

    bool verifyInfluentialUsers(const std::vector<long int>& influential_nodes, bool is_seed_selection, int expected_size) {
        std::stringstream ss;
        ss << "Verifying influential users (size = " << influential_nodes.size() << ", is_seed_selection = " 
           << is_seed_selection << ", expected_size = " << expected_size << ")\n";
        log_message(ss.str());

        bool is_valid = true;

        // 1. Check size constraint
        if (is_seed_selection && influential_nodes.size() > expected_size) {
            ss.str("");
            ss << "Error: Seed selection size (" << influential_nodes.size() << ") exceeds expected size (" 
               << expected_size << ")\n";
            log_message(ss.str());
            is_valid = false;
        } else if (!is_seed_selection && influential_nodes.size() != expected_size) {
            ss.str("");
            ss << "Error: Top influential nodes size (" << influential_nodes.size() << ") does not match expected size (" 
               << expected_size << ")\n";
            log_message(ss.str());
            is_valid = false;
        }

        // 2. Check for duplicates
        std::set<long int> unique_nodes(influential_nodes.begin(), influential_nodes.end());
        if (unique_nodes.size() != influential_nodes.size()) {
            log_message("Error: Influential nodes contain duplicates\n");
            is_valid = false;
        }

        // 3. Verify each node
        for (size_t i = 0; i < influential_nodes.size(); ++i) {
            long int node_id = influential_nodes[i];
            if (node_id < 0 || node_id >= MAX_NODES) {
                ss.str("");
                ss << "Error: Node " << node_id << " is out of bounds\n";
                log_message(ss.str());
                is_valid = false;
                continue;
            }

            double ip;
            int comm_id, followers_count, retweets_count, replies_count, mentions_count;
            if (node_id >= local_start && node_id < local_end) {
                Node& node = nodes[node_id - local_start];
                ip = node.influence_power;
                comm_id = node.community_id;
                followers_count = node.followers_count;
                retweets_count = node.retweets_count;
                replies_count = node.replies_count;
                mentions_count = node.mentions_count;
            } else {
                int dest_rank = node_id / (MAX_NODES / size);
                if (dest_rank >= size) dest_rank = size - 1;
                MPI_Recv(&ip, 1, MPI_DOUBLE, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&comm_id, 1, MPI_INT, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&followers_count, 1, MPI_INT, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&retweets_count, 1, MPI_INT, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&replies_count, 1, MPI_INT, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&mentions_count, 1, MPI_INT, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            int L = 1;
            double I_L = computeInfluenceZone(node_id, L);
            if (ip < IP_THRESHOLD && ip < I_L) {
                ss.str("");
                ss << "Warning: Node " << node_id << " has low influence power (" << ip 
                   << ") below threshold (" << IP_THRESHOLD << ") and influence zone (" << I_L << ")\n";
                log_message(ss.str());
            }

            if (comm_id < 0 || static_cast<size_t>(comm_id) >= communities.size()) {
                ss.str("");
                ss << "Error: Node " << node_id << " has invalid community ID (" << comm_id << ")\n";
                log_message(ss.str());
                is_valid = false;
            }

            if (followers_count == 0 && retweets_count == 0 && replies_count == 0 && mentions_count == 0) {
                ss.str("");
                ss << "Error: Node " << node_id << " has no interactions\n";
                log_message(ss.str());
                is_valid = false;
            }
        }

        // 4. If seed selection, verify BFS tree and black path logic
        if (is_seed_selection && is_valid) {
            std::set<long int> candidate_set(influential_nodes.begin(), influential_nodes.end());
            for (long int seed : influential_nodes) {
                InfluenceBFSTree tree = buildInfluenceBFSTree(seed, candidate_set);
                bool found = false;
                for (size_t j = 0; j < tree.nodes.size(); ++j) {
                    if (tree.nodes[j] == seed && tree.distances[j] == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    ss.str("");
                    ss << "Error: Seed node " << seed << " not found in its own BFS tree\n";
                    log_message(ss.str());
                    is_valid = false;
                }
            }
        }

        // 5. If top influential nodes, verify sorting by influence power
        if (!is_seed_selection && is_valid) {
            for (size_t i = 1; i < influential_nodes.size(); ++i) {
                double ip_prev, ip_curr;
                if (influential_nodes[i-1] >= local_start && influential_nodes[i-1] < local_end) {
                    ip_prev = nodes[influential_nodes[i-1] - local_start].influence_power;
                } else {
                    int dest_rank = influential_nodes[i-1] / (MAX_NODES / size);
                    if (dest_rank >= size) dest_rank = size - 1;
                    MPI_Recv(&ip_prev, 1, MPI_DOUBLE, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }
                if (influential_nodes[i] >= local_start && influential_nodes[i] < local_end) {
                    ip_curr = nodes[influential_nodes[i] - local_start].influence_power;
                } else {
                    int dest_rank = influential_nodes[i] / (MAX_NODES / size);
                    if (dest_rank >= size) dest_rank = size - 1;
                    MPI_Recv(&ip_curr, 1, MPI_DOUBLE, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }
                if (ip_curr > ip_prev) {
                    ss.str("");
                    ss << "Error: Influential nodes not sorted by influence power at index " << i << "\n";
                    log_message(ss.str());
                    is_valid = false;
                }
            }
        }

        int global_valid;
        MPI_Reduce(&is_valid, &global_valid, 1, MPI_INT, MPI_LAND, 0, MPI_COMM_WORLD);
        if (rank == 0) {
            if (global_valid) {
                log_message("Verification passed: Influential users are valid\n");
            } else {
                log_message("Verification failed: Influential users are invalid\n");
            }
        }
        return global_valid;
    }

public:
    Graph() : index(0), rank(0), size(1) {
        // Defer MPI initialization to initializeMPI()
    }

    void initializeMPI() {
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        if (rank == 0) {
            log_file.open("graph_analysis.log", std::ios::app);
            if (!log_file.is_open()) {
                std::cerr << "Failed to open log file\n";
            }
            log_file << "\n=== New Graph Analysis Session ===\n";
        }
    }

    int getRank() const { return rank; }

    ~Graph() {
        if (rank == 0 && log_file.is_open()) {
            log_file.close();
        }
    }

    void saveLogsToFile() {
        if (rank == 0) {
            std::ofstream log_file("graph_analysis.log", std::ios::app); // Append mode
            if (!log_file.is_open()) {
                std::cerr << "Error: Unable to open log file.\n";
                return;
            }
    
            log_file << "\n=== Final Analysis Summary ===\n";
            log_file << "Total Nodes: " << MAX_NODES << "\n";
            log_file << "Number of Communities: " << communities.size() << "\n";
            for (size_t i = 0; i < communities.size(); ++i) {
                log_file << "Community " << i << ": " << communities[i].size() << " nodes\n";
            }
    
            log_file << "Top 10 Influential Nodes:\n";
            auto top_nodes = getTopInfluentialNodes();
            for (const auto& node : top_nodes) {
                log_file << "Node " << node.first << ": Influence Power = "
                         << std::fixed << std::setprecision(6) << node.second
                         << ", Community ID = " << nodes[node.first - local_start].community_id << "\n";
            }
    
            log_file << "================================\n";
            std::cout << "Analysis logs saved to graph_analysis.log\n";
        }
    }
    

    std::vector<std::pair<long int, double>> getTopInfluentialNodes() {
        std::vector<std::pair<long int, double>> local_node_influence;
        local_node_influence.reserve(local_size);
        for (long int i = local_start; i < local_end; ++i) {
            local_node_influence.emplace_back(i, nodes[i - local_start].influence_power);
        }
        std::vector<double> all_influence(MAX_NODES);
        std::vector<double> local_influence(local_size);
        for (long int i = 0; i < local_size; ++i) {
            local_influence[i] = nodes[i].influence_power;
        }
        MPI_Allgather(local_influence.data(), local_size, MPI_DOUBLE, all_influence.data(), local_size, MPI_DOUBLE, MPI_COMM_WORLD);
        std::vector<std::pair<long int, double>> node_influence;
        for (long int i = 0; i < MAX_NODES; ++i) {
            node_influence.emplace_back(i, all_influence[i]);
        }
        std::sort(node_influence.begin(), node_influence.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        std::vector<std::pair<long int, double>> top_nodes(node_influence.begin(), 
            node_influence.begin() + std::min(10, static_cast<int>(node_influence.size())));
        
        std::vector<long int> top_node_ids;
        for (const auto& node : top_nodes) {
            top_node_ids.push_back(node.first);
        }
        verifyInfluentialUsers(top_node_ids, false, 10);
        
        if (rank == 0) {
            log_message("Retrieved top 10 influential nodes\n");
            std::cout << "Top 10 influential nodes retrieved\n";
        }
        return top_nodes;
    }

    long int determineMaxNodes(const std::vector<std::string>& files) {
        long int max_id = 0;
        if (rank == 0) {
            for (const auto& filename : files) {
                if (filename == "higgs-interests.txt") continue;
                std::ifstream file("./higg/" + filename);
                if (!file.is_open()) {
                    log_message("Error: Unable to open file " + filename + "\n");
                    continue;
                }
                std::string line;
                while (std::getline(file, line)) {
                    if (line.empty() || line[0] == '#') continue;
                    long int from, to;
                    double weight;
                    if (sscanf(line.c_str(), "%ld %ld %lf", &from, &to, &weight) >= 2) {
                        max_id = std::max(max_id, std::max(from, to));
                    }
                }
                file.close();
                log_message("Processed file " + filename + ", max node ID found: " + std::to_string(max_id) + "\n");
            }
        }
        MPI_Bcast(&max_id, 1, MPI_LONG, 0, MPI_COMM_WORLD);
        long int result = max_id + 1;
        if (rank == 0) {
            log_message("Determined maximum nodes: " + std::to_string(result) + "\n");
            std::cout << "Determined maximum nodes: " + std::to_string(result) + "\n";
        }
        return result;
    }

    void initialize(long int max_nodes) {
        MAX_NODES = max_nodes;
        local_size = MAX_NODES / size;
        if (rank < MAX_NODES % size) local_size++;
        local_start = rank * (MAX_NODES / size) + std::min(static_cast<long int>(rank), MAX_NODES % size);
        local_end = local_start + local_size;
        nodes.resize(local_size);
        lowlink.resize(MAX_NODES, -1);
        level.resize(MAX_NODES, 0);
        on_stack.resize(MAX_NODES, false);
        for (long int i = 0; i < local_size; ++i) {
            nodes[i] = Node(local_start + i);
        }
        if (rank == 0) {
            log_message("Initialized graph with " + std::to_string(MAX_NODES) + " nodes\n");
            std::cout << "Initialized graph with " + std::to_string(MAX_NODES) + " nodes\n";
        }
    }

    void addEdge(const std::string& filename, long int from, long int to, double weight = 1.0) {
        if (from >= MAX_NODES || to >= MAX_NODES || from < 0 || to < 0) {
            log_message("Invalid edge (" + std::to_string(from) + "," + std::to_string(to) + ") in " + filename + ": Out of bounds\n");
            return;
        }
        if (to >= local_start && to < local_end) {
            Node& node = nodes[to - local_start];
            if (filename.find("higgs-social_network") != std::string::npos) {
                node.followers.push_back(from);
                node.followers_weight.push_back(weight);
                node.followers_count++;
                node.total_followers_weight += weight;
                log_message("Added follower edge (" + std::to_string(from) + "," + std::to_string(to) + ") in social network, weight = " + std::to_string(weight) + "\n");
            } else if (filename.find("higgs-retweet_network") != std::string::npos) {
                node.retweets.push_back(from);
                node.retweet_weight.push_back(weight);
                node.retweets_count++;
                node.total_retweet_weight += weight;
                log_message("Added retweet edge (" + std::to_string(from) + "," + std::to_string(to) + ") in retweet network, weight = " + std::to_string(weight) + "\n");
            } else if (filename.find("higgs-reply_network") != std::string::npos) {
                node.replies.push_back(from);
                node.reply_weight.push_back(weight);
                node.replies_count++;
                node.total_reply_weight += weight;
                log_message("Added reply edge (" + std::to_string(from) + "," + std::to_string(to) + ") in reply network, weight = " + std::to_string(weight) + "\n");
            } else if (filename.find("higgs-mention_network") != std::string::npos) {
                node.mentions.push_back(from);
                node.mention_weight.push_back(weight);
                node.mentions_count++;
                node.total_mention_weight += weight;
                log_message("Added mention edge (" + std::to_string(from) + "," + std::to_string(to) + ") in mention network, weight = " + std::to_string(weight) + "\n");
            }
        }
    }

    void loadFromFile(const std::vector<std::string>& files) {
        if (rank == 0) {
            for (const auto& filename : files) {
                std::ifstream file("./higg/" + filename);
                if (!file.is_open()) {
                    log_message("Error: Unable to open file " + filename + "\n");
                    continue;
                }
                std::vector<std::string> lines;
                std::string line;
                int line_count = 0;
                while (std::getline(file, line)) {
                    if (!line.empty() && line[0] != '#') {
                        lines.push_back(line);
                    }
                    line_count++;
                }
                file.close();
                int lines_size = lines.size();
                MPI_Bcast(&lines_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
                for (const auto& line : lines) {
                    if (filename == "higgs-interests.txt") {
                        std::istringstream iss(line);
                        long int node_id;
                        std::vector<std::string> interests;
                        std::string interest;
                        if (!(iss >> node_id)) {
                            log_message("Invalid line " + std::to_string(line_count) + " in " + filename + ": " + line + "\n");
                            continue;
                        }
                        while (iss >> interest) {
                            if (interest.size() > 255) {
                                log_message("Warning: Truncating long interest string for node " + std::to_string(node_id) + "\n");
                                interest = interest.substr(0, 255);
                            }
                            interests.push_back(interest);
                        }
                        int dest_rank = node_id / (MAX_NODES / size);
                        if (dest_rank >= size) dest_rank = size - 1;
                        if (dest_rank == 0 && node_id >= local_start && node_id < local_end) {
                            nodes[node_id - local_start].setInterests(interests);
                            if (node_id < 5) {
                                std::stringstream ss;
                                ss << "Loaded interests for node " << node_id << ": ";
                                for (const auto& i : interests) ss << i << " ";
                                ss << "\n";
                                log_message(ss.str());
                            }
                        } else {
                            int interest_count = interests.size();
                            MPI_Send(&interest_count, 1, MPI_INT, dest_rank, 0, MPI_COMM_WORLD);
                            std::stringstream ss;
                            for (const auto& i : interests) ss << i << " ";
                            std::string interest_str = ss.str();
                            MPI_Send(interest_str.c_str(), interest_str.size() + 1, MPI_CHAR, dest_rank, 0, MPI_COMM_WORLD);
                        }
                    } else {
                        long int from, to;
                        double weight = 1.0;
                        try {
                            std::istringstream iss(line);
                            iss >> from >> to >> weight;
                            if (iss.fail() && filename.find("higgs-social_network") == std::string::npos) {
                                log_message("Invalid line " + std::to_string(line_count) + " in " + filename + ": " + line + "\n");
                                continue;
                            }
                            int dest_rank = to / (MAX_NODES / size);
                            if (dest_rank >= size) dest_rank = size - 1;
                            if (dest_rank == 0) {
                                addEdge(filename, from, to, weight);
                            } else {
                                MPI_Send(&from, 1, MPI_LONG, dest_rank, 0, MPI_COMM_WORLD);
                                MPI_Send(&to, 1, MPI_LONG, dest_rank, 0, MPI_COMM_WORLD);
                                MPI_Send(&weight, 1, MPI_DOUBLE, dest_rank, 0, MPI_COMM_WORLD);
                                std::string fname = filename;
                                MPI_Send(fname.c_str(), fname.size() + 1, MPI_CHAR, dest_rank, 0, MPI_COMM_WORLD);
                            }
                        } catch (...) {
                            log_message("Error parsing line " + std::to_string(line_count) + " in " + filename + "\n");
                        }
                    }
                }
                log_message("Loaded " + filename + " (" + std::to_string(line_count) + " lines)\n");
            }
        } else {
            for (const auto& filename : files) {
                int lines_size;
                MPI_Bcast(&lines_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
                for (int i = 0; i < lines_size; ++i) {
                    if (filename == "higgs-interests.txt") {
                        int interest_count;
                        MPI_Recv(&interest_count, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        std::vector<char> interest_buffer(interest_count * 256);
                        MPI_Recv(interest_buffer.data(), interest_count * 256, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        std::stringstream ss(std::string(interest_buffer.begin(), interest_buffer.begin() + strlen(interest_buffer.data())));
                        std::vector<std::string> interests;
                        std::string interest;
                        while (ss >> interest) {
                            if (interest.size() > 255) {
                                log_message("Warning: Truncating long interest string in loadFromFile\n");
                                interest = interest.substr(0, 255);
                            }
                            interests.push_back(interest);
                        }
                        long int node_id = local_start + i % local_size;
                        if (node_id < local_end) {
                            nodes[node_id - local_start].setInterests(interests);
                            if (node_id < 5) {
                                std::stringstream ss_log;
                                ss_log << "Loaded interests for node " << node_id << ": ";
                                for (const auto& i : interests) ss_log << i << " ";
                                ss_log << "\n";
                                log_message(ss_log.str());
                            }
                        }
                    } else {
                        long int from, to;
                        double weight;
                        char fname_buffer[256];
                        MPI_Recv(&from, 1, MPI_LONG, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        MPI_Recv(&to, 1, MPI_LONG, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        MPI_Recv(&weight, 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        MPI_Recv(fname_buffer, 256, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        std::string filename_received(fname_buffer);
                        if (to >= local_start && to < local_end) {
                            addEdge(filename_received, from, to, weight);
                        }
                    }
                }
            }
        }
    }

    void partitionGraph() {
        communities.clear();
        index = 0;
        node_stack = std::stack<long int>();
        std::fill(lowlink.begin(), lowlink.end(), -1);
        std::fill(level.begin(), level.end(), 0);
        std::fill(on_stack.begin(), on_stack.end(), false);
        if (rank == 0) {
            log_message("Starting graph partitioning\n");
            for (long int v = 0; v < MAX_NODES; ++v) {
                if (lowlink[v] == -1) {
                    log_message("Starting DFS-SCC from node " + std::to_string(v) + "\n");
                    DFS_SCC(v, communities);
                }
            }
        }
        // Broadcast communities
        int comm_size = communities.size();
        MPI_Bcast(&comm_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (rank != 0) communities.resize(comm_size);
        for (int i = 0; i < comm_size; ++i) {
            int comp_size = communities[i].size();
            MPI_Bcast(&comp_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
            if (rank != 0) communities[i].resize(comp_size);
            MPI_Bcast(communities[i].data(), comp_size, MPI_LONG, 0, MPI_COMM_WORLD);
        }
        MPI_Bcast(lowlink.data(), MAX_NODES, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(level.data(), MAX_NODES, MPI_INT, 0, MPI_COMM_WORLD);
        // Broadcast on_stack using a temporary char array
        std::vector<char> on_stack_buffer(MAX_NODES);
        if (rank == 0) {
            for (long int i = 0; i < MAX_NODES; ++i) {
                on_stack_buffer[i] = on_stack[i] ? 1 : 0;
            }
        }
        MPI_Bcast(on_stack_buffer.data(), MAX_NODES, MPI_CHAR, 0, MPI_COMM_WORLD);
        if (rank != 0) {
            for (long int i = 0; i < MAX_NODES; ++i) {
                on_stack[i] = on_stack_buffer[i] != 0;
            }
        }

        for (size_t i = 0; i < communities.size(); ++i) {
            for (long int v : communities[i]) {
                if (v >= local_start && v < local_end) {
                    nodes[v - local_start].community_id = i;
                }
            }
            log_message("Assigned community ID " + std::to_string(i) + " to " + std::to_string(communities[i].size()) + " nodes\n");
        }

        std::vector<int> max_level(communities.size(), 0);
        for (size_t i = 0; i < communities.size(); ++i) {
            int local_max_level = 0;
            for (long int v : communities[i]) {
                if (v >= local_start && v < local_end) {
                    for (long int w : nodes[v - local_start].followers) {
                        if (w >= MAX_NODES || w < 0 || nodes[w - local_start].community_id == -1) continue;
                        if (nodes[w - local_start].community_id != i) {
                            local_max_level = std::max(local_max_level, level[w] + 1);
                        }
                    }
                }
            }
            int global_max_level;
            MPI_Reduce(&local_max_level, &global_max_level, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
            MPI_Bcast(&global_max_level, 1, MPI_INT, 0, MPI_COMM_WORLD);
            max_level[i] = global_max_level;
        }
        for (size_t i = 0; i < communities.size(); ++i) {
            for (long int v : communities[i]) {
                if (v >= local_start && v < local_end) {
                    level[v] = max_level[i];
                }
            }
            log_message("Assigned level " + std::to_string(max_level[i]) + " to community " + std::to_string(i) + "\n");
        }

        std::vector<bool> is_cac(communities.size(), false);
        for (size_t i = 0; i < communities.size(); ++i) {
            if (communities[i].size() == 1) {
                is_cac[i] = true;
                log_message("Identified single-node community " + std::to_string(i) + "\n");
            }
        }

        for (size_t i = 0; i < communities.size(); ++i) {
            if (!is_cac[i]) continue;
            long int v = communities[i][0];
            bool merged = false;
            for (long int w : nodes[v - local_start].followers) {
                if (w >= MAX_NODES || w < 0 || nodes[w - local_start].community_id == -1) continue;
                int w_comm_id = nodes[w - local_start].community_id;
                if (level[v] == level[w] && !is_cac[w_comm_id] && communities[w_comm_id].size() < 5) {
                    communities[w_comm_id].push_back(v);
                    if (v >= local_start && v < local_end) {
                        nodes[v - local_start].community_id = w_comm_id;
                    }
                    is_cac[i] = false;
                    communities[i].clear();
                    log_message("Merged single-node community " + std::to_string(i) + " into community " + std::to_string(w_comm_id) + "\n");
                    merged = true;
                    break;
                }
            }
            if (!merged) {
                for (long int u : nodes[v - local_start].followers) {
                    if (u >= MAX_NODES || u < 0 || nodes[u - local_start].community_id == -1) continue;
                    int u_comm_id = nodes[u - local_start].community_id;
                    if (level[v] == level[u] && is_cac[u_comm_id] && communities[u_comm_id].size() < 5) {
                        communities[u_comm_id].push_back(v);
                        if (v >= local_start && v < local_end) {
                            nodes[v - local_start].community_id = u_comm_id;
                        }
                        is_cac[i] = false;
                        communities[i].clear();
                        log_message("Merged single-node community " + std::to_string(i) + " into single-node community " + std::to_string(u_comm_id) + "\n");
                        break;
                    }
                }
            }
        }

        if (rank == 0) {
            communities.erase(
                std::remove_if(communities.begin(), communities.end(),
                    [](const std::vector<long int>& c) { return c.empty(); }),
                communities.end()
            );
            log_message("Partitioned graph into " + std::to_string(communities.size()) + " communities\n");
            std::cout << "Partitioned graph into " + std::to_string(communities.size()) + " communities\n";
        }
    }

    const std::vector<std::vector<long int>>& getCommunities() const {
        return communities;
    }

    void calculateInfluencePower() {
        for (long int i = 0; i < local_size; ++i) {
            nodes[i].influence_power = 1.0 / MAX_NODES;
        }
        log_message("Initialized influence power for all nodes to " + std::to_string(1.0 / MAX_NODES) + "\n");

        partitionGraph();
        auto communities = getCommunities();
        int max_component_level = *std::max_element(level.begin(), level.end()) + 1;
        std::set<std::pair<long int, long int>> printed_edges;

        log_message("Starting influence power calculation for " + std::to_string(max_component_level + 1) + " levels\n");
        for (int current_level = 0; current_level <= max_component_level; ++current_level) {
            std::vector<std::vector<long int>> level_components;
            if (rank == 0) {
                for (const auto& component : communities) {
                    if (component.empty()) continue;
                    if (level[component[0]] == current_level) {
                        level_components.push_back(component);
                    }
                }
            }
            int comp_size = level_components.size();
            MPI_Bcast(&comp_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
            if (rank != 0) level_components.resize(comp_size);
            for (int i = 0; i < comp_size; ++i) {
                int comp_node_size = level_components[i].size();
                MPI_Bcast(&comp_node_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
                if (rank != 0) level_components[i].resize(comp_node_size);
                MPI_Bcast(level_components[i].data(), comp_node_size, MPI_LONG, 0, MPI_COMM_WORLD);
            }
            log_message("Processing level " + std::to_string(current_level) + " with " + std::to_string(level_components.size()) + " components\n");

            for (size_t c = rank; c < level_components.size(); c += size) {
                auto& component = level_components[c];
                std::vector<double> new_ip(MAX_NODES, 0.0);

                for (int iter = 0; iter < MAX_ITERATIONS; ++iter) {
                    bool converged = true;
                    for (long int u_i : component) {
                        if (u_i >= MAX_NODES || u_i < 0) continue;
                        double sum = 0.0;
                        std::vector<long int> followers;
                        if (u_i >= local_start && u_i < local_end) {
                            followers = nodes[u_i - local_start].followers;
                        } else {
                            int dest_rank = u_i / (MAX_NODES / size);
                            if (dest_rank >= size) dest_rank = size - 1;
                            int follower_count;
                            MPI_Recv(&follower_count, 1, MPI_INT, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                            followers.resize(follower_count);
                            MPI_Recv(followers.data(), follower_count, MPI_LONG, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        }
                        for (long int u_j : followers) {
                            if (u_j >= MAX_NODES || u_j < 0) continue;
                            double psi = calculateEdgeWeight(u_j, u_i, printed_edges);
                            int outgoing_followers;
                            double ip_j;
                            if (u_j >= local_start && u_j < local_end) {
                                outgoing_followers = nodes[u_j - local_start].followers_count;
                                ip_j = nodes[u_j - local_start].influence_power;
                            } else {
                                int dest_rank = u_j / (MAX_NODES / size);
                                if (dest_rank >= size) dest_rank = size - 1;
                                MPI_Recv(&outgoing_followers, 1, MPI_INT, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                                MPI_Recv(&ip_j, 1, MPI_DOUBLE, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                            }
                            if (outgoing_followers > 0) {
                                sum += psi * ip_j / outgoing_followers;
                            }
                        }
                        double followers_count_u_i;
                        if (u_i >= local_start && u_i < local_end) {
                            followers_count_u_i = nodes[u_i - local_start].followers_count;
                        } else {
                            int dest_rank = u_i / (MAX_NODES / size);
                            if (dest_rank >= size) dest_rank = size - 1;
                            MPI_Recv(&followers_count_u_i, 1, MPI_DOUBLE, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        }
                        new_ip[u_i] = DAMPING_FACTOR * sum + (1 - DAMPING_FACTOR) * followers_count_u_i / static_cast<double>(MAX_NODES);
                        if (std::abs(new_ip[u_i] - (u_i >= local_start && u_i < local_end ? nodes[u_i - local_start].influence_power : 0.0)) > CONVERGENCE_THRESHOLD) {
                            converged = false;
                        }
                    }

                    for (long int u_i : component) {
                        if (u_i >= local_start && u_i < local_end) {
                            nodes[u_i - local_start].influence_power = new_ip[u_i];
                        }
                    }
                    MPI_Allgather(new_ip.data(), MAX_NODES, MPI_DOUBLE, new_ip.data(), MAX_NODES, MPI_DOUBLE, MPI_COMM_WORLD);

                    int global_converged;
                    MPI_Reduce(&converged, &global_converged, 1, MPI_INT, MPI_LAND, 0, MPI_COMM_WORLD);
                    MPI_Bcast(&global_converged, 1, MPI_INT, 0, MPI_COMM_WORLD);
                    if (global_converged) {
                        log_message("Component " + std::to_string(c) + " at level " + std::to_string(current_level) + " converged after " + std::to_string(iter + 1) + " iterations\n");
                        break;
                    }
                }
            }
        }

        double local_ip_sum = 0.0;
        for (long int i = 0; i < local_size; ++i) {
            local_ip_sum += nodes[i].influence_power;
        }
        double ip_sum;
        MPI_Reduce(&local_ip_sum, &ip_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Bcast(&ip_sum, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        if (ip_sum > 0) {
            for (long int i = 0; i < local_size; ++i) {
                nodes[i].influence_power /= ip_sum;
            }
            log_message("Normalized influence powers, sum = " + std::to_string(ip_sum) + "\n");
        }

        for (long int i = 0; i < MAX_NODES && i < 20; ++i) {
            if (i >= local_start && i < local_end) {
                std::stringstream ss;
                ss << "Node " << i << ": Influence Power = " << nodes[i - local_start].influence_power;
                if (nodes[i - local_start].influence_power == 0) {
                    ss << ", Followers = " << nodes[i - local_start].followers_count;
                }
                ss << "\n";
                log_message(ss.str());
            }
        }
        if (rank == 0) {
            log_message("Calculated influence power for all nodes\n");
            std::cout << "Calculated influence power for all nodes\n";
        }
    }

    std::vector<long int> selectSeedCandidates() {
        std::vector<long int> local_candidates;
        for (long int v = local_start; v < local_end; ++v) {
            if (nodes[v - local_start].followers_count == 0 && nodes[v - local_start].retweets_count == 0 &&
                nodes[v - local_start].replies_count == 0 && nodes[v - local_start].mentions_count == 0) {
                continue;
            }
            int L = 1;
            double I_L = computeInfluenceZone(v, L);
            double I_L_plus_1 = computeInfluenceZone(v, L + 1);
            while (I_L >= I_L_plus_1 && nodes[v - local_start].influence_power >= I_L && L < 10) {
                L++;
                I_L = I_L_plus_1;
                I_L_plus_1 = computeInfluenceZone(v, L + 1);
            }
            if (nodes[v - local_start].influence_power >= I_L || nodes[v - local_start].influence_power > IP_THRESHOLD) {
                local_candidates.push_back(v);
                std::stringstream ss;
                ss << "Node " << v << ": Influence Power = " << nodes[v - local_start].influence_power << ", Influence Zone (L=" << L << ") = " << I_L << "\n";
                log_message(ss.str());
            }
        }
        
        // Gather the size of local_candidates from each process
        int local_size = local_candidates.size();
        std::vector<int> candidate_sizes(size);
        MPI_Allgather(&local_size, 1, MPI_INT, candidate_sizes.data(), 1, MPI_INT, MPI_COMM_WORLD);
        
        // Calculate displacements for MPI_Gatherv
        std::vector<int> displacements(size);
        int total_size = 0;
        for (int i = 0; i < size; ++i) {
            displacements[i] = total_size;
            total_size += candidate_sizes[i];
        }
        
        // Allocate receive buffer on rank 0
        std::vector<long int> candidates;
        if (rank == 0) {
            candidates.resize(total_size);
        }
        
        // Use MPI_Gatherv to gather variable-sized candidate lists
        MPI_Gatherv(local_candidates.data(), local_size, MPI_LONG,
                    candidates.data(), candidate_sizes.data(), displacements.data(), MPI_LONG,
                    0, MPI_COMM_WORLD);
        
        if (rank == 0) {
            log_message("Selected " + std::to_string(total_size) + " seed candidates\n");
            std::cout << "Selected " + std::to_string(total_size) + " seed candidates\n";
        }
        
        // Broadcast candidates to all processes
        MPI_Bcast(&total_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (rank != 0) {
            candidates.resize(total_size);
        }
        MPI_Bcast(candidates.data(), total_size, MPI_LONG, 0, MPI_COMM_WORLD);
        
        return candidates;
    }

    std::vector<long int> selectSeeds(int k) {
        std::vector<long int> seeds;
        std::vector<long int> candidates = selectSeedCandidates();
        std::sort(candidates.begin(), candidates.end(),
            [this](long int a, long int b) {
                double ip_a = a >= local_start && a < local_end ? nodes[a - local_start].influence_power : 0.0;
                double ip_b = b >= local_start && b < local_end ? nodes[b - local_start].influence_power : 0.0;
                if (a < local_start || a >= local_end) {
                    int dest_rank = a / (MAX_NODES / size);
                    if (dest_rank >= size) dest_rank = size - 1;
                    MPI_Recv(&ip_a, 1, MPI_DOUBLE, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }
                if (b < local_start || b >= local_end) {
                    int dest_rank = b / (MAX_NODES / size);
                    if (dest_rank >= size) dest_rank = size - 1;
                    MPI_Recv(&ip_b, 1, MPI_DOUBLE, dest_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }
                return ip_a > ip_b;
            });
        std::set<long int> candidate_set(candidates.begin(), candidates.end());
        std::vector<InfluenceBFSTree> trees;
        log_message("Selecting " + std::to_string(k) + " seeds from " + std::to_string(candidates.size()) + " candidates\n");

        for (size_t i = rank; i < candidates.size(); i += size) {
            long int v = candidates[i];
            InfluenceBFSTree tree = buildInfluenceBFSTree(v, candidate_set);
            trees.push_back(tree);
        }
        int local_tree_size = trees.size();
        std::vector<int> tree_sizes(size);
        MPI_Allgather(&local_tree_size, 1, MPI_INT, tree_sizes.data(), 1, MPI_INT, MPI_COMM_WORLD);
        std::vector<InfluenceBFSTree> all_trees;
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < tree_sizes[i]; ++j) {
                if (i == rank && j < local_tree_size) {
                    all_trees.push_back(trees[j]);
                } else {
                    InfluenceBFSTree tree;
                    MPI_Recv(&tree.root, 1, MPI_LONG, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    int node_size;
                    MPI_Recv(&node_size, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    tree.nodes.resize(node_size);
                    tree.distances.resize(node_size);
                    MPI_Recv(tree.nodes.data(), node_size, MPI_LONG, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Recv(tree.distances.data(), node_size, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Recv(&tree.rank, 1, MPI_DOUBLE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    all_trees.push_back(tree);
                }
            }
        }

        while (!candidate_set.empty() && seeds.size() < k) {
            auto max_tree_it = std::max_element(all_trees.begin(), all_trees.end(),
                [](const InfluenceBFSTree& t1, const InfluenceBFSTree& t2) {
                    return t1.nodes.size() < t2.nodes.size();
                });
            if (max_tree_it == all_trees.end()) break;
            long int u_max = max_tree_it->root;

            std::set<long int> black_path;
            for (size_t i = 0; i < max_tree_it->nodes.size(); ++i) {
                long int v = max_tree_it->nodes[i];
                if (candidate_set.count(v) && max_tree_it->distances[i] <= 1) {
                    black_path.insert(v);
                }
            }

            double min_rank = std::numeric_limits<double>::max();
            long int v_min = u_max;
            for (long int v : black_path) {
                auto tree_it = std::find_if(all_trees.begin(), all_trees.end(),
                    [v](const InfluenceBFSTree& t) { return t.root == v; });
                if (tree_it != all_trees.end() && tree_it->rank < min_rank) {
                    min_rank = tree_it->rank;
                    v_min = tree_it->root;
                }
            }

            std::stringstream ss;
            ss << "Selected seed: Node " << v_min << ", Rank = " << min_rank 
               << ", Black path size = " << black_path.size() << "\n";
            log_message(ss.str());
            seeds.push_back(v_min);
            candidate_set.erase(v_min);
            for (long int v : black_path) {
                candidate_set.erase(v);
                all_trees.erase(std::remove_if(all_trees.begin(), all_trees.end(),
                    [v](const InfluenceBFSTree& t) { return t.root == v; }), all_trees.end());
            }
        }

        verifyInfluentialUsers(seeds, true, k);

        if (rank == 0) {
            log_message("Selected " + std::to_string(seeds.size()) + " seeds\n");
            std::cout << "Selected " + std::to_string(seeds.size()) + " seeds\n";
        }
        return seeds;
    }

    void displayNodeParameters(long int node_id) {
        if (node_id < 0 || node_id >= MAX_NODES) {
            log_message("Error: Node ID out of range: " + std::to_string(node_id) + "\n");
            return;
        }
        if (node_id >= local_start && node_id < local_end) {
            Node& node = nodes[node_id - local_start];
            std::stringstream ss;
            ss << "Node " << node.node_id << " Parameters:\n";
            ss << "  Followers: " << node.followers_count << " (Total Weight: " << node.total_followers_weight << ")\n";
            ss << "  Retweets: " << node.retweets_count << " (Total Weight: " << node.total_retweet_weight << ")\n";
            ss << "  Replies: " << node.replies_count << " (Total Weight: " << node.total_reply_weight << ")\n";
            ss << "  Mentions: " << node.mentions_count << " (Total Weight: " << node.total_mention_weight << ")\n";
            ss << "  Interests: ";
            for (const auto& interest : node.interests) {
                ss << interest << " ";
            }
            ss << "\n";
            ss << "  Influence Power: " << node.influence_power << "\n";
            ss << "  Community ID: " << node.community_id << "\n";
            log_message(ss.str());
        }
    }

    void displayFirstFive() {
        log_message("Displaying parameters for first five nodes\n");
        for (long int i = 0; i < MAX_NODES && i < 5; ++i) {
            displayNodeParameters(i);
        }
    }
};
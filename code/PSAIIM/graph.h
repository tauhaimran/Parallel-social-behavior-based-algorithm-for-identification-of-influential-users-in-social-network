#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <random>
#include <stack>
#include <set>
#include <omp.h>
#include <cmath>
#include <queue>
#include <limits>
#include <unordered_map>
#include <functional>
#include <iomanip>

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

    std::vector<Node> nodes;
    long int MAX_NODES = 500000;
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

    struct InfluenceBFSTree {
        long int root;
        std::vector<long int> nodes;
        std::vector<int> distances;
        double rank;
    };

    double calculateJaccard(const std::vector<std::string>& interests1, const std::vector<std::string>& interests2) {
        std::set<std::string> set1(interests1.begin(), interests1.end());
        std::set<std::string> set2(interests2.begin(), interests2.end());
        std::vector<std::string> intersection, union_set;
        std::set_intersection(set1.begin(), set1.end(), set2.begin(), set2.end(), std::back_inserter(intersection));
        std::set_union(set1.begin(), set1.end(), set2.begin(), set2.end(), std::back_inserter(union_set));
        double ci = union_set.empty() ? 0.0 : static_cast<double>(intersection.size()) / union_set.size();
        log_file << "Calculating Jaccard similarity: Intersection size = " << intersection.size() 
                 << ", Union size = " << union_set.size() << ", Similarity = " << ci << "\n";
        return ci;
    }

    double calculateEdgeWeight(long int u_x, long int u_y, std::set<std::pair<long int, long int>>& printed_edges) {
        if (u_x >= MAX_NODES || u_y >= MAX_NODES || u_x < 0 || u_y < 0) {
            log_file << "Invalid edge (" << u_x << "," << u_y << "): Out of bounds\n";
            return 0.0;
        }
        double ci = nodes[u_x].interests.empty() || nodes[u_y].interests.empty() ? 1.0 : calculateJaccard(nodes[u_x].interests, nodes[u_y].interests);
        double sum = 0.0;
        sum += ALPHA_RETWEET * ci * nodes[u_y].total_retweet_weight;
        sum += ALPHA_COMMENT * ci * nodes[u_y].total_reply_weight;
        sum += ALPHA_MENTION * ci * nodes[u_y].total_mention_weight;
        double total_interactions = nodes[u_y].total_retweet_weight + nodes[u_y].total_reply_weight + nodes[u_y].total_mention_weight;
        double psi = total_interactions > 0 ? sum / total_interactions : 0.0;
        if (u_x < 5 && u_y < 5 && printed_edges.insert({u_x, u_y}).second) {
            std::cout << "Edge (" << u_x << "," << u_y << "): Jaccard Similarity = " << ci << ", Edge Weight = " << psi << "\n";
            log_file << "Edge (" << u_x << "," << u_y << "): Jaccard Similarity = " << ci << ", Edge Weight = " << psi << "\n";
        }
        return psi;
    }

    void DFS_SCC(long int v, std::vector<std::vector<long int>>& components) {
        if (v >= MAX_NODES || v < 0 || lowlink[v] != -1) {
            log_file << "Skipping DFS for node " << v << ": Out of bounds or already visited\n";
            return;
        }

        lowlink[v] = level[v] = index++;
        node_stack.push(v);
        on_stack[v] = true;
        log_file << "DFS: Processing node " << v << ", lowlink = " << lowlink[v] << ", level = " << level[v] << "\n";

        for (long int w : nodes[v].followers) {
            if (w >= MAX_NODES || w < 0) continue;
            if (lowlink[w] == -1) {
                log_file << "DFS: Exploring edge from " << v << " to unvisited node " << w << "\n";
                DFS_SCC(w, components);
                lowlink[v] = std::min(lowlink[v], lowlink[w]);
                log_file << "DFS: Updated lowlink for node " << v << " to " << lowlink[v] << "\n";
            } else if (on_stack[w]) {
                lowlink[v] = std::min(lowlink[v], lowlink[w]);
                log_file << "DFS: Found back edge to node " << w << ", updated lowlink for " << v << " to " << lowlink[v] << "\n";
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
                nodes[w].community_id = components.size();
            } while (w != v);
            components.push_back(component);
            log_file << "Found SCC: Component " << components.size() - 1 << " with " << component.size() << " nodes\n";
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
        log_file << "Building BFS tree for root node " << root << "\n";

        while (!q.empty()) {
            long int u = q.front();
            q.pop();
            for (const auto& category : {nodes[u].followers, nodes[u].retweets, nodes[u].replies, nodes[u].mentions}) {
                for (long int w : category) {
                    if (w >= MAX_NODES || w < 0 || visited[w]) continue;
                    visited[w] = true;
                    distance[w] = distance[u] + 1;
                    tree.nodes.push_back(w);
                    tree.distances.push_back(distance[w]);
                    if (candidates.count(w) && distance[w] <= 2) {
                        q.push(w);
                        log_file << "BFS: Added node " << w << " at distance " << distance[w] << "\n";
                    }
                }
            }
        }

        double sum_rank = 0.0;
        for (int d : tree.distances) {
            sum_rank += d;
        }
        tree.rank = tree.nodes.size() > 0 ? sum_rank / tree.nodes.size() : 0.0;
        std::cout << "BFS tree for node " << root << ": " << tree.nodes.size() << " nodes, average distance: " << tree.rank << "\n";
        log_file << "BFS tree for node " << root << ": " << tree.nodes.size() << " nodes, average distance: " << tree.rank << "\n";
        return tree;
    }

    double computeInfluenceZone(long int v, int L) {
        if (v >= MAX_NODES || v < 0) {
            log_file << "Invalid node " << v << " for influence zone computation\n";
            return 0.0;
        }
        std::queue<std::pair<long int, int>> q;
        std::vector<bool> visited(MAX_NODES, false);
        q.push(std::make_pair(v, 0));
        visited[v] = true;
        double sum_ip = 0.0;
        int count = 0;

        log_file << "Computing influence zone for node " << v << " with L = " << L << "\n";
        while (!q.empty()) {
            auto current = q.front();
            q.pop();
            long int u = current.first;
            int dist = current.second;
            if (dist <= L) {
                sum_ip += nodes[u].influence_power;
                count++;
                for (const auto& category : {nodes[u].followers, nodes[u].retweets, nodes[u].replies, nodes[u].mentions}) {
                    for (long int w : category) {
                        if (w >= MAX_NODES || w < 0 || visited[w]) continue;
                        visited[w] = true;
                        q.push(std::make_pair(w, dist + 1));
                        log_file << "Influence zone: Added node " << w << " at distance " << dist + 1 << "\n";
                    }
                }
            }
        }
        double I_L = count > 0 ? sum_ip / count : 0.0;
        if (v < 5) {
            std::cout << "Node " << v << ": Influence Zone (L=" << L << ") = " << I_L << ", Nodes reached = " << count << "\n";
            log_file << "Node " << v << ": Influence Zone (L=" << L << ") = " << I_L << ", Nodes reached = " << count << "\n";
        }
        return I_L;
    }

    bool verifyInfluentialUsers(const std::vector<long int>& influential_nodes, bool is_seed_selection, int expected_size) {
        log_file << "Verifying influential users (size = " << influential_nodes.size() << ", is_seed_selection = " 
                 << is_seed_selection << ", expected_size = " << expected_size << ")\n";

        bool is_valid = true;

        // 1. Check size constraint
        if (is_seed_selection && influential_nodes.size() > expected_size) {
            std::cerr << "Error: Seed selection size (" << influential_nodes.size() << ") exceeds expected size (" 
                      << expected_size << ")\n";
            log_file << "Error: Seed selection size (" << influential_nodes.size() << ") exceeds expected size (" 
                     << expected_size << ")\n";
            is_valid = false;
        } else if (!is_seed_selection && influential_nodes.size() != expected_size) {
            std::cerr << "Error: Top influential nodes size (" << influential_nodes.size() << ") does not match expected size (" 
                      << expected_size << ")\n";
            log_file << "Error: Top influential nodes size (" << influential_nodes.size() << ") does not match expected size (" 
                     << expected_size << ")\n";
            is_valid = false;
        }

        // 2. Check for duplicates
        std::set<long int> unique_nodes(influential_nodes.begin(), influential_nodes.end());
        if (unique_nodes.size() != influential_nodes.size()) {
            std::cerr << "Error: Influential nodes contain duplicates\n";
            log_file << "Error: Influential nodes contain duplicates\n";
            is_valid = false;
        }

        // 3. Verify each node
        for (long int node_id : influential_nodes) {
            // Check node existence and bounds
            if (node_id < 0 || node_id >= MAX_NODES) {
                std::cerr << "Error: Node " << node_id << " is out of bounds\n";
                log_file << "Error: Node " << node_id << " is out of bounds\n";
                is_valid = false;
                continue;
            }

            Node& node = nodes[node_id];

            // Check influence power against threshold or influence zone
            int L = 1;
            double I_L = computeInfluenceZone(node_id, L);
            if (node.influence_power < IP_THRESHOLD && node.influence_power < I_L) {
                std::cerr << "Warning: Node " << node_id << " has low influence power (" << node.influence_power 
                          << ") below threshold (" << IP_THRESHOLD << ") and influence zone (" << I_L << ")\n";
                log_file << "Warning: Node " << node_id << " has low influence power (" << node.influence_power 
                         << ") below threshold (" << IP_THRESHOLD << ") and influence zone (" << I_L << ")\n";
                // Not necessarily invalid, but log as a warning
            }

            // Check community assignment
            if (node.community_id < 0 || static_cast<size_t>(node.community_id) >= communities.size()) {
                std::cerr << "Error: Node " << node_id << " has invalid community ID (" << node.community_id << ")\n";
                log_file << "Error: Node " << node_id << " has invalid community ID (" << node.community_id << ")\n";
                is_valid = false;
            }

            // Check connectivity
            if (node.followers_count == 0 && node.retweets_count == 0 && 
                node.replies_count == 0 && node.mentions_count == 0) {
                std::cerr << "Error: Node " << node_id << " has no interactions\n";
                log_file << "Error: Node " << node_id << " has no interactions\n";
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
                    std::cerr << "Error: Seed node " << seed << " not found in its own BFS tree\n";
                    log_file << "Error: Seed node " << seed << " not found in its own BFS tree\n";
                    is_valid = false;
                }
            }
        }

        // 5. If top influential nodes, verify sorting by influence power
        if (!is_seed_selection && is_valid) {
            for (size_t i = 1; i < influential_nodes.size(); ++i) {
                if (nodes[influential_nodes[i]].influence_power > nodes[influential_nodes[i-1]].influence_power) {
                    std::cerr << "Error: Influential nodes not sorted by influence power at index " << i << "\n";
                    log_file << "Error: Influential nodes not sorted by influence power at index " << i << "\n";
                    is_valid = false;
                }
            }
        }

        if (is_valid) {
            std::cout << "Verification passed: Influential users are valid\n";
            log_file << "Verification passed: Influential users are valid\n";
        } else {
            std::cerr << "Verification failed: Influential users are invalid\n";
            log_file << "Verification failed: Influential users are invalid\n";
        }

        return is_valid;
    }

public:
    Graph() : index(0) {
        log_file.open("graph_analysis.log", std::ios::app);
        if (!log_file.is_open()) {
            std::cerr << "Failed to open log file\n";
        }
        log_file << "\n=== New Graph Analysis Session ===\n";
    }

    ~Graph() {
        if (log_file.is_open()) {
            log_file.close();
        }
    }

    void saveLogsToFile() {
        log_file << "\n=== Final Analysis Summary ===\n";
        log_file << "Total Nodes: " << MAX_NODES << "\n";
        log_file << "Number of Communities: " << communities.size() << "\n";
        for (size_t i = 0; i < communities.size(); ++i) {
            log_file << "Community " << i << ": " << communities[i].size() << " nodes\n";
        }
        log_file << "Top 10 Influential Nodes:\n";
        auto top_nodes = getTopInfluentialNodes();
        for (const auto& node : top_nodes) {
            log_file << "Node " << node.first << ": Influence Power = " << std::fixed << std::setprecision(6) 
                     << node.second << ", Community ID = " << nodes[node.first].community_id << "\n";
        }
        log_file << "================================\n";
        std::cout << "Analysis logs saved to graph_analysis.log\n";
    }

    std::vector<std::pair<long int, double>> getTopInfluentialNodes() {
        std::vector<std::pair<long int, double>> node_influence;
        for (long int i = 0; i < MAX_NODES; ++i) {
            node_influence.emplace_back(i, nodes[i].influence_power);
        }
        std::sort(node_influence.begin(), node_influence.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        std::vector<std::pair<long int, double>> top_nodes(node_influence.begin(), 
            node_influence.begin() + std::min(10, static_cast<int>(node_influence.size())));
        
        // Verify top influential nodes
        std::vector<long int> top_node_ids;
        for (const auto& node : top_nodes) {
            top_node_ids.push_back(node.first);
        }
        verifyInfluentialUsers(top_node_ids, false, 10);
        
        log_file << "Retrieved top 10 influential nodes\n";
        std::cout << "Top 10 influential nodes retrieved\n";
        return top_nodes;
    }

    long int determineMaxNodes(const std::vector<std::string>& files) {
        long int max_id = 0;
        for (const auto& filename : files) {
            if (filename == "higgs-interests.txt") continue;
            std::ifstream file("./higg/" + filename);
            if (!file.is_open()) {
                std::cerr << "Unable to open file: " << filename << "\n";
                log_file << "Error: Unable to open file " << filename << "\n";
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
            log_file << "Processed file " << filename << ", max node ID found: " << max_id << "\n";
        }
        long int result = max_id + 1;
        std::cout << "Determined maximum nodes: " << result << "\n";
        log_file << "Determined maximum nodes: " << result << "\n";
        return result;
    }

    void initialize(long int max_nodes) {
        MAX_NODES = max_nodes;
        nodes.resize(MAX_NODES);
        lowlink.resize(MAX_NODES, -1);
        level.resize(MAX_NODES, 0);
        on_stack.resize(MAX_NODES, false);
        for (long int i = 0; i < MAX_NODES; ++i) {
            nodes[i] = Node(i);
        }
        std::cout << "Initialized graph with " << MAX_NODES << " nodes\n";
        log_file << "Initialized graph with " << MAX_NODES << " nodes\n";
    }

    void addEdge(const std::string& filename, long int from, long int to, double weight = 1.0) {
        if (from >= MAX_NODES || to >= MAX_NODES || from < 0 || to < 0) {
            log_file << "Invalid edge (" << from << "," << to << ") in " << filename << ": Out of bounds\n";
            return;
        }
        Node& node = nodes[to];
        if (filename.find("higgs-social_network") != std::string::npos) {
            node.followers.push_back(from);
            node.followers_weight.push_back(weight);
            node.followers_count++;
            node.total_followers_weight += weight;
            log_file << "Added follower edge (" << from << "," << to << ") in social network, weight = " << weight << "\n";
        } else if (filename.find("higgs-retweet_network") != std::string::npos) {
            node.retweets.push_back(from);
            node.retweet_weight.push_back(weight);
            node.retweets_count++;
            node.total_retweet_weight += weight;
            log_file << "Added retweet edge (" << from << "," << to << ") in retweet network, weight = " << weight << "\n";
        } else if (filename.find("higgs-reply_network") != std::string::npos) {
            node.replies.push_back(from);
            node.reply_weight.push_back(weight);
            node.replies_count++;
            node.total_reply_weight += weight;
            log_file << "Added reply edge (" << from << "," << to << ") in reply network, weight = " << weight << "\n";
        } else if (filename.find("higgs-mention_network") != std::string::npos) {
            node.mentions.push_back(from);
            node.mention_weight.push_back(weight);
            node.mentions_count++;
            node.total_mention_weight += weight;
            log_file << "Added mention edge (" << from << "," << to << ") in mention network, weight = " << weight << "\n";
        }
    }

    void loadFromFile(const std::vector<std::string>& files) {
        for (const auto& filename : files) {
            std::ifstream file("./higg/" + filename);
            if (!file.is_open()) {
                std::cerr << "Unable to open file: " << filename << "\n";
                log_file << "Error: Unable to open file " << filename << "\n";
                continue;
            }
            std::string line;
            int line_count = 0;
            while (std::getline(file, line)) {
                line_count++;
                if (line.empty() || line[0] == '#') continue;
                if (filename == "higgs-interests.txt") {
                    std::istringstream iss(line);
                    long int node_id;
                    std::vector<std::string> interests;
                    std::string interest;
                    if (!(iss >> node_id)) {
                        std::cerr << "Invalid line " << line_count << " in " << filename << ": " << line << "\n";
                        log_file << "Invalid line " << line_count << " in " << filename << ": " << line << "\n";
                        continue;
                    }
                    while (iss >> interest) {
                        interests.push_back(interest);
                    }
                    if (node_id >= 0 && node_id < MAX_NODES) {
                        nodes[node_id].setInterests(interests);
                        if (node_id < 5) {
                            std::cout << "Loaded interests for node " << node_id << ": ";
                            for (const auto& i : interests) std::cout << i << " ";
                            std::cout << "\n";
                            log_file << "Loaded interests for node " << node_id << ": ";
                            for (const auto& i : interests) log_file << i << " ";
                            log_file << "\n";
                        }
                    }
                } else {
                    long int from, to;
                    double weight = 1.0;
                    try {
                        std::istringstream iss(line);
                        iss >> from >> to >> weight;
                        if (iss.fail() && filename.find("higgs-social_network") == std::string::npos) {
                            std::cerr << "Invalid line " << line_count << " in " << filename << ": " << line << "\n";
                            log_file << "Invalid line " << line_count << " in " << filename << ": " << line << "\n";
                            continue;
                        }
                        addEdge(filename, from, to, weight);
                    } catch (...) {
                        std::cerr << "Error parsing line " << line_count << " in " << filename << "\n";
                        log_file << "Error parsing line " << line_count << " in " << filename << "\n";
                    }
                }
            }
            file.close();
            std::cout << "Loaded " << filename << " (" << line_count << " lines)\n";
            log_file << "Loaded " << filename << " (" << line_count << " lines)\n";
        }
    }

    void partitionGraph() {
        communities.clear();
        index = 0;
        node_stack = std::stack<long int>();
        std::fill(lowlink.begin(), lowlink.end(), -1);
        std::fill(level.begin(), level.end(), 0);
        std::fill(on_stack.begin(), on_stack.end(), false);
        log_file << "Starting graph partitioning\n";

        // Step 1: Detect SCCs using follower edges
        for (long int v = 0; v < MAX_NODES; ++v) {
            if (lowlink[v] == -1) {
                log_file << "Starting DFS-SCC from node " << v << "\n";
                DFS_SCC(v, communities);
            }
        }

        // Step 2: Assign community IDs
        for (size_t i = 0; i < communities.size(); ++i) {
            for (long int v : communities[i]) {
                nodes[v].community_id = i;
            }
            log_file << "Assigned community ID " << i << " to " << communities[i].size() << " nodes\n";
        }

        // Step 3: Assign levels to SCCs
        std::vector<int> max_level(communities.size(), 0);
        for (size_t i = 0; i < communities.size(); ++i) {
            for (long int v : communities[i]) {
                for (long int w : nodes[v].followers) {
                    if (w >= MAX_NODES || w < 0 || nodes[w].community_id == -1) continue;
                    if (nodes[w].community_id != i) {
                        max_level[i] = std::max(max_level[i], level[w] + 1);
                    }
                }
            }
        }
        for (size_t i = 0; i < communities.size(); ++i) {
            for (long int v : communities[i]) {
                level[v] = max_level[i];
            }
            log_file << "Assigned level " << max_level[i] << " to community " << i << "\n";
        }

        // Step 4: Merge single-node components (CACs) with stricter size limit
        std::vector<bool> is_cac(communities.size(), false);
        for (size_t i = 0; i < communities.size(); ++i) {
            if (communities[i].size() == 1) {
                is_cac[i] = true;
                log_file << "Identified single-node community " << i << "\n";
            }
        }

        for (size_t i = 0; i < communities.size(); ++i) {
            if (!is_cac[i]) continue;
            long int v = communities[i][0];
            bool merged = false;
            for (long int w : nodes[v].followers) {
                if (w >= MAX_NODES || w < 0 || nodes[w].community_id == -1) continue;
                int w_comm_id = nodes[w].community_id;
                if (level[v] == level[w] && !is_cac[w_comm_id] && communities[w_comm_id].size() < 5) {
                    communities[w_comm_id].push_back(v);
                    nodes[v].community_id = w_comm_id;
                    is_cac[i] = false;
                    communities[i].clear();
                    merged = true;
                    log_file << "Merged single-node community " << i << " into community " << w_comm_id << "\n";
                    break;
                }
            }
            if (!merged) {
                for (long int u : nodes[v].followers) {
                    if (u >= MAX_NODES || u < 0 || nodes[u].community_id == -1) continue;
                    int u_comm_id = nodes[u].community_id;
                    if (level[v] == level[u] && is_cac[u_comm_id] && communities[u_comm_id].size() < 5) {
                        communities[u_comm_id].push_back(v);
                        nodes[v].community_id = u_comm_id;
                        is_cac[i] = false;
                        communities[i].clear();
                        log_file << "Merged single-node community " << i << " into single-node community " << u_comm_id << "\n";
                        break;
                    }
                }
            }
        }

        // Remove empty communities
        communities.erase(
            std::remove_if(communities.begin(), communities.end(),
                [](const std::vector<long int>& c) { return c.empty(); }),
            communities.end()
        );

        std::cout << "Partitioned graph into " << communities.size() << " communities\n";
        log_file << "Partitioned graph into " << communities.size() << " communities\n";
    }

    const std::vector<std::vector<long int>>& getCommunities() const {
        return communities;
    }

    void calculateInfluencePower() {
        for (auto& node : nodes) {
            node.influence_power = 1.0 / MAX_NODES;
        }
        log_file << "Initialized influence power for all nodes to " << 1.0 / MAX_NODES << "\n";

        partitionGraph();
        auto communities = getCommunities();
        int max_component_level = *std::max_element(level.begin(), level.end()) + 1;
        std::set<std::pair<long int, long int>> printed_edges;

        log_file << "Starting influence power calculation for " << max_component_level + 1 << " levels\n";
        for (int current_level = 0; current_level <= max_component_level; ++current_level) {
            std::vector<std::vector<long int>> level_components;
            for (const auto& component : communities) {
                if (component.empty()) continue;
                if (level[component[0]] == current_level) {
                    level_components.push_back(component);
                }
            }
            log_file << "Processing level " << current_level << " with " << level_components.size() << " components\n";

            #pragma omp parallel for
            for (size_t c = 0; c < level_components.size(); ++c) {
                auto& component = level_components[c];
                std::vector<double> new_ip(MAX_NODES, 0.0);

                for (int iter = 0; iter < MAX_ITERATIONS; ++iter) {
                    bool converged = true;
                    for (long int u_i : component) {
                        if (u_i >= MAX_NODES || u_i < 0) continue;
                        double sum = 0.0;
                        for (size_t j = 0; j < nodes[u_i].followers.size(); ++j) {
                            long int u_j = nodes[u_i].followers[j];
                            if (u_j >= MAX_NODES || u_j < 0) continue;
                            double psi = calculateEdgeWeight(u_j, u_i, printed_edges);
                            int outgoing_followers = nodes[u_j].followers_count;
                            if (outgoing_followers > 0) {
                                sum += psi * nodes[u_j].influence_power / outgoing_followers;
                            }
                        }
                        new_ip[u_i] = DAMPING_FACTOR * sum + (1 - DAMPING_FACTOR) * nodes[u_i].followers_count / static_cast<double>(MAX_NODES);
                        if (std::abs(new_ip[u_i] - nodes[u_i].influence_power) > CONVERGENCE_THRESHOLD) {
                            converged = false;
                        }
                    }

                    for (long int u_i : component) {
                        if (u_i >= MAX_NODES || u_i < 0) continue;
                        nodes[u_i].influence_power = new_ip[u_i];
                    }

                    if (converged) {
                        log_file << "Component " << c << " at level " << current_level << " converged after " << iter + 1 << " iterations\n";
                        break;
                    }
                }
            }
        }

        // Normalize IPs to sum to 1
        double ip_sum = 0.0;
        for (const auto& node : nodes) {
            ip_sum += node.influence_power;
        }
        if (ip_sum > 0) {
            for (auto& node : nodes) {
                node.influence_power /= ip_sum;
            }
            log_file << "Normalized influence powers, sum = " << ip_sum << "\n";
        }

        // Debug zero IPs
        for (long int i = 0; i < MAX_NODES && i < 20; ++i) {
            std::cout << "Node " << i << ": Influence Power = " << nodes[i].influence_power;
            log_file << "Node " << i << ": Influence Power = " << nodes[i].influence_power;
            if (nodes[i].influence_power == 0) {
                std::cout << ", Followers = " << nodes[i].followers_count;
                log_file << ", Followers = " << nodes[i].followers_count;
            }
            std::cout << "\n";
            log_file << "\n";
        }
        std::cout << "Calculated influence power for all nodes\n";
        log_file << "Calculated influence power for all nodes\n";
    }

    std::vector<long int> selectSeedCandidates() {
        std::vector<long int> candidates;
        log_file << "Selecting seed candidates\n";
        for (long int v = 0; v < MAX_NODES; ++v) {
            if (nodes[v].followers_count == 0 && nodes[v].retweets_count == 0 &&
                nodes[v].replies_count == 0 && nodes[v].mentions_count == 0) {
                continue;
            }
            int L = 1;
            double I_L = computeInfluenceZone(v, L);
            double I_L_plus_1 = computeInfluenceZone(v, L + 1);
            while (I_L >= I_L_plus_1 && nodes[v].influence_power >= I_L && L < 10) {
                L++;
                I_L = I_L_plus_1;
                I_L_plus_1 = computeInfluenceZone(v, L + 1);
            }
            if (nodes[v].influence_power >= I_L || nodes[v].influence_power > IP_THRESHOLD) {
                candidates.push_back(v);
                std::cout << "Node " << v << ": Influence Power = " << nodes[v].influence_power << ", Influence Zone (L=" << L << ") = " << I_L << "\n";
                log_file << "Node " << v << ": Influence Power = " << nodes[v].influence_power << ", Influence Zone (L=" << L << ") = " << I_L << "\n";
            }
        }
        std::cout << "Selected " << candidates.size() << " seed candidates\n";
        log_file << "Selected " << candidates.size() << " seed candidates\n";
        return candidates;
    }

    std::vector<long int> selectSeeds(int k) {
        std::vector<long int> seeds;
        std::vector<long int> candidates = selectSeedCandidates();
        std::sort(candidates.begin(), candidates.end(),
            [this](long int a, long int b) { return nodes[a].influence_power > nodes[b].influence_power; });
        std::set<long int> candidate_set(candidates.begin(), candidates.end());
        std::vector<InfluenceBFSTree> trees;
        log_file << "Selecting " << k << " seeds from " << candidates.size() << " candidates\n";

        #pragma omp parallel for schedule(dynamic)
        for (size_t i = 0; i < candidates.size(); ++i) {
            long int v = candidates[i];
            InfluenceBFSTree tree = buildInfluenceBFSTree(v, candidate_set);
            #pragma omp critical
            {
                trees.push_back(tree);
            }
        }

        while (!candidate_set.empty() && seeds.size() < k) {
            auto max_tree_it = std::max_element(trees.begin(), trees.end(),
                [](const InfluenceBFSTree& t1, const InfluenceBFSTree& t2) {
                    return t1.nodes.size() < t2.nodes.size();
                });
            if (max_tree_it == trees.end()) break;
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
                auto tree_it = std::find_if(trees.begin(), trees.end(),
                    [v](const InfluenceBFSTree& t) { return t.root == v; });
                if (tree_it != trees.end() && tree_it->rank < min_rank) {
                    min_rank = tree_it->rank;
                    v_min = tree_it->root;
                }
            }

            std::cout << "Selected seed: Node " << v_min << ", Rank = " << min_rank 
                      << ", Black path size = " << black_path.size() << "\n";
            log_file << "Selected seed: Node " << v_min << ", Rank = " << min_rank 
                     << ", Black path size = " << black_path.size() << "\n";
            seeds.push_back(v_min);
            candidate_set.erase(v_min);
            for (long int v : black_path) {
                candidate_set.erase(v);
                trees.erase(std::remove_if(trees.begin(), trees.end(),
                    [v](const InfluenceBFSTree& t) { return t.root == v; }), trees.end());
            }
        }

        // Verify seeds
        verifyInfluentialUsers(seeds, true, k);

        std::cout << "Selected " << seeds.size() << " seeds\n";
        log_file << "Selected " << seeds.size() << " seeds\n";
        return seeds;
    }

    void displayNodeParameters(long int node_id) {
        if (node_id < 0 || node_id >= MAX_NODES) {
            std::cerr << "Node ID out of range: " << node_id << "\n";
            log_file << "Error: Node ID out of range: " << node_id << "\n";
            return;
        }
        Node& node = nodes[node_id];
        std::cout << "Node " << node.node_id << " Parameters:\n";
        std::cout << "  Followers: " << node.followers_count << " (Total Weight: " << node.total_followers_weight << ")\n";
        std::cout << "  Retweets: " << node.retweets_count << " (Total Weight: " << node.total_retweet_weight << ")\n";
        std::cout << "  Replies: " << node.replies_count << " (Total Weight: " << node.total_reply_weight << ")\n";
        std::cout << "  Mentions: " << node.mentions_count << " (Total Weight: " << node.total_mention_weight << ")\n";
        std::cout << "  Interests: ";
        for (const auto& interest : node.interests) {
            std::cout << interest << " ";
        }
        std::cout << "\n";
        std::cout << "  Influence Power: " << node.influence_power << "\n";
        std::cout << "  Community ID: " << node.community_id << "\n";
        log_file << "Node " << node.node_id << " Parameters:\n";
        log_file << "  Followers: " << node.followers_count << " (Total Weight: " << node.total_followers_weight << ")\n";
        log_file << "  Retweets: " << node.retweets_count << " (Total Weight: " << node.total_retweet_weight << ")\n";
        log_file << "  Replies: " << node.replies_count << " (Total Weight: " << node.total_reply_weight << ")\n";
        log_file << "  Mentions: " << node.mentions_count << " (Total Weight: " << node.total_mention_weight << ")\n";
        log_file << "  Interests: ";
        for (const auto& interest : node.interests) {
            log_file << interest << " ";
        }
        log_file << "\n";
        log_file << "  Influence Power: " << node.influence_power << "\n";
        log_file << "  Community ID: " << node.community_id << "\n";
    }

    void displayFirstFive() {
        log_file << "Displaying parameters for first five nodes\n";
        for (long int i = 0; i < MAX_NODES && i < 5; ++i) {
            displayNodeParameters(i);
        }
    }
};
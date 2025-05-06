import os
import random
import networkx as nx
import numpy as np
from itertools import combinations

# Set random seed for reproducibility
random.seed(42)
np.random.seed(42)

# Parameters
NUM_NODES = 2000  # Reduced number of users
SOCIAL_EDGES = 50000  # Target number of follower edges (average degree ~50)
INTERACTION_EDGES = 10000  # Target number of retweet/reply/mention edges per type (average degree ~10)
MAX_INTERESTS = 12  # Max interests per user
MIN_INTERESTS = 7  # Min interests for better clustering
INTEREST_POOL = [
    "politics", "science", "tech", "sports", "music", "movies",
    "literature", "art", "history", "travel", "fashion", "food",
    "gaming", "health", "education", "finance", "nature", "religion"
]
WEIGHT_RANGE = (1, 10)  # Interaction weight range
SIMILARITY_THRESHOLD = 0.2  # Lowered for more interest-based edges

# Create output directory
os.makedirs("./higg", exist_ok=True)

# Calculate Jaccard similarity between two sets of interests
def jaccard_similarity(interests1, interests2):
    set1 = set(interests1)
    set2 = set(interests2)
    intersection = len(set1 & set2)
    union = len(set1 | set2)
    return intersection / union if union > 0 else 0.0

# Generate interests for each node
def assign_interests():
    interests = []
    for _ in range(NUM_NODES):
        num_interests = random.randint(MIN_INTERESTS, MAX_INTERESTS)
        user_interests = random.sample(INTEREST_POOL, num_interests)
        interests.append(user_interests)
    return interests

# Write interests to file (space-separated)
def write_interests(filename, interests):
    with open(filename, 'w') as f:
        for node_id, user_interests in enumerate(interests):
            f.write(f"{node_id} {' '.join(user_interests)}\n")

# Generate scale-free social network with interest-based bias
def generate_social_network(interests):
    m = max(1, SOCIAL_EDGES // NUM_NODES // 2)  # Edges per new node
    G = nx.barabasi_albert_graph(NUM_NODES, m)  # Ensures connected graph
    edges = set()
    
    # Add Barabasi-Albert edges with interest bias
    for u, v in G.edges():
        if len(edges) >= SOCIAL_EDGES:
            break
        if jaccard_similarity(interests[u], interests[v]) >= SIMILARITY_THRESHOLD:
            edges.add((u, v))  # u follows v
    
    # Add interest-based edges to reach target
    nodes = list(range(NUM_NODES))
    random.shuffle(nodes)
    for u, v in combinations(nodes, 2):
        if len(edges) >= SOCIAL_EDGES:
            break
        if (u, v) not in edges and (v, u) not in edges:
            if jaccard_similarity(interests[u], interests[v]) >= SIMILARITY_THRESHOLD:
                edges.add((u, v))
    
    return [(u, v) for u, v in edges]

# Generate interaction network (retweet/reply/mention) with follower and interest bias
def generate_interaction_network(social_edges, interests):
    edges = []
    edge_count = 0
    
    # Create follower map
    follower_map = {i: set() for i in range(NUM_NODES)}
    for u, v in social_edges:
        follower_map[v].add(u)  # u follows v
    
    # 80% of edges from follower relationships
    follower_edges = int(INTERACTION_EDGES * 0.8)
    social_edge_list = list(social_edges)
    random.shuffle(social_edge_list)
    while edge_count < follower_edges and social_edge_list:
        u, v = social_edge_list.pop()
        weight = random.randint(*WEIGHT_RANGE)
        edges.append((u, v, weight))
        edge_count += 1
    
    # Remaining edges based on interest similarity
    nodes = list(range(NUM_NODES))
    while edge_count < INTERACTION_EDGES:
        u, v = random.sample(nodes, 2)
        if u == v or (u, v) in {(e[0], e[1]) for e in edges}:
            continue
        if jaccard_similarity(interests[u], interests[v]) >= SIMILARITY_THRESHOLD:
            weight = random.randint(*WEIGHT_RANGE)
            edges.append((u, v, weight))
            edge_count += 1
    
    return edges

# Write edge list to file
def write_edgelist(filename, edges, has_weight=False):
    with open(filename, 'w') as f:
        for edge in edges:
            if has_weight:
                f.write(f"{edge[0]} {edge[1]} {edge[2]}\n")
            else:
                f.write(f"{edge[0]} {edge[1]} 1.0\n")

# Main dataset generation
def main():
    print("Generating dataset...")

    # Assign interests and write to file
    interests = assign_interests()
    print(f"Assigned interests to {NUM_NODES} nodes")
    write_interests("./higg/higgs-interests.txt", interests)

    # Generate social network
    social_edges = generate_social_network(interests)
    print(f"Generated {len(social_edges)} social edges")

    # Generate interaction networks
    retweet_edges = generate_interaction_network(social_edges, interests)
    reply_edges = generate_interaction_network(social_edges, interests)
    mention_edges = generate_interaction_network(social_edges, interests)
    print(f"Generated {len(retweet_edges)} retweet edges, "
          f"{len(reply_edges)} reply edges, "
          f"{len(mention_edges)} mention edges")

    # Write to files
    write_edgelist("./higg/higgs-social_network.edgelist", social_edges, has_weight=False)
    write_edgelist("./higg/higgs-retweet_network.edgelist", retweet_edges, has_weight=True)
    write_edgelist("./higg/higgs-reply_network.edgelist", reply_edges, has_weight=True)
    write_edgelist("./higg/higgs-mention_network.edgelist", mention_edges, has_weight=True)
    print("Dataset written to ./higg/ directory")

if __name__ == "__main__":
    main()
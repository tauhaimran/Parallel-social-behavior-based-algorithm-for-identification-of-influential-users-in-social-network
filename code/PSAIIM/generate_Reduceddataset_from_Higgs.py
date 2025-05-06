import pandas as pd
import random
import os

# Input and output directories
input_dir = "./Higgs/"
output_dir = "./higg/"
os.makedirs(output_dir, exist_ok=True)

# File names
files = {
    "interests": "higgs-interests.txt",
    "mention": "higgs-mention_network.edgelist",
    "reply": "higgs-reply_network.edgelist",
    "retweet": "higgs-retweet_network.edgelist",
    "social": "higgs-social_network.edgelist"
}

# Fraction of nodes to keep (1%)
node_fraction = 0.05

# Function to read edgelist and collect nodes
def read_edgelist(file_path, has_weight=True):
    try:
        if has_weight:
            df = pd.read_csv(file_path, sep=" ", header=None, names=["node1", "node2", "weight"])
        else:
            df = pd.read_csv(file_path, sep=" ", header=None, names=["node1", "node2"])
        return df
    except Exception as e:
        print(f"Error reading {file_path}: {e}")
        return pd.DataFrame()

# Function to filter edgelist based on selected nodes and normalize node IDs
def filter_edgelist(df, selected_nodes, node_map, output_path, has_weight=True):
    # Filter edges where both nodes are in selected_nodes
    filtered_df = df[df["node1"].isin(selected_nodes) & df["node2"].isin(selected_nodes)].copy()
    
    # Normalize node IDs (node1 follows node2 for social network)
    filtered_df["node1"] = filtered_df["node1"].map(node_map)
    filtered_df["node2"] = filtered_df["node2"].map(node_map)
    
    # Add weight column for social network if missing
    if not has_weight:
        filtered_df["weight"] = 1.0
    
    # Save to output (node1, node2, weight)
    try:
        filtered_df[["node1", "node2", "weight"]].to_csv(output_path, sep=" ", header=False, index=False)
    except Exception as e:
        print(f"Error writing to {output_path}: {e}")
    return filtered_df

# Function to filter interests file based on selected nodes and normalize node IDs
def filter_interests(file_path, output_path, selected_nodes, node_map):
    interests = []
    try:
        with open(file_path, "r") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split()
                node_id = int(parts[0])
                interests_list = parts[1:]
                if node_id in selected_nodes:
                    interests.append([node_id] + interests_list)
    except Exception as e:
        print(f"Error reading {file_path}: {e}")
        return pd.DataFrame()
    
    # Convert to normalized node IDs
    filtered_interests = [[node_map[row[0]]] + row[1:] for row in interests]
    
    # Save to output
    try:
        with open(output_path, "w") as f:
            for row in filtered_interests:
                f.write(" ".join(str(val) for val in row) + "\n")
    except Exception as e:
        print(f"Error writing to {output_path}: {e}")
    
    # Create DataFrame for summary
    if not filtered_interests:
        return pd.DataFrame(columns=["node_id"])
    
    # Determine maximum number of interests
    max_interests = max((len(row) - 1 for row in filtered_interests), default=0)
    columns = ["node_id"] + [f"interest_{i+1}" for i in range(max_interests)]
    
    # Pad rows with fewer interests to match max_interests
    padded_interests = []
    for row in filtered_interests:
        padded_row = row + [""] * (max_interests - (len(row) - 1))
        padded_interests.append(padded_row)
    
    return pd.DataFrame(padded_interests, columns=columns)

# Collect all unique nodes from edgelist files
all_nodes = set()

for network_type in ["mention", "reply", "retweet", "social"]:
    input_path = os.path.join(input_dir, files[network_type])
    has_weight = network_type != "social"
    df = read_edgelist(input_path, has_weight)
    if not df.empty:
        all_nodes.update(df["node1"].values)
        all_nodes.update(df["node2"].values)

# Randomly select 1% of nodes
all_nodes = list(all_nodes)
selected_nodes = set(random.sample(all_nodes, int(len(all_nodes) * node_fraction)))

# Create node ID mapping (normalize to 0 to len(selected_nodes)-1)
node_map = {old_id: new_id for new_id, old_id in enumerate(sorted(selected_nodes))}

# Process edgelist files, keeping only edges with selected nodes
edgelist_dfs = {}

for network_type in ["mention", "reply", "retweet", "social"]:
    input_path = os.path.join(input_dir, files[network_type])
    output_path = os.path.join(output_dir, files[network_type])
    has_weight = network_type != "social"
    
    # Read edgelist
    df = read_edgelist(input_path, has_weight)
    
    # Filter and normalize
    filtered_df = filter_edgelist(df, selected_nodes, node_map, output_path, has_weight)
    edgelist_dfs[network_type] = filtered_df

# Process interests file, keeping only selected nodes
input_interests = os.path.join(input_dir, files["interests"])
output_interests = os.path.join(output_dir, files["interests"])
filtered_interests_df = filter_interests(input_interests, output_interests, selected_nodes, node_map)

# Print summary
print("New dataset creation complete (1% of nodes).")
print(f"Selected {len(selected_nodes)} nodes out of {len(all_nodes)}")
print(f"Mention network: {len(edgelist_dfs['mention'])} edges")
print(f"Reply network: {len(edgelist_dfs['reply'])} edges")
print(f"Retweet network: {len(edgelist_dfs['retweet'])} edges")
print(f"Social network: {len(edgelist_dfs['social'])} edges")
print(f"Interests: {len(filtered_interests_df)} nodes")
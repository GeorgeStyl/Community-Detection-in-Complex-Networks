import numpy as np

def generate_connected_communities(filename, n, k, inter_fraction=0.05, seed=None):
    """
    Generate a graph with n nodes and k dense communities.
    - Each community is fully connected (clique).
    - Each node has at least one inter-community edge.
    - inter_fraction: fraction of community size to use as extra inter-community edges.
    """
    if seed is not None:
        np.random.seed(seed)

    # Assign nodes to communities
    community_sizes = [n // k] * k
    for i in range(n % k):
        community_sizes[i] += 1

    community_nodes = []
    start = 0
    for size in community_sizes:
        community_nodes.append(np.arange(start, start + size))
        start += size

    edges = set()

    # Add intra-community edges (clique)
    for nodes in community_nodes:
        for i_idx, i in enumerate(nodes):
            for j in nodes[i_idx + 1:]:
                edges.add((i, j))

    # Add inter-community edges
    for ci in range(k):
        for cj in range(ci + 1, k):
            nodes_i = community_nodes[ci]
            nodes_j = community_nodes[cj]
            # Ensure each node in i has at least one connection to j and vice versa
            min_edges = min(len(nodes_i), len(nodes_j))
            for idx in range(min_edges):
                edges.add((nodes_i[idx], nodes_j[idx]))
            # Add extra random inter-community edges
            extra_edges = max(1, int(len(nodes_i) * inter_fraction))
            possible_edges = [(i, j) for i in nodes_i for j in nodes_j if (i,j) not in edges]
            np.random.shuffle(possible_edges)
            for i in range(min(extra_edges, len(possible_edges))):
                edges.add(possible_edges[i])

    # Write edge list
    with open(filename, "w") as f:
        for u, v in edges:
            f.write(f"{u} {v}\n")

    print(f"Graph '{filename}' written: {len(edges)} edges, {n} nodes, {k} communities")

# Generate 5 graphs with increasing sizes
# generate_connected_communities("G1.txt", 500,   30,  inter_fraction=0.05, seed=1)
# generate_connected_communities("G2.txt", 5000,  60,  inter_fraction=0.05, seed=1)
# generate_connected_communities("G3.txt", 10000, 80,  inter_fraction=0.03, seed=3)
# generate_connected_communities("G4.txt", 15000, 120, inter_fraction=0.02, seed=4)
# generate_connected_communities("G5.txt", 20000, 180, inter_fraction=0.02, seed=5)

# ==========================================
# GRAPH GENERATION CONFIGURATION
# Goal: G5 target ~120 seconds (2 mins)
# ==========================================

# G1: Quick sanity check
generate_connected_communities("G1.txt", n=3000,  k=40,  inter_fraction=0.05, seed=1)

# G2: Light workload 
generate_connected_communities("G2.txt", n=6000,  k=75,  inter_fraction=0.05, seed=2)

# G3: Medium workload
generate_connected_communities("G3.txt", n=10000, k=110, inter_fraction=0.03, seed=3)

# G4: Heavy workload (~60 seconds)
generate_connected_communities("G4.txt", n=13000, k=110, inter_fraction=0.02, seed=4)

# G5: Max target (~2 minutes)
# We increased N slightly (16k) but decreased k (90).
# This creates larger cliques (more density), increasing runtime to ~120s.
# G5: Max target (~2 minutes)
# n=16000, k=90 -> creates approx 1.4 million edges (vs 0.5m in the 47s run)
generate_connected_communities("G5.txt", n=18000, k=115, inter_fraction=0.02, seed=5)
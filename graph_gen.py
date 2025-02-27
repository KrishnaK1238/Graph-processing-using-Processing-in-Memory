import random

num_nodes = 100000
num_edges = 200000

edges = set()
while len(edges) < num_edges:
    u = random.randint(0, num_nodes - 1)
    v = random.randint(0, num_nodes - 1)
    if u != v:  # Avoid self-loops
        weight = random.randint(1, 20)  # Random weight range
        edges.add((u, v, weight))

with open("graph5.txt", "w") as f:
    f.write(f"{num_nodes} {num_edges}\n")
    for u, v, weight in edges:
        f.write(f"{u} {v} {weight}\n")

print("Generated graph.txt with 1000 nodes and 5000 edges.")

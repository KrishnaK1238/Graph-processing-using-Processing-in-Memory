import random

def generate_graph(num_vertices, num_edges, weight_range, output_file="graph_input.txt"):
    edges = set()

    max_edges = num_vertices * (num_vertices - 1)  # Directed graph without self-loops
    if num_edges > max_edges:
        raise ValueError(f"Too many edges! Max possible edges for {num_vertices} vertices: {max_edges}")

    # Generate random edges
    while len(edges) < num_edges:
        u = random.randint(0, num_vertices - 1)
        v = random.randint(0, num_vertices - 1)
        weight = random.randint(1, weight_range)

        if u != v and (u, v) not in edges:  # Avoid self-loops and duplicate edges
            edges.add((u, v, weight))

    # Write to file
    with open(output_file, "w") as f:
        f.write(f"{num_vertices} {num_edges}\n")
        for u, v, weight in edges:
            f.write(f"{u} {v} {weight}\n")

    print(f"Graph with {num_vertices} vertices and {num_edges} edges saved to {output_file}.")

# Function call
generate_graph(num_vertices=50000, num_edges=80000, weight_range=100, output_file="graph5.txt")

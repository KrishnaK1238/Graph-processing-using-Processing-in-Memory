#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#define INFINITY 1000000

typedef struct {
    int u, v, weight;
} Edge;

// Function declarations
void read_graph_cpu(const char *filename, int *num_vertices, int *num_edges, Edge **edges);
double cpu_sssp(const char *filename);

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *graph_file = argv[1];

    printf("\nRunning SSSP on CPU...\n");
    double execution_time = cpu_sssp(graph_file);
    printf("CPU Total Execution Time: %.3f ms\n", execution_time);

    return 0;
}

// Function definitions
void read_graph_cpu(const char *filename, int *num_vertices, int *num_edges, Edge **edges) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    fscanf(file, "%d %d", num_vertices, num_edges);
    *edges = (Edge *)malloc((*num_edges) * sizeof(Edge));
    if (!(*edges)) {
        perror("Failed to allocate memory for edges");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < *num_edges; i++) {
        fscanf(file, "%d %d %d", &(*edges)[i].u, &(*edges)[i].v, &(*edges)[i].weight);
    }

    fclose(file);
}

double cpu_sssp(const char *filename) {
    int num_vertices, num_edges;
    Edge *edges;

    read_graph_cpu(filename, &num_vertices, &num_edges, &edges);

    int *distances = (int *)malloc(num_vertices * sizeof(int));
    if (!distances) {
        perror("Failed to allocate memory for distances");
        free(edges);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_vertices; i++) {
        distances[i] = INFINITY;
    }
    distances[0] = 0;

    clock_t start_time = clock();

    for (int i = 0; i < num_vertices - 1; i++) {
        for (int j = 0; j < num_edges; j++) {
            int u = edges[j].u;
            int v = edges[j].v;
            int weight = edges[j].weight;

            if (distances[u] != INFINITY && distances[u] + weight < distances[v]) {
                distances[v] = distances[u] + weight;
            }
        }
    }

    clock_t end_time = clock();
    double execution_time = (double)(end_time - start_time) * 1000 / CLOCKS_PER_SEC;

    free(edges);
    free(distances);

    return execution_time;
}

#include <assert.h>
#include <dpu.h>
#include <dpu_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
// Infinity value for initialization
#define INFINITY 1000000

typedef struct {
    int u, v, weight;
} Edge;

// Timer function
double get_time_in_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

// Function to read the graph from a file
void read_graph(const char *filename, int *num_vertices, int *num_edges, Edge **edges) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    // Read the number of vertices and edges
    fscanf(file, "%d %d", num_vertices, num_edges);

    // Allocate memory for edges
    *edges = (Edge *)malloc((*num_edges) * sizeof(Edge));
    if (!(*edges)) {
        perror("Failed to allocate memory for edges");
        exit(EXIT_FAILURE);
    }

    // Read edges
    for (int i = 0; i < *num_edges; i++) {
        fscanf(file, "%d %d %d", &(*edges)[i].u, &(*edges)[i].v, &(*edges)[i].weight);
    }

    fclose(file);
}

// CPU-based SSSP
double cpu_sssp(const char *filename) {
    int num_vertices, num_edges;
    Edge *edges;

    // Read the graph from the file
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }
    fscanf(file, "%d %d", &num_vertices, &num_edges);
    edges = (Edge *)malloc(num_edges * sizeof(Edge));
    for (int i = 0; i < num_edges; i++) {
        fscanf(file, "%d %d %d", &edges[i].u, &edges[i].v, &edges[i].weight);
    }
    fclose(file);

    // Allocate memory for distances
    int *distances = (int *)malloc(num_vertices * sizeof(int));
    if (!distances) {
        perror("Failed to allocate memory for distances");
        free(edges);
        exit(EXIT_FAILURE);
    }

    // Initialize distances
    for (int i = 0; i < num_vertices; i++) {
        distances[i] = INFINITY;
    }
    distances[0] = 0; // Source vertex

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

    // Calculate total execution time
    double total_execution_time_ms = (double)(end_time - start_time) * 1000 / CLOCKS_PER_SEC;

    // Free resources
    free(edges);
    free(distances);

    return total_execution_time_ms;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *graph_file = argv[1];
    struct dpu_set_t set, dpu;

    printf("\nRunning SSSP on CPU...\n");
    double cpu_execution_time_ms = cpu_sssp(graph_file);
    printf("CPU Total Execution Time: %.3f ms\n", cpu_execution_time_ms);

    int num_vertices, num_edges;
    Edge *edges;

    // Read the graph from the file
    read_graph(graph_file, &num_vertices, &num_edges, &edges);

    // Allocate memory for distances
    int *distances = (int *)malloc(num_vertices * sizeof(int));
    if (!distances) {
        perror("Failed to allocate memory for distances");
        free(edges);
        return EXIT_FAILURE;
    }

    // Initialize distances
    for (int i = 0; i < num_vertices; i++) {
        distances[i] = INFINITY;
    }
    distances[0] = 0; // Source vertex

    // Track execution time
    double start_time = get_time_in_seconds();

    // Allocate and load DPUs
    uint32_t nr_dpus = 1;
    DPU_ASSERT(dpu_alloc(nr_dpus, NULL, &set));
    DPU_ASSERT(dpu_load(set, "./sssp_dpu", NULL));
    DPU_ASSERT(dpu_get_nr_dpus(set, &nr_dpus));
    printf("Number of DPUs allocated: %u\n", nr_dpus);

    // Measure CPU-to-DPU data transfer time
    double transfer_start_time = get_time_in_seconds();

    DPU_FOREACH(set, dpu) {
        DPU_ASSERT(dpu_copy_to(dpu, "edges", 0, edges, num_edges * sizeof(Edge)));
        DPU_ASSERT(dpu_copy_to(dpu, "distances", 0, distances, num_vertices * sizeof(int)));
        DPU_ASSERT(dpu_copy_to(dpu, "NUM_VERTICES", 0, &num_vertices, sizeof(num_vertices)));
        DPU_ASSERT(dpu_copy_to(dpu, "NUM_EDGES", 0, &num_edges, sizeof(num_edges)));
    }

    double transfer_end_time = get_time_in_seconds();
    double cpu_to_dpu_time_ms = (transfer_end_time - transfer_start_time) * 1000;

    // Launch the DPU program
    double dpu_execution_start_time = get_time_in_seconds();
    DPU_ASSERT(dpu_launch(set, DPU_SYNCHRONOUS));
    double dpu_execution_end_time = get_time_in_seconds();
    double dpu_execution_time_ms = (dpu_execution_end_time - dpu_execution_start_time) * 1000;

    // Retrieve results
    double dpu_to_cpu_start_time = get_time_in_seconds();
    DPU_FOREACH(set, dpu) {
        DPU_ASSERT(dpu_copy_from(dpu, "distances", 0, distances, num_vertices * sizeof(int)));
    }
    double dpu_to_cpu_end_time = get_time_in_seconds();
    double dpu_to_cpu_time_ms = (dpu_to_cpu_end_time - dpu_to_cpu_start_time) * 1000;

    double end_time = get_time_in_seconds();

    // Calculate total execution time
    double total_execution_time_ms = (end_time - start_time) * 1000;
    printf("printing log for dpu:\n");
    DPU_FOREACH(set, dpu) {
     DPU_ASSERT(dpu_log_read(dpu, stdout));
  }

    // Calculate metrics
    size_t memory_transferred = (num_edges * sizeof(Edge)) + (num_vertices * sizeof(int)) + (2 * sizeof(int));

    size_t total_dpu_memory = 64 * 1024 * 1024; // 64 MB per DPU
    size_t memory_used = (num_edges * sizeof(Edge)) + (num_vertices * sizeof(int));
    double memory_utilization = (double)memory_used / total_dpu_memory * 100;

    // Print metrics
    printf("\nMetrics:\n");
    printf("Total Execution Time: %.3f ms\n", total_execution_time_ms);
    printf("CPU to DPU Transfer Time: %.3f ms\n", cpu_to_dpu_time_ms);
    printf("DPU Execution Time: %.3f ms\n", dpu_execution_time_ms);
    printf("DPU to CPU Transfer Time: %.3f ms\n", dpu_to_cpu_time_ms);
    printf("Memory Transferred: %.2f KB\n", memory_transferred / 1024.0);
    printf("DPU Memory Utilization: %.6f%%\n", memory_utilization);

    // Print CPU vs DPU comparison
    printf("\nComparison:\n");
    printf("CPU Total Execution Time: %.3f ms\n", cpu_execution_time_ms);
    printf("DPU Total Execution Time: %.3f ms\n", total_execution_time_ms);

    // Free resources
    DPU_ASSERT(dpu_free(set));
    free(edges);
    free(distances);

    return 0;
}

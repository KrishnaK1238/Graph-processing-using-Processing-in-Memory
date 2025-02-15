#include <assert.h>
#include <dpu.h>
#include <dpu_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define INFINITY 1000000

typedef struct {
    int u, v, weight;
} Edge;

double get_time_in_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

void read_graph(const char *filename, int *num_vertices, int *num_edges, Edge **edges) {
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

double cpu_sssp(const char *filename, int* cpu_distances) {
    int num_vertices, num_edges;
    Edge *edges;

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

    if (!cpu_distances) {
        perror("Failed to allocate memory for distances");
        free(edges);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_vertices; i++) {
        cpu_distances[i] = INFINITY;
    }
    cpu_distances[0] = 0;

    clock_t start_time = clock();

    for (int i = 0; i < num_vertices - 1; i++) {
        for (int j = 0; j < num_edges; j++) {
            int u = edges[j].u;
            int v = edges[j].v;
            int weight = edges[j].weight;

            if (cpu_distances[u] != INFINITY && cpu_distances[u] + weight < cpu_distances[v]) {
                cpu_distances[v] = cpu_distances[u] + weight;
            }
        }
    }

    clock_t end_time = clock();
    double total_execution_time_ms = (double)(end_time - start_time) * 1000 / CLOCKS_PER_SEC;


    free(edges);
    return total_execution_time_ms;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *graph_file = argv[1];
    struct dpu_set_t set, dpu;

    

    int num_vertices, num_edges;
    Edge *edges;

    read_graph(graph_file, &num_vertices, &num_edges, &edges);
    int* cpu_distances = (int *)malloc(num_vertices * sizeof(int));
   
    int *dpu_distances = (int *)malloc(num_vertices * sizeof(int));
    	
    printf("\nRunning SSSP on CPU...\n");
    double cpu_execution_time_ms = cpu_sssp(graph_file, cpu_distances);
    printf("CPU Total Execution Time: %.3f ms\n", cpu_execution_time_ms);

  
    if (!dpu_distances) {
        perror("Failed to allocate memory for distances");
        free(edges);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < num_vertices; i++) {
        dpu_distances[i] = INFINITY;
    }
    dpu_distances[0] = 0;

    double start_time = get_time_in_seconds();

    uint32_t nr_dpus = 4;
    DPU_ASSERT(dpu_alloc(nr_dpus, NULL, &set));
    DPU_ASSERT(dpu_load(set, "./sssp_dpu", NULL));
    DPU_ASSERT(dpu_get_nr_dpus(set, &nr_dpus));
    printf("Number of DPUs allocated: %u\n", nr_dpus);
    printf("number of vertices: %u, number of edges: %u\n", num_vertices, num_edges);

    double transfer_start_time = get_time_in_seconds();
    int edges_per_dpu = num_edges / nr_dpus;
    int remaining_edges = num_edges % nr_dpus;
    int edge_start = 0;
    int *temp_dpu_distances = dpu_distances;

    int dpu_id = 0;
    DPU_FOREACH(set, dpu) {
    printf("DPU %d is assigned:\n", dpu_id);
    printf("   - Number of Vertices: %d\n", num_vertices);
    printf("   - Number of Edges: %d\n", num_edges);
        int allocated_edges = edges_per_dpu + (dpu_id < remaining_edges ? 1 : 0);
        int edge_end = edge_start + allocated_edges;
        int node_min = INFINITY, node_max = -1;

    DPU_ASSERT(dpu_copy_to(dpu, "edges", 0, edges, num_edges * sizeof(Edge)));
    DPU_ASSERT(dpu_copy_to(dpu, "distances", 0, dpu_distances, num_vertices * sizeof(int)));
    DPU_ASSERT(dpu_copy_to(dpu, "NUM_VERTICES", 0, &num_vertices, sizeof(num_vertices)));
    DPU_ASSERT(dpu_copy_to(dpu, "NUM_EDGES", 0, &num_edges, sizeof(num_edges)));

    dpu_id++;
    }

    double transfer_end_time = get_time_in_seconds();
    double cpu_to_dpu_time_ms = (transfer_end_time - transfer_start_time) * 1000;

    double dpu_execution_start_time = get_time_in_seconds();
    DPU_ASSERT(dpu_launch(set, DPU_SYNCHRONOUS));
    double dpu_execution_end_time = get_time_in_seconds();
    double dpu_execution_time_ms = (dpu_execution_end_time - dpu_execution_start_time) * 1000;

    double dpu_to_cpu_start_time = get_time_in_seconds();
    DPU_FOREACH(set, dpu) {
        DPU_ASSERT(dpu_copy_from(dpu, "distances", 0, dpu_distances, num_vertices * sizeof(int)));
    }
    double dpu_to_cpu_end_time = get_time_in_seconds();
    double dpu_to_cpu_time_ms = (dpu_to_cpu_end_time - dpu_to_cpu_start_time) * 1000;

    double end_time = get_time_in_seconds();
    double total_execution_time_ms = (end_time - start_time) * 1000;
    
    printf("\nCPU Computed Shortest Distances\n");
    for (int i = 0; i <= 10 && i < num_vertices; i++) {
        printf("Vertex %d: %d\n", i, cpu_distances[i]);
    }

    printf("\nDPU Computed Shortest Distances\n");
    for (int i = 0; i <= 10 && i < num_vertices; i++) {
        printf("Vertex %d: %d\n", i, dpu_distances[i]);
    }
    
    for (int i = 0; i < num_vertices; i++){
    	if (cpu_distances[i] == dpu_distances[i]){
    		printf("i: %d, cpu distances equals to dpu distances\n", i);
    	} else if (cpu_distances[i] != dpu_distances[i]){
            printf("i: %d, cpu distances does not equal to dpu distances\n", i);
        }
    }

    printf("\nMetrics:\n");
    printf("Total Execution Time: %.3f ms\n", total_execution_time_ms);
    printf("CPU to DPU Transfer Time: %.3f ms\n", cpu_to_dpu_time_ms);
    printf("DPU Execution Time: %.3f ms\n", dpu_execution_time_ms);
    printf("DPU to CPU Transfer Time: %.3f ms\n", dpu_to_cpu_time_ms);

    size_t memory_transferred = (num_edges * sizeof(Edge)) + (num_vertices * sizeof(int)) + (2 * sizeof(int));
    size_t total_dpu_memory = 64 * 1024 * 1024;
    size_t memory_used = (num_edges * sizeof(Edge)) + (num_vertices * sizeof(int));
    double memory_utilization = (double)memory_used / total_dpu_memory * 100;

    printf("Memory Transferred: %.2f KB\n", memory_transferred / 1024.0);
    printf("DPU Memory Utilization: %.6f%%\n", memory_utilization);

    printf("\nComparison:\n");
    printf("CPU Total Execution Time: %.3f ms\n", cpu_execution_time_ms);
    printf("DPU Total Execution Time: %.3f ms\n", total_execution_time_ms);

    printf("\nDPU Log Output:\n");
    DPU_FOREACH(set, dpu) {
        DPU_ASSERT(dpu_log_read(dpu, stdout));
    }

    DPU_ASSERT(dpu_free(set));
    free(edges);
    free(dpu_distances);
    free(cpu_distances);

    return 0;
}

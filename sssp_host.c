#include <assert.h>
#include <dpu.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define INFINITY 1000000

// Initialize the variables vertices and distances
typedef struct {
    int32_t u, v, weight;
        int32_t padding;  
} Edge;

double get_time_in_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

// Function to read graph from an input text file
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

// CPU implementation of the SSSP algorithm
double cpu_sssp(int num_vertices, int num_edges, Edge *edges, int *cpu_distances) {
    for (int i = 0; i < num_vertices; i++) {
        cpu_distances[i] = INFINITY; // all the distances are set to infinity until the nodes have been discovered 
    }
    cpu_distances[0] = 0; // vertex 0's distance is set to 0 as it is the source node

    double start_time = get_time_in_seconds(); // CPU execution time starts

    for (int i = 0; i < num_vertices - 1; i++) { // Perform (num_vertices - 1) iterations to relax all edges

        for (int j = 0; j < num_edges; j++) { // Iterate through all edges in the graph
            int u = edges[j].u;  // Source vertex of the edge
            int v = edges[j].v; // Destination vertex of the edge
            int weight = edges[j].weight; // Weight of the edge (u -> v)

        // Relaxation Step:
        // If the current shortest distance to 'u' is not INFINITY,
        // and the path through 'u' gives a shorter distance to 'v',
        // then update the shortest distance to 'v'.
            if (cpu_distances[u] != INFINITY && cpu_distances[u] + weight < cpu_distances[v]) {
                cpu_distances[v] = cpu_distances[u] + weight;
                
            }
        }
      
    }

    double end_time = get_time_in_seconds(); // CPU execution time ends
    return (end_time - start_time) * 1000; // Execution time in ms
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <graph_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *graph_file = argv[1];
    struct dpu_set_t set, dpu; // Declare DPU set

    // Graph properties
    int32_t num_vertices, num_edges;
    Edge *edges;

    // Read graph from file
    read_graph(graph_file, &num_vertices, &num_edges, &edges);

    // Allocate memory for CPU and DPU distance arrays
    int32_t *cpu_distances = (int *)malloc(num_vertices * sizeof(int));
    int32_t *dpu_distances = (int *)malloc(num_vertices * sizeof(int));

    if (!cpu_distances || !dpu_distances) {
        perror("Failed to allocate memory for distances");
        free(edges);
        return EXIT_FAILURE;
    }
    // Compute the CPU execution time
    printf("\nRunning SSSP on CPU...\n");
    double cpu_execution_time_ms = cpu_sssp(num_vertices, num_edges, edges, cpu_distances);
    printf("CPU Execution Time: %.3f ms\n", cpu_execution_time_ms); 

    // Initialize DPU distances array
    for (int i = 0; i < num_vertices; i++) {
        dpu_distances[i] = INFINITY;
    }
    dpu_distances[0] = 0;

    double start_time = get_time_in_seconds(); // start DPU execution timer
    // Allocate DPUs
    int32_t NR_DPUS = 16;
    DPU_ASSERT(dpu_alloc(NR_DPUS, "backend=simulator", &set));
    DPU_ASSERT(dpu_load(set, "./sssp_dpu", NULL));

    // Divide edges among DPUs
    int32_t edges_per_dpu = (num_edges + NR_DPUS - 1) / NR_DPUS;
    int dpu_id = 0;

    double transfer_start_time = get_time_in_seconds(); // start CPU-DPU transfer timer

    // Transfer edge partitions to each DPU
    DPU_FOREACH(set, dpu) {
        int32_t start_idx = dpu_id * edges_per_dpu;
        int32_t end_idx = (start_idx + edges_per_dpu < num_edges) ? (start_idx + edges_per_dpu) : num_edges;
        int32_t partition_size = end_idx - start_idx;
        printf("DPU %d is handling edges [%d - %d] (%d edges)\n", dpu_id, start_idx, end_idx - 1, partition_size);

        // Transfer edge data to DPU
        DPU_ASSERT(dpu_copy_to(dpu, "edges", 0, &edges[start_idx], partition_size * sizeof(Edge)));
        DPU_ASSERT(dpu_copy_to(dpu, "NUM_VERTICES", 0, &num_vertices, sizeof(num_vertices)));
        DPU_ASSERT(dpu_copy_to(dpu, "NUM_EDGES", 0, &partition_size, sizeof(partition_size)));

        dpu_id++;  
    }
    // Transfer initial distances array to DPUs
    DPU_FOREACH(set, dpu) {
        DPU_ASSERT(dpu_copy_to(dpu, "distances", 0, dpu_distances, num_vertices * sizeof(int)));
    }

    double transfer_end_time = get_time_in_seconds(); // stop timer for CPU-DPU transfer time
    double cpu_to_dpu_time_ms = (transfer_end_time - transfer_start_time) * 1000; // calculate CPU-DPU transfer time in ms

 
    for (int iter = 0; iter < num_vertices - 1; iter++) {
        DPU_ASSERT(dpu_launch(set, DPU_SYNCHRONOUS)); // launch DPUs

        bool any_updates = false;
        int temp_distances[num_vertices];

        // retreive update distances from DPUs
        DPU_FOREACH(set, dpu) {
            DPU_ASSERT(dpu_copy_from(dpu, "distances", 0, temp_distances, num_vertices * sizeof(int32_t)));

            for (int i = 0; i < num_vertices; i++) {
                if (temp_distances[i] < dpu_distances[i]) {
                    dpu_distances[i] = temp_distances[i];
                    any_updates = true;
                }
            }
        }

        if (!any_updates) {
            break;
        }
         // Copy updated distances back to DPUs for the next iteration
        DPU_FOREACH(set, dpu) {
            DPU_ASSERT(dpu_copy_to(dpu, "distances", 0, dpu_distances, num_vertices * sizeof(int32_t)));
        }
    }
    printf("\nFirst 10 distances:\n");
    for (int i = 0; i < 10 && i < num_vertices; i++) {
        printf("Vertex %d: CPU = %d, DPU = %d", i, cpu_distances[i], dpu_distances[i]);
        

        if (cpu_distances[i] != dpu_distances[i]) {
            printf("  [Mismatch!]");
        }
        printf("\n");
    }



    double dpu_to_cpu_start_time = get_time_in_seconds();
    DPU_FOREACH(set, dpu) {
        DPU_ASSERT(dpu_copy_from(dpu, "distances", 0, dpu_distances, num_vertices * sizeof(int)));
    }
    double dpu_to_cpu_end_time = get_time_in_seconds();
    double dpu_to_cpu_time_ms = (dpu_to_cpu_end_time - dpu_to_cpu_start_time) * 1000;

    double end_time = get_time_in_seconds();
    double total_execution_time_ms = (end_time - start_time) * 1000;

    printf("\nComparison of CPU and DPU Results:\n");
    int correct = 0, incorrect = 0;
    for (int i = 0; i < num_vertices; i++) {
        if (cpu_distances[i] == dpu_distances[i]) {
            correct++;
        } else {
            incorrect++;
        }
    }
    printf("Matching distances: %d\n", correct);
    printf("Mismatched distances: %d\n", incorrect);

    printf("\nMetrics:\n");
    printf("CPU Execution Time: %.3f ms\n", cpu_execution_time_ms);
    printf("DPU Execution Time: %.3f ms\n", total_execution_time_ms);
    printf("CPU to DPU Transfer Time: %.3f ms\n", cpu_to_dpu_time_ms);
    printf("DPU to CPU Transfer Time: %.3f ms\n", dpu_to_cpu_time_ms);



    DPU_ASSERT(dpu_free(set));
    free(edges);
    free(dpu_distances);
    free(cpu_distances);

    return 0;
}

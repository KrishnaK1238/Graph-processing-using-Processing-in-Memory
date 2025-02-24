#include <defs.h>
#include <mram.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_EDGES 20000
#define MAX_VERTICES 5000

typedef struct {
    int u, v, weight;
} Edge;

__mram_noinit Edge edges[MAX_EDGES];
__mram_noinit int distances[MAX_VERTICES];
__host int NUM_VERTICES;
__host int NUM_EDGES;              

int main() {
    bool updated = true;
    uint32_t tasklet_id = me();
    printf("tasklet_id %u \n", tasklet_id);
    if (tasklet_id == 0){
    	printf("Number of tasklets: %u \n", NR_TASKLETS);
    	}
 // Ensure Bellman-Ford runs for exactly V-1 iterations while allowing early stopping
for (int iter = 0; iter < NUM_VERTICES - 1; iter++) {  // ✅ Force exactly V-1 iterations
    updated = false;

    for (int i = 0; i < NUM_EDGES; i++) {  // ✅ Process all edges
        int u = edges[i].u;
        int v = edges[i].v;
        int weight = edges[i].weight;

        if (distances[u] != 1000000 && distances[u] + weight < distances[v]) {
            distances[v] = distances[u] + weight;
            updated = true;
        }
    }

    if (!updated) {  // ✅ Stop early if no changes (optimization)
        break;
    }
}

    return 0;
}

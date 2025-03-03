#include <defs.h>
#include <mram.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <barrier.h>

#define MAX_EDGES 2000000
#define MAX_VERTICES 5000000


typedef struct {
    int u, v, weight;
        int32_t padding; 
} Edge;

__mram_noinit Edge edges[MAX_EDGES];
__mram_noinit int distances[MAX_VERTICES];
__host int NUM_VERTICES;
__host int NUM_EDGES;



int main() {
    bool updated = true;
    uint32_t tasklet_id = me();
    printf("tasklet_id %u: stack = %u \n", tasklet_id, check_stack());
    

    int edges_per_tasklet = NUM_EDGES / NR_TASKLETS;
    int start = tasklet_id * edges_per_tasklet;
    int end = (tasklet_id == NR_TASKLETS - 1) ? NUM_EDGES : (start + edges_per_tasklet);


    for (int iter = 0; iter < NUM_VERTICES - 1; iter++) {  
        updated = false;

        for (int i = start; i < end; i++) { 
            int u = edges[i].u;
            int v = edges[i].v;
            int weight = edges[i].weight;

            if (distances[u] != 1000000 && distances[u] + weight < distances[v]) {
                distances[v] = distances[u] + weight;
                updated = true;
            }
        }

        if (!updated) break;
}

    return 0;
}

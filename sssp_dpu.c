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
    while (updated) {
        updated = false;

        for (int i = 0; i < NUM_EDGES; i++) {
            int u = edges[i].u;
            int v = edges[i].v;
            int weight = edges[i].weight;

            if (distances[u] != 1000000 && distances[u] + weight < distances[v]) {
                distances[v] = distances[u] + weight;
                updated = true;
            }
        }
    }

    return 0;
}

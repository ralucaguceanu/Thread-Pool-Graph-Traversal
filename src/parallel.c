// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS		4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
/* TODO: Define graph synchronization mechanisms. */
static pthread_mutex_t lock_sum, lock_graph;

/* TODO: Define graph task argument. */
typedef struct {
	unsigned int idx;
} os_task_arg;

os_task_arg *get_task_argument(unsigned int neighbour_idx)
{
	os_task_arg *arg = malloc(sizeof(os_task_arg));

	arg->idx = neighbour_idx;
	return arg;
}

static void process_node_neighbors(void *arg)
{
	/* TODO: Implement thread-pool based processing of graph. */
	os_node_t *node;
	os_task_arg *argument = (os_task_arg *) arg;
	unsigned int idx = argument->idx;

	node = graph->nodes[idx];

	pthread_mutex_lock(&lock_sum);
	sum += node->info;
	pthread_mutex_unlock(&lock_sum);

	for (unsigned int i = 0; i < node->num_neighbours; i++) {
		pthread_mutex_lock(&lock_graph);
		unsigned int neighbour_idx = node->neighbours[i];

		if (graph->visited[neighbour_idx] == NOT_VISITED) {
			graph->visited[neighbour_idx] = DONE;
			pthread_mutex_unlock(&lock_graph);

			os_task_arg *arg = get_task_argument(neighbour_idx);
			os_task_t *t = create_task(process_node_neighbors, arg, free);

			enqueue_task(tp, t);
		} else {
			pthread_mutex_unlock(&lock_graph);
		}
	}
}

static void process_node(unsigned int idx)
{
	/* TODO: Implement thread-pool based processing of graph. */
	os_node_t *node;

	node = graph->nodes[idx];
	graph->visited[idx] = DONE;
	int node_info = node->info;

	sum += node_info;

	// BFS
	for (unsigned int i = 0; i < node->num_neighbours; i++) {
		pthread_mutex_lock(&lock_graph);
		unsigned int neighbour_idx = node->neighbours[i];

		if (graph->visited[neighbour_idx] == NOT_VISITED) {
			graph->visited[neighbour_idx] = DONE;
			pthread_mutex_unlock(&lock_graph);

			os_task_arg *arg = get_task_argument(neighbour_idx);
			os_task_t *t = create_task(process_node_neighbors, arg, free);

			enqueue_task(tp, t);
		} else {
			pthread_mutex_unlock(&lock_graph);
		}
	}
}

int main(int argc, char *argv[])
{
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	/* TODO: Initialize graph synchronization mechanisms. */
	pthread_mutex_init(&lock_sum, NULL);
	pthread_mutex_init(&lock_graph, NULL);

	tp = create_threadpool(NUM_THREADS);
	process_node(0);

	// cand programul se intoarce la thread-ul principal,
	// setam work_done true si trimitem broadcast catre
	// toate thread-urile pentru a le debloca
	pthread_mutex_lock(&tp->lock);
	tp->work_done = 1;
	pthread_cond_broadcast(&tp->condition_variable);
	pthread_mutex_unlock(&tp->lock);

	wait_for_completion(tp);
	destroy_threadpool(tp);

	pthread_mutex_destroy(&lock_graph);
	pthread_mutex_destroy(&lock_sum);

	printf("%d", sum);

	return 0;
}

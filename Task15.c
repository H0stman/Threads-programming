/***************************************************************************
 *
 * Sequential version of Quick sort
 *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define KILO (1024)
#define MEGA (1024*1024)
#define MAX_ITEMS (64*MEGA)
#define swap(v, a, b) {unsigned tmp; tmp=v[a]; v[a]=v[b]; v[b]=tmp;}
#define MAX_THREADS 8

static int* v;

struct arg
{
	unsigned high;
	unsigned low;
	unsigned thread_depth;
};

static void print_array(void)
{
	int i;
	for (i = 0; i < MAX_ITEMS; i++)
		printf("%d ", v[i]);
	printf("\n");
}

static void init_array(void)
{
	int i;
	v = (int*)malloc(MAX_ITEMS * sizeof(int));
	for (i = 0; i < MAX_ITEMS; i++)
		v[i] = rand();
}

static unsigned partition(int* v, unsigned low, unsigned high, unsigned pivot_index)
{
	/* move pivot to the bottom of the vector */
	if (pivot_index != low)
		swap(v, low, pivot_index);

	pivot_index = low;
	low++;

	/* invariant:
	 * v[i] for i less than low are less than or equal to pivot
	 * v[i] for i greater than high are greater than pivot
	 */

	 /* move elements into place */
	while (low <= high)
	{
		if (v[low] <= v[pivot_index])
			low++;
		else if (v[high] > v[pivot_index])
			high--;
		else
			swap(v, low, high);
	}

	/* put pivot back between two groups */
	if (high != pivot_index)
		swap(v, pivot_index, high);
	return high;
}

static void quick_sort(int* v, unsigned low, unsigned high)
{
	unsigned pivot_index;

	/* no need to sort a vector of zero or one element */
	if (low >= high)
		return;

	/* select the pivot value */
	pivot_index = (low + high) / 2;

	/* partition the vector */
	pivot_index = partition(v, low, high, pivot_index);

	/* sort the two sub arrays */
	if (low < pivot_index)
		quick_sort(v, low, pivot_index - 1);
	if (pivot_index < high)
		quick_sort(v, pivot_index + 1, high);
}

static void* quick_sort_par(void* args)
{
	pthread_t thread = PTHREAD_CREATE_JOINABLE;
	unsigned pivot_index;
	struct arg* t_args = args;

	/* no need to sort a vector of zero or one element */
	if (t_args->low >= t_args->high)
		return NULL;

	/* select the pivot value */
	pivot_index = (t_args->low + t_args->high) / 2;

	/* partition the vector */
	pivot_index = partition(v, t_args->low, t_args->high, pivot_index);

	struct arg new_t, this_t;

	/* sort the two sub arrays */
	if (t_args->low < pivot_index)
	{
		if (t_args->thread_depth > 1)
		{
			new_t.thread_depth = t_args->thread_depth / 2;
			new_t.low = t_args->low;
			new_t.high = pivot_index - 1;
			pthread_create(&thread, NULL, quick_sort_par, &new_t);
		}
		else
			quick_sort(v, t_args->low, pivot_index - 1);
	}

	if (pivot_index < t_args->high)
	{
		if (t_args->thread_depth > 1)
		{
			this_t.low = pivot_index + 1;
			this_t.high = t_args->high;
			this_t.thread_depth = t_args->thread_depth / 2;
			quick_sort_par(&this_t);
		}
		else
			quick_sort(v, pivot_index + 1, t_args->high);
	}

	pthread_join(thread, NULL); //Waits for thread t to terminate
}

int main(int argc, char** argv)
{
	init_array();
	//print_array();
	struct arg args = { .thread_depth = MAX_THREADS, .low = 0, .high = MAX_ITEMS - 1 };
	clock_t begin = clock();
	quick_sort_par(&args);
	clock_t end = clock();
	double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
	printf("Time par: %f s\n", time_spent);
	// init_array();
	// begin = clock();
	//quick_sort(v, 0, MAX_ITEMS - 1);
	// end = clock();
	// time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
	// printf("Time seq: %f s\n", time_spent);
	//print_array();
	return 0;
}

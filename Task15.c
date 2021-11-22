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

void* pass_args(void* args);

static int* v;

struct t_data
{
	unsigned high;
	unsigned low;
	unsigned t;
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


static void quick_sort_par(unsigned thread_count, unsigned low, unsigned high)
{
	pthread_t thread;
	int errcode = -1;
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
	{
		if (thread_count > 1)
		{
			struct t_data* copy = malloc(sizeof(struct t_data)); //Make new argument struct for new thread
			copy->t = thread_count / 2;
			copy->low = low;
			copy->high = pivot_index - 1;
			pthread_create(&thread, NULL, pass_args, (void*)copy); //Struct is passed to helperfunction on new thread which "unpacks the arguments" and calls quick_sort
		}
		else
			quick_sort(v, low, pivot_index - 1);
	}

	if (pivot_index < high)
	{
		if (thread_count > 1)
		{
			struct t_data* copy = malloc(sizeof(struct t_data)); //Make new argument struct for new thread
			copy->t = thread_count / 2;
			copy->low = pivot_index + 1;
			copy->high = high;
			pthread_create(&thread, NULL, pass_args, (void*)copy); //Struct is passed to helperfunction on new thread which "unpacks the arguments" and calls quick_sort
		}
		else
			quick_sort(v, pivot_index + 1, high);
	}
	pthread_join(thread, NULL); //Waits for thread t to terminate
}

void* pass_args(void* args)
{
	struct t_data* passed_arguments = (struct t_data*)args;
	quick_sort_par(passed_arguments->t, passed_arguments->low, passed_arguments->high);
	return NULL;
}

int main(int argc, char** argv)
{
	init_array();
	//print_array();
	clock_t begin = clock();
	quick_sort_par(MAX_THREADS, 0, MAX_ITEMS - 1);
	clock_t end = clock();
	double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
	printf("Time par: %f\n", time_spent);
	init_array();
	begin = clock();
	quick_sort(v, 0, MAX_ITEMS - 1);
	end = clock();
	time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
	printf("Time seq: %f\n", time_spent);
	//print_array();
}

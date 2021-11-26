#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <math.h>
#include <time.h>
#include <stdatomic.h>
#define MAX_SIZE 4096

typedef double matrix[MAX_SIZE][MAX_SIZE];

int	N;		            /* matrix size		*/
int	maxnum;		        /* max number of element*/
char* Init;		        /* matrix init type	*/
int	PRINT;		        /* print switch		*/
matrix	A;		        /* matrix A		*/
double	b[MAX_SIZE];	/* vector b             */
double	y[MAX_SIZE];	/* vector y             */

/* forward declarations */
void work(void);
void Init_Matrix(void);
void Print_Matrix(void);
void Init_Default(void);
int Read_Options(int, char**);

//Thread functions
void* division(void*);
void* elimination(void*);

unsigned int numThreads = 7;
int main(int argc, char** argv)
{
    int i, timestart, timeend, iter;
    
    Init_Default();		        /* Init default values	*/
    Read_Options(argc, argv);	/* Read arguments	*/
    Init_Matrix();		        /* Init the matrix	*/
    work();
    if (PRINT == 1)
        Print_Matrix();
}

typedef struct threadArgs {
    int threadStarted;
    int k;
} threadArgs;

//#define numThreads 7
pthread_t threads[256] = { 0 };
//An array of bools. Used to signal that a row is ready to be used in the division step.
_Atomic int* divReadyFlags;
//Keeps track of the current number of threads alive, so that we do not create more than numThreads.
unsigned int AliveThreads = 0;
//Signals to the main thread when the last row is done. Is needed since all threads are detached.
_Atomic unsigned int Done = 0;

//The mutex used to secure the AliveThreads variable.
pthread_mutex_t aliveMutex;
//A mutex array used to lock a row when a thread is working on it during the elimination step.
pthread_mutex_t* rowMutex;

void* elimination(void* params)
{
    threadArgs* args = (threadArgs*)params;
    int k = args->k;
    int j;
    int i;
    //During elimination we have to lock the row we are on. But before we unlock it we also have to lock the next row.
    for (i = k + 1; i < N; i++) {
        if (i == k + 1)
        {
            pthread_mutex_lock(&rowMutex[i]);
        }
        
	    for (j = k + 1; j < N; j++)
        {
            A[i][j] = A[i][j] - A[i][k] * A[k][j];
        }
		        
	    b[i] = b[i] - A[i][k] * y[k];
	    A[i][k] = 0.0;
        if (i + 1 != N)
        {
            pthread_mutex_lock(&rowMutex[i + 1]);
        }
        pthread_mutex_unlock(&rowMutex[i]);
        //Set the current row as being ready for dividing if it is the first row we handled.
        //This means that the row "i" will not have any more elimination work done on it.
        if (i == k + 1)
        {
            divReadyFlags[i] = 1;
        }
	}

    //If a thread was started to run this function we reduce the amount of live threads when we return here.
    if (args->threadStarted)
    {   
        pthread_mutex_lock(&aliveMutex);
        AliveThreads--;
        pthread_mutex_unlock(&aliveMutex);
    }

    free(args);

    //If its the last row we signal that we are now done.
    if (k == N - 1)
    {
        Done = 1;
    }
    return NULL;
}

void work(void)
{
    pthread_mutex_init(&aliveMutex, NULL);
    
    divReadyFlags = malloc(sizeof(int) * N);
    memset(divReadyFlags, 0, sizeof(divReadyFlags));
    
    rowMutex = malloc(sizeof(pthread_mutex_t) * N);
    memset(rowMutex, 0, sizeof(rowMutex));

    for (int l = 0; l < N; l++)
    {
        pthread_mutex_init(&rowMutex[l], NULL);
    }

    int k;

    threadArgs* divArgs = malloc(sizeof(threadArgs));

    //Here the algorithm starts
    for (k = 0; k < N; k++)
    {
        //If we are not on the first row we wait until the row is flagged as being ready for dividing.
        if (k != 0)
        {
            while(!divReadyFlags[k]);
        }
        
        divArgs->k = k;

        int j;
        double divider = 1.0 / A[k][k];
        for (j = k + 1; j < N; j++)
        {
            A[k][j] = A[k][j] / A[k][k];
        }
            
        y[k] = b[k] / A[k][k];
        A[k][k] = 1.0;

        //After division we let the division function start the elimination step.
        threadArgs* elimArgs = malloc(sizeof(threadArgs));

        elimArgs->k = k;

        //If we can create more threads we do so, otherwise we do the function call in this current thread.
        pthread_mutex_lock(&aliveMutex);
        if (AliveThreads < numThreads)
        {
            elimArgs->threadStarted = 1;
            pthread_create(&threads[AliveThreads], NULL, elimination, elimArgs);
            pthread_detach(threads[AliveThreads]);
            AliveThreads++;
            pthread_mutex_unlock(&aliveMutex);
        }
        else
        {
            pthread_mutex_unlock(&aliveMutex);
            elimArgs->threadStarted = 0;
            elimination(elimArgs);
        }
    }

    //Wait for all the work to be done, needs to be here since all threads get detached instead of joined.
    while(!Done);

    free(divArgs);
    free(divReadyFlags);

    for (int l = 0; l < N; l++)
    {
        pthread_mutex_destroy(&rowMutex[l]);
    }

    pthread_mutex_destroy(&aliveMutex);
}

void Init_Matrix()
{
    int i, j;

    printf("\nsize      = %dx%d ", N, N);
    printf("\nmaxnum    = %d \n", maxnum);
    printf("Init	  = %s \n", Init);
    printf("Initializing matrix...");

    if (strcmp(Init, "rand") == 0)
    {
        for (i = 0; i < N; i++)
        {
            for (j = 0; j < N; j++)
            {
                if (i == j) /* diagonal dominance */
                    A[i][j] = (double)(rand() % maxnum) + 5.0;
                else
                    A[i][j] = (double)(rand() % maxnum) + 1.0;
            }
        }
    }
    if (strcmp(Init, "fast") == 0)
    {
        for (i = 0; i < N; i++)
        {
            for (j = 0; j < N; j++)
            {
                if (i == j) /* diagonal dominance */
                    A[i][j] = 5.0;
                else
                    A[i][j] = 2.0;
            }
        }
    }

    /* Initialize vectors b and y */
    for (i = 0; i < N; i++)
    {
        b[i] = 2.0;
        y[i] = 1.0;
    }

    printf("done \n\n");
}

void Print_Matrix()
{
    int i, j;

    printf("Matrix A:\n");
    for (i = 0; i < N; i++)
    {
        printf("[");
        for (j = 0; j < N; j++)
            printf(" %5.2f,", A[i][j]);
        printf("]\n");
    }
    printf("Vector b:\n[");
    for (j = 0; j < N; j++)
        printf(" %5.2f,", b[j]);
    printf("]\n");
    printf("Vector y:\n[");
    for (j = 0; j < N; j++)
        printf(" %5.2f,", y[j]);
    printf("]\n");
    printf("\n\n");
}

void Init_Default()
{
    N = 2048;
    Init = "rand";
    maxnum = 15.0;
    PRINT = 0;
}

int Read_Options(int argc, char** argv)
{
    char* prog;

    prog = *argv;
    while (++argv, --argc > 0)
        if (**argv == '-')
            switch (*++ * argv)
            {
            case 'n':
                --argc;
                N = atoi(*++argv);
                break;
            case 'h':
                printf("\nHELP: try sor -u \n\n");
                exit(0);
                break;
            case 'u':
                printf("\nUsage: gaussian [-n problemsize]\n");
                printf("           [-D] show default values \n");
                printf("           [-h] help \n");
                printf("           [-I init_type] fast/rand \n");
                printf("           [-m maxnum] max random no \n");
                printf("           [-P print_switch] 0/1 \n");
                exit(0);
                break;
            case 'D':
                printf("\nDefault:  n         = %d ", N);
                printf("\n          Init      = rand");
                printf("\n          maxnum    = 5 ");
                printf("\n          P         = 0 \n\n");
                exit(0);
                break;
            case 'I':
                --argc;
                Init = *++argv;
                break;
            case 'm':
                --argc;
                maxnum = atoi(*++argv);
                break;
            case 'P':
                --argc;
                PRINT = atoi(*++argv);
                break;
            default:
                printf("%s: ignored option: -%s\n", prog, *argv);
                printf("HELP: try %s -u \n\n", prog);
                break;
            }
}

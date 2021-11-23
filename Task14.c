/***************************************************************************
 *
 * Sequential version of Gaussian elimination
 *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <math.h>
#include <time.h>

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

typedef struct Job {
    int handled;    //0 if the job is handled by the thread and 1 if it has been.
    int typeOfJob;  //0 for division, 1 for elimination
    int args[4];
} Job;

#define numThreads 1
Job jobQueue[numThreads] = { 0 };

const int minElemsPerThread = 200;

int terminateThreads = 0;
pthread_mutex_t queueMutex[numThreads] = { 0 };

int condCount = 0;
_Atomic int atomCounter = 0;

void doJob(Job* job)
{
    switch (job->typeOfJob)
    {
        case 0: //Division step
        {
            int elementsToUpdate = job->args[0];
            int k = job->args[1];
            int startingIndex = job->args[2];

            //If it is the last job on the row we have to adjust the amount of index' to update.
            if (startingIndex + elementsToUpdate > N)
            {
                startingIndex = N - startingIndex;
            }

            double kElem = 1.0 / A[k][k];
            for (size_t j = startingIndex; j < elementsToUpdate; j++)
            {
                A[k][j] = A[k][j] * kElem;
            }
            
            atomCounter++;
            break;
        }
        case 1: //Elimination step
        {
            int count = job->args[0];
            int k = job->args[1];
            int i = job->args[2];
            int j = job->args[3];
            for (size_t l = 0; l < count; l++)
            {
                //Break out of the loop and dont update if we get outside the triangle.
                //Can only happen on the last part of each diagonal
                if (j < i)
                {
                    break;
                }
                A[i][j] = A[i][j] - A[i][k] * A[k][j]; // Elimination step

                //Move the indexes to the next element
                i += 1;
                j -= 1;
            }
            atomCounter++;
            break;
        }
        default:
        {
            break;
        }
    }
}

typedef struct threadArgs {
    int id;
} threadArgs;

void* waitForJob(void* params)
{
    struct threadArgs* args = (threadArgs*)params;
    int id = args->id;

    while (1)
    {
        Job currentJob;
        int gotJob = 0;

        //pthread_mutex_lock(&(queueMutex[id]));
        if (jobQueue[id].handled == 0)
        {
            currentJob = jobQueue[id];
            jobQueue[id].handled = 1;
            //pthread_mutex_unlock(&(queueMutex[id]));

            doJob(&currentJob);
        }
        else
        {
            //pthread_mutex_unlock(&(queueMutex[id]));
        }

        if (terminateThreads)
        {
            break;
        }
        
    }
    free(args);
}

double divisionTime = 0.0;
size_t divisionCount = 0;

size_t divisionCountTwo = 0;

double divisionTimeTwo = 0.0;
double divisionTimeTwoOne = 0.0;
double divisionTimeTwoTwo = 0.0;
double divisionTimeTwoThree = 0.0;
double divisionTimeThree = 0.0;

double elimTime = 0.0;
double elimTimeTwo = 0.0;
double elimTimeThree = 0.0;
double elimTimeFour = 0.0;
size_t eliminationCount = 0;

double elimTimeColumn = 0.0;
size_t eliminationCountColumn = 0;

void work(void)
{
    pthread_t threads[numThreads];
    for (size_t l = 0; l < numThreads; l++)
    {
        pthread_mutex_init(&(queueMutex[l]), NULL);

        //Set all the jobs to handled so that the threads wait if they get to the conditional and the main thread has not assigned work yet.
        jobQueue[l].handled = 1;

        threadArgs* args = malloc(sizeof(struct threadArgs));
        args->id = l;
        if (pthread_create(&threads[l], NULL, &waitForJob, args) != 0)
        {
            perror("Could not create thread.\n");
        }
    }
    
    int i, j, k;

    //Division starts here.
    int minElems = numThreads * minElemsPerThread;
    float oneDivNumThreads = 1.0 / (float)numThreads;
    for (k = 0; k < N; k++) //For every row.     
    {
        printf("%d\n", k);
        //divisionCount++;
        clock_t startDiv_t = clock();
        
        int totalElems = N - (k + 1);
        
        int nrOfElems;
        if (totalElems > minElems)
        {
            nrOfElems = (int)(totalElems * oneDivNumThreads) + 1;
        }
        else
        {
            nrOfElems = minElemsPerThread;
        }

        j = k + 1; //j is starting element
        size_t l = 0;
        for (l; l < numThreads; l++)
        {
            //divisionCountTwo++;
            //At the end there will be less than 8 elems to calculate. In this case we break out of the for loop.
            if (j >= N)
            {
                break;
            }

            /*
            Job job = {
                .handled = 0
                .typeOfJob = 0,
                .args[0] = nrOfElems,   //How many elements it should calculate.
                .args[1] = k,           //What row it is on.
                .args[2] = j,           //Starting index on the row.
                .args[3] = 0
            };
            */
            
            //pthread_mutex_lock(&(queueMutex[l]));
            
            jobQueue[l].typeOfJob = 0;
            jobQueue[l].args[0] = nrOfElems;
            jobQueue[l].args[1] = k;
            jobQueue[l].args[2] = j;
            jobQueue[l].handled = 0;
            //pthread_mutex_unlock(&(queueMutex[l]));
            
            j += nrOfElems;  //Move on the row.
        }
        
        condCount = l;
        y[k] = b[k] / A[k][k];

        while (atomCounter != condCount);
        atomCounter = 0;

        A[k][k] = 1.0f;

        clock_t endDiv_t = clock();
        divisionTime += (double)(endDiv_t - startDiv_t) / CLOCKS_PER_SEC;
        //printf("Division time: %f\n", (double)(endDiv_t - startDiv_t) / CLOCKS_PER_SEC);

        //Elimination starts here
        i = k + 1; //Next row from the one we are on.
        int index = 1;
        for (j = k + 1; j < N; j++) //For every element on the next row.
        {
            //eliminationCount++;
            clock_t startElim_t = clock();
            //Calculate number of elements to calculate in this "batch"
            
            int totalElemsElim = ceil((float)index * 0.5f);
            
            int nrOfElemsElim;
            if (totalElemsElim > minElems)
            {
                nrOfElemsElim = (int)(totalElemsElim * oneDivNumThreads) + 1;
            }
            else
            {
                nrOfElemsElim = minElemsPerThread;
            }
            
            clock_t endElim_t = clock();
            elimTime += (double)(endElim_t - startElim_t) / CLOCKS_PER_SEC;

            startElim_t = clock();
            //Start a batch.
            int yi = i; //index
            int xi = j; //index
            size_t l = 0;
            for (l; l < numThreads; l++)
            {
                //If the x-coordinate is smaller than the y-coordinate we are outside our triangle.
                if (xi < yi)
                {
                    break;
                }
                
                /*
                Job job = {
                    .handled = 0,
                    .typeOfJob = 1,             //Type of job, elimination in this case
                    .args[0] = nrOfElemsElim,  //Number of elements to calculate in this thread.
                    .args[1] = k,               //Current division row.
                    .args[2] = yi,              //Starting element y-coordinate
                    .args[3] = xi               //Starting element x-coordinate
                };
                */

               //pthread_mutex_lock(&(queueMutex[l]));
                jobQueue[l].typeOfJob = 1;
                jobQueue[l].args[0] = nrOfElemsElim;
                jobQueue[l].args[1] = k;
                jobQueue[l].args[2] = yi;
                jobQueue[l].args[3] = xi;
                jobQueue[l].handled = 0;
                //pthread_mutex_unlock(&(queueMutex[l]));

                //Move the starting coordinate for the next thread
                yi += nrOfElemsElim;
                xi -= nrOfElemsElim;
            }
            condCount = l;
            endElim_t = clock();
            elimTimeTwo += (double)(endElim_t - startElim_t) / CLOCKS_PER_SEC;

            startElim_t = clock();
            
            while (atomCounter != condCount);
            atomCounter = 0;


            endElim_t = clock();
            elimTimeThree += (double)(endElim_t - startElim_t) / CLOCKS_PER_SEC;

            startElim_t = clock();
            
            //If the last element we updated was on the diagonal we set the element to the left of it to 0.0.
            int lastItemRow = yi + nrOfElems;
            int lastItemColumn = xi - nrOfElems;
            if (lastItemRow == lastItemColumn)
            {
                b[lastItemRow] = b[lastItemRow] - A[lastItemRow][lastItemColumn] * y[lastItemColumn];
                A[lastItemRow][lastItemColumn] = 0.0f;
            }

            endElim_t = clock();

            //printf("Elim time 1: %f\n", (double)(endDiv_t - startDiv_t) / CLOCKS_PER_SEC);
            index++;
            elimTimeFour += (double)(endElim_t - startElim_t) / CLOCKS_PER_SEC;
        }
        j--; //Reduce j by one to be on the last column of the matrix.
        //We will now go through the last column and do the elimination step of those diagonals.
        index = N - (k + 2);
        for (i = k + 2; i < N; i++)
        {
            clock_t startElim_t = clock();

            int totalElemsElim = ceil((float)index * 0.5f);

            int nrOfElemsElim;
            if (totalElemsElim > minElems)
            {
                nrOfElemsElim = (int)(totalElemsElim * oneDivNumThreads) + 1;
            }
            else
            {
                nrOfElemsElim = minElemsPerThread;
            }
            
            //Start a batch.
            int yi = i; //index
            int xi = j; //index
            size_t l = 0;
            for (l; l < numThreads; l++)
            {
                //If the x-coordinate is smaller than the y-coordinate we are outside our triangle.
                if (xi < yi)
                {
                    break;
                }

                /*
                Job job = {
                    .typeOfJob = 1,             //Type of job, elimination in this case
                    .args[0] = elemsPerThread,  //Number of elements to calculate in this thread.
                    .args[1] = k,               //Current division row.
                    .args[2] = yi,              //Starting element y-coordinate
                    .args[3] = xi               //Starting element x-coordinate
                };
                */
               
                //pthread_mutex_lock(&(queueMutex[l]));
                jobQueue[l].typeOfJob = 1;
                jobQueue[l].args[0] = nrOfElemsElim;
                jobQueue[l].args[1] = k;
                jobQueue[l].args[2] = yi;
                jobQueue[l].args[3] = xi;
                jobQueue[l].handled = 0;
                //pthread_mutex_unlock(&(queueMutex[l]));

                //Move the starting coordinate for the next thread
                yi += nrOfElemsElim;
                xi -= nrOfElemsElim;
            }
            condCount = l;

            while (atomCounter != condCount);
            atomCounter = 0;
            
            //If the last element we updated was on the diagonal we set the element to the left of it to 0.0.
            int lastItemRow = yi + nrOfElems;
            int lastItemColumn = xi - nrOfElems;
            if (lastItemRow == lastItemColumn)
            {
                b[lastItemRow] = b[lastItemRow] - A[lastItemRow][lastItemColumn] * y[lastItemColumn];
                A[lastItemRow][lastItemColumn] = 0.0f;
            }

            clock_t endElim_t = clock();

            elimTimeColumn += (double)(endElim_t - startElim_t) / CLOCKS_PER_SEC;
            //printf("Elim time 2: %f\n", (double)(endDiv_t - startDiv_t) / CLOCKS_PER_SEC);
            index--;
        }
    }
    
    terminateThreads = 1;
    for (size_t l = 0; l < numThreads; l++)
    {
        if (pthread_join(threads[l], NULL) != 0)
        {
            perror("Could not join thread.\n");
        }
    }

    for (size_t l = 0; l < numThreads; l++)
    {
        pthread_mutex_destroy(&(queueMutex[l]));
    }
    
    //printf("Average division time one: %f\n", divisionTime / divisionCount);
    //printf("Average division time two: %f\n", divisionTimeTwo / divisionCountTwo);
    //printf("Average division time twoone: %f\n", divisionTimeTwoOne / divisionCountTwo);
    //printf("Average division time twotwo: %f\n", divisionTimeTwoTwo / divisionCountTwo);
    //printf("Average division time three: %f\n", divisionTimeThree / divisionCount);
    //printf("Average elimination time: %f\n", elimTime / eliminationCount);
    //printf("Average elimination column time: %f\n", elimTimeColumn / eliminationCountColumn);
    printf("Division time: %f\n", divisionTime);
    printf("Elimination time 1: %f\n", elimTime);
    printf("Elimination time 2: %f\n", elimTimeTwo);
    printf("Elimination time 3: %f\n", elimTimeThree);
    printf("Elimination time 4: %f\n", elimTimeFour);
    printf("Elimination column time: %f\n", elimTimeColumn);
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
    if (PRINT == 1)
        Print_Matrix();
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

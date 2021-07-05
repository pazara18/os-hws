/*
 * Name-Surname: Abdulkadir Pazar
 * Student No: 150180028
 * Date: 08.05.2021
 * 3 running modes: No argument, 1 argument, 2 argument
 * 
 * In no argument mode program attempts to read inputs from input.txt in the working directory and writes to output.txt
 * 
 * In 1 argumnet mode program reads inputs from file given as 1st command line argument and writes to output.txt
 * 
 * In 2 argument mode program reads inputs from file given as 1st command line argument and
 * writes to file given as 2nd command line argument 
 */
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int KEYSEM;
int KEYSEM2;
int KEYSHM;
void initKeys(char *argv[])//Taken from Week8 recitation codes
{
    char *keyString = malloc(strlen(argv[0]) + 1);
    strcat(keyString, argv[0]);

    KEYSEM = ftok(keyString, 1);
    KEYSEM2 = ftok(keyString, 2);
    KEYSHM = ftok(keyString, 3);
    free(keyString);
}

void sem_signal(int semid, int val)//Taken from Week8 recitation codes
{
    struct sembuf semaphore;
    semaphore.sem_num = 0;
    semaphore.sem_op = val;
    semaphore.sem_flg = 0;
    semop(semid, &semaphore, 1);
}

void sem_wait(int semid, int val)//Taken from Week8 recitation codes
{
    struct sembuf semaphore;
    semaphore.sem_num = 0;
    semaphore.sem_op = (-1 * val);
    semaphore.sem_flg = 0;
    semop(semid, &semaphore, 1);
}

int main(int argc, char *argv[])
{
    int shmid;//shared memory id
    int *globalcp = NULL;//pointer to global memory area
    int termSem = 0, lock = 0;//two semaphores
    int f;//return fork value
    int child[2];//child process array
    int i, myOrder = 0;
    initKeys(argv);

    FILE *read = NULL, *write = NULL;
    if (argc == 1)//no argument mode
    {
        read = fopen("./input.txt", "r");
        write = fopen("./output.txt", "w");
    }
    else if(argc == 2)//1 argument mode
    {
        read = fopen(argv[1], "r");
        write = fopen("./output.txt", "w");
    }
    else//2 argument mode
    {
        read = fopen(argv[1], "r");
        write = fopen(argv[2], "w");
    }
    if (!read)//if no file exists
    {
        printf("unable to open file!");
        return 0;
    }
    int size, threshold;
    fscanf(read, "%d\n", &threshold);
    fscanf(read, "%d\n", &size);
    //  creating a semaphore for synchronization(value = 0)
    //between parent and its children
    termSem = semget(KEYSEM2, 1, 0700 | IPC_CREAT);
    semctl(termSem, 0, SETVAL, 0);

    //  creating a semaphore for synchronization(value = 0)
    //between child processes - syncrhonization since children write to different areas of shm
    lock = semget(KEYSEM, 1, 0700 | IPC_CREAT);
    semctl(lock, 0, SETVAL, 0);

    //  creating a shared memory area
    shmid = shmget(KEYSHM, (2 * size + 4) * sizeof(int), 0700 | IPC_CREAT);

    //  attaching the shared memory segment identified by shmid
    //to the address space of the calling process(parent)
    globalcp = (int *)shmat(shmid, 0, 0);
    globalcp[0] = size;
    globalcp[1] = threshold;

    int j;
    for (j = 0; j < size; ++j)
        fscanf(read, "%d", &globalcp[j + 4]);

    fclose(read);

    //  create 2 child processes
    for (i = 0; i < 2; ++i)
    {
        f = fork();
        if (f < 0)
        {
            printf("FORK error...\n");
            exit(1);
        }
        if (f == 0)
            break;
        child[i] = f;
    }
    if (f != 0)//parent process code
    {
        sem_signal(termSem, 2);//run children first

        sem_wait(termSem, 4); // wait for children to be done
        // print to file

        fprintf(write, "%d\n%d\n", threshold, size);

        for (i = 0; i < size; ++i)
            fprintf(write, "%d ", globalcp[i + 4]);
        fprintf(write, "\n%d\n", globalcp[2]);

        for (i = 0; i < globalcp[2]; ++i)
            fprintf(write, "%d ", globalcp[i + 4 + size]);
        fprintf(write, "\n%d\n", globalcp[3]);

        for (i = 0; i < globalcp[3]; ++i)
            fprintf(write, "%d ", globalcp[i + 4 + size + globalcp[2]]);
        fprintf(write, "\n");

        fclose(write);
        shmdt(globalcp);

        //  removing the created semaphores and shared memory
        semctl(termSem, 0, IPC_RMID, 0);
        semctl(lock, 0, IPC_RMID, 0);
        shmctl(shmid, IPC_RMID, 0);

        //  parent process is exiting
        exit(0);
    }
    else
    {
        //  to understand which child process is running
        myOrder = i;

        //  returning the semaphore ids for KEYSEM and KEYSEM2
        lock = semget(KEYSEM, 1, 0);
        termSem = semget(KEYSEM2, 1, 0);
        sem_wait(termSem, 1);//wait for parent

        //  returning the shared memory id associated with KEYSHM
        shmid = shmget(KEYSHM, (2 * size + 4) * sizeof(int), 0);

        //  attaching the shared memory segment identified by shmid
        //to the address space of the calling process(child)
        globalcp = (int*)shmat(shmid, 0, 0);

        if (myOrder == 0) //first children handles x and values <= M
        {
            int x = 0;
            for (i = 0; i < globalcp[0]; ++i)//iterate over array to calculate x
                if (globalcp[i + 4] <= globalcp[1]) x++;
            globalcp[2] = x;//write x to shm area

            sem_signal(lock, 1); // lets the other child process run after writing x to memory
            int j = 0;
            for (i = 0, j = 0; i < size && j < x; ++i)
            {
                if (globalcp[i + 4] <= globalcp[1])
                {
                    globalcp[j+ size + 4] = globalcp[i + 4];
                    j++;
                }
            }
        }

        else //second children handles y and values > M
        {
            int y = 0;
            for (i = 0; i < globalcp[0]; ++i)//iterate over array to calculate y
                if (globalcp[i + 4] > globalcp[1]) y++;
            globalcp[3] = y;

            sem_wait(lock, 1); // waits for other child to write the x value to shared memory
            int j = 0;
            for (i = 0, j = 0; i < size && j < y; ++i)
            {
                if (globalcp[i + 4] > threshold)
                {
                    globalcp[j + size + globalcp[2] + 4] = globalcp[i + 4];
                    j++;
                }
            }
        }

        //  detaching the shared memory segment from the address
        //space of the calling process(child)
        shmdt(globalcp);

        //  increase semaphore by 1(synchorinization with parent)
        sem_signal(termSem, 2);

        //  child process is exiting
        exit(0);
    }
    return 0;
}

/*
 * Name: Abdulkadir Pazar
 * Student No: 150180028
 * Date: 09/06/2021
 * Compile command = gcc -std=c99 -pthread main.c -o main
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>

#define NUMBER_OF_NEWS_PER_PUBLISHER 3
#define SUBSCRIBER_COUNT 2
#define PUBLISHER_COUNT 3

typedef struct news_block
{
    char data[256];
    int read_count;
    int pub_sem_id;
} news_block;

int write_index = 0;

//Binary semaphore initialized as 1 in the init function
int* pub_sem_arr;

//Counting semaphore initialized as 0 in the init function 
int* sub_sem_arr;

//Array for number of news for each publisher
int* number_of_news;

//Mutex semaphore for circular buffer (initialized as 1)
int mutex;

//Circular buffer for news
news_block* news_buffer;

void init_keys_sems(char** argv)//Adapted from recitation codes
{
    pub_sem_arr = malloc(sizeof(int) * PUBLISHER_COUNT);
    sub_sem_arr = malloc(sizeof(int) * SUBSCRIBER_COUNT);
    news_buffer = malloc(sizeof(news_block) * PUBLISHER_COUNT);
    number_of_news = malloc(sizeof(int) * PUBLISHER_COUNT);

    int sem_key = 0;
    int sem_id = 1;
    //Create and init publisher semaphores
    for(int i = 0; i < PUBLISHER_COUNT; i++)
    {
        sem_key = ftok(argv[0], sem_id);
        pub_sem_arr[i] = semget(sem_key, 1, 0700|IPC_CREAT);
        semctl(pub_sem_arr[i], 0, SETVAL, 1);
        sem_id++;
        number_of_news[i] = NUMBER_OF_NEWS_PER_PUBLISHER;
    }

    //Create and init subscriber semaphores
    for(int i = 0; i < SUBSCRIBER_COUNT; i++)
    {
        sem_key = ftok(argv[0], sem_id);
        sub_sem_arr[i] = semget(sem_key, 1, 0700|IPC_CREAT);
        semctl(sub_sem_arr[i], 0, SETVAL, 0);
        sem_id++;
    }

    //Create buffer mutex semaphore
    sem_key = ftok(argv[0], sem_id);
    mutex = semget(sem_key, 1, 0700|IPC_CREAT);
    semctl(mutex, 0, SETVAL, 1);
}

void sem_signal(int semid, int val)//Taken from recitation codes
{
    struct sembuf semaphore;
    semaphore.sem_num = 0;
    semaphore.sem_op = val;
    semaphore.sem_flg = 0;
    semop(semid, &semaphore, 1);
}

void sem_wait(int semid, int val)//Taken from recitation codes
{
    struct sembuf semaphore;
    semaphore.sem_num = 0;
    semaphore.sem_op = (-1 * val);
    semaphore.sem_flg = 0;
    semop(semid, &semaphore, 1);
}

void news_generator(int news_number, char* dest)//Utility function to generate news in format "News%d"
{
    char str[10] = "News";
    snprintf(str, 10, "News%d", news_number);
    strcpy(dest,str);
}

//Subscriber thread
void* read_news(void* args)
{
    int local_reading_index = 0;
    int sem_index = *((int*) args);
    free(args);
    int total_news = 0;

    for(int i = 0; i < PUBLISHER_COUNT; i++)//Calculate number of total news to read all news
        total_news += number_of_news[i];

    for(int read_news_count = 0; read_news_count < total_news; read_news_count++)//Loop to read all news
    {
        sem_wait(sub_sem_arr[sem_index], 1);
        sem_wait(mutex, 1);//Mutex for buffer
        printf("Subscriber #%d read: %s\n", sem_index + 1, news_buffer[local_reading_index].data);   
        news_buffer[local_reading_index].read_count++;//Increment read count
        if(news_buffer[local_reading_index].read_count == SUBSCRIBER_COUNT)//if a news has been read by all subscribers
        {
            sem_signal(news_buffer[local_reading_index].pub_sem_id, 1);//Signal publisher
        }
        local_reading_index = (local_reading_index + 1) % PUBLISHER_COUNT;//Increment read index to read next news in buffer
        sem_signal(mutex, 1);//Release mutex
    }
    pthread_exit(NULL);
}

//Publisher thread
void* publish(void* args)
{
    int sem_index = *((int*) args);
    free(args);
    for(int i = 0; i < number_of_news[sem_index]; i++)//Publish news as many as given number of news
    {
        sem_wait(pub_sem_arr[sem_index], 1);
        sem_wait(mutex, 1);//Mutex for buffer
        int news_index = i + sem_index * number_of_news[sem_index];
        char str[256];
        news_generator(news_index + 1, str);
        strcpy(news_buffer[write_index].data, str);
        news_buffer[write_index].pub_sem_id = pub_sem_arr[sem_index];//Write publisher id to block
        news_buffer[write_index].read_count = 0;//Initialize read count as 0 
        printf("Publisher #%d published: %s\n", sem_index + 1, str);

        write_index = (write_index + 1) % PUBLISHER_COUNT;//Increment write index for next publisher
        for(int j = 0; j < SUBSCRIBER_COUNT; j++)//Signal all subscribers
        {
            sem_signal(sub_sem_arr[j], 1);
        }
        sem_signal(mutex, 1);//Release mutex
    }
    pthread_exit(NULL);
}

int main(int argc, char** argv)
{
    init_keys_sems(argv);
    pthread_t publisher_threads[PUBLISHER_COUNT];
    pthread_t subscriber_threads[SUBSCRIBER_COUNT];

    int* arg = NULL;//To pass int parameter to thread without pointer-to-int cast warning

    for(int i = 0; i < PUBLISHER_COUNT; i++)
    {
        arg = malloc(sizeof(*arg));
        *arg = i;
        if(pthread_create(&publisher_threads[i], NULL, publish, (void*)arg) != 0)
        {
            printf("Publisher thread #%d creation failed\n", i);
            break;
        }
    }

    for(int i = 0; i < SUBSCRIBER_COUNT; i++)
    {
        arg = malloc(sizeof(*arg));
        *arg = i;
        if(pthread_create(&subscriber_threads[i], NULL, read_news, (void*)arg) != 0)
        {
            printf("Subscriber thread #%d creation failed\n", i);
            break;
        }
    }

    for(int i = 0; i < PUBLISHER_COUNT; i++)
    {
        if(pthread_join(publisher_threads[i], NULL) != 0)
        {
            printf("Publisher thread #%d join failed\n", i);
            break;
        }
    }

    for(int i = 0; i < SUBSCRIBER_COUNT; i++)
    {
        if(pthread_join(subscriber_threads[i], NULL) != 0)
        {
            printf("Subscriber thread #%d join failed\n", i);
            break;
        }
    }

    for(int i = 0; i < PUBLISHER_COUNT; i++)
    {
        semctl(pub_sem_arr[i], 0, IPC_RMID, 0);
    }

    for(int i = 0; i < SUBSCRIBER_COUNT; i++)
    {
        semctl(sub_sem_arr[i], 0, IPC_RMID, 0);
    }

    semctl(mutex, 0, IPC_RMID, 0);
    free(pub_sem_arr);
    free(sub_sem_arr);
    free(news_buffer);
    free(number_of_news);
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/ipc.h>
#include <pthread.h>

#include "csapp.c"
#include "utility.c"
#include "main.h"

//Shared globalmake counters
int num_signals_sent_usr1;
int num_signals_sent_usr2;

//Incremented inside signal handler
volatile int num_signals_received_usr1;
volatile int num_signals_received_usr2;
volatile int reporter_counter;

//Mutexes
pthread_mutex_t signal_sent_usr1_lock;
pthread_mutex_t signal_sent_usr2_lock;
pthread_mutex_t signal_received_usr1_lock;
pthread_mutex_t signal_received_usr2_lock;
pthread_mutex_t thread_args_lock;
pthread_mutex_t reporter_counter_lock;

//Contains common arguments for the 3 types of threads- generators, receivers and reporter
struct ThreadArgs
{
    useconds_t *execution_time_in_us;
    int *signal_count;
};

//Allocate thread arguments and get pointer to pass as arguments in thread creation
struct ThreadArgs *allocate_thread_arguments(useconds_t *execution_time_in_us, int *signal_count)
{
    struct ThreadArgs *args = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs));
    args->execution_time_in_us = execution_time_in_us;
    args->signal_count = signal_count;

    return args;
}

//Increment report_counter when signal type received in reporter thread
void reporter_counter_handler1(int sig)
{
    sigset_t mask, prev_mask;

    Sigemptyset(&mask);
    Sigaddset(&mask, SIGUSR2);                 //Add SIGUSR2 to blocked signals
    Sigprocmask(SIG_BLOCK, &mask, &prev_mask); //Block signals

    //perform critical section since it's multithreaded
    pthread_mutex_lock(&reporter_counter_lock);
    reporter_counter += 1;
    pthread_mutex_unlock(&reporter_counter_lock);

    Sigprocmask(SIG_SETMASK, &prev_mask, NULL); //Restore signals
}

void reporter_counter_handler2(int sig)
{
    sigset_t mask, prev_mask;

    Sigemptyset(&mask);
    Sigaddset(&mask, SIGUSR1);                 //Add SIGUSR1 to blocked signals
    Sigprocmask(SIG_BLOCK, &mask, &prev_mask); //Block signals

    //perform critical section
    pthread_mutex_lock(&reporter_counter_lock);
    reporter_counter += 1;
    pthread_mutex_unlock(&reporter_counter_lock);

    Sigprocmask(SIG_SETMASK, &prev_mask, NULL); //Restore signals
}

int main(int argc, char *argv[])
{
    pthread_t signal_generators_pool[NUM_SIGNAL_GENERATORS];
    pthread_t signal_receivers_pool[NUM_SIGNAL_RECEIVERS];
    pthread_t reporter_thread;

    int signal_count = 0;
    useconds_t execution_time_in_us = 0; //Time elapsed in micro seconds

    //Initialize mutexes
    if (pthread_mutex_init(&signal_sent_usr1_lock, NULL) != 0)
    {
        unix_error("Mutex initilization error");
    }
    if (pthread_mutex_init(&signal_sent_usr2_lock, NULL) != 0)
    {
        unix_error("Mutex initilization error");
    }
    if (pthread_mutex_init(&signal_received_usr1_lock, NULL) != 0)
    {
        unix_error("Mutex initilization error");
    }
    if (pthread_mutex_init(&signal_received_usr2_lock, NULL) != 0)
    {
        unix_error("Mutex initilization error");
    }
    if (pthread_mutex_init(&thread_args_lock, NULL) != 0)
    {
        unix_error("Mutex initilization error");
    }
    if (pthread_mutex_init(&reporter_counter_lock, NULL) != 0)
    {
        unix_error("Mutex initilization error");
    }

    struct ThreadArgs *args = allocate_thread_arguments(execution_time_in_us, signal_count);

    Pthread_create(&reporter_thread, NULL, reporter_thread_fn, args);
    for (int i = 0; i < NUM_SIGNAL_GENERATORS; i++)
        Pthread_create(&signal_generators_pool[i], NULL, generate_signals_thread, args);
    int i;
    for (i = 0; i < NUM_SIGNAL_RECEIVERS / 2; i++) // loop will run for first half which handles SIGUSR1
    {
        Pthread_create(&signal_receivers_pool[i], NULL, receive_signal_usr1_thread, args);
    }
    for (; i < NUM_SIGNAL_RECEIVERS; i++) // loop will run for second half which handles SIGUSR2
    {
        Pthread_create(&signal_receivers_pool[i], NULL, receive_signal_usr2_thread, args);
    }

    // //PART A:
    // while (args->execution_time_in_us < MAX_RUNTIME_IN_SEC * 1000000)
    // {
    //     pause();
    // }

    //Part B
    while (args->signal_count < MAX_SIGNALS)
    {
        pause();
    }

    //Once execution time has passed or signal count is higher than max, kill child threads
    pthread_kill(reporter_thread, SIGTERM);
    for (int i = 0; i < NUM_SIGNAL_GENERATORS; i++)
    {
        if (pthread_kill(signal_generators_pool[i], SIGTERM) != 0)
        {
            unix_error("Thread kill error for signal generators");
        }
    }
    for (int i = 0; i < NUM_SIGNAL_RECEIVERS; i++)
    {
        if (pthread_kill(signal_receivers_pool[i], SIGTERM) != 0)
        {
            unix_error("Thread join error for singal receivers");
        }
    }

    pthread_mutex_destroy(&signal_sent_usr1_lock);
    pthread_mutex_destroy(&signal_sent_usr2_lock);
    pthread_mutex_destroy(&signal_received_usr1_lock);
    pthread_mutex_destroy(&signal_received_usr2_lock);
    pthread_mutex_destroy(&thread_args_lock);
    pthread_mutex_destroy(&reporter_counter_lock);

    return 0;
}

void *generate_signals_thread(void *args)
{
    while (1)
    {
        pthread_mutex_lock(&thread_args_lock);
        struct ThreadArgs *thread_state = (struct ThreadArgs *)args;

        //Create a random processing delay
        struct timeval tv;                                                                        // Acts as faster and precise random seeder
        useconds_t processing_delay = (useconds_t)generate_random_int_between(10000, 100000, tv); //delay between 0.01 0.1 and seconds
        usleep(processing_delay);

        //Update shared state
        thread_state->execution_time_in_us += processing_delay;
        thread_state->signal_count += 1;

        pthread_mutex_unlock(&thread_args_lock);

        //Create a random selection of the two signal types to send
        int signal_type_decider = generate_random_int_between(0, 1, tv);
        if (signal_type_decider == 0)
        {
            pthread_mutex_lock(&signal_sent_usr1_lock);
            num_signals_sent_usr1 += 1;
            pthread_mutex_unlock(&signal_sent_usr1_lock);

            Kill(0, SIGUSR1); //Send SIGUSR1 signal to peers
        }
        else
        {
            pthread_mutex_lock(&signal_sent_usr2_lock);
            num_signals_sent_usr2 += 1;
            pthread_mutex_unlock(&signal_sent_usr2_lock);

            Kill(0, SIGUSR2); //Send SIGUSR2 signal to peers
        }
    }
}

void *receive_signal_usr1_thread(void *arg)
{
    //Install signal handler and spin
    Signal(SIGUSR1, handle_signal_usr1_thread); //wrapper over sigaction() which is more portable than signal()
    while (1)
        pause();
}

void *receive_signal_usr2_thread(void *arg)
{
    //Install signal handler and spin
    Signal(SIGUSR2, handle_signal_usr2_thread);
    while (1)
        pause();
}

void *reporter_thread_fn(void *args)
{
    // Signal(SIGUSR1, reporter_counter_handler1);
    // Signal(SIGUSR2, reporter_counter_handler2);

    int current;
    int prevcurrent;

    //For storing previous results
    int signals_sent_usr1_prev;
    int signals_received_usr1_prev;
    int signals_sent_usr2_prev;
    int signals_received_usr2_prev;
    int reception_time_interval_for_ten_signals;
    float avg_reception_time_sigusr1;
    float avg_reception_time_sigusr2;

    //For counting occurance
    int sigusr1_received_count;
    int sigusr2_received_count;
    int new_time_in_us;
    int old_time_in_us;
    float new_time_in_seconds;
    float old_time_in_seconds;

    pthread_mutex_lock(&thread_args_lock);
    struct ThreadArgs *thread_state = (struct ThreadArgs *)args;
    pthread_mutex_unlock(&thread_args_lock);

    while (1)
    {
        pthread_mutex_lock(&signal_sent_usr1_lock);
        pthread_mutex_lock(&signal_sent_usr2_lock);
        pthread_mutex_lock(&signal_received_usr1_lock);
        pthread_mutex_lock(&signal_received_usr2_lock);
        current = num_signals_received_usr1 + num_signals_received_usr2;
        new_time_in_us = thread_state->execution_time_in_us;
        new_time_in_seconds = ((float)new_time_in_us) / 1000000;
        if (current % 10 == 0 && current != prevcurrent)
        // if (reporter_counter == 10) //TODO: not working...
        {
            sigusr1_received_count = num_signals_received_usr1 - signals_received_usr1_prev;
            sigusr2_received_count = num_signals_received_usr2 - signals_received_usr2_prev;
            reception_time_interval_for_ten_signals = new_time_in_us - old_time_in_us;
            if (sigusr1_received_count > 0)
            {
                avg_reception_time_sigusr1 = ((float)reception_time_interval_for_ten_signals / (float)sigusr1_received_count) / 1000000; //Convert micro sec to sec
            }
            else
            {
                avg_reception_time_sigusr1 = 0;
            }
            if (sigusr2_received_count > 0)
            {
                avg_reception_time_sigusr2 = ((float)reception_time_interval_for_ten_signals / (float)sigusr2_received_count) / 1000000; //Convert micro sec to sec
            }
            else
            {
                avg_reception_time_sigusr2 = 0;
            }
            printf("System time = %0.4f seconds\n", new_time_in_seconds);
            printf("Total signals= %d\n", thread_state->signal_count);
            printf("Total number of SIGUSR1 type signals sent = %d\n", num_signals_sent_usr1);
            printf("Total number of SIGUSR1 type signals received =  %d\n", num_signals_received_usr1);
            printf("Average reception time for SIGUSR1 signals in last 10 signals = %0.4f seconds per signal\n", avg_reception_time_sigusr1);
            printf("Total number of SIGUSR2 type signals sent = %d\n", num_signals_sent_usr2);
            printf("Total number of SIGUSR2 type signals received = %d\n", num_signals_received_usr2);
            printf("Average reception time for SIGUSR2 signals in last 10 signals = %0.4f seconds per signal\n", avg_reception_time_sigusr2);
            printf("\n");
            signals_sent_usr1_prev = num_signals_sent_usr1;
            signals_received_usr1_prev = num_signals_received_usr1;
            signals_sent_usr2_prev = num_signals_sent_usr2;
            signals_received_usr2_prev = num_signals_received_usr2;
            old_time_in_us = new_time_in_us;
            old_time_in_seconds = new_time_in_seconds;

            //Reset reporter counter
            // pthread_mutex_lock(&reporter_counter_lock);
            // reporter_counter = 0;
            // pthread_mutex_unlock(&reporter_counter_lock);
        }
        prevcurrent = current;
        pthread_mutex_unlock(&signal_sent_usr1_lock);
        pthread_mutex_unlock(&signal_sent_usr2_lock);
        pthread_mutex_unlock(&signal_received_usr1_lock);
        pthread_mutex_unlock(&signal_received_usr2_lock);
    };
}

void handle_signal_usr1_thread(int sig)
{
    // Sio_putl(pthread_self()); //TODO: Always the same thread...

    sigset_t mask, prev_mask;

    Sigemptyset(&mask);
    Sigaddset(&mask, SIGUSR2);                 //Add SIGUSR2 to blocked signals
    Sigprocmask(SIG_BLOCK, &mask, &prev_mask); //Block signals

    //perform critical section since it's multithreaded
    pthread_mutex_lock(&signal_received_usr1_lock);
    num_signals_received_usr1 += 1;
    pthread_mutex_unlock(&signal_received_usr1_lock);
    // Sio_puts("IN type 1\n");

    Sigprocmask(SIG_SETMASK, &prev_mask, NULL); //Restore signals
}

void handle_signal_usr2_thread(int sig)
{
    // Sio_putl(pthread_self());
    sigset_t mask, prev_mask;
    Sigemptyset(&mask);
    Sigaddset(&mask, SIGUSR1);                 //Add SIGUSR2 to blocked signals
    Sigprocmask(SIG_BLOCK, &mask, &prev_mask); //Block signals

    //perform critical section since it's multithreaded
    pthread_mutex_lock(&signal_received_usr2_lock);
    num_signals_received_usr2 += 1;
    pthread_mutex_unlock(&signal_received_usr2_lock);
    // Sio_puts("IN type 2\n");

    Sigprocmask(SIG_SETMASK, &prev_mask, NULL); //Restore signals
}

void ignore_signals()
{
    Signal(SIGUSR1, SIG_IGN);
    Signal(SIGUSR2, SIG_IGN);
}

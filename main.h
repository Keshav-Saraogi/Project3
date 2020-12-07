#define MAX_RUNTIME_IN_SEC 30
// #define MAX_RUNTIME_IN_SEC 2
#define MAX_SIGNALS 100000 //Total number of signals to be generated
// #define MAX_SIGNALS 100
#define NUM_SIGNAL_GENERATORS 3
#define NUM_SIGNAL_RECEIVERS 4
#define RANDOM_SEEDER 3
struct ProcessArgs
{
    //Shared counters (race-prone)
    int num_signals_sent_usr1;
    int num_signals_sent_usr2;
    volatile int num_signals_received_usr1; //Accessed inside signal handler
    volatile int num_signals_received_usr2;
    int total_signal_count;
    useconds_t execution_time_in_us;
    int reporter_counter;
};
void generate_signals(struct ProcessArgs *shm_ptr);
void *receive_signal_usr1_thread(void *arg);
void *receive_signal_usr2_thread(void *arg);
void receive_signal_usr1_process();
void receive_signal_usr2_process();
void handler_signal_usr1(int sig);
void handler_signal_usr2(int sig);
void *generate_signals_thread(void *args);
void handle_signal_usr1_thread(int sig);
void handle_signal_usr2_thread(int sig);
void *reporter_thread_fn(void *args);
void reporter();
void ignore_signals();

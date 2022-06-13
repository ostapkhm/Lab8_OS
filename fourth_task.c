#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

pthread_mutex_t mutex_sum;
pthread_mutex_t mutex_write_read;

int threads_amount;
bool was_interrupted = false;
unsigned long long int max_iter = 0;
unsigned long long int *iteration_counter_arr = NULL;
unsigned long long int iterations_per_check = 10;
double res_pi = 0;


void *partial_sum(void *arg){
    //Assume that every thread should sum such terms which series_idxes differ by threads_amount
    // idx - idx of current thread

    int idx = *(int *)arg;

    double current_sum = 0;
    int series_idx;
    int sign;
    bool terminating = false;

    sigset_t set;
    int ret_code;

    // SIGINT initialization
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    ret_code = pthread_sigmask(SIG_BLOCK, &set, NULL);
    if (ret_code != 0) {
        perror("Fail sigmask");
        exit(EXIT_FAILURE);
    }


    int i = 0;
    while(!terminating){
        series_idx = idx + threads_amount * i;
        if(series_idx % 2 == 0) sign = 1; else sign = -1;
        current_sum += sign * (1.0 / (2 * series_idx + 1));

        iteration_counter_arr[idx]++;

        if((i % iterations_per_check) == 0){
            // Check if user trying to interrupt other threads
            pthread_mutex_lock(&mutex_write_read);
            if(was_interrupted){
                terminating = true;
            }
            pthread_mutex_unlock(&mutex_write_read);
        }
        i++;
    }

    // After interrupting finish summing rest terms as to increase precise
    for(; i < max_iter; i++){
        series_idx = idx + threads_amount * i;
        if(series_idx % 2 == 0) sign = 1; else sign = -1;
        current_sum += sign * (1.0 / (2 * series_idx + 1));
    }

    pthread_mutex_lock(&mutex_sum);
    res_pi += current_sum;
    pthread_mutex_unlock(&mutex_sum);
}


void handle_sigint() {
    max_iter = 0;
    pthread_mutex_lock(&mutex_write_read);

    // Find max iterations
    for (int i = 0; i < threads_amount; i++) {
        if (iteration_counter_arr[i] > max_iter) {
            max_iter = iteration_counter_arr[i];
        }
    }
    was_interrupted = true;
    pthread_mutex_unlock(&mutex_write_read);
}

int main(int argc, char *argv[]) {
    // Parse arguments
    if(argc != 2){
        printf("Invalid input! Usage - %s threads_amount\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    else{
        threads_amount = atoi(argv[1]);
    }

    // Setting handler
    if (SIG_ERR == signal(SIGINT, handle_sigint)) {
        perror("Fail signal");
        exit(EXIT_FAILURE);
    }

    pthread_t th[threads_amount];
    int ids[threads_amount];
    iteration_counter_arr = (unsigned long long int*)calloc(threads_amount, sizeof(unsigned long long));

    // Creating threads
    for(int i = 0; i < threads_amount; i++){
        ids[i] = i;
        if(pthread_create(&th[i], NULL, &partial_sum, &ids[i]) != 0){
            perror("Failed to create thread!\n");
            exit(EXIT_FAILURE);
        }
    }

    // Joining threads
    for(int i = 0; i < threads_amount; i++){
        if(pthread_join(th[i], NULL) != 0){
            perror("Failed to join thread\n");
            exit(EXIT_FAILURE);
        }
    }

    printf("\nCalculated pi = %.16f\n", res_pi * 4.0);
    printf("Iteration were made -> %lld\n", max_iter * threads_amount);

    free(iteration_counter_arr);
    return 0;
}
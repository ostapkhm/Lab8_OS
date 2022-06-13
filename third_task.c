#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdbool.h>

int buffer_size;
int producers_num;
int consumers_num;


// start_word included in words_amount
struct producer_struct{
    char *file_name;
    int start_word_idx;
    int words_amount;
    int poison_pills_amount;
};

sem_t sem_empty;
sem_t sem_full;
sem_t sem_lock;

char **buffer;
int count = 0;

void push(char *string){
    for(int i = count; i > 0; i--){
        buffer[i] = buffer[i - 1];
    }
    buffer[0] = string;
    count++;
}

int get_words_amount(char *file_name){
    FILE* file = fopen(file_name, "r");

    if(!file){
        printf("Failed to open this file!\n");
        exit(EXIT_FAILURE);
    }

    int words_amount = 0;
    char ch;
    for(int i = 0; (ch = getc(file)) != EOF; i++){
        if(ch == 32 || ch == 10){
            words_amount++;
        }
    }

    fclose(file);
    return words_amount;
}

int get_file_idx(unsigned int word_idx, char *file_name){
    // return pointer in file from which the word with word_idx starts
    FILE* file = fopen(file_name, "r");

    if(!file){
        printf("Failed to open this file!\n");
        exit(EXIT_FAILURE);
    }

    int words_amount = 0;
    char ch;
    for(int i = 0; (ch = getc(file)) != EOF; i++){
        if(ch == 32 || ch == 10){
            words_amount++;
        }

        if(word_idx == words_amount){
            fclose(file);

            if(word_idx == 0) return 0;
            return i + 1;
        }
    }

    fclose(file);
    return -1;
}

void* producer(void *args) {
    struct producer_struct* producer_info = (struct producer_struct*)args;

    int start_idx = get_file_idx(producer_info->start_word_idx, producer_info->file_name);

    FILE* file = fopen(producer_info->file_name, "r");
    fseek (file , start_idx, SEEK_SET);

    int words_amount = 0;
    int value;
    bool no_words_remain = false;

    while (1) {
        //Produce a word from a file until file ends or exceed a limit of produced words

        char *word = (char*) malloc(1024);
        value = fscanf(file, " %1023s", word);
        words_amount++;
        no_words_remain = (value != 1 || words_amount > producer_info->words_amount);

        if(no_words_remain){
            break;
        }

        //Creating a deadlock
        sem_wait(&sem_lock);
        sem_wait(&sem_empty);
        buffer[count] = word;
        count++;
        sem_post(&sem_lock);
        sem_post(&sem_full);
    }

    fclose(file);

    // Create poison pills
    int poison_pills_amount = 0;
    while(poison_pills_amount < producer_info->poison_pills_amount){
        //Add poison pill to the buffer
        sem_wait(&sem_empty);
        sem_wait(&sem_lock);
        push(NULL);
        sem_post(&sem_lock);
        sem_post(&sem_full);
        poison_pills_amount++;
    }
}

void* consumer(void *args) {
    pthread_t *tid = (pthread_t*)args;
    char *word;
    int words_amount = 0;

    while(1) {
        //Remove word from the buffer
        sem_wait(&sem_full);
        sem_wait(&sem_lock);
        word = buffer[count - 1];
        count--;
        sem_post(&sem_lock);
        sem_post(&sem_empty);

        // If word is a poison pill - kill a thread
        if(word == NULL){
            break;
        }

        //Consume this word, printing it with additional info and deleting it from heap
        printf("%s, TID - %ld\n", word, *tid);
        free(word);
        words_amount++;
    }
}

int main(int argc, char *argv[]) {
    char *file_name;

    if(argc != 5){
        printf("Invalid input! Usage - ./program file_name buffer_size producers_amount consumers_amount\n");
        exit(EXIT_FAILURE);
    }
    else{
        file_name = argv[1];
        buffer_size = atoi(argv[2]);
        producers_num = atoi(argv[3]);
        consumers_num = atoi(argv[4]);
    }

    pthread_t th[producers_num + consumers_num];
    buffer = (char **) (char *) malloc(buffer_size);

    sem_init(&sem_empty, 0, buffer_size);
    sem_init(&sem_full, 0, 0);
    sem_init(&sem_lock, 0, 1);

    struct producer_struct producers_struct[producers_num];

    int words_amount = get_words_amount(file_name);

    //Creating producers threads depending on amount of words in file and on amount of consumers
    int words_amount_for_producer = words_amount / producers_num;
    int poison_pills_amount_for_producer = consumers_num / producers_num;

    for(int i = 0; i < producers_num; i++){
        producers_struct[i].file_name = file_name;
        producers_struct[i].start_word_idx = i * words_amount_for_producer;
        producers_struct[i].words_amount = words_amount_for_producer;
        producers_struct[i].poison_pills_amount = poison_pills_amount_for_producer;

        if(i == producers_num - 1){
            producers_struct[i].words_amount += (words_amount - words_amount_for_producer * producers_num);
            producers_struct[i].poison_pills_amount += (consumers_num - poison_pills_amount_for_producer * producers_num);
        }

        if(pthread_create(&th[i], NULL, &producer, &producers_struct[i]) != 0){
            perror("Failed to create producers' thread");
            exit(EXIT_FAILURE);
        }
    }

    //Creating consumers threads
    for(int i = producers_num; i < consumers_num + producers_num; i++){
        if(pthread_create(&th[i], NULL, &consumer, &th[i]) != 0){
            perror("Failed to create consumers' thread");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i < consumers_num + producers_num; i++){
        if(pthread_join(th[i], NULL) != 0){
            perror("Failed to join thread");
            exit(EXIT_FAILURE);
        }
    }

    sem_destroy(&sem_empty);
    sem_destroy(&sem_full);
    sem_destroy(&sem_lock);

    free(buffer);
    return 0;
}
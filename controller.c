
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>

#define SHM_NAME "/ball_control"
#define SHM_SIZE 12

typedef struct {
    float height;
    float speed;
    float reference;
} BallData;

BallData *data;
sem_t tick_sem;

void alarm_handler(int sig) {
    (void)sig;
    sem_post(&tick_sem);
}

void *input_thread(void *arg) {
    (void)arg;
    float new_reference;

    while (1) {
        printf("Enter reference height (0-50): ");
        fflush(stdout);

        if (scanf("%f", &new_reference) != 1)
            break;

        if (new_reference >= 0.0f &&
            new_reference <= 50.0f) {
            data->reference = new_reference;
            printf("New reference: %.2f cm\n",
                   new_reference);
        } else {
            printf("Invalid height.\n");
        }
    }

    return NULL;
}

void *control_thread(void *arg) {
    (void)arg;

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);

    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    const float Kp = 0.04f;
    const float bias = 25.0f / 35.0f;

    while (1) {
        while (sem_wait(&tick_sem) == -1) {
            if (errno != EINTR) {
                perror("sem_wait");
                return NULL;
            }
        }

        float height = data->height;
        float reference = data->reference;

        float error = reference - height;
        float speed = bias + Kp * error;

        if (speed > 1.0f)
            speed = 1.0f;

        if (speed < 0.0f)
            speed = 0.0f;

        data->speed = speed;

        printf("Height: %.2f | Ref: %.2f | Fan: %.2f\n",
               height, reference, speed);
    }

    return NULL;
}

int main(void) {
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);

    if (fd == -1) {
        perror("shm_open");
        return 1;
    }

    data = mmap(NULL, SHM_SIZE,
                PROT_READ | PROT_WRITE,
                MAP_SHARED, fd, 0);

    if (data == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    close(fd);

    if (sem_init(&tick_sem, 0, 0) == -1) {
        perror("sem_init");
        return 1;
    }

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);

    pthread_sigmask(SIG_BLOCK, &set, NULL);

    struct sigaction sa = {0};
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    pthread_t input, control;

    pthread_create(&input, NULL, input_thread, NULL);
    pthread_create(&control, NULL, control_thread, NULL);

    struct itimerval timer = {0};

    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 100000;

    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 100000;

    setitimer(ITIMER_REAL, &timer, NULL);

    pthread_join(input, NULL);
    pthread_join(control, NULL);

    return 0;
}


#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

volatile sig_atomic_t ticks = 0;

void handler(int signo) {
    // 1. Record that SIGALRM arrived
    if (signo == SIGALRM) {
        ticks++;
    }
}

int main() {

    struct sigaction action = {0};
    action.sa_handler = handler;

    // 2. Initialize signal mask and register handler
    sigemptyset(&action.sa_mask);
    sigaction(SIGALRM, &action, NULL);

    timer_t timer_id;

    // 3. Create the timer
    timer_create(CLOCK_MONOTONIC, NULL, &timer_id);

    struct itimerspec timer = {0};

    // 4. Configure initial expiration: 1 second
    timer.it_value.tv_sec = 1;
    timer.it_value.tv_nsec = 0;

    // 5. Configure repetition interval: 1 second
    timer.it_interval.tv_sec = 1;
    timer.it_interval.tv_nsec = 0;

    // 6. Arm the timer
    timer_settime(timer_id, 0, &timer, NULL);

    // 7. Observe and print timer expirations
    sig_atomic_t last_tick = 0;

    while (last_tick < 5) {
        if (ticks != last_tick) {
            last_tick = ticks;
            printf("Timer went off! Tick: %d\n",
                   (int)last_tick);
        }
    }

    timer_delete(timer_id);

    return 0;
}

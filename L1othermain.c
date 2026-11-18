#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define NUM_CORES 3
int target_cpus[NUM_CORES] = {4, 5, 6};
volatile int target_loads[NUM_CORES] = {15, 50, 5};  // Initial percentages
volatile int running = 1;

// Worker function with variable duty cycle to simulate changing CPU percentages
void* load_worker(void* arg) {
  int index = *((int*)arg);
  int core_id = target_cpus[index];

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
    perror("Failed to set load generator affinity");
  }

  while (running) {
    int load = target_loads[index];  // e.g., 15, 50, or 5 percent load
    if (load > 0) {
      // Active work period proportional to load percentage (out of 100ms cycle)
      int work_time = load * 1000;           // microseconds
      int sleep_time = (100 - load) * 1000;  // microseconds

      clock_t start = clock();
      while ((clock() - start) * 1000000 / CLOCKS_PER_SEC < work_time);
      usleep(sleep_time);
    } else {
      usleep(100000);
    }
  }
  return NULL;
}

// Controller thread that changes load targets randomly every 2 seconds
void* load_controller(void* arg) {
  while (running) {
    sleep(5);
    // Randomly assign target percentages among 5%, 15%, 50%, 80%
    int possible_loads[] = {5, 15, 50, 80};
    for (int i = 0; i < NUM_CORES; i++) {
      target_loads[i] = possible_loads[rand() % 4];
    }
    printf(
        "[Load Gen Controller] Updated CPU loads -> CPU 4: %d%%, CPU 5: %d%%, "
        "CPU 6: %d%%\n",
        target_loads[0], target_loads[1], target_loads[2]);
  }
  return NULL;
}

int main() {
  srand(time(NULL));
  pthread_t load_threads[NUM_CORES];
  pthread_t controller_thread;
  int indices[NUM_CORES];

  printf(
      "[Load Gen] Starting dynamic variable load generator on CPUs 4, 5, "
      "6...\n");

  for (int i = 0; i < NUM_CORES; i++) {
    indices[i] = i;
    pthread_create(&load_threads[i], NULL, load_worker, &indices[i]);
  }
  pthread_create(&controller_thread, NULL, load_controller, NULL);

  sleep(40);  // Run load generation for 12 seconds

  running = 0;
  for (int i = 0; i < NUM_CORES; i++) {
    pthread_join(load_threads[i], NULL);
  }
  pthread_join(controller_thread, NULL);

  printf("[Load Gen] Terminating load generator.\n");
  return 0;
}
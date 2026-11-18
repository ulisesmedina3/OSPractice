#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int shared_array[20] = {0};


// Internal helper struct used only within the function to read raw ticks
typedef struct {
  unsigned long long user;
  unsigned long long nice;
  unsigned long long system;
  unsigned long long idle;
} cpu_ticks_t;

// Helper function to read raw ticks once from /proc/stat
cpu_ticks_t read_cpu_ticks(int cpu_num) {
  cpu_ticks_t ticks = {0, 0, 0, 0};
  FILE* fp = fopen("/proc/stat", "r");
  if (!fp) return ticks;

  char line[256];
  char target_cpu_str[16];
  snprintf(target_cpu_str, sizeof(target_cpu_str), "cpu%d", cpu_num);

  while (fgets(line, sizeof(line), fp)) {
    if (strncmp(line, target_cpu_str, strlen(target_cpu_str)) == 0 &&
        line[strlen(target_cpu_str)] == ' ') {
      sscanf(line, "%*s %llu %llu %llu %llu", &ticks.user, &ticks.nice,
             &ticks.system, &ticks.idle);
      break;
    }
  }
  fclose(fp);
  return ticks;
}

// Required function: computes and returns the current CPU usage percentage via
// delta sampling
double get_cpu_usage_percentage(int cpu_num) {
  // Take sample 1
  cpu_ticks_t t1 = read_cpu_ticks(cpu_num);

  // Wait for a short sampling window (e.g., 200 milliseconds)
  usleep(200000);

  // Take sample 2
  cpu_ticks_t t2 = read_cpu_ticks(cpu_num);

  unsigned long long total1 = t1.user + t1.nice + t1.system + t1.idle;
  unsigned long long active1 = t1.user + t1.nice + t1.system;

  unsigned long long total2 = t2.user + t2.nice + t2.system + t2.idle;
  unsigned long long active2 = t2.user + t2.nice + t2.system;

  unsigned long long total_delta = total2 - total1;
  unsigned long long active_delta = active2 - active1;

  if (total_delta == 0) return 0.0;

  // Return real-time percentage
  return ((double)active_delta / (double)total_delta) * 100.0;
}


int main() {
  
  return 0;
}
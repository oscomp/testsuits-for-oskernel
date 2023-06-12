#include<stdio.h>
#include<stdlib.h>
#include<time.h>

int main(void) {
   struct timespec time_start={0, 0},time_end={0, 0};
   clock_gettime(CLOCK_REALTIME, &time_start);
   printf("start time %lus,%lu ns\n", time_start.tv_sec, time_start.tv_nsec);
   for (int i = 1; i > 0; i++) {
        continue;
   }
   clock_gettime(CLOCK_REALTIME, &time_end);
   printf("end   time %lus,%lu ns\n", time_end.tv_sec, time_end.tv_nsec);
   printf("duration:%lus %luns\n", time_end.tv_sec-time_start.tv_sec, time_end.tv_nsec-time_start.tv_nsec);
   return 0;
}
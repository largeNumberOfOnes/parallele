/*
 * This program examines the various thread attributes of
 *                                              the pthread library.
 */

#include <stdio.h>
#include <pthread.h>
#include <errno.h>

void print_sched_policy(int policy) {
    switch(policy) {
        case SCHED_FIFO:  printf("SCHED_FIFO"); break;
        case SCHED_RR:    printf("SCHED_RR"); break;
        case SCHED_OTHER: printf("SCHED_OTHER"); break;
        default:          printf("Unknown (%d)", policy);
    }
}

int main() {
    pthread_attr_t attr;
    int ret;

    pthread_attr_init(&attr);
    
    int scope;
    pthread_attr_getscope(&attr, &scope);
    printf("Thread scope: %s\n", 
        scope == PTHREAD_SCOPE_SYSTEM ? "PTHREAD_SCOPE_SYSTEM" : 
        scope == PTHREAD_SCOPE_PROCESS ? "PTHREAD_SCOPE_PROCESS" :
        "Unknown"
    );
    
    int detachst;
    pthread_attr_getdetachstate(&attr, &detachst);
    printf("Detach state: %s\n",
        detachst == PTHREAD_CREATE_JOINABLE ? "PTHREAD_CREATE_JOINABLE" :
        detachst == PTHREAD_CREATE_DETACHED ? "PTHREAD_CREATE_DETACHED" :
        "Unknown"
    );

    void* stackaddr;
    size_t stacksize;
    pthread_attr_getstack(&attr, &stackaddr, &stacksize);
    pthread_attr_getstacksize(&attr, &stacksize);
    printf("Stack address: %p\n", stackaddr);
    printf("Stack size: %zu bytes\n", stacksize);
    
    int inheritsched;
    pthread_attr_getinheritsched(&attr, &inheritsched);
    printf("Inherit sched: %s\n",
        inheritsched == PTHREAD_INHERIT_SCHED ? "PTHREAD_INHERIT_SCHED" :
        inheritsched == PTHREAD_EXPLICIT_SCHED ? "PTHREAD_EXPLICIT_SCHED" :
        "Unknown"
    );
    
    int schedpolicy;
    pthread_attr_getschedpolicy(&attr, &schedpolicy);
    printf("Scheduling policy: ");
    print_sched_policy(schedpolicy);
    printf("\n");
    
    struct sched_param param;
    pthread_attr_getschedparam(&attr, &param);
    printf("Scheduling priority: %d\n", param.sched_priority);
    
    pthread_attr_destroy(&attr);
    
    return 0;
}
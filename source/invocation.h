//
// Created by pedro on 17-10-2023.
//

#ifndef SOURCE_INVOCATION_H
#define SOURCE_INVOCATION_H

#include <pthread.h>
#include "types.h"

#define MEGA 1000000

void line_to_invocation(invocation_t * invocation, char *line);
void allocate_invocation(args_t *args);
int getTid(int n_threads, pid_t * thread_ids, pthread_mutex_t * lock);


#endif //SOURCE_INVOCATION_H

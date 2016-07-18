#ifndef PATH_EXEC_H
#define PATH_EXEC_H

#include "hal.h"
#include "sensors.h"

typedef struct PathExecInputs {
    unsigned int drive;
    unsigned int backwards;
    double next_x;
    double next_y;
} PathExecInputs;

typedef struct PathExecLocals {
    hal_time time_entered;
    double start_x;
    double start_y;
    double need_rot;
    double need_dist;
    double normal_x;
    double normal_y;
    int state;
} PathExecLocals;

typedef struct PathExecState {
    PathExecLocals locals;
    unsigned int done;
    unsigned int see_obstacle;
} PathExecState;

void pe_reset(PathExecState* pe);
void pe_step(PathExecInputs* inputs, PathExecState* pe, Sensors* sens);

#endif

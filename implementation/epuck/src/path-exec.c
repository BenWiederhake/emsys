#include <math.h>
#include <stdio.h>

#include "bellman-ford/bellman-ford.h"
#include "hal.h"
#include "log_config.h"
#include "pi.h" /* M_PI */
#include "path-exec.h"
#include "sensors.h"
#include "state-machine-common.h"

/* ===== BEGIN CUSTOM SMC ===== */

#define PE_MOTOR_MV (3)
#define PE_MOTOR_ROT (4)

#define PE_MV_PER_SEC PE_MOTOR_MV
#define PE_ROT_PER_SEC (PE_MOTOR_ROT * 2 / 5.3)

/* ===== END CUSTOM SMC ===== */


#define TOLERANCE_ANGLE (15 * M_PI / 180)

/* If we moved *back* by this much, restart. */
#define PE_PROGRESS_MIN (-1.2)

/* Should be roughly PE_DIST_TOLERANCE + STEPPING_DIST  */
#define PE_DIST_RECOMPUTE (0.3 + inputs->tol + STEPPING_DIST)

#define PE_BACKOUT_DIST 3.0

enum PE_STATES {
    PE_inactive,
    PE_compute,
    PE_rotate, /* turn to */
    PE_rot_wait_for_LPS,
    PE_drive, /* tune in */
    PE_profit, /* and drop out */
    PE_wet_feet /* OH SHIT OH SHIT OH SHI- */
};
/* static_assert(sizeof(enum PE_STATES) == sizeof(PathExecLocals::state),
 *               "Bad things"); */
typedef char check_pe_states_size[
    (sizeof(enum PE_STATES) == sizeof(int)) ? 1 : -1];

void pe_reset(PathExecState* pe) {
    pe->done = 0;
    pe->see_obstacle = 0;
    pe->locals.state = PE_inactive;
    pe->locals.approx_rot_speed = PE_ROT_PER_SEC;
    /* No further initialization needed because PE_inactive doesn't
       read from locals.time_entered or locals.start_* or locals.need_* */
}

#define PE_CRASH_TOLERANCE 1.5

static unsigned int near_crash_p(PathExecInputs* inputs, Sensors* sens) {
    if (inputs->backwards) {
        return sens->proximity[PROXIMITY_M_150] < PE_CRASH_TOLERANCE
            || sens->proximity[PROXIMITY_P_150] < PE_CRASH_TOLERANCE;
    } else {
        return sens->proximity[PROXIMITY_M_20] < PE_CRASH_TOLERANCE
            || sens->proximity[PROXIMITY_P_20] < PE_CRASH_TOLERANCE
            || sens->proximity[PROXIMITY_M_45] < PE_CRASH_TOLERANCE
            || sens->proximity[PROXIMITY_P_45] < PE_CRASH_TOLERANCE;
    }
}

void pe_step(PathExecInputs* inputs, PathExecState* pe, Sensors* sens) {
    double progress, dist;
    PathExecLocals* l;

    l = &pe->locals;

    if (!inputs->drive) {
        /* We just got notified that we shouldn't be running anymore. */
        pe_reset(pe);
        /* It's okay to 'halt()' many times per second. */
        smc_halt();
        /* Don't set 'time_entered'; not needed. */
        return;
    }

    progress = inputs->next.x - sens->current.x;
    dist = inputs->next.y - sens->current.y;
    dist = sqrt(dist * dist + progress * progress);
    if (dist >= PE_DIST_RECOMPUTE) {
        /* Trigger re-pathing. */
        #ifdef LOG_TRANSITIONS_PATH_EXEC
        hal_print("PE: too far away!");
        #endif
        smc_halt();
        l->state = PE_profit;
        pe->see_obstacle = 1;
        return;
    }

    switch (l->state) {
    case PE_inactive:
        if(inputs->drive) {
            l->state = PE_compute;
        }
        break;
    case PE_compute:
        {
            double start_dir;
            #ifdef LOG_TRANSITIONS_PATH_EXEC
            hal_print("PE:compute");
            #endif
            l->start.x = sens->current.x;
            l->start.y = sens->current.y;
            start_dir = sens->current.phi;
            l->dst_dir = atan2(inputs->next.y - l->start.y,
                               inputs->next.x - l->start.x);
            if(inputs->backwards) {
                l->dst_dir += M_PI;
            }
            l->init_dir = start_dir;
            /* Rotation in radians: */
            l->need_rot = angle_dist(l->dst_dir, start_dir);

            /* Code from the transitions */
            l->state = PE_rotate;
            l->time_entered = hal_get_time();
            if (l->need_rot < -TOLERANCE_ANGLE) {
                l->need_rot *= -1;
                hal_set_speed(PE_MOTOR_ROT, -PE_MOTOR_ROT);
                #ifdef LOG_TRANSITIONS_PATH_EXEC
                hal_print("pe:rot right");
                #endif
            } else if (l->need_rot > TOLERANCE_ANGLE) {
                hal_set_speed(-PE_MOTOR_ROT, PE_MOTOR_ROT);
                #ifdef LOG_TRANSITIONS_PATH_EXEC
                hal_print("pe:rot left");
                #endif
            } else {
                #ifdef LOG_TRANSITIONS_PATH_EXEC
                hal_print("pe:no rotate");
                #endif
                l->state = PE_drive;
                if (inputs->backwards) {
                    hal_set_speed(-PE_MOTOR_MV, -PE_MOTOR_MV);
                } else {
                    hal_set_speed(PE_MOTOR_MV, PE_MOTOR_MV);
                }
            }

            /* Rotation in seconds: */
            l->need_rot /= l->approx_rot_speed;
            l->normal.x = -(inputs->next.y - l->start.y);
            l->normal.y =   inputs->next.x - l->start.x;
            l->need_dist = l->normal.x * l->normal.x + l->normal.y * l->normal.y;
            l->need_dist = sqrt(l->need_dist);
            l->normal.x /= l->need_dist;
            l->normal.y /= l->need_dist;
            l->need_dist /= PE_MV_PER_SEC;
        }
        break;
    case PE_rotate:
         if (smc_time_passed_p(l->time_entered, l->need_rot)
                || fabs(angle_dist(l->dst_dir, sens->current.phi)) <= TOLERANCE_ANGLE) {
            #ifdef LOG_TRANSITIONS_PATH_EXEC
            hal_print("pe:done rotate");
            #endif
            l->need_rot = hal_get_time() - l->time_entered;
            smc_halt();
            l->time_entered = hal_get_time();
            l->state = PE_rot_wait_for_LPS;
        }
        break;
    case PE_rot_wait_for_LPS:
        if(smc_time_passed_p(l->time_entered, 2.2)) {
            double phi_diff = fabs(angle_dist(l->dst_dir, sens->current.phi));
            if (phi_diff <= TOLERANCE_ANGLE) {
                l->state = PE_drive;
                l->time_entered = hal_get_time();
                if (inputs->backwards) {
                    hal_set_speed(-PE_MOTOR_MV, -PE_MOTOR_MV);
                } else {
                    hal_set_speed(PE_MOTOR_MV, PE_MOTOR_MV);
                }
            } else {
                double actually_rotated_per_sec, actually_rotated;
                actually_rotated = fabs(angle_dist(sens->current.phi, l->init_dir));
                actually_rotated_per_sec = actually_rotated / l->need_rot;
                #ifdef LOG_TRANSITIONS_PATH_EXEC
                sprintf(hal_get_printbuf(), "PE:rerotate,time=%.2f,phi_diff=%.2f,rotted=%.2f",
                    l->need_rot, fabs(angle_dist(l->dst_dir, sens->current.phi)),
                    actually_rotated);
                hal_print(hal_get_printbuf());
                sprintf(hal_get_printbuf(), "PE:...,oldspeed=%.2f,newspeed=%.2f",
                    l->approx_rot_speed, actually_rotated_per_sec);
                hal_print(hal_get_printbuf());
                #endif
                l->approx_rot_speed = actually_rotated_per_sec;
                l->state = PE_compute;
            }
        }
        break;
    case PE_drive:
        if (near_crash_p(inputs, sens)) {
            #ifdef LOG_TRANSITIONS_PATH_EXEC
            hal_print("PE:nearcrash");
            #endif
            smc_move_back();
            l->time_entered = hal_get_time();
            l->state = PE_wet_feet;
            break;
        }
        {
            double stray = 0;
            stray += (inputs->next.x - sens->current.x) * l->normal.x;
            stray += (inputs->next.y - sens->current.y) * l->normal.y;
            stray = fabs(stray);
            if (stray >= inputs->tol / 2.0) {
                #ifdef LOG_TRANSITIONS_PATH_EXEC
                hal_print("PE:strayed->restart");
                #endif
                /* Whoopsie daisy. */
                l->state = PE_compute;
                smc_halt();
                break;
            }

            progress = 0;
            progress += -(inputs->next.x - sens->current.x) * l->normal.y;
            progress +=  (inputs->next.y - sens->current.y) * l->normal.x;
            progress = fabs(progress);
            if (progress <= PE_PROGRESS_MIN) {
                /* Whoopsie daisy. */
                #ifdef LOG_TRANSITIONS_PATH_EXEC
                hal_print("PE:went back->restart");
                #endif
                l->state = PE_compute;
                smc_halt();
                break;
            }
            if (dist < inputs->tol) {
                l->state = PE_profit;
                #ifdef LOG_TRANSITIONS_PATH_EXEC
                hal_print("PE:done step");
                #endif
                smc_halt();
                pe->done = 1;
            } else if (smc_time_passed_p(l->time_entered, l->need_dist)) {
                #ifdef LOG_TRANSITIONS_PATH_EXEC
                hal_print("PE:too far away->restart PE");
                #endif
                l->state = PE_compute;
                smc_halt();
            }
        }
        break;
    case PE_wet_feet:
        if (smc_time_passed_p(l->time_entered, PE_BACKOUT_DIST / SMC_MV_PER_SEC)) {
            #ifdef LOG_TRANSITIONS_PATH_EXEC
            hal_print("PE:backed outta it");
            #endif
            pe->see_obstacle = 1;
        }
        break;
    case PE_profit:
        /* Nothing to do here. */
        break;
    default:
        /* Uhh */
        hal_print("PathExec illegal state?!  Resetting ...");
        assert(0);
        pe_reset(pe);
        smc_halt();
        break;
    }
}

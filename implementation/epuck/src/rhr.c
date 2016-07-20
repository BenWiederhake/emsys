/*
 * Right-Hand Follower (Right Hand Rule)
 */

#include "hal.h"
#include "pi.h" /* M_PI */
#include "rhr.h"
#include "state-machine-common.h"

enum RHR_STATES {
    RHR_check_wall, /* I was tempted to call this "czech wall" */
    RHR_find_wall,
    RHR_wall_wait,
    RHR_wall_orient,
    RHR_avoid_coll,
    RHR_claustrophobia,
    RHR_follow,
    RHR_go_on,
    RHR_stay_close,
    RHR_run_close
};
/* static_assert(sizeof(enum RHR_STATES) == sizeof(RhrState::state),
 *               "Bad things"); */
typedef char check_rhr_states_size[
    (sizeof(enum RHR_STATES) == sizeof(int)) ? 1 : -1];

static const double RHR_CONF_CORNER_D = 9.5;
static const double RHR_CONF_CORNER_X = 11;
static const double RHR_CONF_WALL_THRESH = 2;
static const double RHR_CONF_WALL_D = 1;
static const double RHR_CONF_STROKE_THRESH = 1.8;

void rhr_reset(RhrState* rhr) {
    rhr->state = RHR_check_wall;
    /* No further initialization needed because RHR_check_wall doesn't
       read from rhr->time_entered */
}

static void find_wall(RhrState* rhr, Sensors* sens) {
    /* see virt_proto/software/find_wall.m */
    static const unsigned int order[NUM_PROXIMITY] =
        {PROXIMITY_M_20, PROXIMITY_M_45, PROXIMITY_P_20, PROXIMITY_M_90,
         PROXIMITY_P_45, PROXIMITY_M_150, PROXIMITY_P_90, PROXIMITY_P_150};
    /* rot_angles = prox_sensor_angle - (-20); */
    unsigned i;

    assert(sizeof(prox_sensor_angle) / sizeof(prox_sensor_angle[0])
            == NUM_PROXIMITY);
    assert(sizeof(sens->proximity) / sizeof(sens->proximity[0])
            == NUM_PROXIMITY);

    rhr->wall_p = 0;
    for(i = 0; i < 8; ++i) {
        unsigned int s = order[i];
        if (sens->ir[s] <= SMC_SENSE_TOL) {
            rhr->wall_p = 1;
            /* rot_angles[s] */
            rhr->wall_rot = prox_sensor_angle[s] - prox_sensor_angle[PROXIMITY_M_20];
            return;
        }
    }
}

void rhr_step(RhrState* rhr, Sensors* sens) {
    const int old_state = rhr->state;
    const int feel_stroke = sens->proximity[PROXIMITY_P_20] <= RHR_CONF_STROKE_THRESH;
    switch (rhr->state) {
    case RHR_check_wall:
        find_wall(rhr, sens);
        if (!rhr->wall_p) {
            rhr->state = RHR_find_wall;
            smc_move();
        } else {
            if (rhr->wall_rot < 0) {
                smc_rot_right();
                rhr->wall_rot = -rhr->wall_rot;
            } else {
                smc_rot_left();
            }
            rhr->state = RHR_wall_wait;
        }
        break;
    case RHR_find_wall:
        if (sens->proximity[PROXIMITY_M_20] <= SMC_SENSE_TOL
            || sens->proximity[PROXIMITY_M_45] <= SMC_SENSE_TOL
            || sens->proximity[PROXIMITY_P_20] <= SMC_SENSE_TOL
            || sens->proximity[PROXIMITY_P_45] <= SMC_SENSE_TOL) {
            rhr->state = RHR_check_wall;
            /* Keep going (don't change motors) */
            break;
        }
        break;
    case RHR_wall_wait:
        if (smc_time_passed_p(rhr->time_entered, rhr->wall_rot / SMC_ROT_PER_SEC)) {
            rhr->state = RHR_wall_orient;
            smc_move();
        }
        break;
    case RHR_wall_orient:
        if (sens->proximity[PROXIMITY_M_45] <= SMC_SENSE_TOL
            || feel_stroke) {
            rhr->state = RHR_avoid_coll;
            smc_rot_left();
        }
        break;
    case RHR_avoid_coll:
        if (sens->proximity[PROXIMITY_M_90] <= SMC_SENSE_TOL) {
            rhr->state = RHR_claustrophobia;
            smc_rot_left();
        } else if (sens->proximity[PROXIMITY_M_45] > SMC_SENSE_TOL
                   && smc_time_passed_p(rhr->time_entered, 5 * SMC_SECS_PER_DEGREE)) {
            rhr->state = RHR_wall_orient;
            smc_move();
        } else if (smc_time_passed_p(rhr->time_entered, 360 * SMC_SECS_PER_DEGREE)) {
            /* After a whole turn, we have lost the wall apparently */
            rhr->state = RHR_run_close;
            smc_move();
        }
        break;
    case RHR_claustrophobia:
        if (sens->proximity[PROXIMITY_M_20] > SMC_SENSE_TOL
            && smc_time_passed_p(rhr->time_entered, 5 * SMC_SECS_PER_DEGREE)
            && !feel_stroke) {
            rhr->state = RHR_follow;
            smc_move();
        }
        break;
    case RHR_follow:
        {
            int waited_long_enough;
            int have_20, have_90;
            have_20 = sens->proximity[PROXIMITY_M_20] <= SMC_SENSE_TOL;
            have_90 = sens->proximity[PROXIMITY_M_90] <= RHR_CONF_WALL_THRESH;
            waited_long_enough = smc_time_passed_p(rhr->time_entered, RHR_CONF_WALL_D / SMC_MV_PER_SEC);
            if(have_20 || (have_90 && waited_long_enough)) {
                rhr->state = RHR_claustrophobia;
                smc_rot_left();
            } else if (sens->proximity[PROXIMITY_M_45] > SMC_SENSE_TOL) {
                rhr->state = RHR_go_on;
            }
        }
        break;
    case RHR_go_on:
        if (sens->proximity[PROXIMITY_M_20] <= SMC_SENSE_TOL) {
            rhr->state = RHR_claustrophobia;
            smc_rot_left();
        } else if (sens->proximity[PROXIMITY_M_45] <= SMC_SENSE_TOL) {
            rhr->state = RHR_follow;
            /* Should already be moving, but be extra sure. */
            smc_move();
        } else if (smc_time_passed_p(rhr->time_entered, RHR_CONF_CORNER_D / SMC_MV_PER_SEC)) {
            rhr->state = RHR_stay_close;
            smc_rot_right();
        }
        break;
    case RHR_stay_close:
        if (smc_time_passed_p(rhr->time_entered, 70 * SMC_SECS_PER_DEGREE)) {
            rhr->state = RHR_run_close;
            smc_move();
        } else if (feel_stroke) {
            rhr->state = RHR_avoid_coll;
            smc_rot_left();
        }
        break;
    case RHR_run_close:
        if (sens->proximity[PROXIMITY_M_20] <= SMC_SENSE_TOL) {
            rhr->state = RHR_wall_orient;
            smc_move();
        } else if (sens->proximity[PROXIMITY_M_45] > SMC_SENSE_TOL
                   && smc_time_passed_p(rhr->time_entered, RHR_CONF_CORNER_X / SMC_MV_PER_SEC)) {
            rhr->state = RHR_stay_close;
            smc_rot_right();
        } else if (feel_stroke) {
            rhr->state = RHR_avoid_coll;
            smc_rot_left();
        }
        break;
    default:
        /* Uhh */
        hal_print("RHR illegal state?!  Resetting ...");
        assert(0);
        rhr_reset(rhr);
        hal_set_speed(0, 0);
        break;
    }

    if (rhr->state != old_state) {
        rhr->time_entered = hal_get_time();
    }
}

/*
 * Matlab Hardware Abstraction Layer
 *
 *
 * Communication Subsystem Example:
 *
 *  void hal_send_example(void) {
 *      unsigned int number;
 *      MatlabBot* bot = current;
 *      if (bot->com != NULL) {
 *          for (number = 0; number < bot->com->length; number++) {
 *              // select bot to send data to
 *              matlab_select_bot((long) bot->com->bots[number], (long) bot->com);
 *              // TODO: call appropriate receive function directly
 *          }
 *      }
 *      // select sending bot
 *      matlab_select_bot((long) bot, (long) bot->com);
 *  }
 */

#include <assert.h>
#include <math.h> /* Only for "time feedback" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal.h"

#include "matlab.h"
#include "tinbot.h"
#include "t2t-parse.h"

/* Should be the same to check package length during testing. */
#define TIN_PACKAGE_MAX_LENGTH 128

static Mode mode = ALONE;

/* Currently, "TinBot Software > Chart > Explore > debug_info > size" is set to 10 */
typedef char check_debug_array_length_against_matlab[(DEBUG_CAT_NUM <= 10) ? 1 : -1];

static MatlabBot* current;

const double ir_sensor_angle[6] = {
    0*M_PI/3,
    1*M_PI/3,
    2*M_PI/3,
    3*M_PI/3,
    4*M_PI/3,
    5*M_PI/3
};


/* Implementation of hal.h */

hal_time hal_get_time() {
    return (hal_time)(1000 * current->raw_time);
}

void hal_set_speed(double left, double right) {
    current->motor_left = left;
    current->motor_right = right;
}

double hal_get_speed_right(void) {
    return current->motor_right;
}

double hal_get_speed_left(void) {
    return current->motor_left;
}

void hal_set_led(unsigned int led, unsigned int value) {
    (void)led;
    (void)value;
    /* Ignore */
}
void hal_set_front_led(unsigned int value) {
    (void)value;
    /* Ignore */
}

void hal_send_put(char* buf, unsigned int length) {
    /* Two checks in case of overflow. */
    assert(length <= TIN_PACKAGE_MAX_LENGTH);
    assert(current->com_buf_used + length <= TIN_PACKAGE_MAX_LENGTH);
    memcpy(current->com_buf + current->com_buf_used, buf, length);
    current->com_buf_used += length;
}

static void tx_package(MatlabBot* sender, char command, MatlabBot* receiver) {
    int is_ours = 1;
    if (sender != receiver) {
        is_ours = 0;
        matlab_select_bot((long)receiver, 0);
    }
    switch (command) {
        case T2T_COMMAND_HEARTBEAT:
            assert(sender->com_buf_used == 0);
            t2t_parse_heartbeat(receiver->tinbot);
            break;
        case T2T_COMMAND_FOUND_PHI:
            assert(sender->com_buf_used == 4);
            if (!is_ours) {
                t2t_parse_found_phi(receiver->tinbot, sender->com_buf, sender->com_buf_used);
            }
            break;
        case T2T_COMMAND_FOUND_XY:
            assert(sender->com_buf_used == 3);
            t2t_parse_found_xy(receiver->tinbot, is_ours, sender->com_buf, sender->com_buf_used);
            break;
        case T2T_COMMAND_UPDATE_MAP:
            assert(sender->com_buf_used == 66);
            t2t_parse_update_map(receiver->tinbot, is_ours, sender->com_buf, sender->com_buf_used);
            break;
        case T2T_COMMAND_DOCKED:
            assert(sender->com_buf_used == 0);
            if (!is_ours) {
                t2t_parse_docked(receiver->tinbot);
            }
            break;
        case T2T_COMMAND_COMPLETED:
            assert(sender->com_buf_used == 0);
            t2t_parse_completed(receiver->tinbot);
            break;
        default:
            /* Ignore ... -ish. */
            {
                char buf[1000];
                sprintf(buf, "hal_matlab.c: tx_package: stray command 0x%02x?!", command);
                hal_print(buf);
            }
            break;
    }
}

void hal_send_done(char command) {
    MatlabBot* real_current;
    MatlabCom* com;
    unsigned int i;

    while (!current->com)
        /* Intentional infinite loop, as assert() is unreliable. */
        ;
    assert(current->com);

    com = current->com;
    real_current = current;
    for (i = 0; i < com->length; ++i) {
        tx_package(real_current, command, com->bots[i]);
    }

    matlab_select_bot((long)real_current, 0);
    current->com_buf_used = 0;
}

void hal_print(const char* message) {
    static int have_fp = 0;
    static FILE* fp = 0;

    /* If it fails, prefer hanging over undefined behavior */
    while (!have_fp) {
        /* You will find this file in the same directory as "setup.m" */
        fp = fopen ("hal_print.txt","w");
        fputs("Open!\n", fp);
        have_fp = (fp != NULL);
    }
    fprintf(fp, "[%8.3f]: %s\n", current->raw_time, message);
    fflush(fp);
}

void hal_debug_out(DebugCategory key, double value) {
    assert(key < DEBUG_CAT_NUM);
    current->debug_info[key] = value;
    /* current->debug_info[DEBUG_CAT_OWN_TIME] = fmod(current->raw_time, 1); */
}


/* Implementation of matlab.h */

#define MAP_WIDTH 100
#define MAP_HEIGHT 100

long matlab_create_bot() {
    MatlabBot* matlab_bot = malloc(sizeof(MatlabBot));
    int i;
    matlab_bot->com = NULL;
    matlab_bot->tinbot = malloc(sizeof(TinBot));
    matlab_bot->map_container.accu = map_heap_alloc(MAP_WIDTH, MAP_HEIGHT);
    matlab_bot->map_container.prox = map_heap_alloc(MAP_PROXIMITY_SIZE, MAP_PROXIMITY_SIZE);
    /* setup() must be able to call hal_* functions.
     * (But not hal_send_* obviously.) */
    matlab_bot->com_buf = NULL;
    matlab_bot->com_buf_used = 0;
    matlab_select_bot((long)matlab_bot, 0);
    matlab_bot->raw_time = 0;
    set_mode(matlab_bot->tinbot, mode);
    setup(matlab_bot->tinbot);
    for (i = 0; i < DEBUG_CAT_NUM; ++i) {
        matlab_bot->debug_info[i] = 0.0 / 0.0;
    }
    /* This buffer will be read by other tinbots, and therefore "needs" to be
     * 2-byte aligned.  However, x86 is pretty permissive, meaning the access
     * will be a bit slower, in the unlikely event that malloc() gives us
     * a 2-bytes-unaligned address. */
    matlab_bot->com_buf = malloc(TIN_PACKAGE_MAX_LENGTH);
    return (long) matlab_bot;
}

void matlab_destroy_bot(long matlab_bot) {
    MatlabBot* bot = (MatlabBot*) matlab_bot;
    map_heap_free(bot->map_container.accu);
    map_heap_free(bot->map_container.prox);
    free(bot->com_buf);
    free(bot->tinbot);
    free(bot);
}

void matlab_select_bot(long matlab_bot, long matlab_com) {
    current = (MatlabBot*) matlab_bot;
    map_heap_container = &(current->map_container);
    if (!current->com && matlab_com != 0) {
        current->com = (MatlabCom*) matlab_com;
        assert(current->com->length < NUM_COMMUNICATION_BOTS);
        current->com->bots[current->com->length] = current;
        current->com->length++;
    }
}

void matlab_loop(void) {
    loop(current->tinbot);
}

double matlab_get_motor_left() {
    return current->motor_left;
}

double matlab_get_motor_right() {
    return current->motor_right;
}

double matlab_get_debug_info(int i) {
    if (i < 0 || i >= DEBUG_CAT_NUM) {
        return 0.0 / 0.0;
    }
    return current->debug_info[i];
}

void matlab_update_proximity(double* proximity) {
    update_proximity(current->tinbot, proximity);
}

void matlab_update_ir(double* ir_raw) {
    int number;
    int ir[6];
    for (number = 0; number < 6; number++) {
        ir[number] = (int) ir_raw[number];
    }
    update_ir(current->tinbot, ir);
}

void matlab_update_lps(double* lps) {
    update_lps(current->tinbot, lps[0], lps[1], lps[2]);
}

void matlab_update_attached(double attached) {
    update_victim_pickup(current->tinbot, attached > 0);
}

void matlab_update_time(double time) {
    current->raw_time = time;
}


long matlab_create_com(void) {
    MatlabCom* matlab_com = malloc(sizeof(MatlabCom));
    matlab_com->length = 0;
    return (long) matlab_com;
}

void matlab_destroy_com(long matlab_com) {
    free((MatlabCom*) matlab_com);
}

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hal.h"
#include "pi.h"

#include "commands.h"

#include "t2t.h"
#include "t2t-parse.h"

#define BUFFER_ALIGN __attribute__ ((aligned(8)))

/* run this function to ensure correct encoding */
void t2t_check_types(void) {
    unsigned char buffer[8] BUFFER_ALIGN;
    /* check type sizes */
    assert(sizeof(float) == 4);
    /* check for little endian encoding */
    ((uint16_t*) buffer)[0] = 0xFF42;
    assert(buffer[0] == 0x42);
    assert(buffer[1] == 0xFF);
}

/* ===== Sending ===== */

void t2t_send_heartbeat(void) {
    hal_send_done(CMD_T2T_HEARTBEAT, 1);
}

void t2t_send_found_phi(double x, double y, double victim_phi) {
    char buffer[4 * 3] BUFFER_ALIGN;
    ((float*) buffer)[0] = (float)x;
    ((float*) buffer)[1] = (float)y;
    ((float*) buffer)[2] = (float)victim_phi;
    hal_send_put(buffer, sizeof(buffer));
    hal_send_done(CMD_T2T_VICTIM_PHI, 1);
}

void send_found_phi(double x, double y, double victim_phi) {
    char buffer[4 * 3] BUFFER_ALIGN;
    ((float*) buffer)[0] = (float)x;
    ((float*) buffer)[1] = (float)y;
    ((float*) buffer)[2] = (float)victim_phi;
    hal_send_put(buffer, sizeof(buffer));
    hal_send_done(CMD_VICTIM_PHI, 0);
}

void t2t_send_found_xy(double x, double y, int iteration) {
    char buffer[2 * 4 + 2] BUFFER_ALIGN;
    ((float*) buffer)[0] = (float) x;
    ((float*) buffer)[1] = (float) y;
    ((int16_t*) buffer)[4] = (int16_t) iteration;
    hal_send_put(buffer, sizeof(buffer));
    hal_send_done(CMD_T2T_VICTIM_XY, 1);
}

void t2t_send_update_map(int x, int y, Map* map) {
    char buffer[2 * 2] BUFFER_ALIGN;
    ((int16_t*) buffer)[0] = (int16_t)x;
    ((int16_t*) buffer)[1] = (int16_t)y;
    hal_send_put(buffer, sizeof(buffer));
    hal_send_put((char*)map_serialize(map), MAP_PROXIMITY_BUF_SIZE);
    hal_send_done(CMD_T2T_UPDATE_MAP, 1);
}

void t2t_send_docked(void) {
    hal_send_done(CMD_T2T_DOCKED, 1);
}

void t2t_send_completed(void) {
    hal_send_done(CMD_T2T_COMPLETED, 1);
}


/* ===== Receiving ===== */

#ifndef NDEBUG
#   ifdef assert
#       undef assert
#   endif
#   define assert(EX) do { \
        if (!(EX)) { \
            t2t_tmp_print(&bot->rx_buffer, "!_" #EX); \
        } \
    } while (0)
#endif

void t2t_parse_heartbeat(TinBot* bot) {
    t2t_receive_heartbeat(bot);
}

void t2t_parse_found_phi(TinBot* bot, char* data, unsigned int length) {
    assert(length == 4 * 3);
    t2t_receive_found_phi(bot, ((float*) data)[0], ((float*) data)[1], ((float*) data)[2]);
}

void t2t_parse_phi_correction(TinBot* bot, char* data, unsigned int length) {
    assert(length == 4 + 2);
    t2t_receive_phi_correction(bot, ((float*) data)[0], ((uint16_t*) (data + 4))[0]);
}

void t2t_parse_found_xy(TinBot* bot, int is_ours, char* data, unsigned int length) {
    float x, y;
    int iteration;

    assert(length == 4 + 4 + 2);
    x = ((float*) data)[0];
    y = ((float*) data)[1];
    iteration = ((int16_t*) data)[4];
    t2t_receive_found_xy(bot, is_ours, x, y, iteration);
}

void t2t_parse_update_map(TinBot* bot, char* data, unsigned int length) {
    assert(length == 4 + MAP_PROXIMITY_BUF_SIZE);
    t2t_receive_update_map(bot, ((int16_t*)data)[0], ((int16_t*)data)[1], map_deserialize((unsigned char*)data + 4));
}

void t2t_parse_docked(TinBot* bot) {
    t2t_receive_docked(bot);
}

void t2t_parse_completed(TinBot* bot) {
    t2t_receive_completed(bot);
}

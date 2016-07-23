/*
 * Hardware Abstraction Layer
 */

#ifndef HAL_H
#define HAL_H

#define BROADCAST_ADDRESS 0xFF

/*
 * TODO: constants for LEDs and ON, OFF
 */

typedef unsigned long hal_time;

hal_time hal_get_time(void);

void hal_set_powersave(unsigned int enabled);

void hal_set_speed(double left, double right);

double hal_get_speed_left(void);
double hal_get_speed_right(void);

void hal_set_led(unsigned int led, unsigned int value);
void hal_set_front_led(unsigned int value);

void hal_send_put(char* buf, unsigned int length);

void hal_set_heartbeat(unsigned int enabled);

void hal_send_done(char command, int is_broadcast);

char* hal_get_printbuf(void);

void hal_print(const char *message);

typedef enum DebugCategory {
    DEBUG_CAT_VD_STATE,
    DEBUG_CAT_VD_VICTIM_FOUND,
    DEBUG_CAT_VD_VICTIM_PHI,
    DEBUG_CAT_VD_GIVE_UP,
    DEBUG_CAT_VD_ON_PERCENTAGE,
    DEBUG_CAT_VD_AVG_ANGLE,
    /* DEBUG_CAT_OWN_TIME, */
    DEBUG_CAT_NUM
} DebugCategory;

void hal_debug_out(DebugCategory key, double value);

void __assert_hal(const char *msg, const char *file, int line);

#undef assert
#define assert(EX) (void)((EX) || (__assert_hal(#EX, __FILE__, __LINE__), 0))

/* For debugging */
typedef struct hal_epuck_motor_wrapper {
    double left, right;
} hal_epuck_motor_wrapper;

#endif

#ifndef SENSORS_H
#define SENSORS_H

enum Proximity_Order {
    /* If you change this, please also change:
     * - rhr.c -> find_wall -> sense_angles */
    PROXIMITY_M_150,
    PROXIMITY_M_90,
    PROXIMITY_M_45,
    PROXIMITY_M_20,
    PROXIMITY_P_20,
    PROXIMITY_P_45,
    PROXIMITY_P_90,
    PROXIMITY_P_150,
    NUM_PROXIMITY
};

enum IR_Order {
    IR_Z_0,
    IR_P_60,
    IR_P_120,
    IR_X_180,
    IR_M_120,
    IR_M_60,
    NUM_IR
};

typedef struct Sensors {
    double proximity[NUM_PROXIMITY];
    int ir[NUM_IR];
    int victim_attached;
    struct {
        double x;
        double y;
        double direction;
    } current;
} Sensors;

#endif
#include <stdio.h>

#include "hal.h"
#include "mock.h"
#include "pi.h"
#include "victim-direction.h"

#define IR_COMPLETION_TIME (16651UL)
#define IR_PASS_TIME ((hal_time)((IR_COMPLETION_TIME*40)/360))
/* Half the "IR pause time", plus the full I2C delay: */
#define IR_DELAY_TIME ((hal_time)(150 / 2 + 50))

static const double true_victim_phi = 350 * M_PI / 180;

static int cyclic_within_p(hal_time begin, hal_time now, hal_time end) {
    /* Victim-direction reads "old" data. */
    if (now < IR_DELAY_TIME) {
        now += IR_COMPLETION_TIME - IR_DELAY_TIME;
    } else {
        now -= IR_DELAY_TIME;
    }
    while (now < begin) {
        /* 'mod' might be faster, but whatever. */
        now += IR_COMPLETION_TIME;
    }
    return now <= end;
}

int main() {
    VDState vds;
    Sensors sens;
    int i;
    hal_time core_time[NUM_IR];
    T2TData_VicFix vdf;

    /* Initialization */
    vd_reset(&vds);
    sens.current.x = 0;
    sens.current.y = 0;
    sens.current.phi = 0;
    for (i = 0; i < NUM_IR; ++i) {
        const double core_angle = true_victim_phi + 2 * M_PI - ir_sensor_angle[i];
        core_time[i] = (hal_time)( core_angle * IR_COMPLETION_TIME / (2 * M_PI) );
    }
    assert(6 == NUM_IR);
    printf("Core times: %07lu %07lu %07lu %07lu %07lu %07lu\n",
        core_time[0], core_time[1], core_time[2], core_time[3], core_time[4], core_time[5]);
    printf("Pass time:  %07lu\n", IR_PASS_TIME);
    tests_mock_expect_set_enabled(0);
    vdf.have_incoming_fix = 0;

    /* Running */
    while (hal_get_time() < 20000) {
        for (i = 0; i < NUM_IR; ++i) {
            sens.ir[i] = cyclic_within_p(core_time[i] - IR_PASS_TIME, hal_get_time(), core_time[i] + IR_PASS_TIME);
        }
        if (fabs(hal_get_speed_left()) > 0.5) {
            sens.current.phi = hal_get_time() * 2 * M_PI / IR_COMPLETION_TIME;
        }
        vd_step(&vdf, &vds, &sens);
        if (hal_get_time() % 4000 == 0) {
            printf("Debug data: @%5.3f %d%d%d%d%d%d state%1d %5.3f@%1d/%1d %5.1f%% g%5.3f\n",
                sens.current.phi,
                sens.ir[0], sens.ir[1], sens.ir[2], sens.ir[3], sens.ir[4], sens.ir[5],
                vds.locals.state,
                vds.victim_phi,
                vds.victim_found,
                vds.give_up,
                vds.locals.counter_on * 100.0 / (NUM_IR * vds.locals.counter_total),
                vds.locals.weighted_sum / vds.locals.counter_on);
        }
        tests_mock_tick(10);
    }
    printf("result: true_phi=%5.3f v_phi=%5.3f have_phi=%1d give_up=%1d\n",
        true_victim_phi,
        vds.victim_phi,
        vds.victim_found,
        vds.give_up);
    if (vds.victim_found || vds.give_up || fabs(true_victim_phi - vds.victim_phi) > 2 * M_PI / 180) {
        printf("\t=> and that's BAD!\n");
        return 1;
    }
    printf("\t=> and that's GOOD!\n");
    free_printbuf();

    return 0;
}

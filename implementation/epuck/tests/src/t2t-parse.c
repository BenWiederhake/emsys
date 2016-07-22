#include <stdio.h>

#include "commands.h"
#include "hal.h"
#include "map_heap.h"
#include "mock.h"
#include "t2t-parse.h"

static void test_heartbeat(void) {
    ExpectPackage pkg = {CMD_T2T_HEARTBEAT, 0, NULL, 1};
    tests_mock_expect_assert_done();

    tests_mock_expect_next(&pkg);
    t2t_send_heartbeat();
    tests_mock_expect_assert_done();
}

/*
 * >>> Commands.T2T_VICTIM_PHI.encode(3.1415, 42.24, -10.22)
 * b'V\x0eI@\xc3\xf5(B\x1f\x85#\xc1'
 */
static unsigned char phi_data1[] = {86, 14, 73, 64, 195, 245, 40, 66, 31, 133, 35, 193};

/*
 * >>> Commands.T2T_VICTIM_PHI.encode(0, 99, 256.001 / 1000.0)
 * b'\x00\x00\x00\x00\x00\x00\xc6B\x90\x12\x83>'
 */
static unsigned char phi_data2[] = {0, 0, 0, 0, 0, 0, 198, 66, 144, 18, 131, 62};

static void test_check_phi1(void) {
    ExpectPackage pkg = {CMD_VICTIM_PHI, sizeof(phi_data1), NULL, 0};
    pkg.data = phi_data1;
    tests_mock_expect_assert_done();

    tests_mock_expect_next(&pkg);
    send_found_phi(3.1415, 42.24, -10.22);
    tests_mock_expect_assert_done();
}

static void test_check_phi2(void) {
    ExpectPackage pkg = {CMD_VICTIM_PHI, sizeof(phi_data2), NULL, 0};
    pkg.data = phi_data2;
    tests_mock_expect_assert_done();

    tests_mock_expect_next(&pkg);
    send_found_phi(0, 99, 256.001 / 1000.0);
    tests_mock_expect_assert_done();
}

static void test_found_phi1(void) {
    ExpectPackage pkg = {CMD_T2T_VICTIM_PHI, sizeof(phi_data1), NULL, 1};
    pkg.data = phi_data1;
    tests_mock_expect_assert_done();

    tests_mock_expect_next(&pkg);
    t2t_send_found_phi(3.1415, 42.24, -10.22);
    tests_mock_expect_assert_done();
}

static void test_found_phi2(void) {
    ExpectPackage pkg = {CMD_T2T_VICTIM_PHI, sizeof(phi_data2), NULL, 1};
    pkg.data = phi_data2;
    tests_mock_expect_assert_done();

    tests_mock_expect_next(&pkg);
    t2t_send_found_phi(0, 99, 256.001 / 1000.0);
    tests_mock_expect_assert_done();
}

static void test_found_xy(void) {
    unsigned char data[4 + 4 + 2] = {0xcd, 0xcc, 0x44, 0x41, 0x66, 0x66, 0x36, 0x42, 0, 1};
    ExpectPackage pkg = {CMD_T2T_VICTIM_XY, sizeof(data), NULL, 1};
    pkg.data = data;
    tests_mock_expect_assert_done();

    tests_mock_expect_next(&pkg);
    t2t_send_found_xy(12.3, 45.6, 256);
    tests_mock_expect_assert_done();
}

static void test_update_map(void) {
    Map* m;
    unsigned char data[4 + MAP_PROXIMITY_BUF_SIZE] = {0};
    ExpectPackage pkg = {CMD_T2T_UPDATE_MAP, sizeof(data), NULL, 1};
    pkg.data = data;
    tests_mock_expect_assert_done();

    data[0] = 99;
    data[1] = 0;
    data[2] = 3;
    data[3] = 0;
    /* Positions (4,3)-(7,3) are stored in the byte at offset 11 of the map. */
    data[4 + 11] = 0x89; /* 0b10 00 10 01 */
    m = map_heap_alloc(MAP_PROXIMITY_SIZE, MAP_PROXIMITY_SIZE);
    map_set_field(m, 4 + 0, 3, 1);
    map_set_field(m, 4 + 1, 3, 2);
    map_set_field(m, 4 + 2, 3, 0);
    map_set_field(m, 4 + 3, 3, 2);

    tests_mock_expect_next(&pkg);
    t2t_send_update_map(99, 3, m);
    tests_mock_expect_assert_done();

    map_heap_free(m);
}

static void test_docked(void) {
    ExpectPackage pkg = {CMD_T2T_DOCKED, 0, NULL, 1};
    tests_mock_expect_assert_done();

    tests_mock_expect_next(&pkg);
    t2t_send_docked();
    tests_mock_expect_assert_done();
}

static void test_completed(void) {
    ExpectPackage pkg = {CMD_T2T_COMPLETED, 0, NULL, 1};
    tests_mock_expect_assert_done();

    tests_mock_expect_next(&pkg);
    t2t_send_completed();
    tests_mock_expect_assert_done();
}

#define RUN(x) x()

int main() {
    RUN(test_heartbeat);
    RUN(test_check_phi1);
    RUN(test_check_phi2);
    RUN(test_found_phi1);
    RUN(test_found_phi2);
    RUN(test_found_xy);
    RUN(test_update_map);
    RUN(test_docked);
    RUN(test_completed);
    printf("\t=> GOOD\n");
    return 0;
}

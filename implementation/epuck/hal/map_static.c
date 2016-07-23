#include "../../../libraries/tinpuck/include/tinpuck/com.h"

#include "map.h"
#include "hal.h"

/* ===== Implementation of map.h, using the static section (.bss) ===== */

/* These buffers will be accessed by map_move / map_merge, and therefore
 * need to be 2-byte aligned. */
static unsigned char map_accu[MAP_INTERNAL_DATA_SIZE(MAP_MAX_WIDTH,MAP_MAX_HEIGHT)]
    __attribute__ ((aligned (2)));
static unsigned char map_prox[MAP_INTERNAL_DATA_SIZE(MAP_PROXIMITY_SIZE,MAP_PROXIMITY_SIZE)]
    __attribute__ ((aligned (2)));

/* struct Map;
 * THERE IS NO MAP */

static Map* as_map(unsigned char* data) {
    return (Map*)(void*)data;
}

Map* map_get_accumulated() {
    return as_map(map_accu);
}

Map* map_get_proximity() {
    return as_map(map_prox);
}

int map_get_width(Map* map) {
    return map == map_get_accumulated() ? MAP_MAX_WIDTH : MAP_PROXIMITY_SIZE;
}

int map_get_height(Map* map) {
    return map == map_get_accumulated() ? MAP_MAX_HEIGHT : MAP_PROXIMITY_SIZE;
}

Map* map_deserialize(unsigned char* buffer) {
    return as_map(buffer);
}

unsigned char* map_serialize(Map* map) {
    return (unsigned char*)(void*)map;
}


/* ===== Implementation of hal_get_printbuf, using the static section (.bss) ===== */

static char printbuf[TIN_PACKAGE_MAX_LENGTH];

char* hal_get_printbuf(void) {
    return printbuf;
}

#include <stdlib.h> /* malloc, free */
#include <string.h> /* memset */

#include "hal.h"
#include "map_heap.h"

/* ===== Implementation of map.h, using the heap ===== */

Map* map_get_accumulated() {
    return map_heap_container->accu;
}

Map* map_get_proximity() {
    return map_heap_container->prox;
}

int map_get_width(Map* map) {
    return map->width;
}

int map_get_height(Map* map) {
    return map->height;
}

unsigned char* map_serialize(Map* map) {
    assert(map);
    assert(map->data);
    return map->data;
}

Map* map_deserialize(unsigned char* buffer) {
    map_heap_container->view_buffer.width = MAP_PROXIMITY_SIZE;
    map_heap_container->view_buffer.height = MAP_PROXIMITY_SIZE;
    map_heap_container->view_buffer.data = buffer;
    return &map_heap_container->view_buffer;
}

/* ===== Implementation of hal_get_printbuf, using the heap ===== */

static char* printbuf = NULL;

char* hal_get_printbuf(void) {
    if (printbuf == NULL) {
        printbuf = malloc(128);
    }
    return printbuf;
}


/* ===== Implementation of map_heap.h ===== */

MapContainer* map_heap_container;

Map* map_heap_alloc(int w, int h) {
    Map* map;
    assert(w <= MAP_MAX_WIDTH && h <= MAP_MAX_HEIGHT);
    assert(w % 4 == 0);
    assert(h % 2 == 0);
    /* This buffer will be read by other tinbots, and therefore "needs" to be
     * 2-byte aligned.  However, x86 is pretty permissive, meaning the access
     * will be a bit slower, in the unlikely event that malloc() gives us
     * a 2-bytes-unaligned address. */
    map = malloc(sizeof(Map));
    map->data = malloc((unsigned long)MAP_INTERNAL_DATA_SIZE(w,h));
    assert(map->data);
    map->width = w;
    map->height = h;
    /* In contrast to static memory, heap memory is not initially zero-ed out. */
    assert(0 == FIELD_UNKNOWN);
    map_clear(map);
    return map;
}

void map_heap_free(Map* map) {
    free(map->data);
    map->data = NULL;
    free(map);
}

void free_printbuf(void) {
    if (printbuf != NULL) {
        free(printbuf);
        printbuf = NULL;
    }
}

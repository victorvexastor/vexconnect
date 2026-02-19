/* seen.c — LRU deduplication cache for packet IDs */

#include "vex.h"
#include <string.h>
#include <time.h>

void vex_seen_init(vex_seen_cache_t *cache) {
    memset(cache, 0, sizeof(*cache));
    cache->count = 0;
}

/* Check if packet_id has been seen. Returns 1 if seen, 0 if new */
int vex_seen_check(vex_seen_cache_t *cache, const uint8_t *packet_id) {
    time_t now = time(NULL);

    for (int i = 0; i < cache->count; i++) {
        if (!cache->entries[i].active) continue;

        /* Expired? */
        if (now - cache->entries[i].timestamp > VEX_SEEN_TTL_SEC) {
            cache->entries[i].active = 0;
            continue;
        }

        if (memcmp(cache->entries[i].packet_id, packet_id, 8) == 0) {
            return 1;  /* Seen */
        }
    }

    return 0;  /* New */
}

/* Add a packet_id to the seen cache */
void vex_seen_add(vex_seen_cache_t *cache, const uint8_t *packet_id) {
    time_t now = time(NULL);

    /* Find an empty or expired slot */
    for (int i = 0; i < VEX_SEEN_CAPACITY; i++) {
        if (!cache->entries[i].active ||
            (now - cache->entries[i].timestamp > VEX_SEEN_TTL_SEC)) {
            memcpy(cache->entries[i].packet_id, packet_id, 8);
            cache->entries[i].timestamp = now;
            cache->entries[i].active = 1;
            if (i >= cache->count) cache->count = i + 1;
            return;
        }
    }

    /* Cache full — evict oldest */
    int oldest_idx = 0;
    time_t oldest_time = cache->entries[0].timestamp;
    for (int i = 1; i < VEX_SEEN_CAPACITY; i++) {
        if (cache->entries[i].timestamp < oldest_time) {
            oldest_time = cache->entries[i].timestamp;
            oldest_idx = i;
        }
    }

    memcpy(cache->entries[oldest_idx].packet_id, packet_id, 8);
    cache->entries[oldest_idx].timestamp = now;
    cache->entries[oldest_idx].active = 1;
}

/* Remove expired entries */
void vex_seen_prune(vex_seen_cache_t *cache) {
    time_t now = time(NULL);
    for (int i = 0; i < cache->count; i++) {
        if (cache->entries[i].active &&
            (now - cache->entries[i].timestamp > VEX_SEEN_TTL_SEC)) {
            cache->entries[i].active = 0;
        }
    }
}

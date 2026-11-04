#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum { POLICY_LRU, POLICY_FIFO, POLICY_LFU } Policy;

typedef struct {
    bool valid;
    uint64_t tag;
    uint64_t last_access_time;
    uint64_t insertion_time;
    uint64_t access_count;
} CacheLine;

typedef struct {
    CacheLine *lines;
} CacheSet;

typedef struct {
    uint32_t capacity;
    uint32_t block_size;
    uint32_t associativity;
    uint32_t num_sets;
    Policy policy;
    CacheSet *sets;
    
    // Global Telemetry
    uint64_t total_accesses;
    uint64_t total_hits;
    uint64_t total_misses;
    
    // Sliding Window Telemetry[cite: 5, 6]
    uint32_t window_size;
    uint32_t window_hits;
    uint32_t window_misses;
} Cache;

Cache* cache_create(uint32_t capacity, uint32_t block_size, uint32_t associativity, Policy policy, uint32_t window_size) {
    Cache *c = (Cache*) malloc(sizeof(Cache));
    c->capacity = capacity;
    c->block_size = block_size;
    c->associativity = associativity;
    c->num_sets = capacity / (block_size * associativity);
    c->policy = policy;
    c->total_accesses = 0;
    c->total_hits = 0;
    c->total_misses = 0;
    c->window_size = window_size;
    c->window_hits = 0;
    c->window_misses = 0;

    c->sets = (CacheSet*) malloc(sizeof(CacheSet) * c->num_sets);
    for (uint32_t i = 0; i < c->num_sets; i++) {
        c->sets[i].lines = (CacheLine*) calloc(associativity, sizeof(CacheLine));
    }
    return c;
}

int select_victim(Cache *c, uint32_t set_idx) {
    CacheSet *set = &c->sets[set_idx];
    int victim = 0;
    
    if (c->policy == POLICY_LRU) {
        uint64_t min_time = set->lines[0].last_access_time;
        for (uint32_t i = 1; i < c->associativity; i++) {
            if (set->lines[i].last_access_time < min_time) {
                min_time = set->lines[i].last_access_time;
                victim = i;
            }
        }
    } else if (c->policy == POLICY_FIFO) {
        uint64_t min_time = set->lines[0].insertion_time;
        for (uint32_t i = 1; i < c->associativity; i++) {
            if (set->lines[i].insertion_time < min_time) {
                min_time = set->lines[i].insertion_time;
                victim = i;
            }
        }
    } else if (c->policy == POLICY_LFU) {
        uint64_t min_freq = set->lines[0].access_count;
        for (uint32_t i = 1; i < c->associativity; i++) {
            if (set->lines[i].access_count < min_freq) {
                min_freq = set->lines[i].access_count;
                victim = i;
            }
        }
    }
    return victim;
}

bool cache_access(Cache *c, uint64_t addr, FILE *csv_out) {
    c->total_accesses++;
    uint64_t block_addr = addr / c->block_size;
    uint32_t set_idx = block_addr % c->num_sets;
    uint64_t tag = block_addr / c->num_sets;
    
    CacheSet *set = &c->sets[set_idx];
    
    // Check for Hit
    for (uint32_t i = 0; i < c->associativity; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            c->total_hits++;
            c->window_hits++;
            set->lines[i].last_access_time = c->total_accesses;
            set->lines[i].access_count++;
            goto log_step;
        }
    }
    
    // Miss Handling
    c->total_misses++;
    c->window_misses++;
    
    // Find Invalid Line or Evict Victim
    int target_slot = -1;
    for (uint32_t i = 0; i < c->associativity; i++) {
        if (!set->lines[i].valid) {
            target_slot = i;
            break;
        }
    }
    
    if (target_slot == -1) {
        target_slot = select_victim(c, set_idx);
    }
    
    // Insert Line
    set->lines[target_slot].valid = true;
    set->lines[target_slot].tag = tag;
    set->lines[target_slot].last_access_time = c->total_accesses;
    set->lines[target_slot].insertion_time = c->total_accesses;
    set->lines[target_slot].access_count = 1;

log_step:
    // Log sliding window telemetry[cite: 5, 6]
    if (csv_out && (c->total_accesses % c->window_size == 0)) {
        double window_wmr = (double)c->window_misses / c->window_size;
        fprintf(csv_out, "%lu,%.4f\n", c->total_accesses, window_wmr);
        c->window_hits = 0;
        c->window_misses = 0;
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc < 7) {
        printf("Usage: %s <trace_file> <capacity_bytes> <assoc> <policy: LRU|FIFO|LFU> <window_size> <output_csv>\n", argv[0]);
        return 1;
    }

    char *trace_file = argv[1];
    uint32_t capacity = atoi(argv[2]);
    uint32_t assoc = atoi(argv[3]);
    char *pol_str = argv[4];
    uint32_t window_size = atoi(argv[5]);
    char *csv_file = argv[6];

    Policy policy = POLICY_LRU;
    if (strcmp(pol_str, "FIFO") == 0) policy = POLICY_FIFO;
    else if (strcmp(pol_str, "LFU") == 0) policy = POLICY_LFU;

    Cache *cache = cache_create(capacity, 64, assoc, policy, window_size);

    FILE *tf = fopen(trace_file, "r");
    FILE *cf = fopen(csv_file, "w");
    fprintf(cf, "AccessCount,WMR\n");

    char type;
    uint64_t addr;
    int size;

    while (fscanf(tf, " %c %lx %d", &type, &addr, &size) == 3) {
        cache_access(cache, addr, cf);
    }

    fclose(tf);
    fclose(cf);

    double ghr = (double)cache->total_hits / cache->total_accesses;
    printf("Policy: %s | Accesses: %lu | Hits: %lu | Misses: %lu | GHR: %.4f\n",
           pol_str, cache->total_accesses, cache->total_hits, cache->total_misses, ghr);

    return 0;
}
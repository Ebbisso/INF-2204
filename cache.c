#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum { POLICY_LRU, POLICY_FIFO, POLICY_LFU, POLICY_ARC } Policy;

typedef struct {
    bool valid;
    uint64_t tag;
    uint64_t last_access_time;
    uint64_t insertion_time;
    uint64_t access_count;
    bool is_t2; // False = T1 (recency), True = T2 (frequency)
} CacheLine;

typedef struct {
    bool valid;
    uint64_t tag;
    uint64_t time;
} GhostLine;

typedef struct {
    CacheLine *lines;
    GhostLine *b1; // Ghost list B1 (evicted from T1)
    GhostLine *b2; // Ghost list B2 (evicted from T2)
    int b1_count;
    int b2_count;
    double p;      // Target size for T1
} CacheSet;

typedef struct {
    uint32_t capacity;
    uint32_t block_size;
    uint32_t associativity;
    uint32_t num_sets;
    Policy policy;
    CacheSet *sets;
    
    uint64_t total_accesses;
    uint64_t total_hits;
    uint64_t total_misses;
    
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
        c->sets[i].b1 = (GhostLine*) calloc(associativity, sizeof(GhostLine));
        c->sets[i].b2 = (GhostLine*) calloc(associativity, sizeof(GhostLine));
        c->sets[i].b1_count = 0;
        c->sets[i].b2_count = 0;
        c->sets[i].p = 0.0;
    }
    return c;
}

int select_victim_standard(Cache *c, uint32_t set_idx) {
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

void arc_replace(Cache *c, uint32_t set_idx, bool hit_in_b2) {
    CacheSet *set = &c->sets[set_idx];
    int t1_count = 0;
    for (uint32_t i = 0; i < c->associativity; i++) {
        if (set->lines[i].valid && !set->lines[i].is_t2) t1_count++;
    }

    int victim = -1;
    bool victim_from_t1 = false;

    if (t1_count > 0 && ((t1_count > set->p) || (hit_in_b2 && t1_count == (int)set->p))) {
        uint64_t min_time = UINT64_MAX;
        for (uint32_t i = 0; i < c->associativity; i++) {
            if (set->lines[i].valid && !set->lines[i].is_t2) {
                if (set->lines[i].last_access_time < min_time) {
                    min_time = set->lines[i].last_access_time;
                    victim = i;
                }
            }
        }
        victim_from_t1 = true;
    } else {
        uint64_t min_time = UINT64_MAX;
        for (uint32_t i = 0; i < c->associativity; i++) {
            if (set->lines[i].valid && set->lines[i].is_t2) {
                if (set->lines[i].last_access_time < min_time) {
                    min_time = set->lines[i].last_access_time;
                    victim = i;
                }
            }
        }
        if (victim == -1) {
            uint64_t min_t = UINT64_MAX;
            for (uint32_t i = 0; i < c->associativity; i++) {
                if (set->lines[i].valid && set->lines[i].last_access_time < min_t) {
                    min_t = set->lines[i].last_access_time;
                    victim = i;
                }
            }
            victim_from_t1 = !set->lines[victim].is_t2;
        }
    }

    if (victim != -1 && set->lines[victim].valid) {
        GhostLine *ghost_list = victim_from_t1 ? set->b1 : set->b2;
        int *g_count = victim_from_t1 ? &set->b1_count : &set->b2_count;

        if (*g_count >= (int)c->associativity) {
            int oldest = 0;
            uint64_t min_t = ghost_list[0].time;
            for (int i = 1; i < *g_count; i++) {
                if (ghost_list[i].time < min_t) {
                    min_t = ghost_list[i].time;
                    oldest = i;
                }
            }
            ghost_list[oldest].tag = set->lines[victim].tag;
            ghost_list[oldest].time = c->total_accesses;
            ghost_list[oldest].valid = true;
        } else {
            ghost_list[*g_count].tag = set->lines[victim].tag;
            ghost_list[*g_count].time = c->total_accesses;
            ghost_list[*g_count].valid = true;
            (*g_count)++;
        }

        set->lines[victim].valid = false;
    }
}

bool cache_access_arc(Cache *c, uint32_t set_idx, uint64_t tag, FILE *csv_out) {
    CacheSet *set = &c->sets[set_idx];
    
    for (uint32_t i = 0; i < c->associativity; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            c->total_hits++;
            c->window_hits++;
            set->lines[i].last_access_time = c->total_accesses;
            set->lines[i].access_count++;
            set->lines[i].is_t2 = true;
            return true;
        }
    }

    c->total_misses++;
    c->window_misses++;

    int b1_idx = -1, b2_idx = -1;
    for (int i = 0; i < set->b1_count; i++) {
        if (set->b1[i].valid && set->b1[i].tag == tag) { b1_idx = i; break; }
    }
    for (int i = 0; i < set->b2_count; i++) {
        if (set->b2[i].valid && set->b2[i].tag == tag) { b2_idx = i; break; }
    }

    if (b1_idx != -1) {
        double delta = (set->b2_count >= set->b1_count) ? 1.0 : ((double)set->b2_count / set->b1_count);
        set->p += delta;
        if (set->p > c->associativity) set->p = c->associativity;
        set->b1[b1_idx].valid = false;
        arc_replace(c, set_idx, false);
    } else if (b2_idx != -1) {
        double delta = (set->b1_count >= set->b2_count) ? 1.0 : ((double)set->b1_count / set->b2_count);
        set->p -= delta;
        if (set->p < 0.0) set->p = 0.0;
        set->b2[b2_idx].valid = false;
        arc_replace(c, set_idx, true);
    } else {
        int valid_count = 0;
        for (uint32_t i = 0; i < c->associativity; i++) {
            if (set->lines[i].valid) valid_count++;
        }
        if (valid_count >= (int)c->associativity) {
            arc_replace(c, set_idx, false);
        }
    }

    int empty_slot = -1;
    for (uint32_t i = 0; i < c->associativity; i++) {
        if (!set->lines[i].valid) { empty_slot = i; break; }
    }
    if (empty_slot != -1) {
        set->lines[empty_slot].valid = true;
        set->lines[empty_slot].tag = tag;
        set->lines[empty_slot].last_access_time = c->total_accesses;
        set->lines[empty_slot].insertion_time = c->total_accesses;
        set->lines[empty_slot].access_count = 1;
        set->lines[empty_slot].is_t2 = (b1_idx != -1 || b2_idx != -1);
    }

    return false;
}

bool cache_access(Cache *c, uint64_t addr, FILE *csv_out) {
    c->total_accesses++;
    uint64_t block_addr = addr / c->block_size;
    uint32_t set_idx = block_addr % c->num_sets;
    uint64_t tag = block_addr / c->num_sets;
    
    if (c->policy == POLICY_ARC) {
        cache_access_arc(c, set_idx, tag, csv_out);
        goto log_step;
    }

    CacheSet *set = &c->sets[set_idx];
    
    for (uint32_t i = 0; i < c->associativity; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            c->total_hits++;
            c->window_hits++;
            set->lines[i].last_access_time = c->total_accesses;
            set->lines[i].access_count++;
            goto log_step;
        }
    }
    
    c->total_misses++;
    c->window_misses++;
    
    int target_slot = -1;
    for (uint32_t i = 0; i < c->associativity; i++) {
        if (!set->lines[i].valid) {
            target_slot = i;
            break;
        }
    }
    
    if (target_slot == -1) {
        target_slot = select_victim_standard(c, set_idx);
    }
    
    set->lines[target_slot].valid = true;
    set->lines[target_slot].tag = tag;
    set->lines[target_slot].last_access_time = c->total_accesses;
    set->lines[target_slot].insertion_time = c->total_accesses;
    set->lines[target_slot].access_count = 1;

log_step:
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
        printf("Usage: %s <trace_file> <capacity_bytes> <assoc> <policy: LRU|FIFO|LFU|ARC> <window_size> <output_csv>\n", argv[0]);
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
    else if (strcmp(pol_str, "ARC") == 0) policy = POLICY_ARC;

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
#include "memcheck.h"
#include "cache_stats.h"

/* For this assignment, you will consider a variety of cache configurations.
 * For each cache you configure & simulate, you gererate statistics
 * for that cache with an instance of this struct
 */
cache_stats_t *make_cache_stats() {
  cache_stats_t *stats = my_malloc(sizeof(cache_stats_t));

  stats->n_cpu_accesses = 0;
  stats->n_hits = 0;
  stats->n_stores = 0;
  stats->n_writebacks = 0;

  stats->hit_rate = 0.0;

  stats->B_bus_to_cache = 0;

  stats->B_cache_to_bus_wb = 0;
  stats->B_cache_to_bus_wt = 0;

  stats->B_total_traffic_wb = 0;
  stats->B_total_traffic_wt = 0;

  return stats;
}

/** Deallocate all memory associated with cache stats
  */
void free_cache_stats(cache_stats_t *stats) {
  // Directly free memory
  my_free(stats);
  return;
}

void update_stats(cache_stats_t *stats, bool hit_f, bool writeback_f,
                  enum action_t action) {

  if (writeback_f)
    stats->n_writebacks++;

  if (hit_f)
    stats->n_hits++;

  if (action == STORE)
    stats->n_stores++;

  stats->n_cpu_accesses++;
}

// could do this in the previous method, but that's a lot of extra divides...
void calculate_stat_rates(cache_stats_t *stats, int block_size) {

  stats->hit_rate = stats->n_hits / (double)stats->n_cpu_accesses;

  // FIX THIS CODE!
  // you will need to modify this function in order to properly
  // calculate wb and wt data
  long n_miss = stats->n_cpu_accesses - stats->n_hits;

  stats->B_bus_to_cache =
      (int)(n_miss)*block_size; // upgrade misses don't trigger a WB

  stats->B_cache_to_bus_wb = stats->n_writebacks * block_size;
  stats->B_cache_to_bus_wt =
      stats->n_stores *
      4; // hypothetical write-thru statistics for the same cache

  stats->B_total_traffic_wb = stats->B_bus_to_cache + stats->B_cache_to_bus_wb;
  stats->B_total_traffic_wt = stats->B_bus_to_cache + stats->B_cache_to_bus_wt;
}

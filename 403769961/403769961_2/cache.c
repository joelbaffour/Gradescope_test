#include <math.h>
#include <stdio.h>

#include "cache.h"
#include "cache_stats.h"
#include "print_helpers.h"

void update_cache_lru(cache_t *cache, int which_set, int touched_way);

cache_t *make_cache(int capacity, int block_size, int assoc) {
  cache_t *cache = my_malloc(sizeof(cache_t));
  cache->stats = make_cache_stats();

  cache->capacity = capacity;     // in B
  cache->block_size = block_size; // in B
  cache->assoc = assoc;           // 1, 2, 3... etc.

  // FIX THIS CODE!
  // first, correctly set these 5 variables. THEY ARE ALL WRONG
  // note: you may find math.h's log2 function useful
  cache->n_cache_line = capacity / block_size;
  cache->n_set = cache->n_cache_line / assoc;
  cache->n_offset_bit = log2(block_size);
  cache->n_index_bit = log2(cache->n_set);
  cache->n_tag_bit = ADDRESS_SIZE - cache->n_index_bit - cache->n_offset_bit;

  // next create the cache lines and the array of LRU bits
  // - malloc an array with n_rows
  // - for each element in the array, malloc another array with n_col
  // FIX THIS CODE!
  cache->lines = my_malloc(cache->n_set * sizeof(cache_line_t *));
  for (int i = 0; i < cache->n_set; i++) {
    cache->lines[i] = my_malloc(assoc * sizeof(cache_line_t));
  }
  cache->lru_way = my_malloc(cache->n_set * sizeof(int));

  // initializes cache tags to 0, dirty bits to false, and LRU bits to 0
  // FIX THIS CODE!
  for (int i = 0; i < cache->n_set; i++) {
    for (int j = 0; j < cache->assoc; j++) {
      cache->lines[i][j].tag = 0;
      cache->lines[i][j].dirty_f = false;
      cache->lines[i][j].is_valid = false;
    }
    cache->lru_way[i] = 0;
  }

  return cache;
}

/** Given a configured cache, deallocate all memory associated with it
 */
void free_cache(cache_t *cache) {
  // Free cache stats
  free_cache_stats(cache->stats);
  
  // Free all cache lines
  for (int i = 0; i < cache->n_set; i++) {
    my_free(cache->lines[i]);
  }

  // Free cache line array
  my_free(cache->lines);

  // Free LRU data
  //my_free(cache->lru_way);

  // Free overall cache
  my_free(cache);

  return;
}

/* Given a configured cache, returns the tag portion of the given address.
 *
 * Example: a cache with 4 bits each in tag, index, offset
 * in binary -- get_cache_tag(0b111101010001) returns 0b1111
 * in decimal -- get_cache_tag(3921) returns 15
 */
unsigned long get_cache_tag(cache_t *cache, unsigned long addr) {
  // just shift it over, killing the index and the offset
  return addr >> (cache->n_offset_bit + cache->n_index_bit);
}

/* Given a configured cache, returns the index portion of the given address.
 *
 * Example: a cache with 4 bits each in tag, index, offset
 * in binary -- get_cache_index(0b111101010001) returns 0b0101
 * in decimal -- get_cache_index(3921) returns 5
 */
unsigned long get_cache_index(cache_t *cache, unsigned long addr) {
  int intAddr = (int)addr;
  // just shift it over to kill the offset bits
  int tmp = intAddr >> cache->n_offset_bit;

  int mask1 = (1 << cache->n_index_bit) - 1;
  // now mask it to get just the N index bits
  int tmp2 = tmp & mask1;
  return tmp2;
}

/* Given a configured cache, returns the given address with the offset bits
 * zeroed out.
 *
 * Example: a cache with 4 bits each in tag, index, offset
 * in binary -- get_cache_block_addr(0b111101010001) returns 0b111101010000
 * in decimal -- get_cache_block_addr(3921) returns 3920
 */
unsigned long get_cache_block_addr(cache_t *cache, unsigned long addr) {
  // just shift it over and then back again
  unsigned long blockAddr =
      addr >> (unsigned long)cache->n_offset_bit;         // shift over
  return blockAddr << (unsigned long)cache->n_offset_bit; // shift back
}

int find_valid_way(cache_t *cache, int set, unsigned long tag) {

  // look in each way
  for (int way = 0; way < cache->assoc; way++) {
    if (cache->lines[set][way].tag == tag && cache->lines[set][way].is_valid) {
      // found a valid entry, return its location!
      return way;
    }
  }
  return -1;
}

void process_miss(cache_t *cache, int set, unsigned long tag,
                  enum action_t action) {

  bool writeback_f = false;

  // THIS IS A MISS, where should the new line go?
  int victim_way = cache->lru_way[set];
  log_way(victim_way);
  cache_line_t *victim_line = &cache->lines[set][victim_way];

  // enter new tag value
  victim_line->tag = tag;

  writeback_f = (victim_line->dirty_f && (victim_line->is_valid));
  victim_line->dirty_f = (action == STORE);

  victim_line->is_valid = true;

  update_cache_lru(cache, set, victim_way);

  update_stats(cache->stats, false /* hit_f */, writeback_f, action);
}

/* this method takes a cache, an address, and an action
 * it proceses the cache access. functionality in no particular order:
 *   - look up the address in the cache, determine if hit or miss
 *   - update the LRU_way, cacheTags, is_valid, dirty flags if necessary
 *   - update the cache statistics
 * return true if there was a hit, false if there was a miss
 * Use the "get" helper functions above. They make your life easier.
 */
bool access_cache(cache_t *cache, unsigned long addr, enum action_t action) {
  // BEGIN ACCESS SOLUTION PART 1
  int set = get_cache_index(cache, addr);
  unsigned long tag = get_cache_tag(cache, addr);

  log_set(set); // in case verbose mode wants to print it
  int way = find_valid_way(cache, set, tag);
  bool writeback_f = false;

  // MISS
  if (way == -1) {
    process_miss(cache, set, tag, action);
    return MISS;
  }

  log_way(way);

  // HIT
  update_cache_lru(cache, set, way);

  if (action == STORE)
    cache->lines[set][way].dirty_f = true;

  // update the cache stats
  update_stats(cache->stats, true /* hit */, writeback_f, action);
  return HIT;
}

/*
 * LRU cannot be maintained with a single counter if there are
 * more than 2 ways. So we'll just use an approximation:
 *      one way: LRU bit is always 0 ((0 + 1) % 1 = 0)
 * 	two ways: LRU bit is always the way you DIDN'T just touch.
 *      > 2 ways: LRU bit is always 1 higher (w/wrap-around) than the way you
 * just touched. For example, if there are 4 ways, and you touch way 0, then the
 * new LRU should be 1. If you touch way 3, the new LRU should be 0. theSet:
 * identifies the set in the cache we're talking about touchedWay: identifies
 * the way we just touched.
 */
void update_cache_lru(cache_t *cache, int which_set, int touched_way) {
  cache->lru_way[which_set] = (touched_way + 1) % cache->assoc;
}

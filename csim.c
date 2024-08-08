#include "cachelab.h"
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#ifdef DEBUG
#define DEBUG_PRINT(x) printf x
#else
#define DEBUG_PRINT(x)                                                         \
  do {                                                                         \
  } while (0)
#endif

#define ADDRESS_LENGTH 64
#define u64 uint64_t

int v = 0;
u64 lru_counter = 1;

typedef struct Stats {
  int hits;
  int misses;
  int evictions;
} Stats;

typedef struct Line {
  int valid;
  u64 tag;
  u64 lru;
} Line;

typedef struct Set {
  Line *lines;
} Set;

typedef struct Cache {
  Set *sets;

  // config
  int set_bits;
  int set_count;
  int line_count;
  int block_bits;
  int block_count;
  int tag_bits;

  // metadata
  Stats *stats;
} Cache;

Cache *cache_new(int s, int E, int b) {

  Cache *c = malloc(sizeof(Cache));
  if (c == NULL)
    return NULL;

  // stats
  c->stats = (Stats *)malloc(sizeof(Stats));
  if (c->stats == NULL) {
    free(c);
    return NULL;
  }
  c->stats->hits = 0;
  c->stats->misses = 0;
  c->stats->evictions = 0;

  int set_count = pow(2, s);
  int block_count = pow(2, b);

  // config
  c->set_count = set_count;
  c->set_bits = s;
  c->line_count = E;
  c->block_bits = b;
  c->block_count = block_count;
  c->tag_bits = ADDRESS_LENGTH - (s + b);

  // sets
  c->sets = (Set *)malloc(sizeof(Set) * set_count);
  if (c->sets == NULL) {
    free(c->stats);
    free(c);
    return NULL;
  }

  // lines
  for (int i = 0; i < set_count; i++) {
    c->sets[i].lines = (Line *)malloc(E * sizeof(Line));
    if (c->sets[i].lines == NULL) {
      // Free previously allocated memory
      for (int j = 0; j < i; j++) {
        free(c->sets[j].lines);
      }
      free(c->sets);
      free(c->stats);
      free(c);
      return NULL;
    }
    for (int j = 0; j < E; j++) {
      c->sets[i].lines[j].valid = 0;
      c->sets[i].lines[j].tag = 0;
      c->sets[i].lines[j].lru = 0;
    }
  }

  return c;
}

void cache_free(Cache *c) {
  if (c == NULL)
    return;

  for (int i = 0; i < c->set_count; i++) {
    free(c->sets[i].lines);
  }
  free(c->sets);
  free(c->stats);
  free(c);
}

int get_block_offset(Cache *c, u64 address) {
  u64 mask = pow(2, c->block_bits) - 1;
  return address & mask;
}

int get_set_index(Cache *c, u64 address) {
  u64 mask = pow(2, c->set_bits) - 1;
  return (address >> c->block_bits) & mask;
}

u64 get_tag(Cache *c, u64 address) {
  return (address >> (c->block_bits + c->set_bits));
}

int evict(Cache *c, int set_index) {
  u64 min_lru = ULONG_MAX;
  int line_index = 0;

  for (int i = 0; i < c->line_count; i++) {
    Line l = c->sets[set_index].lines[i];
    if (l.lru < min_lru) {
      min_lru = l.lru;
      line_index = i;
    }
  }

  if (c->sets[set_index].lines[line_index].valid) {
    c->stats->evictions++;
    if (v)
      printf("eviction ");
  }
  return line_index;
}

void cache_read(Cache *c, u64 address, int size) {
  int set_index = get_set_index(c, address);
  u64 tag = get_tag(c, address);

  for (int i = 0; i < c->line_count; i++) {
    if (c->sets[set_index].lines[i].valid == 1 &&
        c->sets[set_index].lines[i].tag == tag) {
      c->sets[set_index].lines[i].lru = lru_counter++;
      c->stats->hits++;
      return;
    }
  }

  // miss
  c->stats->misses++;

  int line_index = evict(c, set_index);
  // replace
  c->sets[set_index].lines[line_index].valid = 1;
  c->sets[set_index].lines[line_index].tag = tag;
  c->sets[set_index].lines[line_index].lru = lru_counter++;
  ;
}

void printUsage(char *argv[]) {
  printf("Options:\n");
  printf("  -h         Print this help message.\n");
  printf("  -v         Optional verbose flag.\n");
  printf("  -s <num>   Number of set index bits.\n");
  printf("  -E <num>   Number of lines per set.\n");
  printf("  -b <num>   Number of block offset bits.\n");
  printf("  -t <file>  Trace file.\n");
  printf("\nExamples:\n");
  printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
  printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
  exit(0);
}

void run_trace(Cache *c, char *trace_file) {
  FILE *fp = fopen(trace_file, "r");
  char command;
  u64 address;
  int size;
  while (fscanf(fp, " %c %lx,%d", &command, &address, &size) == 3) {
    DEBUG_PRINT(("%c %lx %d\n", command, address, size));
    if (v) {
      if (command == 'I')
        continue;
      printf("%c %lx,%d ", command, address, size);
    }
    switch (command) {
    case 'L':
      cache_read(c, address, size);
      break;
    case 'S':
      cache_read(c, address, size);
      break;
    case 'M':
      cache_read(c, address, size);
      cache_read(c, address, size);
      break;
    default:
      break;
    }
    if (v && command != 'I')
      printf("\n");
  }

  fclose(fp);
}

int main(int argc, char **argv) {
  int s = 0;            // set bits
  int E = 0;            // # of lines
  int b = 0;            // block bits
  char *trace_file = 0; // trace file

  char opt;
  while ((opt = getopt(argc, argv, "s:E:b:t:vh")) != -1) {
    switch (opt) {
    case 's':
      s = atoi(optarg);
      break;
    case 'E':
      E = atoi(optarg);
      break;
    case 't':
      trace_file = optarg;
      break;
    case 'b':
      b = atoi(optarg);
      break;
    case 'v':
      v = 1;
      break;
    case 'h':
      printUsage(argv);
      return 0;
    default:
      printUsage(argv);
      return 0;
    }
  }

  DEBUG_PRINT(("%d-%d-%d\n", s, E, b));
  Cache *c = cache_new(s, E, b);
  run_trace(c, trace_file);
  printSummary(c->stats->hits, c->stats->misses, c->stats->evictions);
  // cache_free(c);
  return 0;
}

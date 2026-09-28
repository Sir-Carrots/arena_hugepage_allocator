#pragma once

#define HUGEPAGE_2MB (2 * 1024 * 1024)
#define HUGEPAGE_1GB (1ULL * 1024 * 1024 * 1024)
#include <sys/mman.h>
#include <unistd.h>
typedef enum page_type : unsigned char{
  HP,
  THP,
  GP,
  NONE,
} page_type;

typedef struct arena {
  void *page_addr;
  void *bump_pointer;
  page_type page_type;
} arena;

arena new_HP();
arena new_GP();
arena new_THP();
void *ahalloc (arena *arena_ptr, size_t size);
int ahfree (arena *arena_ptr);

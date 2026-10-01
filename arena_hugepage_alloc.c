#include "arena_hugepage_alloc.h"

INLINE void* new_HP_helper() {
  return mmap(NULL,
              HUGEPAGE_2MB,
              PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
              -1,
              0);
}

arena new_HP() {
  void* page_addr = new_HP_helper();
  if (page_addr == MAP_FAILED) {return (arena){NULL, NULL, NONE};}
  return (arena){page_addr, (char *)page_addr + sizeof(void *), HP};
}

INLINE void* new_GP_helper() {
  return mmap(NULL,
              HUGEPAGE_1GB,
              PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB,
              -1,
              0);
}

arena new_GP() {
  void* page_addr = new_GP_helper();
  if (page_addr == MAP_FAILED) {return (arena){NULL, NULL, NONE};}
  return (arena){page_addr, (char *)page_addr +sizeof(void *), GP};
}

INLINE void* new_THP_helper() {
  return mmap(NULL,
              HUGEPAGE_2MB,
              PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS,
              -1,
              0);
}

arena new_THP() {
  void* page_addr = new_THP_helper();
  if (page_addr == MAP_FAILED) {return (arena){NULL, NULL, NONE};}
  return (arena){page_addr, (char *)page_addr + sizeof(void *), THP};
}

void *ahalloc (arena *arena_ptr, size_t size) {
  if (arena_ptr->page_type == NONE) {return NULL;}
  else if (arena_ptr->page_type == HP || arena_ptr->page_type == THP && size > HUGEPAGE_2MB) {return NULL;}
  else if (arena_ptr->page_type == GP && size > HUGEPAGE_1GB) {return NULL;}

  else if (arena_ptr->page_type == HP 
          && (char *)arena_ptr->bump_pointer + size > (char *)arena_ptr->page_addr + HUGEPAGE_2MB) {
    void *temp_page_addr = new_HP_helper();
    *(void **)arena_ptr->page_addr = temp_page_addr;
    arena_ptr->bump_pointer = temp_page_addr + sizeof(void *);
  }
  else if (arena_ptr->page_type == THP 
          && (char *)arena_ptr->bump_pointer + size > (char *)arena_ptr->page_addr + HUGEPAGE_2MB) {
    void *temp_page_addr = new_THP_helper();
    *(void **)arena_ptr->page_addr = temp_page_addr;
    arena_ptr->bump_pointer = temp_page_addr + sizeof(void *);
  }
  else if (arena_ptr->page_type == GP 
          && (char *)arena_ptr->bump_pointer + size > (char *)arena_ptr->page_addr + HUGEPAGE_1GB) {
    void *temp_page_addr = new_GP_helper();
    *(void **)arena_ptr->page_addr = temp_page_addr;
    arena_ptr->bump_pointer = temp_page_addr + sizeof(void *);
  }

  return (void *)((arena_ptr->bump_pointer += size) - size);
}

static int free_HP (void *page_addr, int accumulator) {
  if (page_addr == NULL) {return accumulator;}
  void *next_page = *(void **)page_addr;
  accumulator += munmap(page_addr, HUGEPAGE_2MB);
  MUSTTAIL return free_HP(next_page, accumulator);
}

static int free_GP (void *page_addr, int accumulator) {
  if (page_addr == NULL) {return accumulator;}
  void *next_page = *(void **)page_addr;
  accumulator += munmap(page_addr, HUGEPAGE_2MB);
  MUSTTAIL return free_GP(next_page, accumulator);
}

int ahfree (arena *arena_ptr) {
  switch (arena_ptr->page_type) {
    case THP:
    case HP:
      if(free_HP(arena_ptr->page_addr, 0) != 0) {return -1;}
      return 0;
    case GP:
      if(free_GP(arena_ptr->page_addr, 0) != 0) {return -1;}
      return 0;
    case NONE:
      return -1;
  }
}

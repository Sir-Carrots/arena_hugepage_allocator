#define _GNU_SOURCE
#include "arena_hugepage_alloc.h"
#include <stddef.h>
#include <sys/mman.h>

INLINE void* new_HP_helper() {
  return mmap(NULL,
              HUGEPAGE_2MB,
              PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_2MB,
              -1,
              0);
}

arena new_HP() {
  
  //Constructing a wrapper around the mmap call

  void* page_addr = new_HP_helper();
  if (page_addr == MAP_FAILED) {return (arena){NULL,NULL, NULL, NONE};}
  return (arena){page_addr,page_addr, (void *)((size_t)page_addr + PAGE_HEADER_SIZE), HP};
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
  if (page_addr == MAP_FAILED) {return (arena){NULL, NULL, NULL, NONE};}
  return (arena){page_addr, page_addr, (void *)((size_t)page_addr + PAGE_HEADER_SIZE), GP};
}

INLINE void* new_THP_helper() {
  void *pre_formed_THP = mmap(NULL,
              HUGEPAGE_2MB,
              PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS,
              -1,
              0);
  if (pre_formed_THP == MAP_FAILED) {return pre_formed_THP;}

  /*After we get back 2MB generic memory for the OS, we ask it to group the pages together to form a THP 
   * To note: we can ask the OS to restructure our memory even after out memory requrest because pages
  Are given out on a lazy basis.*/

  if(madvise(pre_formed_THP, HUGEPAGE_2MB, MADV_HUGEPAGE) != 0) {
    munmap(pre_formed_THP, HUGEPAGE_2MB);
    return MAP_FAILED;
  }
  return pre_formed_THP;
}

arena new_THP() {
  void* page_addr = new_THP_helper();
  if (page_addr == MAP_FAILED) {return (arena){NULL, NULL, NULL, NONE};}
  return (arena){page_addr, page_addr, (void *)((size_t)page_addr + PAGE_HEADER_SIZE), THP};
}

void *ahalloc (arena *arena_ptr, size_t size) {

  //Ensuring that the requested sub allocation is memory aligned by padding it.
  
  size_t alignment = _Alignof(max_align_t);
  size_t remainder = size % alignment;
  size += alignment - remainder;

  //Taking care of invalid/ impossible sub allocation requests.

  if (arena_ptr->page_type == NONE) {return NULL;}
  else if ((arena_ptr->page_type == HP || arena_ptr->page_type == THP) && size > (HUGEPAGE_2MB - PAGE_HEADER_SIZE)) {return NULL;}
  else if (arena_ptr->page_type == GP && size > (HUGEPAGE_1GB - PAGE_HEADER_SIZE)) {return NULL;}

  /*In case the requested memory is larger than space available in current page, we meed to perform
  * an another mmap call, put the address of that hugepage into the current_page pointer,
  * put in the begenning of the previous page (for page chaining), and move the bump pointer
  * to the earliest available space in the new memory page*/

  else if (arena_ptr->page_type == HP 
          && ((size_t)arena_ptr->bump_pointer + size) > ((size_t)arena_ptr->current_page + HUGEPAGE_2MB)) {
    void *temp_addr = new_HP_helper();
    if (temp_addr == MAP_FAILED) {return NULL;}
    *(void **)arena_ptr->current_page = temp_addr;
    arena_ptr->current_page = temp_addr;
    arena_ptr->bump_pointer = (void *)((size_t)arena_ptr->current_page + PAGE_HEADER_SIZE);
  }
  else if (arena_ptr->page_type == THP 
          && ((unsigned char *)arena_ptr->bump_pointer + size) > ((unsigned char *)arena_ptr->current_page + HUGEPAGE_2MB)) {
    void *temp_addr = new_THP_helper();
    if (temp_addr == MAP_FAILED) {return NULL;}
    *(void **)arena_ptr->current_page = temp_addr;
    arena_ptr->current_page = temp_addr;
    arena_ptr->bump_pointer = (void *)((size_t)arena_ptr->current_page + PAGE_HEADER_SIZE);
  }
  else if (arena_ptr->page_type == GP 
          && ((unsigned char *)arena_ptr->bump_pointer + size) > ((unsigned char *)arena_ptr->current_page + HUGEPAGE_1GB)) {
    void *temp_addr = new_GP_helper();
    if (temp_addr == MAP_FAILED) {return NULL;}
    *(void **)arena_ptr->current_page = temp_addr;
    arena_ptr->current_page = temp_addr;
    arena_ptr->bump_pointer = (void *)((size_t)arena_ptr->current_page + PAGE_HEADER_SIZE);
  }

  //On every sub allocation, we just give out the bump pointer location and advance it by the requested size

  void *result = arena_ptr->bump_pointer;
  arena_ptr->bump_pointer = (void *)((unsigned char *)arena_ptr->bump_pointer + size);
  return result;
}

/*Here we use a recursive helper function to free the entire page chain. I realise
* That recursives are generally looked on poorly for good reason, but I have used the
* force TCO compiler flags to ensure there is no stack overflow. If you're on MSVC,
* which has no 'Musttail' flag, I'm sorry. (Use O2/O3 compilation in this case)*/

static int free_HP (void *page_addr, int accumulator) {
  if (page_addr == NULL) {return accumulator;}
  void *next_page = *(void **)page_addr;
  accumulator += munmap(page_addr, HUGEPAGE_2MB);
  MUSTTAIL return free_HP(next_page, accumulator);
}

static int free_GP (void *page_addr, int accumulator) {
  if (page_addr == NULL) {return accumulator;}
  void *next_page = *(void **)page_addr;
  accumulator += munmap(page_addr, HUGEPAGE_1GB);
  MUSTTAIL return free_GP(next_page, accumulator);
}

int ahfree (arena *arena_ptr) {
  switch (arena_ptr->page_type) {
    case THP:
    case HP:
      if(free_HP(arena_ptr->page_addr, 0) != 0) {return -1;}
      break;
    case GP:
      if(free_GP(arena_ptr->page_addr, 0) != 0) {return -1;}
      break;
    case NONE:
    default:
      return -1;
  }

  /*Finally, we set all values of the arena struct to 0 so that it is neutralized and cannot
  be used again*/

  arena_ptr->page_addr = NULL;
  arena_ptr->bump_pointer = NULL;
  arena_ptr->current_page = NULL;
  arena_ptr->page_type = NONE;
  return 0;
}

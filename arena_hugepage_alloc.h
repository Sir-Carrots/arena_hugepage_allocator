#pragma once

#define HUGEPAGE_2MB (2 * 1024 * 1024)
#define HUGEPAGE_1GB (1ULL * 1024 * 1024 * 1024)
#define PAGE_HEADER_SIZE \
    ((sizeof(void *) + _Alignof(max_align_t) - 1) / \
     _Alignof(max_align_t) * _Alignof(max_align_t))

#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>

#if !defined(__linux__)
#error "arena_hugepage_alloc currently requires Linux"
#endif

#ifndef COMPILER_FLAGS_H
#define COMPILER_FLAGS_H

#if defined(_MSC_VER)
    #define INLINE static __forceinline

#elif defined(__GNUC__) || defined(__clang__)
    #define INLINE static inline __attribute__((always_inline))

#else
    #define INLINE static inline
#endif


#ifndef __has_c_attribute
    #define __has_c_attribute(x) 0
#endif

#ifndef __has_attribute
    #define __has_attribute(x) 0
#endif


#if defined(__clang__)
    #if __has_attribute(musttail)
        #define MUSTTAIL [[clang::musttail]]
    #else
        #define MUSTTAIL
    #endif

#elif defined(__GNUC__)
    #if __has_c_attribute(gnu::musttail)
        #define MUSTTAIL [[gnu::musttail]]
    #elif __has_attribute(musttail)
        #define MUSTTAIL __attribute__((musttail))
    #else
        #define MUSTTAIL
    #endif

#else
    #define MUSTTAIL
#endif

#endif
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

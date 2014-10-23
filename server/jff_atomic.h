#ifndef _ATOMIC_H
#define _ATOMIC_H

/// @brief atomically adds a_count to the variable pointed by a_ptr
/// @return the value that had previously been in memory
#define JFF_AtomicAdd(a_ptr,a_count) __sync_fetch_and_add (a_ptr, a_count)

/// @brief atomically substracts a_count from the variable pointed by a_ptr
/// @return the value that had previously been in memory
#define JFF_AtomicSub(a_ptr,a_count) __sync_fetch_and_sub (a_ptr, a_count)

/// @brief Compare And Swap
///        If the current value of *a_ptr is a_oldVal, then write a_newVal into *a_ptr
/// @return true if the comparison is successful and a_newVal was written
#define JFF_CAS(a_ptr, a_oldVal, a_newVal) __sync_bool_compare_and_swap(a_ptr, a_oldVal, a_newVal)

#endif

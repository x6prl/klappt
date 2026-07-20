#pragma once

#include "SDL3/SDL_atomic.h"

using AtomicInt = SDL_AtomicInt;

namespace Atomic {
constexpr int FALSE = 0;
constexpr int TRUE = 1;
inline AtomicInt init(int val) { return {.value = val}; }
inline int get(AtomicInt *aint) { return SDL_GetAtomicInt(aint); }
inline bool compare_and_swap(AtomicInt *aint, int oldval, int newval) {
	return SDL_CompareAndSwapAtomicInt(aint, oldval, newval);
}
inline int inc(AtomicInt *aint, int val = 1) {
	return SDL_AddAtomicInt(aint, val);
}
inline void set(AtomicInt *aint, int val) { SDL_SetAtomicInt(aint, val); }
inline AtomicInt init(bool val) { return {.value = val ? TRUE : FALSE}; }
inline bool is_false(AtomicInt *aint) {
	return SDL_GetAtomicInt(aint) == FALSE;
}
inline bool is_true(AtomicInt *aint) { return !is_false(aint); }
inline void set_true(AtomicInt *aint) { SDL_SetAtomicInt(aint, TRUE); }
inline void set_false(AtomicInt *aint) { SDL_SetAtomicInt(aint, FALSE); }
} // namespace Atomic

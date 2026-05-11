#include <stdint.h>

#include "arena.h"
#include "str_view.h"

void shuffle_ints(int *a, Size n, uint64_t *rng_state); 
Size shuffle_str_views(StrView *a, Size n, uint64_t *rng_state); 
// TODO: replace with shuffle_str_views
void shuffle_str_views_and_indices(StrView *a, Size*b, Size n, uint64_t *rng_state); 

// [from, to)
Size random_num(Size from, Size to, uint64_t* rng_state);

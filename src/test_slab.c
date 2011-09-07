
#include "slab.h"

int
main(void)
{
    test_bit_set();
    test_slab_allocator();
    test_size_to_cache();

    return 0;
}

#include "slab.h"
#include "test_base.h"

int main(void)
{
    TEST_START();
    test_bit_set();
    TEST_END("test: bit_set");
    
    TEST_START();
    test_slab_allocator();
    TEST_END("test: slab_allocator");
    
    TEST_START();    
    test_size_to_cache();
    TEST_END("test: size_to_cache");

    return 0;
}

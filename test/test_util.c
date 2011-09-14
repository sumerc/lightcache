

#include "util.h"
#include "test_base.h"

int main(void)
{
    TEST_START();    
    test_endianness();
    TEST_END("test: endianness");
    
    TEST_START();    
    test_util_routines();
    TEST_END("test: util_routines");
    
    return 0;
}

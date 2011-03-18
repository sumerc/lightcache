#include "stdio.h"
#include "stdint.h"

int 
main(void)
{
    printf("size of pointer:%u\r\n", sizeof(void *));
    printf("size of int:%u\r\n", sizeof(int));
    printf("size of long:%u\r\n", sizeof(long));
    printf("size of long int:%u\r\n", sizeof(long int));
    printf("size of long long:%u\r\n", sizeof(long long));
    printf("size of long long int:%u\r\n", sizeof(long long int));
    printf("size of uint32_t:%u\r\n", sizeof(uint32_t));
    printf("size of uint64_t:%u\r\n", sizeof(uint64_t));
}
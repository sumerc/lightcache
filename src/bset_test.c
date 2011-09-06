
#include "stdint.h"
#include "stddef.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"
#include "math.h"
#include "limits.h"

#include "stdio.h"
#include "sys/time.h"


// When this is 32 bit and we take a 64 bit param in nlz(), what happens is that
// it is silently casted to 64 bit value. So no problem will occur. But what happens
// if the PC is really 32-bit? Test that...
typedef uint64_t word_t;

#define WORD_COUNT 4
#define WORD_SIZE_IN_BITS (sizeof(word_t) * CHAR_BIT)   // in bits

typedef struct {
    word_t words[WORD_COUNT];
} bitset_t;

inline unsigned int
bindex(unsigned int b)
{
    return b / WORD_SIZE_IN_BITS;
}

inline unsigned int
bloffset(unsigned int b)
{
    return b % WORD_SIZE_IN_BITS;
}

void
dump_bitset(bitset_t *bts)
{
    int i,j;
    
    for(i=0; i<WORD_COUNT; i++) {
        for(j=0;j<sizeof(word_t);j+=sizeof(unsigned int)) {            
            printf("word %d is 0x%x\r\n", i, *(unsigned int *)(&bts->words[i]+j));
        }
    }
}

void
set_bit(bitset_t *bts, unsigned int b)
{
    assert(b < WORD_COUNT*WORD_SIZE_IN_BITS);

    bts->words[bindex(b)] |= (word_t)1 << (bloffset(b));
}
void
clear_bit(bitset_t *bts, unsigned int b)
{
    assert(b < WORD_COUNT*WORD_SIZE_IN_BITS);

    bts->words[bindex(b)] &= ~((word_t)1 << (bloffset(b)));
}

unsigned int
get_bit(bitset_t *bts, unsigned int b)
{
    assert(b < WORD_COUNT*WORD_SIZE_IN_BITS);

    return (bts->words[bindex(b)] >> (bloffset(b))) & 1;
}

int
nlz32(register uint32_t x)
{
    static const int debruij_tab[32] = {
        0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
        31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
    };

    return debruij_tab[(((x&-x) * 0x077CB531U)) >> 27];
}

int 
nlz64(uint64_t x) {
    static const unsigned int debruij_tab[64] =
    {
        0,  1,  2, 53,  3,  7, 54, 27,
        4, 38, 41,  8, 34, 55, 48, 28,
       62,  5, 39, 46, 44, 42, 22,  9,
       24, 35, 59, 56, 49, 18, 29, 11,
       63, 52,  6, 26, 37, 40, 33, 47,
       61, 45, 43, 21, 23, 58, 17, 10,
       51, 25, 36, 32, 60, 20, 57, 16,
       50, 31, 19, 15, 30, 14, 13, 12,
    };
    return debruij_tab[((x&-x)*0x022fdd63cc95386d) >> 58];
}


// Using ffsll() on an 64-bit machine gains no performance at all.
static int
ff_setbit(bitset_t *bts)
{
    int i, j;
    word_t w;

    for(i=0; i<WORD_COUNT; i++) {
        w = bts->words[i];
        printf("vusil %d\r\n", sizeof(word_t));
        if(w) {
            j = nlz64(w);
            printf("dd:%d, %d, %d\r\n", j, i, j + (i*WORD_SIZE_IN_BITS));
            //return (WORD_SIZE_IN_BITS-j-1 + (i*WORD_SIZE_IN_BITS));
            return (j + (i*WORD_SIZE_IN_BITS));
        }
    }
    
    return -1;
}

void
test_bs(void)
{
    bitset_t *bs;
    
    bs = malloc(sizeof(bitset_t));
    memset(bs, 0x00 ,sizeof(bitset_t));
    //bs->words[0] = 0xFFFFEEEEDDDDEEEE;
    set_bit(bs, 63);
    dump_bitset(bs);
    ff_setbit(bs);
    /*
    set_bit(bs, 32); // MSB of second word
    set_bit(bs, 63);
    dump_bitset(bs);
    assert(bs->words[1] == 0x80000001);
    ff_setbit(bs);
    clear_bit(bs, 32);
    clear_bit(bs, 63);
    set_bit(bs, 35);
    dump_bitset(bs);
    assert(ff_setbit(bs) == 35);
    memset(bs, 0x00 ,sizeof(bitset_t));
    assert(ff_setbit(bs) == -1);
    set_bit(bs, 96);
    set_bit(bs, 97);
    set_bit(bs, 104);
    
    assert(ff_setbit(bs) == 96);
    clear_bit(bs, 96);
    clear_bit(bs, 97);
    assert(ff_setbit(bs) == 104);
    */
    return;
}

int
main(void)
{
    test_bs();

    return 0;
}

/*
// TODO: When below is used instead of ffs, runtime increases %25.
// 1) Removed if check to the loop, now we have same performance as ffs()
// 2) Using x/n as a register ensures run-time consistency. Now it is 
//    always the best case.
static inline int 
nlz(register word_t x) {
   register int n;
   
   n = 0;
   if (x <= 0x0000FFFF) {n = n +16; x = x <<16;}
   if (x <= 0x00FFFFFF) {n = n + 8; x = x << 8;}
   if (x <= 0x0FFFFFFF) {n = n + 4; x = x << 4;}
   if (x <= 0x3FFFFFFF) {n = n + 2; x = x << 2;}
   if (x <= 0x7FFFFFFF) {n = n + 1;}
   return n;
}

static int
ff_setbit(bitset_t *bts)
{
    int i, j;
    word_t w;
    
    for(i=0; i<WORD_COUNT; i++) {
        w = bts->words[i];
        if (w) {
           j = nlz(w);
           return (j + (i * WORD_SIZE_IN_BITS));
        }
    }
    
    return -1;
}

int 
nlz2(register unsigned int v) 
{
    register unsigned int c;
    
    if (v & 0x1) 
{
  // special case for odd v (assumed to happen half of the time)
  c = 0;
}
else
{
  c = 1;
  if ((v & 0xffff) == 0) 
  {  
    v >>= 16;  
    c += 16;
  }
  if ((v & 0xff) == 0) 
  {  
    v >>= 8;  
    c += 8;
  }
  if ((v & 0xf) == 0) 
  {  
    v >>= 4;
    c += 4;
  }
  if ((v & 0x3) == 0) 
  {  
    v >>= 2;
    c += 2;
  }
  c -= v & 0x1;
}	
return c;
}
*/
/* MIT License
 *
 * Copyright (c) 2016 Omar Alvarez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define BLAKE2B_BLOCKBYTES 128
#define BLAKE2B_OUTBYTES 64

#if defined(_MSC_VER)
# define CRYPTO_ALIGN(x) __declspec(align(x))
#else
# define CRYPTO_ALIGN(x) __attribute__((aligned(x)))
#endif

//TODO Inline giving problems htole, load, etc.

uint32_t htole32(uint32_t host_32bits)
{
    return __builtin_bswap32(host_32bits);
}

#ifndef ROTR64
#define ROTR64(x, y)  (((x) >> (y)) ^ ((x) << (64 - (y))))
#endif

uint64_t load64( const void *src )
{
#if defined(NATIVE_LITTLE_ENDIAN)
    uint64_t w;
    int i;
    //memcpy(&w, src, sizeof w);
    for(i = 0; i < sizeof(w); ++i)
        (&w)[i] = src[i];
    return w;
#else
    const uint8_t *p = ( const uint8_t * )src;
    uint64_t w = *p++;
    w |= ( uint64_t )( *p++ ) <<  8;
    w |= ( uint64_t )( *p++ ) << 16;
    w |= ( uint64_t )( *p++ ) << 24;
    w |= ( uint64_t )( *p++ ) << 32;
    w |= ( uint64_t )( *p++ ) << 40;
    w |= ( uint64_t )( *p++ ) << 48;
    w |= ( uint64_t )( *p++ ) << 56;
    return w;
#endif
}

void store64( void *dst, uint64_t w )
{
#if defined(NATIVE_LITTLE_ENDIAN)
    memcpy(dst, &w, sizeof w);
#else
    uint8_t *p = ( uint8_t * )dst;
    *p++ = ( uint8_t )w; w >>= 8;
    *p++ = ( uint8_t )w; w >>= 8;
    *p++ = ( uint8_t )w; w >>= 8;
    *p++ = ( uint8_t )w; w >>= 8;
    *p++ = ( uint8_t )w; w >>= 8;
    *p++ = ( uint8_t )w; w >>= 8;
    *p++ = ( uint8_t )w; w >>= 8;
    *p++ = ( uint8_t )w;
#endif
}

/*static inline unsigned long rotr64( __const unsigned long w, __const unsigned c ) {
 return ( w >> c ) | ( w << ( 64 - c ) );
 }*/

typedef uint32_t eh_index;

typedef CRYPTO_ALIGN(64) struct crypto_generichash_blake2b_state {
    
    uint64_t h[8];
    uint64_t t[2];
    uint64_t f[2];
    uint8_t  buf[2 * BLAKE2B_BLOCKBYTES];
    size_t   buflen;
    uint8_t  last_node;
    
} crypto_generichash_blake2b_state;

typedef crypto_generichash_blake2b_state eh_hash_state;

static const uint64_t blake2b_IV[8] =
{
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
    0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
    0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

static const uint8_t blake2b_sigma[12][16] =
{
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 } ,
    { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 } ,
    { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 } ,
    {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 } ,
    {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 } ,
    {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 } ,
    { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 } ,
    { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 } ,
    {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 } ,
    { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13 , 0 } ,
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 } ,
    { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 }
};


int blake2b_increment_counter( eh_hash_state *S, const uint64_t inc )
{
    S->t[0] += inc;
    S->t[1] += ( S->t[0] < inc );
    return 0;
}

int blake2b_is_lastblock( const eh_hash_state *S )
{
    return S->f[0] != 0;
}

int blake2b_set_lastnode( eh_hash_state *S )
{
    S->f[1] = -1;
    return 0;
}

int blake2b_set_lastblock( eh_hash_state *S )
{
    if( S->last_node ) blake2b_set_lastnode( S );
    
    S->f[0] = -1;
    return 0;
}

static int blake2b_compress( eh_hash_state *S, const uint8_t block[BLAKE2B_BLOCKBYTES] )
{
    uint64_t m[16];
    uint64_t v[16];
    int i;
    
    for( i = 0; i < 16; ++i )
        m[i] = load64( block + i * sizeof( m[i] ) );
    
    for( i = 0; i < 8; ++i )
        v[i] = S->h[i];
    
    v[ 8] = blake2b_IV[0];
    v[ 9] = blake2b_IV[1];
    v[10] = blake2b_IV[2];
    v[11] = blake2b_IV[3];
    v[12] = S->t[0] ^ blake2b_IV[4];
    v[13] = S->t[1] ^ blake2b_IV[5];
    v[14] = S->f[0] ^ blake2b_IV[6];
    v[15] = S->f[1] ^ blake2b_IV[7];
#define G(r,i,a,b,c,d) \
do { \
a = a + b + m[blake2b_sigma[r][2*i+0]]; \
d = ROTR64(d ^ a, 32); \
c = c + d; \
b = ROTR64(b ^ c, 24); \
a = a + b + m[blake2b_sigma[r][2*i+1]]; \
d = ROTR64(d ^ a, 16); \
c = c + d; \
b = ROTR64(b ^ c, 63); \
} while(0)
#define ROUND(r)  \
do { \
G(r,0,v[ 0],v[ 4],v[ 8],v[12]); \
G(r,1,v[ 1],v[ 5],v[ 9],v[13]); \
G(r,2,v[ 2],v[ 6],v[10],v[14]); \
G(r,3,v[ 3],v[ 7],v[11],v[15]); \
G(r,4,v[ 0],v[ 5],v[10],v[15]); \
G(r,5,v[ 1],v[ 6],v[11],v[12]); \
G(r,6,v[ 2],v[ 7],v[ 8],v[13]); \
G(r,7,v[ 3],v[ 4],v[ 9],v[14]); \
} while(0)
    ROUND( 0 );
    ROUND( 1 );
    ROUND( 2 );
    ROUND( 3 );
    ROUND( 4 );
    ROUND( 5 );
    ROUND( 6 );
    ROUND( 7 );
    ROUND( 8 );
    ROUND( 9 );
    ROUND( 10 );
    ROUND( 11 );
    
    for( i = 0; i < 8; ++i )
        S->h[i] = S->h[i] ^ v[i] ^ v[i + 8];
    
#undef G
#undef ROUND
    return 0;
}

/* inlen now in bytes */
int blake2b_update(eh_hash_state *S, const uint8_t *in, uint64_t inlen) {
    
    while( inlen > 0 )
    {
        size_t left = S->buflen;
        size_t fill = 2 * BLAKE2B_BLOCKBYTES - left;
        
        if( inlen > fill )
        {
            memcpy( S->buf + left, in, fill ); /* Fill buffer */
            S->buflen += fill;
            blake2b_increment_counter( S, BLAKE2B_BLOCKBYTES );
            blake2b_compress( S, S->buf ); /* Compress */
            memcpy( S->buf, S->buf + BLAKE2B_BLOCKBYTES, BLAKE2B_BLOCKBYTES ); /* Shift buffer left */
            S->buflen -= BLAKE2B_BLOCKBYTES;
            in += fill;
            inlen -= fill;
        }
        else /* inlen <= fill */
        {
            memcpy( S->buf + left, in, inlen );
            S->buflen += inlen; /* Be lazy, do not compress */
            in += inlen;
            inlen -= inlen;
        }
    }
    
    return 0;
    
}

int blake2b_final( eh_hash_state *S, uint8_t *out, uint8_t outlen )
{
    uint8_t buffer[BLAKE2B_OUTBYTES] = {0};
    
    if( out == NULL || outlen == 0 || outlen > BLAKE2B_OUTBYTES )
        return -1;
    
    if( blake2b_is_lastblock( S ) )
        return -1;
    
    if( S->buflen > BLAKE2B_BLOCKBYTES )
    {
        blake2b_increment_counter( S, BLAKE2B_BLOCKBYTES );
        blake2b_compress( S, S->buf );
        S->buflen -= BLAKE2B_BLOCKBYTES;
        memcpy( S->buf, S->buf + BLAKE2B_BLOCKBYTES, S->buflen );
    }
    
    blake2b_increment_counter( S, S->buflen );
    blake2b_set_lastblock( S );
    memset( S->buf + S->buflen, 0, 2 * BLAKE2B_BLOCKBYTES - S->buflen ); /* Padding */
    blake2b_compress( S, S->buf );
    
    for( int i = 0; i < 8; ++i ) /* Output full hash to temp buffer */
        store64( buffer + sizeof( S->h[i] ) * i, S->h[i] );
    
    memcpy( out, buffer, outlen );
    return 0;
}

void generate_hash(const eh_hash_state * base_state, eh_index g, unsigned char * hash, size_t h_len) {
    
    eh_hash_state state = *base_state;
    eh_index lei = htole32(g);
    blake2b_update(&state, (const unsigned char *) &lei, sizeof(eh_index));
    blake2b_final(&state, hash, h_len);
    
}

int main(void)
{
    
    unsigned long n = 200;
    unsigned long k = 9;
    
    size_t collision_bit_length = n / (k + 1);
    eh_index init_size = 1 << (collision_bit_length + 1);
    unsigned long indices_per_hash_out = 512 / n;
    size_t collision_byte_length = (collision_bit_length + 7) / 8;
    size_t hash_len = (k + 1) * collision_byte_length;
    size_t len_indices = sizeof(eh_index);
    size_t full_width = 2 * collision_byte_length + sizeof(eh_index) * (1 << (k-1));
    const size_t hash_output = indices_per_hash_out * n / 8;
    
    unsigned char tmp_hash[hash_output];
    
    //TODO We need to create this state with Block info, now it is empty
    eh_hash_state base_state;
    printf("Init Size: %i\n", init_size);
    /*for (eh_index i = 0; init_size; ++i) {
     
     generate_hash(&base_state, i, tmp_hash, hash_output);
     
     }*/
    
    return 0;
    
}

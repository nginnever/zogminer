#define EQUIHASH_N 200
#define EQUIHASH_K 9

#define NUM_COLLISION_BITS (EQUIHASH_N / (EQUIHASH_K + 1))
#define NUM_INDICES (1 << EQUIHASH_K)

#define NUM_COMPRESSED_INDICE_BITS 16
#define NUM_DECOMPRESSED_INDICE_BITS (NUM_COLLISION_BITS+1)

#define NUM_INDICE_BYTES_PER_ELEMENT (((NUM_INDICES/2) * NUM_COMPRESSED_INDICE_BITS + 7) / 8)
#define NUM_VALUES (1 << (NUM_COLLISION_BITS+1))
#define NUM_INDICES_PER_BUCKET (1 << 10)
#define NUM_STEP_INDICES (8*NUM_VALUES)
#define NUM_BUCKETS (1 << NUM_COLLISION_BITS)/NUM_INDICES_PER_BUCKET
#define DIGEST_SIZE 32




/* START OF BLAKE2B CODE */
/*
   BLAKE2 reference source code package - reference C implementations
  
   Copyright 2012, Samuel Neves <sneves@dei.uc.pt>.  You may use this under the
   terms of the CC0, the OpenSSL Licence, or the Apache Public License 2.0, at
   your option.  The terms of these licenses can be found at:
  
   - CC0 1.0 Universal : http://creativecommons.org/publicdomain/zero/1.0
   - OpenSSL license   : https://www.openssl.org/source/license.html
   - Apache 2.0        : http://www.apache.org/licenses/LICENSE-2.0
  
   More information about the BLAKE2 hash function can be found at
   https://blake2.net.
*/


typedef uchar uint8_t;
typedef unsigned uint32_t;
typedef ulong uint64_t;

#ifdef BLAKE2_NO_INLINE
#define BLAKE2_LOCAL_INLINE(type) static type
#endif

#ifndef BLAKE2_LOCAL_INLINE
#define BLAKE2_LOCAL_INLINE(type) static inline type
#endif

enum blake2b_constant
{
  BLAKE2B_BLOCKBYTES = 128,
  BLAKE2B_OUTBYTES   = 64,
  BLAKE2B_KEYBYTES   = 64,
  BLAKE2B_SALTBYTES  = 16,
  BLAKE2B_PERSONALBYTES = 16
};

typedef struct __blake2b_state
{
  uint64_t h[8];
  uint64_t t[2];
  uint64_t f[2];
  uint8_t  buf[2 * BLAKE2B_BLOCKBYTES];
  size_t   buflen;
  uint8_t  last_node;
} blake2b_state;

typedef struct __blake2b_param
{
  uint8_t  digest_length; /* 1 */
  uint8_t  key_length;    /* 2 */
  uint8_t  fanout;        /* 3 */
  uint8_t  depth;         /* 4 */
  uint32_t leaf_length;   /* 8 */
  uint64_t node_offset;   /* 16 */
  uint8_t  node_depth;    /* 17 */
  uint8_t  inner_length;  /* 18 */
  uint8_t  reserved[14];  /* 32 */
  uint8_t  salt[BLAKE2B_SALTBYTES]; /* 48 */
  uint8_t  personal[BLAKE2B_PERSONALBYTES];  /* 64 */
} blake2b_param;


int blake2b_init( blake2b_state *S, const uint8_t outlen );
int blake2b_init_key( blake2b_state *S, const uint8_t outlen, const void *key, const uint8_t keylen );
int blake2b_init_param( blake2b_state *S, const blake2b_param *P );
int blake2b_update( blake2b_state *S, const uint8_t *in, uint64_t inlen );
int blake2b_final( blake2b_state *S, uint8_t *out, uint8_t outlen );


int memcmp(void* s1, void* s2, size_t n) {
    unsigned char u1, u2;

    for ( ; n-- ; s1++, s2++) {
        u1 = * (unsigned char *) s1;
        u2 = * (unsigned char *) s2;
        if ( u1 != u2) {
            return (u1-u2);
        }
    }
    return 0;
}

void *memset(void *dst, int c, size_t n) {
    if (n) {
        char *d = dst;
 
        do {
            *d++ = c;
        } while (--n);
    }
    return dst;
}


void memcpy(void *dest, void *src, size_t n) {
   char *csrc = (char *)src;
   char *cdest = (char *)dest;
 
   for (int i=0; i<n; i++)
       cdest[i] = csrc[i];
}




BLAKE2_LOCAL_INLINE(uint32_t) load32( const void *src )
{
  uint32_t w;
  memcpy(&w, src, sizeof w);
  return w;
}

BLAKE2_LOCAL_INLINE(uint64_t) load64( const void *src )
{
  uint64_t w;
  memcpy(&w, src, sizeof w);
  return w;
}

BLAKE2_LOCAL_INLINE(void) store32( void *dst, uint32_t w )
{
  memcpy(dst, &w, sizeof w);
}

BLAKE2_LOCAL_INLINE(void) store64( void *dst, uint64_t w )
{
  memcpy(dst, &w, sizeof w);
}

BLAKE2_LOCAL_INLINE(uint64_t) load48( const void *src )
{
  const uint8_t *p = ( const uint8_t * )src;
  uint64_t w = *p++;
  w |= ( uint64_t )( *p++ ) <<  8;
  w |= ( uint64_t )( *p++ ) << 16;
  w |= ( uint64_t )( *p++ ) << 24;
  w |= ( uint64_t )( *p++ ) << 32;
  w |= ( uint64_t )( *p++ ) << 40;
  return w;
}

BLAKE2_LOCAL_INLINE(void) store48( void *dst, uint64_t w )
{
  uint8_t *p = ( uint8_t * )dst;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w; w >>= 8;
  *p++ = ( uint8_t )w;
}

BLAKE2_LOCAL_INLINE(uint32_t) rotl32( const uint32_t w, const unsigned c )
{
  return ( w << c ) | ( w >> ( 32 - c ) );
}

BLAKE2_LOCAL_INLINE(uint64_t) rotl64( const uint64_t w, const unsigned c )
{
  return ( w << c ) | ( w >> ( 64 - c ) );
}

BLAKE2_LOCAL_INLINE(uint32_t) rotr32( const uint32_t w, const unsigned c )
{
  return ( w >> c ) | ( w << ( 32 - c ) );
}

BLAKE2_LOCAL_INLINE(uint64_t) rotr64( const uint64_t w, const unsigned c )
{
  return ( w >> c ) | ( w << ( 64 - c ) );
}

/* prevents compiler optimizing out memset() */
BLAKE2_LOCAL_INLINE(void) secure_zero_memory(void *v, size_t n)
{
  //void *(*const volatile memset_v)(void *, int, size_t) = &memset;
  memset(v, 0, n);
}


__constant static const uint64_t blake2b_IV[8] =
{
  0x6a09e667f3bcc908UL, 0xbb67ae8584caa73bUL,
  0x3c6ef372fe94f82bUL, 0xa54ff53a5f1d36f1UL,
  0x510e527fade682d1UL, 0x9b05688c2b3e6c1fUL,
  0x1f83d9abfb41bd6bUL, 0x5be0cd19137e2179UL
};

__constant static const uint8_t blake2b_sigma[12][16] =
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


BLAKE2_LOCAL_INLINE(int) blake2b_set_lastnode( blake2b_state *S )
{
  S->f[1] = -1;
  return 0;
}

BLAKE2_LOCAL_INLINE(int) blake2b_clear_lastnode( blake2b_state *S )
{
  S->f[1] = 0;
  return 0;
}

/* Some helper functions, not necessarily useful */
BLAKE2_LOCAL_INLINE(int) blake2b_is_lastblock( const blake2b_state *S )
{
  return S->f[0] != 0;
}

BLAKE2_LOCAL_INLINE(int) blake2b_set_lastblock( blake2b_state *S )
{
  if( S->last_node ) blake2b_set_lastnode( S );

  S->f[0] = -1;
  return 0;
}

BLAKE2_LOCAL_INLINE(int) blake2b_clear_lastblock( blake2b_state *S )
{
  if( S->last_node ) blake2b_clear_lastnode( S );

  S->f[0] = 0;
  return 0;
}

BLAKE2_LOCAL_INLINE(int) blake2b_increment_counter( blake2b_state *S, const uint64_t inc )
{
  S->t[0] += inc;
  S->t[1] += ( S->t[0] < inc );
  return 0;
}



/* Parameter-related functions */
BLAKE2_LOCAL_INLINE(int) blake2b_param_set_digest_length( blake2b_param *P, const uint8_t digest_length )
{
  P->digest_length = digest_length;
  return 0;
}

BLAKE2_LOCAL_INLINE(int) blake2b_param_set_fanout( blake2b_param *P, const uint8_t fanout )
{
  P->fanout = fanout;
  return 0;
}

BLAKE2_LOCAL_INLINE(int) blake2b_param_set_max_depth( blake2b_param *P, const uint8_t depth )
{
  P->depth = depth;
  return 0;
}

BLAKE2_LOCAL_INLINE(int) blake2b_param_set_leaf_length( blake2b_param *P, const uint32_t leaf_length )
{
  store32( &P->leaf_length, leaf_length );
  return 0;
}

BLAKE2_LOCAL_INLINE(int) blake2b_param_set_node_offset( blake2b_param *P, const uint64_t node_offset )
{
  store64( &P->node_offset, node_offset );
  return 0;
}

BLAKE2_LOCAL_INLINE(int) blake2b_param_set_node_depth( blake2b_param *P, const uint8_t node_depth )
{
  P->node_depth = node_depth;
  return 0;
}

BLAKE2_LOCAL_INLINE(int) blake2b_param_set_inner_length( blake2b_param *P, const uint8_t inner_length )
{
  P->inner_length = inner_length;
  return 0;
}

BLAKE2_LOCAL_INLINE(int) blake2b_init0( blake2b_state *S )
{
  memset( S, 0, sizeof( blake2b_state ) );

  for( int i = 0; i < 8; ++i ) S->h[i] = blake2b_IV[i];

  return 0;
}

/* init xors IV with input parameter block */
int blake2b_init_param( blake2b_state *S, const blake2b_param *P )
{
  const uint8_t *p = ( const uint8_t * )( P );

  blake2b_init0( S );

  /* IV XOR ParamBlock */
  for( size_t i = 0; i < 8; ++i )
    S->h[i] ^= load64( p + sizeof( S->h[i] ) * i );

  return 0;
}



int blake2b_init( blake2b_state *S, const uint8_t outlen )
{
  blake2b_param P[1];

  if ( ( !outlen ) || ( outlen > BLAKE2B_OUTBYTES ) ) return -1;

  P->digest_length = outlen;
  P->key_length    = 0;
  P->fanout        = 1;
  P->depth         = 1;
  store32( &P->leaf_length, 0 );
  store64( &P->node_offset, 0 );
  P->node_depth    = 0;
  P->inner_length  = 0;
  memset( P->reserved, 0, sizeof( P->reserved ) );
  memset( P->salt,     0, sizeof( P->salt ) );
  memset( P->personal, 0, sizeof( P->personal ) );
  return blake2b_init_param( S, P );
}


int blake2b_init_key( blake2b_state *S, const uint8_t outlen, const void *key, const uint8_t keylen )
{
  blake2b_param P[1];

  if ( ( !outlen ) || ( outlen > BLAKE2B_OUTBYTES ) ) return -1;

  if ( !key || !keylen || keylen > BLAKE2B_KEYBYTES ) return -1;

  P->digest_length = outlen;
  P->key_length    = keylen;
  P->fanout        = 1;
  P->depth         = 1;
  store32( &P->leaf_length, 0 );
  store64( &P->node_offset, 0 );
  P->node_depth    = 0;
  P->inner_length  = 0;
  memset( P->reserved, 0, sizeof( P->reserved ) );
  memset( P->salt,     0, sizeof( P->salt ) );
  memset( P->personal, 0, sizeof( P->personal ) );

  if( blake2b_init_param( S, P ) < 0 ) return -1;

  {
    uint8_t block[BLAKE2B_BLOCKBYTES];
    memset( block, 0, BLAKE2B_BLOCKBYTES );
    memcpy( block, key, keylen );
    blake2b_update( S, block, BLAKE2B_BLOCKBYTES );
    secure_zero_memory( block, BLAKE2B_BLOCKBYTES ); /* Burn the key from stack */
  }
  return 0;
}

static int blake2b_compress( blake2b_state *S, const uint8_t block[BLAKE2B_BLOCKBYTES] )
{
  uint64_t m[16];
  uint64_t v[16];
  int i;

  for( i = 0; i < 16; ++i )
    m[i] = ((uint64_t*)block)[i];

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
    d = rotr64(d ^ a, 32); \
    c = c + d; \
    b = rotr64(b ^ c, 24); \
    a = a + b + m[blake2b_sigma[r][2*i+1]]; \
    d = rotr64(d ^ a, 16); \
    c = c + d; \
    b = rotr64(b ^ c, 63); \
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
int blake2b_update( blake2b_state *S, const uint8_t *in, uint64_t inlen )
{
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

/* Is this correct? */
int blake2b_final( blake2b_state *S, uint8_t *out, uint8_t outlen )
{
  uint8_t buffer[BLAKE2B_OUTBYTES] = {0};

  if( out == 0 || outlen == 0 || outlen > BLAKE2B_OUTBYTES )
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

/* inlen, at least, should be uint64_t. Others can be size_t. */
void blake2b(uint8_t *out, 
                      const void *in, 
                      const void *key, 
                      const uint8_t outlen, 
                      const uint64_t inlen, 
                      uint8_t keylen)
{
  blake2b_state S[1];

  /* Verify parameters */
  if ( 0 == in && inlen > 0 ) return;

  if ( 0 == out ) return;

  if( 0 == key && keylen > 0 ) return;

  if( !outlen || outlen > BLAKE2B_OUTBYTES ) return;

  if( keylen > BLAKE2B_KEYBYTES ) return;

  if( keylen > 0 )
  {
    if( blake2b_init_key( S, outlen, key, keylen ) < 0 ) return;
  }
  else
  {
    if( blake2b_init( S, outlen ) < 0 ) return;
  }

  blake2b_update( S, ( const uint8_t * )in, inlen );
  blake2b_final( S, out, outlen );
}

/* END OF BLAKE2B CODE */



/* STRUCTS */

typedef struct element
{
    uint8_t digest[DIGEST_SIZE];
    uint32_t a;
    uint32_t b;
} element_t;

typedef struct bucket
{
    uint32_t tmp;
    volatile uint32_t size;
    element_t data[NUM_INDICES_PER_BUCKET*4];
} bucket_t;

typedef struct element_indice
{
    uint32_t a;
    uint32_t b;
} element_indice_t;


/* UTIL */


uint32_t mask_collision_bits_private(uint8_t* data, size_t start) {
    size_t byte_index = start / 8;
    size_t bit_index = start % 8;
    uint32_t n = ((data[byte_index] << (bit_index)) & 0xff) << 12;
    n |= ((data[byte_index+1]) << (bit_index+4));
    n |= ((data[byte_index+2]) >> (4-bit_index));
    return n;
}

uint32_t mask_collision_bits_global(__global uint8_t* data, size_t start) {
    size_t byte_index = start / 8;
    size_t bit_index = start % 8;
    uint32_t n = ((data[byte_index] << (bit_index)) & 0xff) << 12;
    n |= ((data[byte_index+1]) << (bit_index+4));
    n |= ((data[byte_index+2]) >> (4-bit_index));
    return n;
}

void xor_elements(__global uint8_t* dst, __global uint8_t* a, __global uint8_t* b) {
    ((__global uint64_t*)dst)[0] = ((__global uint64_t*)a)[0] ^ ((__global uint64_t*)b)[0];
    ((__global uint64_t*)dst)[1] = ((__global uint64_t*)a)[1] ^ ((__global uint64_t*)b)[1];
    ((__global uint64_t*)dst)[2] = ((__global uint64_t*)a)[2] ^ ((__global uint64_t*)b)[2];
    dst[24] = a[24] ^ b[24];
}

// copy from local to global mem
void memcpy_private2global(__global void *dest, void *src, size_t n) {
   char *csrc = (char *)src;
   __global char *cdest = (__global char *)dest;
 
   for (int i=0; i<n; i++)
       cdest[i] = csrc[i];
}

__kernel void initial_bucket_hashing(__global bucket_t* dst, __global const blake2b_state* digest)
{
    uint8_t new_digest[2*DIGEST_SIZE];
    memset(new_digest, '\0', 2*DIGEST_SIZE);
    size_t start = get_global_id(0) * ((NUM_VALUES / 2) / get_global_size(0));
    size_t end = (get_global_id(0) + 1) * ((NUM_VALUES / 2) / get_global_size(0));
    uint32_t z = get_global_id(0);

    for(uint32_t i = start; i < end; ++i){
        blake2b_state current_digest;
        current_digest = *digest;
        blake2b_update(&current_digest, (uint8_t*)&i, sizeof(uint32_t));
        blake2b_final(&current_digest, (uint8_t*)(new_digest), 50);

        {
            uint32_t new_index = mask_collision_bits_private(new_digest, 0) / NUM_INDICES_PER_BUCKET;
            // fill the buckets with elements
            __global bucket_t* bucket = dst + new_index;
            __global element_t* new_el = bucket->data + atomic_add(&bucket->size, 1);
            // set the index of the initial first half of the hash
            new_el->a = i*2;
            // copy the first half of new hash to global element digest
            memcpy_private2global(new_el->digest, new_digest, DIGEST_SIZE);
        }

        {
            // select the last 25 bytes of the generated hash for second index
            uint32_t new_index = mask_collision_bits_private(new_digest + 25, 0) / NUM_INDICES_PER_BUCKET;
            // fill the buckets with elements
            __global bucket_t* bucket = dst + new_index;
            __global element_t* new_el = bucket->data + atomic_add(&bucket->size, 1);
            // set the index of the initial first half of the hash
            new_el->a = i*2+1;
            // copy the first half of new hash to global element digest
            memcpy_private2global(new_el->digest, new_digest + 25, DIGEST_SIZE);
        }

    }
}


__kernel void bucket_collide_and_hash(__global bucket_t* dst, __global bucket_t* src, __global element_indice_t* indices, uint32_t step_index) {
    uint32_t z = get_global_id(0);
    // select the starting bit based on the current step ROUND
    // this will increment by by 20 bits in n=200... 0,20,40,60...140
    // step 9 is bits 160-180
    size_t start_bit = ((step_index-1) * NUM_COLLISION_BITS);
    size_t last_bit = ((step_index) * NUM_COLLISION_BITS);
    // the start is the global id since the number of buckets
    // is equal to the number of global work items 1024
    size_t start = get_global_id(0) * (NUM_BUCKETS / get_global_size(0));
    // end is one index up in the global work items
    size_t end = (get_global_id(0)+1) * (NUM_BUCKETS / get_global_size(0));
    // the indice index is offset from the work item id by 
    // the number of indices needed to be in each work item
    size_t indice_index = get_global_id(0) * (NUM_STEP_INDICES / get_global_size(0));
    // get the last steps indices from global mem
    __global element_indice_t* old_indices = indices + NUM_STEP_INDICES*(step_index-1);

    for(uint32_t i = start; i < end; ++i){
        __global bucket_t* bucket = src + 1;
        uint8_t sub_bucket_sizes[NUM_INDICES_PER_BUCKET];
        uint32_t sub_buckets[NUM_INDICES_PER_BUCKET][16];
        memset(sub_bucket_sizes, '\0', NUM_INDICES_PER_BUCKET * sizeof(uint8_t));

        for(uint32_t j = 0; j < bucket->size; ++j){
            uint32_t sub_index = mask_collision_bits_global((bucket->data+j)->digest, start_bit) % NUM_INDICES_PER_BUCKET;
            sub_buckets[sub_index][sub_bucket_sizes[sub_index]++] = j;
        }

        for(uint32_t o = 0; o < NUM_INDICES_PER_BUCKET; ++o){
            uint32_t* sub_bucket_indices = sub_buckets[o];
            uint32_t sub_bucket_size = sub_bucket_sizes[o];

            for(uint32_t j = 0; j < sub_bucket_size && sub_bucket_size > 1; ++j){
                __global element_t* base = bucket->data + sub_bucket_indices[j];
                uint32_t base_bits = mask_collision_bits_global(base->digest, last_bit);
                old_indices[indice_index].a = base->a;
                old_indices[indice_index].b = base->b;

                for(uint32_t k = j + 1; k < sub_bucket_size; ++k){
                    __global element_t* el = bucket->data + sub_bucket_indices[k];
                    uint32_t new_index = base_bits ^ mask_collision_bits_global(el->digest, last_bit);
                    if(new_index == 0) continue;
                    new_index /= NUM_INDICES_PER_BUCKET;

                    __global bucket_t* dst_bucket = dst + new_index;
                    __global element_t* new_el = dst_bucket->data + atomic_add(&dst_bucket->size, 1);
                    xor_elements(new_el->digest, base->digest, el->digest);
                    new_el->a = indice_index;
                    new_el->b = indice_index + (k-j);
                }
                indice_index++;
            }
        }

        bucket->size = 0;
    }
    
}

__kernel void produce_solutions(__global bucket_t* src, __global element_indice_t* src_indices, __global const blake2b_state* digest) {
    uint32_t z = get_global_id(0);
    uint32_t n_solutions = 0;
    size_t start_bit = ((EQUIHASH_K-1) * NUM_COLLISION_BITS);
    size_t last_bit = ((EQUIHASH_K) * NUM_COLLISION_BITS);
    size_t start = get_global_id(0) * (NUM_BUCKETS / get_global_size(0));
    size_t end = (get_global_id(0) + 1) * (NUM_BUCKETS / get_global_size(0));

    __global element_indice_t* indices[9];
    for(size_t i = 0; i < EQUIHASH_K; ++i){
        indices[i] = src_indices + NUM_STEP_INDICES * i;
    }

    for(uint32_t i = start; i < end; ++i){
        __global bucket_t* bucket = src + i;
        uint32_t sub_bucket_sizes[NUM_INDICES_PER_BUCKET];
        uint32_t sub_buckets[NUM_INDICES_PER_BUCKET][20];
        memset(sub_bucket_sizes, '\0', NUM_INDICES_PER_BUCKET * sizeof(uint32_t));

    }
}

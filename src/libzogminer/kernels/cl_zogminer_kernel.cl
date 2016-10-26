/* START OF BLAKE2B CODE */


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


// TODO REMOVE THIS LINE FOR GPU
#ifdef cl_intel_printf
#pragma OPENCL EXTENSION cl_intel_printf : enable
#endif

#define EQUIHASH_N 200
#define EQUIHASH_K 9

#define NUM_COLLISION_BITS (EQUIHASH_N / (EQUIHASH_K + 1))
#define NUM_INDICES (1 << EQUIHASH_K)

#define NUM_VALUES (1 << (NUM_COLLISION_BITS+1))
#define NUM_BUCKETS (1 << NUM_COLLISION_BITS)
#define DIGEST_SIZE 25



typedef struct element {
    uint32_t digest_index;
    uint32_t parent_bucket_data;
    //uint32_t parent_bucket_index;
    //uint32_t a_parent_bucket_sub_index;
    //uint32_t b_parent_bucket_sub_index;
} element_t;

typedef struct bucket {
    element_t data[18];
    volatile unsigned size;
} bucket_t;

typedef uint64_t digest_t[(DIGEST_SIZE + sizeof(uint64_t) - 1) / sizeof(uint64_t)];


void set_element_digest_index(__global element_t* dst, uint32_t digest_index) {
    dst->digest_index = digest_index;
}

void set_element_parent_bucket_data(__global element_t* dst, uint32_t parent_bucket_index, uint8_t a, uint8_t b) {
    dst->parent_bucket_data = (parent_bucket_index << 8) | ((a & 0xf) << 4) | (b & 0xf);
    //dst->parent_bucket_index = parent_bucket_index;
    //dst->a_parent_bucket_sub_index = a;
    //dst->b_parent_bucket_sub_index = b;
}

void get_element_parent_bucket_data(element_t* src, uint32_t* parent_bucket_index, uint8_t* a, uint8_t* b) {
  *parent_bucket_index = src->parent_bucket_data >> 8;
  *a = (src->parent_bucket_data >> 4) & 0xf;
  *b = (src->parent_bucket_data & 0xf);
}

uint32_t mask_collision_bits(__global uint8_t* data, size_t bit_index) {
    uint32_t n = ((*data << (bit_index)) & 0xff) << 12;
    n |= ((*(++data)) << (bit_index+4));
    n |= ((*(++data)) >> (4-bit_index));
    return n;
}


uint32_t mask_collision_bits_step0(uint8_t* data, size_t bit_index) {
    uint32_t n = ((*data << (bit_index)) & 0xff) << 12;
    n |= ((*(++data)) << (bit_index+4));
    n |= ((*(++data)) >> (4-bit_index));
    return n;
}

void memcpy_step0(__global void *dest, void *src, size_t n) {
   char *csrc = (char *)src;
   __global char *cdest = (__global char *)dest;

   for (int i=0; i<n; i++)
       cdest[i] = csrc[i];
}



void xor_elements(__global uint8_t* dst, __global uint8_t* a, __global uint8_t* b) {
    ((__global uint64_t*)dst)[0] = ((__global uint64_t*)a)[0] ^ ((__global uint64_t*)b)[0];
    ((__global uint64_t*)dst)[1] = ((__global uint64_t*)a)[1] ^ ((__global uint64_t*)b)[1];
    ((__global uint64_t*)dst)[2] = ((__global uint64_t*)a)[2] ^ ((__global uint64_t*)b)[2];
    dst[24] = a[24] ^ b[24];
}


__kernel void initial_bucket_hashing(__global bucket_t* dst_buckets, __global digest_t* dst_digests, __constant const blake2b_state* digest, __global volatile uint32_t* new_digest_index) {
    uint8_t new_digest[2*DIGEST_SIZE];
    memset(new_digest, '\0', 2*DIGEST_SIZE);
    size_t start = get_global_id(0) * ((NUM_VALUES / 2) / get_global_size(0));
    size_t end = (get_global_id(0)+1) * ((NUM_VALUES / 2) / get_global_size(0));
    uint64_t tmp = *new_digest_index;

    for(uint32_t i = start; i < end; ++i) {
        blake2b_state current_digest = *digest;
        blake2b_update(&current_digest, (uint8_t*)&i, sizeof(uint32_t));
        blake2b_final(&current_digest, (uint8_t*)(new_digest), 2*DIGEST_SIZE);

        for(uint32_t j = 0; j < 2; ++j) {
            uint32_t new_index = mask_collision_bits_step0(new_digest + (j*EQUIHASH_N/8), 0);
            __global element_t* new_el = dst_buckets[new_index].data + atomic_add(&dst_buckets[new_index].size, 1);
            //new_el->digest_index = atomic_add(new_digest_index, 1);
            new_el->digest_index = (get_global_id(0) * 2) + j;

            set_element_parent_bucket_data(new_el, i*2 + j, 0, 0);
            memcpy_step0((__global void*)(dst_digests + new_el->digest_index), new_digest + (j*EQUIHASH_N/8), DIGEST_SIZE);
        }
    }
}

__kernel void bucket_collide_and_hash(__global digest_t* dst_digests, __global digest_t* src_digests, __global bucket_t* buckets, uint32_t step_index, __global volatile uint32_t* new_digest_index) {
    size_t start_bit = (step_index*NUM_COLLISION_BITS);
    size_t byte_index = start_bit / 8;
    size_t bit_index = start_bit % 8;
    size_t start = get_global_id(0) * (NUM_BUCKETS / get_global_size(0));
    size_t end = (get_global_id(0)+1) * (NUM_BUCKETS / get_global_size(0));

  __global bucket_t* src_buckets = buckets + (step_index-1)*NUM_BUCKETS;
  __global bucket_t* dst_buckets = buckets + step_index*NUM_BUCKETS;
  for(uint32_t current_bucket_index = start; current_bucket_index < end; ++current_bucket_index) {
    __global bucket_t* bucket = src_buckets+current_bucket_index;
    //bucket->size = bucket->size < 13 ? bucket->size : 13;

    for(size_t a = 0; a < bucket->size; ++a) {
        element_t base = bucket->data[a];

        __global uint8_t* base_digest = (__global uint8_t*)src_digests[base.digest_index];
        uint32_t base_collision_bits = mask_collision_bits(base_digest + byte_index, bit_index);
        for(size_t b = a+1; b < bucket->size; ++b) {
            element_t el = bucket->data[b];
            __global uint8_t* el_digest = (__global uint8_t*)src_digests[el.digest_index];
            uint32_t new_index = base_collision_bits ^ mask_collision_bits(el_digest + byte_index, bit_index);
            if(new_index == 0) continue;

            __global element_t* new_el = dst_buckets[new_index].data + atomic_add(&dst_buckets[new_index].size, 1);
            set_element_parent_bucket_data(new_el, current_bucket_index, a, b);
            new_el->digest_index = atomic_add(new_digest_index, 1);
            //for(uint32_t h = 0; h < 2; h++){
            //    new_el->digest_index = (get_global_id(0) * 2) + h;
            //}

            xor_elements((__global uint8_t*)(dst_digests + new_el->digest_index), base_digest, el_digest);
        }
    }
    bucket->size = 0;
  }
}


void decompress_indices(uint32_t* dst_uncompressed_indices, __global bucket_t* buckets, __global element_t* old_src) {
    element_t elements[EQUIHASH_K][NUM_INDICES];
    elements[0][0] = *old_src;

    for(size_t i = 0; i < EQUIHASH_K-1; ++i) {
        for(size_t j = 0; j < (1 << i); ++j) {
            element_t* src = elements[i] + j;
            uint32_t parent_bucket_index;
            uint8_t a;
            uint8_t b;
            get_element_parent_bucket_data(src, &parent_bucket_index, &a, &b);

            __global bucket_t* parent_bucket = buckets + ((EQUIHASH_K-2-i) * NUM_BUCKETS) + parent_bucket_index;
            elements[i+1][2*j] = parent_bucket->data[a];
            elements[i+1][2*j+1] = parent_bucket->data[b];
        }
    }

    for(size_t j = 0; j < NUM_INDICES/2; ++j) {
        element_t* src = elements[EQUIHASH_K-1] + j;
        uint32_t parent_bucket_index;
        uint8_t a;
        uint8_t b;
        get_element_parent_bucket_data(src, &parent_bucket_index, &a, &b);
        *dst_uncompressed_indices = parent_bucket_index;
        dst_uncompressed_indices++;
    }
}

__kernel void produce_solutions(__global uint32_t* dst_solutions, __global volatile uint32_t* n_solutions, __global bucket_t* buckets, __global digest_t* src_digests, __constant blake2b_state* digest) {
    size_t start_bit = (EQUIHASH_K*NUM_COLLISION_BITS);
    size_t byte_index = start_bit / 8;
    size_t bit_index = start_bit % 8;
    __global bucket_t* src_buckets = buckets + (EQUIHASH_K-1)*NUM_BUCKETS;
    size_t start = get_global_id(0) * (NUM_BUCKETS / get_global_size(0));
    size_t end = (get_global_id(0)+1) * (NUM_BUCKETS / get_global_size(0));

    for(size_t i = start; i < end; ++i) {
        __global bucket_t* bucket = src_buckets + i;
        int has_dupe = 0;
        for(size_t a = 0; a < bucket->size && !has_dupe; ++a) {
            __global element_t* base = bucket->data + a;
            for(size_t b = a+1; b < bucket->size; ++b) {
                __global element_t* el = bucket->data + b;
                uint32_t ai = mask_collision_bits(((__global uint8_t*)src_digests[base->digest_index]) + byte_index, bit_index);
                uint32_t bi = mask_collision_bits(((__global uint8_t*)src_digests[el->digest_index]) + byte_index, bit_index);
                if(ai == bi && ai != 0) {
                    if(b != bucket->size-1) {
                        //uint32_t ci = mask_collision_bits(((__global uint8_t*)src_digests[(bucket->data + b+1)->digest_index]) + byte_index, bit_index);
                        //if(ci == bi) {
                        //    has_dupe = 1;
                        //    break;
                        //}
                    }


                    uint32_t uncompressed_indices[NUM_INDICES];
                    decompress_indices(uncompressed_indices, buckets, base);
                    decompress_indices(uncompressed_indices + NUM_INDICES/2, buckets, el);

                    for(size_t k = 0; k < NUM_INDICES && !has_dupe; ++k) {
                        //printf("%u ", uncompressed_indices[k]);
                        for(size_t o = k+1; o < NUM_INDICES && !has_dupe; ++o) {
                            if(uncompressed_indices[k] == uncompressed_indices[o]) {
                                has_dupe = 1;
                            }
                        }
                    }
                    //has_dupe = 1;
                    //printf("\n\n");
                    if(!has_dupe) {
                        //atomic_add(n_solutions, 1);
                        memcpy_step0(dst_solutions + atomic_add(n_solutions, 1)*NUM_INDICES, uncompressed_indices, NUM_INDICES*sizeof(uint32_t));
                    } else {
                        break;
                    }
                }
            }
        }
        bucket->size = 0;
    }
}

//#pragma OPENCL EXTENSION cl_amd_printf : enable

#define BLAKE2B_BLOCKBYTES 128
#define BLAKE2B_OUTBYTES 64

#define n 200
#define k   9

#define collision_bit_length n / (k + 1)
#define init_size 1 << (collision_bit_length + 1)
#define indices_per_hash_out 512 / n
#define collision_byte_length (collision_bit_length + 7) / 8
#define hash_len (k + 1) * collision_byte_length
#define len_indices sizeof(eh_index)
#define full_width 2 * collision_byte_length + sizeof(eh_index) * (1 << (k-1))
#define hash_output indices_per_hash_out * n / 8

#define bswap32(x) (as_uint(as_uchar4(x).wzyx))

#if defined (__ENDIAN_LITTLE__)
#define htole32(x)              (x)
#define htobe32(x)              bswap32(x)
#else
#define htole32(x)              bswap32(x)
#define htobe32(x)              (x)
#endif

typedef uint eh_index;

typedef __attribute__((aligned(64))) struct crypto_generichash_blake2b_state {
    
    ulong h[8];
    ulong t[2];
    ulong f[2];
    uchar  buf[2 * BLAKE2B_BLOCKBYTES];
    size_t   buflen;
    uchar  last_node;
    
} crypto_generichash_blake2b_state;

typedef crypto_generichash_blake2b_state eh_hash_state;

__constant static ulong blake2b_IV[8] =
{
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
    0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
    0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

__constant static uchar blake2b_sigma[12][16] =
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

static inline ulong rotr64( __const ulong w, __const unsigned c ) { return ( w >> c ) | ( w << ( 64 - c ) ); }

void memcpy(void *dest, void *src, size_t len) {
    
    // Typecast src and dest addresses to (char *)
    char * csrc = (char *) src;
    char * cdest = (char *) dest;
    
    // Copy contents of src[] to dest[]
    for (int i = 0; i < len; i++)
        cdest[i] = csrc[i];
    
}

//INFO Possible issues with endianness here, look at C version
//I think that if little endian with a for loop is safer, but could not get it to work on OCL
ulong load64( __const void *src )
{
#if defined(__ENDIAN_LITTLE__)
    
    ulong w;
    memcpy(&w, src, sizeof(w));
    return w;
    /*__const ulong *p = ( __const ulong * )src;
    ulong w = *p;
    return w;*/
#else
    __const uchar *p = ( __const uchar * )src;
    ulong w = *p++;
    w |= ( ulong )( *p++ ) <<  8;
    w |= ( ulong )( *p++ ) << 16;
    w |= ( ulong )( *p++ ) << 24;
    w |= ( ulong )( *p++ ) << 32;
    w |= ( ulong )( *p++ ) << 40;
    w |= ( ulong )( *p++ ) << 48;
    w |= ( ulong )( *p++ ) << 56;
    return w;
#endif
}

//INFO Same problem as store
void store64( void *dst, ulong w )
{
#if defined(__ENDIAN_LITTLE__)
    memcpy(dst, &w, sizeof(w));
    /*ulong *p = ( ulong * )dst;
    *p = w;*/
#else
    uchar *p = ( uchar * )dst;
    *p++ = ( uchar )w; w >>= 8;
    *p++ = ( uchar )w; w >>= 8;
    *p++ = ( uchar )w; w >>= 8;
    *p++ = ( uchar )w; w >>= 8;
    *p++ = ( uchar )w; w >>= 8;
    *p++ = ( uchar )w; w >>= 8;
    *p++ = ( uchar )w; w >>= 8;
    *p++ = ( uchar )w;
#endif
}

int blake2b_increment_counter( eh_hash_state *S, __const ulong inc )
{
    S->t[0] += inc;
    S->t[1] += ( S->t[0] < inc );
    return 0;
}

int blake2b_is_lastblock( __const eh_hash_state *S )
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

static int blake2b_compress( eh_hash_state *S, __const uchar block[BLAKE2B_BLOCKBYTES] )
{
    ulong m[16];
    ulong v[16];
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

//INFO Possible issues with manual memcpys, look at commented memcpys
int blake2b_update(eh_hash_state *S, __const uchar *in, ulong inlen) {
    
    //int i;
    
    while( inlen > 0 )
    {
        size_t left = S->buflen;
        size_t fill = 2 * BLAKE2B_BLOCKBYTES - left;
        
        if( inlen > fill )
        {
            memcpy( S->buf + left, in, fill ); /* Fill buffer */
            /*for( i = 0; i < fill; ++i )
                (S->buf + left)[i] = in[i];*/
            S->buflen += fill;
            blake2b_increment_counter( S, BLAKE2B_BLOCKBYTES );
            blake2b_compress( S, S->buf ); /* Compress */
            memcpy( S->buf, S->buf + BLAKE2B_BLOCKBYTES, BLAKE2B_BLOCKBYTES ); /* Shift buffer left */
            /*for( i = 0; i < BLAKE2B_BLOCKBYTES; ++i )
                S->buf[i] = (S->buf + BLAKE2B_BLOCKBYTES)[i];*/
            S->buflen -= BLAKE2B_BLOCKBYTES;
            in += fill;
            inlen -= fill;
        }
        else /* inlen <= fill */
        {
            memcpy( S->buf + left, in, inlen );
            /*for( i = 0; i < inlen; ++i )
                (S->buf + left)[i] = in[i];*/
            S->buflen += inlen; /* Be lazy, do not compress */
            in += inlen;
            inlen -= inlen;
        }
    }
    
    return 0;
    
}

//INFO Again custom memcpy and memset, danger...
int blake2b_final( eh_hash_state *S, uchar *out, uchar outlen )
{
    uchar buffer[BLAKE2B_OUTBYTES] = {0};
    int i;
    
    //INFO out == NULL changed to 0, since null is not available in OpenCL
    //although it should never be null or equal to 0 (in this case) I think 
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
        /*for( i = 0; i < S->buflen; ++i )
            S->buf[i] = (S->buf + BLAKE2B_BLOCKBYTES)[i];*/
        
    }
    
    blake2b_increment_counter( S, S->buflen );
    blake2b_set_lastblock( S );
    //memset( S->buf + S->buflen, 0, 2 * BLAKE2B_BLOCKBYTES - S->buflen ); /* Padding */
    for( i = 0; i < 2 * BLAKE2B_BLOCKBYTES - S->buflen; ++i )
        (S->buf + S->buflen)[i] = 0;
    
    blake2b_compress( S, S->buf );
    
    for( i = 0; i < 8; ++i ) /* Output full hash to temp buffer */
        store64( buffer + sizeof( S->h[i] ) * i, S->h[i] );
    
    memcpy( out, buffer, outlen );
    /*for( i = 0; i < outlen; ++i )
        out[i] = buffer[i];*/
    
    return 0;
}

void generate_hash(__const eh_hash_state * base_state, eh_index g, uchar * hash, size_t h_len) {
    
    eh_hash_state state = *base_state;
    //INFO we should guarantee order, I hope htole its working ok, also custom
    eh_index lei = htole32(g);
    blake2b_update(&state, (__const uchar *) &lei, sizeof(eh_index));
    blake2b_final(&state, hash, h_len);
    
}

void expand_array(__const uchar* in, size_t in_len,
                 uchar* out, size_t out_len,
                 size_t bit_len, size_t byte_pad) {
    
    size_t out_width = (bit_len + 7) / 8 + byte_pad;
    
    uint bit_len_mask = ((uint) 1 << bit_len) - 1;
    
    // The acc_bits least-significant bits of acc_value represent a bit sequence
    // in big-endian order.
    size_t acc_bits = 0;
    uint acc_value = 0;
    
    size_t j = 0;
    for (size_t i = 0; i < in_len; i++) {
        acc_value = (acc_value << 8) | in[i];
        acc_bits += 8;
        
        // When we have bit_len or more bits in the accumulator, write the next
        // output element.
        if (acc_bits >= bit_len) {
            acc_bits -= bit_len;
            for (size_t x = 0; x < byte_pad; x++) {
                out[j+x] = 0;
            }
            for (size_t x = byte_pad; x < out_width; x++) {
                out[j+x] = (
                            // Big-endian
                            acc_value >> (acc_bits+(8*(out_width-x-1)))
                            ) & (
                                 // Apply bit_len_mask across byte boundaries
                                 (bit_len_mask >> (8*(out_width-x-1))) & 0xFF
                                 );
            }
            j += out_width;
        }
    }
    
}

void eh_index_to_array(__const eh_index i, uchar * array) {
    
    //INFO More possible issues, freaking endianness
    eh_index bei = htobe32(i);
    memcpy(array, &bei, sizeof(eh_index));
    
}

__kernel void list_gen(__global ulong * headerIn) {
    
    size_t i = get_global_id(0);
    //output[i] = inputA[i] + inputB[i];
    
    //printf("%i\n", headerIn[3]);
    
    uchar tmp_hash[hash_output];
    eh_hash_state base_state;
    generate_hash(&base_state, i, tmp_hash, hash_output);
    
    eh_index j;
    for (j = 0; j < indices_per_hash_out; ++j) {
        
        if(i + j >= init_size)
            break;
        
        //tmp_hash+(j*N/8), N/8, HashLength, collision_bit_len, (i*indices_per_hash_out)+j
        //expand_arr -> hash_in|tmp_hash+(i*N/8)|, h_in_len|N/8|, hash, h_len|HashLength|,cbitlen|collision_bit_len|, 0
        //Eh_ind_to_arr -> i|(g*indices_per_hash_out)+i|, hash+hlen|full_width+HashLength|
        uchar hash[full_width];
        expand_array(tmp_hash+(j*n/8), n/8, hash, hash_len, collision_bit_length, 0);
        eh_index_to_array((i*indices_per_hash_out)+j, hash + (size_t) hash_len);
        
    }
    
}

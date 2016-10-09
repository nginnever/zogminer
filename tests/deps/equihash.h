#ifndef BITCOIN_EQUIHASH_H
#define BITCOIN_EQUIHASH_H

#include "sodium.h"

#include <cstring>
#include <exception>
#include <functional>
#include <memory>
#include <set>
#include <vector>


typedef crypto_generichash_blake2b_state eh_HashState;
typedef uint32_t eh_index;
typedef uint8_t eh_trunc;

void ExpandArray(const unsigned char* in, size_t in_len,
                 unsigned char* out, size_t out_len,
                 size_t bit_len, size_t byte_pad=0);
void CompressArray(const unsigned char* in, size_t in_len,
                   unsigned char* out, size_t out_len,
size_t bit_len, size_t byte_pad=0);

#endif // BITCOIN_EQUIHASH_H
/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:4;tab-width:4;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright The Mbed TLS Contributors                                          │
│                                                                              │
│ Licensed under the Apache License, Version 2.0 (the "License");              │
│ you may not use this file except in compliance with the License.             │
│ You may obtain a copy of the License at                                      │
│                                                                              │
│     http://www.apache.org/licenses/LICENSE-2.0                               │
│                                                                              │
│ Unless required by applicable law or agreed to in writing, software          │
│ distributed under the License is distributed on an "AS IS" BASIS,            │
│ WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.     │
│ See the License for the specific language governing permissions and          │
│ limitations under the License.                                               │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/dce.h"
#include "libc/intrin/asan.internal.h"
#include "libc/macros.internal.h"
#include "libc/nexgen32e/nexgen32e.h"
#include "libc/nexgen32e/sha.h"
#include "libc/nexgen32e/x86feature.h"
#include "libc/str/str.h"
#include "third_party/mbedtls/common.h"
#include "third_party/mbedtls/endian.h"
#include "third_party/mbedtls/error.h"
#include "third_party/mbedtls/md.h"
#include "third_party/mbedtls/platform.h"
#include "third_party/mbedtls/sha256.h"

asm(".ident\t\"\\n\\n\
Mbed TLS (Apache 2.0)\\n\
Copyright ARM Limited\\n\
Copyright Mbed TLS Contributors\"");
asm(".include \"libc/disclaimer.inc\"");
/* clang-format off */

/**
 * @fileoverview FIPS-180-2 compliant SHA-256 implementation
 *
 * The SHA-256 Secure Hash Standard was published by NIST in 2002.
 *
 * @see http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf
 */

#define SHA256_VALIDATE_RET(cond)                           \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_SHA256_BAD_INPUT_DATA )
#define SHA256_VALIDATE(cond)  MBEDTLS_INTERNAL_VALIDATE( cond )

#if !defined(MBEDTLS_SHA256_ALT)

/**
 * \brief          This function clones the state of a SHA-256 context.
 *
 * \param dst      The destination context. This must be initialized.
 * \param src      The context to clone. This must be initialized.
 */
void mbedtls_sha256_clone( mbedtls_sha256_context *dst,
                           const mbedtls_sha256_context *src )
{
    SHA256_VALIDATE( dst );
    SHA256_VALIDATE( src );
    *dst = *src;
}

int mbedtls_sha256_starts_224( mbedtls_sha256_context *ctx )
{
    SHA256_VALIDATE_RET( ctx );
    ctx->total[0] = 0;
    ctx->total[1] = 0;
    ctx->state[0] = 0xC1059ED8;
    ctx->state[1] = 0x367CD507;
    ctx->state[2] = 0x3070DD17;
    ctx->state[3] = 0xF70E5939;
    ctx->state[4] = 0xFFC00B31;
    ctx->state[5] = 0x68581511;
    ctx->state[6] = 0x64F98FA7;
    ctx->state[7] = 0xBEFA4FA4;
    ctx->is224 = true;
    return( 0 );
}

int mbedtls_sha256_starts_256( mbedtls_sha256_context *ctx )
{
    SHA256_VALIDATE_RET( ctx );
    ctx->total[0] = 0;
    ctx->total[1] = 0;
    ctx->state[0] = 0x6A09E667;
    ctx->state[1] = 0xBB67AE85;
    ctx->state[2] = 0x3C6EF372;
    ctx->state[3] = 0xA54FF53A;
    ctx->state[4] = 0x510E527F;
    ctx->state[5] = 0x9B05688C;
    ctx->state[6] = 0x1F83D9AB;
    ctx->state[7] = 0x5BE0CD19;
    ctx->is224 = false;
    return( 0 );
}

/**
 * \brief          This function starts a SHA-224 or SHA-256 checksum
 *                 calculation.
 *
 * \param ctx      The context to use. This must be initialized.
 * \param is224    This determines which function to use. This must be
 *                 either \c 0 for SHA-256, or \c 1 for SHA-224.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha256_starts_ret( mbedtls_sha256_context *ctx, int is224 )
{
    SHA256_VALIDATE_RET( ctx );
    SHA256_VALIDATE_RET( is224 == 0 || is224 == 1 );
    if( !is224 )
        return mbedtls_sha256_starts_256( ctx );
    else
        return mbedtls_sha256_starts_224( ctx );
}

#if !defined(MBEDTLS_SHA256_PROCESS_ALT)
#define K kSha256

#define  SHR(x,n) (((x) & 0xFFFFFFFF) >> (n))
#define ROTR(x,n) (SHR(x,n) | ((x) << (32 - (n))))

#define S0(x) (ROTR(x, 7) ^ ROTR(x,18) ^  SHR(x, 3))
#define S1(x) (ROTR(x,17) ^ ROTR(x,19) ^  SHR(x,10))

#define S2(x) (ROTR(x, 2) ^ ROTR(x,13) ^ ROTR(x,22))
#define S3(x) (ROTR(x, 6) ^ ROTR(x,11) ^ ROTR(x,25))

#define F0(x,y,z) (((x) & (y)) | ((z) & ((x) | (y))))
#define F1(x,y,z) ((z) ^ ((x) & ((y) ^ (z))))

#define R(t)                                                        \
    (                                                               \
        local.W[t] = S1(local.W[(t) -  2]) + local.W[(t) -  7] +    \
                     S0(local.W[(t) - 15]) + local.W[(t) - 16]      \
    )

#define P(a,b,c,d,e,f,g,h,x,K)                                      \
    do                                                              \
    {                                                               \
        local.temp1 = (h) + S3(e) + F1((e),(f),(g)) + (K) + (x);    \
        local.temp2 = S2(a) + F0((a),(b),(c));                      \
        (d) += local.temp1; (h) = local.temp1 + local.temp2;        \
    } while( 0 )

/**
 * \brief          This function processes a single data block within
 *                 the ongoing SHA-256 computation. This function is for
 *                 internal use only.
 *
 * \param ctx      The SHA-256 context. This must be initialized.
 * \param data     The buffer holding one block of data. This must
 *                 be a readable buffer of length \c 64 Bytes.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_internal_sha256_process( mbedtls_sha256_context *ctx,
                                     const unsigned char data[64] )
{
    struct
    {
        uint32_t temp1, temp2, W[64];
        uint32_t A[8];
    } local;
    unsigned int i;

    SHA256_VALIDATE_RET( ctx != NULL );
    SHA256_VALIDATE_RET( (const unsigned char *)data != NULL );

    if( !IsTiny() || X86_NEED( SHA ) )
    {
        if( X86_HAVE( SHA ) &&
            X86_HAVE( SSE2 ) &&
            X86_HAVE( SSSE3 ) )
        {
            if( IsAsan() )
                __asan_verify( data, 64 );
            sha256_transform_ni( ctx->state, data, 1 );
            return( 0 );
        }
        if( X86_HAVE( BMI2 ) &&
            X86_HAVE( AVX  ) &&
            X86_HAVE( AVX2 ) )
        {
            if( IsAsan() )
                __asan_verify( data, 64 );
            sha256_transform_rorx( ctx->state, data, 1 );
            return( 0 );
        }
    }

    for( i = 0; i < 8; i++ )
        local.A[i] = ctx->state[i];

#if defined(MBEDTLS_SHA256_SMALLER)
    for( i = 0; i < 64; i++ ) {
        if( i < 16 )
            GET_UINT32_BE( local.W[i], data, 4 * i );
        else
            R( i );
        P( local.A[0], local.A[1], local.A[2], local.A[3], local.A[4],
           local.A[5], local.A[6], local.A[7], local.W[i], K[i] );
        local.temp1 = local.A[7]; local.A[7] = local.A[6];
        local.A[6] = local.A[5]; local.A[5] = local.A[4];
        local.A[4] = local.A[3]; local.A[3] = local.A[2];
        local.A[2] = local.A[1]; local.A[1] = local.A[0];
        local.A[0] = local.temp1;
    }
#else /* MBEDTLS_SHA256_SMALLER */
    for( i = 0; i < 16; i++ )
        GET_UINT32_BE( local.W[i], data, 4 * i );
    for( i = 0; i < 16; i += 8 ) {
        P( local.A[0], local.A[1], local.A[2], local.A[3], local.A[4],
           local.A[5], local.A[6], local.A[7], local.W[i+0], K[i+0] );
        P( local.A[7], local.A[0], local.A[1], local.A[2], local.A[3],
           local.A[4], local.A[5], local.A[6], local.W[i+1], K[i+1] );
        P( local.A[6], local.A[7], local.A[0], local.A[1], local.A[2],
           local.A[3], local.A[4], local.A[5], local.W[i+2], K[i+2] );
        P( local.A[5], local.A[6], local.A[7], local.A[0], local.A[1],
           local.A[2], local.A[3], local.A[4], local.W[i+3], K[i+3] );
        P( local.A[4], local.A[5], local.A[6], local.A[7], local.A[0],
           local.A[1], local.A[2], local.A[3], local.W[i+4], K[i+4] );
        P( local.A[3], local.A[4], local.A[5], local.A[6], local.A[7],
           local.A[0], local.A[1], local.A[2], local.W[i+5], K[i+5] );
        P( local.A[2], local.A[3], local.A[4], local.A[5], local.A[6],
           local.A[7], local.A[0], local.A[1], local.W[i+6], K[i+6] );
        P( local.A[1], local.A[2], local.A[3], local.A[4], local.A[5],
           local.A[6], local.A[7], local.A[0], local.W[i+7], K[i+7] );
    }
    for( i = 16; i < 64; i += 8 ) {
        P( local.A[0], local.A[1], local.A[2], local.A[3], local.A[4],
           local.A[5], local.A[6], local.A[7], R(i+0), K[i+0] );
        P( local.A[7], local.A[0], local.A[1], local.A[2], local.A[3],
           local.A[4], local.A[5], local.A[6], R(i+1), K[i+1] );
        P( local.A[6], local.A[7], local.A[0], local.A[1], local.A[2],
           local.A[3], local.A[4], local.A[5], R(i+2), K[i+2] );
        P( local.A[5], local.A[6], local.A[7], local.A[0], local.A[1],
           local.A[2], local.A[3], local.A[4], R(i+3), K[i+3] );
        P( local.A[4], local.A[5], local.A[6], local.A[7], local.A[0],
           local.A[1], local.A[2], local.A[3], R(i+4), K[i+4] );
        P( local.A[3], local.A[4], local.A[5], local.A[6], local.A[7],
           local.A[0], local.A[1], local.A[2], R(i+5), K[i+5] );
        P( local.A[2], local.A[3], local.A[4], local.A[5], local.A[6],
           local.A[7], local.A[0], local.A[1], R(i+6), K[i+6] );
        P( local.A[1], local.A[2], local.A[3], local.A[4], local.A[5],
           local.A[6], local.A[7], local.A[0], R(i+7), K[i+7] );
    }
#endif /* MBEDTLS_SHA256_SMALLER */

    for( i = 0; i < 8; i++ )
        ctx->state[i] += local.A[i];

    /* Zeroise buffers and variables to clear sensitive data from memory. */
    mbedtls_platform_zeroize( &local, sizeof( local ) );

    return( 0 );
}

#endif /* !MBEDTLS_SHA256_PROCESS_ALT */

/**
 * \brief          This function feeds an input buffer into an ongoing
 *                 SHA-256 checksum calculation.
 *
 * \param ctx      The SHA-256 context. This must be initialized
 *                 and have a hash operation started.
 * \param input    The buffer holding the data. This must be a readable
 *                 buffer of length \p ilen Bytes.
 * \param ilen     The length of the input data in Bytes.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha256_update_ret( mbedtls_sha256_context *ctx,
                               const unsigned char *input,
                               size_t ilen )
{
    int ret = MBEDTLS_ERR_THIS_CORRUPTION;
    size_t fill;
    uint32_t left;

    SHA256_VALIDATE_RET( ctx != NULL );
    SHA256_VALIDATE_RET( ilen == 0 || input != NULL );

    if( ilen == 0 )
        return( 0 );

    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += (uint32_t) ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if( ctx->total[0] < (uint32_t) ilen )
        ctx->total[1]++;

    if( left && ilen >= fill )
    {
        memcpy( (void *) (ctx->buffer + left), input, fill );
        if( ( ret = mbedtls_internal_sha256_process( ctx, ctx->buffer ) ) != 0 )
            return( ret );
        input += fill;
        ilen  -= fill;
        left = 0;
    }

    if( ilen >= 64 )
    {
        if( !IsTiny() &&
            X86_HAVE( SHA ) &&
            X86_HAVE( SSE2 ) &&
            X86_HAVE( SSSE3 ) )
        {
            if( IsAsan() )
                __asan_verify( input, ilen );
            sha256_transform_ni( ctx->state, input, ilen / 64 );
            input += ROUNDDOWN( ilen, 64 );
            ilen  -= ROUNDDOWN( ilen, 64 );
        }
        else if( !IsTiny() &&
                 X86_HAVE( BMI  ) &&
                 X86_HAVE( BMI2 ) &&
                 X86_HAVE( AVX2 ) )
        {
            if( IsAsan() )
                __asan_verify( input, ilen );
            sha256_transform_rorx( ctx->state, input, ilen / 64 );
            input += ROUNDDOWN( ilen, 64 );
            ilen  -= ROUNDDOWN( ilen, 64 );
        }
        else
        {
            do
            {
                if(( ret = mbedtls_internal_sha256_process( ctx, input ) ))
                    return( ret );
                input += 64;
                ilen  -= 64;
            }
            while( ilen >= 64 );
        }
    }

    if( ilen > 0 )
        memcpy( (void *) (ctx->buffer + left), input, ilen );

    return( 0 );
}

/**
 * \brief          This function finishes the SHA-256 operation, and writes
 *                 the result to the output buffer.
 *
 * \param ctx      The SHA-256 context. This must be initialized
 *                 and have a hash operation started.
 * \param output   The SHA-224 or SHA-256 checksum result.
 *                 This must be a writable buffer of length \c 32 Bytes.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha256_finish_ret( mbedtls_sha256_context *ctx,
                               unsigned char output[32] )
{
    int ret = MBEDTLS_ERR_THIS_CORRUPTION;
    uint32_t used;
    uint32_t high, low;

    SHA256_VALIDATE_RET( ctx != NULL );
    SHA256_VALIDATE_RET( (unsigned char *)output != NULL );

    /*
     * Add padding: 0x80 then 0x00 until 8 bytes remain for the length
     */
    used = ctx->total[0] & 0x3F;

    ctx->buffer[used++] = 0x80;

    if( used <= 56 )
    {
        /* Enough room for padding + length in current block */
        mbedtls_platform_zeroize( ctx->buffer + used, 56 - used );
    }
    else
    {
        /* We'll need an extra block */
        mbedtls_platform_zeroize( ctx->buffer + used, 64 - used );

        if( ( ret = mbedtls_internal_sha256_process( ctx, ctx->buffer ) ) != 0 )
            return( ret );

        mbedtls_platform_zeroize( ctx->buffer, 56 );
    }

    /*
     * Add message length
     */
    high = ( ctx->total[0] >> 29 )
         | ( ctx->total[1] <<  3 );
    low  = ( ctx->total[0] <<  3 );

    PUT_UINT32_BE( high, ctx->buffer, 56 );
    PUT_UINT32_BE( low,  ctx->buffer, 60 );

    if( ( ret = mbedtls_internal_sha256_process( ctx, ctx->buffer ) ) != 0 )
        return( ret );

    /*
     * Output final state
     */
    PUT_UINT32_BE( ctx->state[0], output,  0 );
    PUT_UINT32_BE( ctx->state[1], output,  4 );
    PUT_UINT32_BE( ctx->state[2], output,  8 );
    PUT_UINT32_BE( ctx->state[3], output, 12 );
    PUT_UINT32_BE( ctx->state[4], output, 16 );
    PUT_UINT32_BE( ctx->state[5], output, 20 );
    PUT_UINT32_BE( ctx->state[6], output, 24 );

    if( ctx->is224 == 0 )
        PUT_UINT32_BE( ctx->state[7], output, 28 );

    return( 0 );
}

#endif /* !MBEDTLS_SHA256_ALT */

/**
 * \brief          This function calculates the SHA-224 or SHA-256
 *                 checksum of a buffer.
 *
 *                 The function allocates the context, performs the
 *                 calculation, and frees the context.
 *
 *                 The SHA-256 result is calculated as
 *                 output = SHA-256(input buffer).
 *
 * \param input    The buffer holding the data. This must be a readable
 *                 buffer of length \p ilen Bytes.
 * \param ilen     The length of the input data in Bytes.
 * \param output   The SHA-224 or SHA-256 checksum result. This must
 *                 be a writable buffer of length \c 32 Bytes.
 * \param is224    Determines which function to use. This must be
 *                 either \c 0 for SHA-256, or \c 1 for SHA-224.
 */
int mbedtls_sha256_ret( const void *input,
                        size_t ilen,
                        unsigned char output[32],
                        int is224 )
{
    int ret = MBEDTLS_ERR_THIS_CORRUPTION;
    mbedtls_sha256_context ctx;

    SHA256_VALIDATE_RET( is224 == 0 || is224 == 1 );
    SHA256_VALIDATE_RET( ilen == 0 || input != NULL );
    SHA256_VALIDATE_RET( (unsigned char *)output != NULL );

    mbedtls_sha256_init( &ctx );

    if( ( ret = mbedtls_sha256_starts_ret( &ctx, is224 ) ) != 0 )
        goto exit;

    if( ( ret = mbedtls_sha256_update_ret( &ctx, input, ilen ) ) != 0 )
        goto exit;

    if( ( ret = mbedtls_sha256_finish_ret( &ctx, output ) ) != 0 )
        goto exit;

exit:
    mbedtls_sha256_free( &ctx );

    return( ret );
}

noinstrument int mbedtls_sha256_ret_224( const void *input, size_t ilen, unsigned char *output )
{
    return mbedtls_sha256_ret( input, ilen, output, true );
}

noinstrument int mbedtls_sha256_ret_256( const void *input, size_t ilen, unsigned char *output )
{
    return mbedtls_sha256_ret( input, ilen, output, false );
}

const mbedtls_md_info_t mbedtls_sha224_info = {
    "SHA224",
    MBEDTLS_MD_SHA224,
    28,
    64,
    (void *)mbedtls_sha256_starts_224,
    (void *)mbedtls_sha256_update_ret,
    (void *)mbedtls_internal_sha256_process,
    (void *)mbedtls_sha256_finish_ret,
    mbedtls_sha256_ret_224,
};

const mbedtls_md_info_t mbedtls_sha256_info = {
    "SHA256",
    MBEDTLS_MD_SHA256,
    32,
    64,
    (void *)mbedtls_sha256_starts_256,
    (void *)mbedtls_sha256_update_ret,
    (void *)mbedtls_internal_sha256_process,
    (void *)mbedtls_sha256_finish_ret,
    mbedtls_sha256_ret_256,
};

#if defined(MBEDTLS_SELF_TEST)
/*
 * FIPS-180-2 test vectors
 */
static const unsigned char sha256_test_buf[3][57] =
{
    { "abc" },
    { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" },
    { "" }
};

static const size_t sha256_test_buflen[3] =
{
    3, 56, 1000
};

static const unsigned char sha256_test_sum[6][32] =
{
    /*
     * SHA-224 test vectors
     */
    { 0x23, 0x09, 0x7D, 0x22, 0x34, 0x05, 0xD8, 0x22,
      0x86, 0x42, 0xA4, 0x77, 0xBD, 0xA2, 0x55, 0xB3,
      0x2A, 0xAD, 0xBC, 0xE4, 0xBD, 0xA0, 0xB3, 0xF7,
      0xE3, 0x6C, 0x9D, 0xA7 },
    { 0x75, 0x38, 0x8B, 0x16, 0x51, 0x27, 0x76, 0xCC,
      0x5D, 0xBA, 0x5D, 0xA1, 0xFD, 0x89, 0x01, 0x50,
      0xB0, 0xC6, 0x45, 0x5C, 0xB4, 0xF5, 0x8B, 0x19,
      0x52, 0x52, 0x25, 0x25 },
    { 0x20, 0x79, 0x46, 0x55, 0x98, 0x0C, 0x91, 0xD8,
      0xBB, 0xB4, 0xC1, 0xEA, 0x97, 0x61, 0x8A, 0x4B,
      0xF0, 0x3F, 0x42, 0x58, 0x19, 0x48, 0xB2, 0xEE,
      0x4E, 0xE7, 0xAD, 0x67 },

    /*
     * SHA-256 test vectors
     */
    { 0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA,
      0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
      0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C,
      0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD },
    { 0x24, 0x8D, 0x6A, 0x61, 0xD2, 0x06, 0x38, 0xB8,
      0xE5, 0xC0, 0x26, 0x93, 0x0C, 0x3E, 0x60, 0x39,
      0xA3, 0x3C, 0xE4, 0x59, 0x64, 0xFF, 0x21, 0x67,
      0xF6, 0xEC, 0xED, 0xD4, 0x19, 0xDB, 0x06, 0xC1 },
    { 0xCD, 0xC7, 0x6E, 0x5C, 0x99, 0x14, 0xFB, 0x92,
      0x81, 0xA1, 0xC7, 0xE2, 0x84, 0xD7, 0x3E, 0x67,
      0xF1, 0x80, 0x9A, 0x48, 0xA4, 0x97, 0x20, 0x0E,
      0x04, 0x6D, 0x39, 0xCC, 0xC7, 0x11, 0x2C, 0xD0 }
};

/**
 * \brief          The SHA-224 and SHA-256 checkup routine.
 *
 * \return         \c 0 on success.
 * \return         \c 1 on failure.
 */
int mbedtls_sha256_self_test( int verbose )
{
    int i, j, k, buflen, ret = 0;
    unsigned char *buf;
    unsigned char sha256sum[32];
    mbedtls_sha256_context ctx;
    buf = mbedtls_calloc( 1024, sizeof(unsigned char) );
    if( NULL == buf )
    {
        if( verbose != 0 )
            mbedtls_printf( "Buffer allocation failed\n" );
        return( 1 );
    }
    mbedtls_sha256_init( &ctx );
    for( i = 0; i < 6; i++ )
    {
        j = i % 3;
        k = i < 3;
        if( verbose != 0 )
            mbedtls_printf( "  SHA-%d test #%d: ", 256 - k * 32, j + 1 );
        if( ( ret = mbedtls_sha256_starts_ret( &ctx, k ) ) != 0 )
            goto fail;
        if( j == 2 )
        {
            memset( buf, 'a', buflen = 1000 );
            for( j = 0; j < 1000; j++ )
            {
                ret = mbedtls_sha256_update_ret( &ctx, buf, buflen );
                if( ret != 0 )
                    goto fail;
            }
        }
        else
        {
            ret = mbedtls_sha256_update_ret( &ctx, sha256_test_buf[j],
                                             sha256_test_buflen[j] );
            if( ret != 0 )
                 goto fail;
        }
        if( ( ret = mbedtls_sha256_finish_ret( &ctx, sha256sum ) ) != 0 )
            goto fail;
        if( timingsafe_bcmp( sha256sum, sha256_test_sum[i], 32 - k * 4 ) != 0 )
        {
            ret = 1;
            goto fail;
        }
        if( verbose != 0 )
            mbedtls_printf( "passed\n" );
    }
    if( verbose != 0 )
        mbedtls_printf( "\n" );
    goto exit;
fail:
    if( verbose != 0 )
        mbedtls_printf( "failed\n" );
exit:
    mbedtls_sha256_free( &ctx );
    mbedtls_free( buf );
    return( ret );
}

#endif /* MBEDTLS_SELF_TEST */

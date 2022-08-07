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
#include "libc/intrin/asan.internal.h"
#include "libc/literal.h"
#include "libc/macros.internal.h"
#include "libc/nexgen32e/nexgen32e.h"
#include "libc/nexgen32e/x86feature.h"
#include "libc/str/str.h"
#include "third_party/mbedtls/chk.h"
#include "third_party/mbedtls/common.h"
#include "third_party/mbedtls/endian.h"
#include "third_party/mbedtls/error.h"
#include "third_party/mbedtls/md.h"
#include "third_party/mbedtls/platform.h"
#include "third_party/mbedtls/sha512.h"

asm(".ident\t\"\\n\\n\
Mbed TLS (Apache 2.0)\\n\
Copyright ARM Limited\\n\
Copyright Mbed TLS Contributors\"");
asm(".include \"libc/disclaimer.inc\"");
/* clang-format off */

/**
 * @fileoverview FIPS-180-2 compliant SHA-384/512 implementation
 *
 * The SHA-512 Secure Hash Standard was published by NIST in 2002.
 *
 * @see http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf
 */

void sha512_transform_rorx(mbedtls_sha512_context *, const uint8_t *, int);

#if defined(MBEDTLS_SHA512_C)

#define SHA512_VALIDATE_RET(cond)                           \
    MBEDTLS_INTERNAL_VALIDATE_RET( cond, MBEDTLS_ERR_SHA512_BAD_INPUT_DATA )
#define SHA512_VALIDATE(cond)  MBEDTLS_INTERNAL_VALIDATE( cond )

#if !defined(MBEDTLS_SHA512_ALT)

#define sha512_put_uint64_be    PUT_UINT64_BE

/**
 * \brief          This function clones the state of a SHA-512 context.
 *
 * \param dst      The destination context. This must be initialized.
 * \param src      The context to clone. This must be initialized.
 */
void mbedtls_sha512_clone( mbedtls_sha512_context *dst,
                           const mbedtls_sha512_context *src )
{
    SHA512_VALIDATE( dst );
    SHA512_VALIDATE( src );
    *dst = *src;
}

int mbedtls_sha512_starts_384( mbedtls_sha512_context *ctx )
{
    SHA512_VALIDATE_RET( ctx );
    ctx->total[0] = 0;
    ctx->total[1] = 0;
    ctx->state[0] = 0xCBBB9D5DC1059ED8;
    ctx->state[1] = 0x629A292A367CD507;
    ctx->state[2] = 0x9159015A3070DD17;
    ctx->state[3] = 0x152FECD8F70E5939;
    ctx->state[4] = 0x67332667FFC00B31;
    ctx->state[5] = 0x8EB44A8768581511;
    ctx->state[6] = 0xDB0C2E0D64F98FA7;
    ctx->state[7] = 0x47B5481DBEFA4FA4;
    ctx->is384 = true;
    return( 0 );
}

int mbedtls_sha512_starts_512( mbedtls_sha512_context *ctx )
{
    SHA512_VALIDATE_RET( ctx );
    ctx->total[0] = 0;
    ctx->total[1] = 0;
    ctx->state[0] = 0x6A09E667F3BCC908;
    ctx->state[1] = 0xBB67AE8584CAA73B;
    ctx->state[2] = 0x3C6EF372FE94F82B;
    ctx->state[3] = 0xA54FF53A5F1D36F1;
    ctx->state[4] = 0x510E527FADE682D1;
    ctx->state[5] = 0x9B05688C2B3E6C1F;
    ctx->state[6] = 0x1F83D9ABFB41BD6B;
    ctx->state[7] = 0x5BE0CD19137E2179;
    ctx->is384 = false;
    return( 0 );
}

/**
 * \brief          This function starts a SHA-384 or SHA-512 checksum
 *                 calculation.
 *
 * \param ctx      The SHA-512 context to use. This must be initialized.
 * \param is384    Determines which function to use. This must be
 *                 either \c 0 for SHA-512, or \c 1 for SHA-384.
 *
 * \note           When \c MBEDTLS_SHA512_NO_SHA384 is defined, \p is384 must
 *                 be \c 0, or the function will return
 *                 #MBEDTLS_ERR_SHA512_BAD_INPUT_DATA.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha512_starts_ret( mbedtls_sha512_context *ctx, int is384 )
{
    SHA512_VALIDATE_RET( ctx );
    SHA512_VALIDATE_RET( is384 == 0 || is384 == 1 );
    if( !is384 )
        return mbedtls_sha512_starts_512( ctx );
    else
        return mbedtls_sha512_starts_384( ctx );
}

#if !defined(MBEDTLS_SHA512_PROCESS_ALT)

#define SHR(x,n)  ((x) >> (n))
#define ROR(x,n)  (SHR((x),(n)) | ((x) << (64 - (n))))
#define S0(x)     (ROR(x, 1) ^ ROR(x, 8) ^ SHR(x, 7))
#define S1(x)     (ROR(x,19) ^ ROR(x,61) ^ SHR(x, 6))
#define S2(x)     (ROR(x,28) ^ ROR(x,34) ^ ROR(x,39))
#define S3(x)     (ROR(x,14) ^ ROR(x,18) ^ ROR(x,41))
#define F0(x,y,z) (((x) & (y)) | ((z) & ((x) | (y))))
#define F1(x,y,z) ((z) ^ ((x) & ((y) ^ (z))))
#define P(a,b,c,d,e,f,g,h,x,k)                                      \
    do                                                              \
    {                                                               \
        local.temp1 = (h) + S3(e) + F1((e),(f),(g)) + (k) + (x);    \
        local.temp2 = S2(a) + F0((a),(b),(c));                      \
        (d) += local.temp1; (h) = local.temp1 + local.temp2;        \
    } while( 0 )

/**
 * \brief          This function processes a single data block within
 *                 the ongoing SHA-512 computation.
 *                 This function is for internal use only.
 *
 * \param ctx      The SHA-512 context. This must be initialized.
 * \param data     The buffer holding one block of data. This
 *                 must be a readable buffer of length \c 128 Bytes.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_internal_sha512_process( mbedtls_sha512_context *ctx,
                                     const unsigned char data[128] )
{
    int i;
    struct
    {
        uint64_t temp1, temp2, W[80];
        uint64_t A[8];
    } local;
    SHA512_VALIDATE_RET( ctx != NULL );
    SHA512_VALIDATE_RET( (const unsigned char *)data != NULL );

    if( !IsTiny() && X86_HAVE(AVX2) )
    {
        if (IsAsan())
            __asan_verify(data, 128);
        sha512_transform_rorx(ctx, data, 1);
        return 0;
    }

    for( i = 0; i < 8; i++ )
        local.A[i] = ctx->state[i];
#if defined(MBEDTLS_SHA512_SMALLER)
    for( i = 0; i < 80; i++ )
    {
        if( i < 16 )
        {
            GET_UINT64_BE( local.W[i], data, i << 3 );
        }
        else
        {
            local.W[i] = S1(local.W[i -  2]) + local.W[i -  7] +
                   S0(local.W[i - 15]) + local.W[i - 16];
        }
        P( local.A[0], local.A[1], local.A[2], local.A[3], local.A[4],
           local.A[5], local.A[6], local.A[7], local.W[i], kSha512[i] );
        local.temp1 = local.A[7];
        local.A[7] = local.A[6];
        local.A[6] = local.A[5];
        local.A[5] = local.A[4];
        local.A[4] = local.A[3];
        local.A[3] = local.A[2];
        local.A[2] = local.A[1];
        local.A[1] = local.A[0];
        local.A[0] = local.temp1;
    }
#else /* MBEDTLS_SHA512_SMALLER */
    for( i = 0; i < 16; i++ )
    {
        GET_UINT64_BE( local.W[i], data, i << 3 );
    }
    for( ; i < 80; i++ )
    {
        local.W[i] = S1(local.W[i -  2]) + local.W[i -  7] +
               S0(local.W[i - 15]) + local.W[i - 16];
    }
    i = 0;
    do
    {
        P( local.A[0], local.A[1], local.A[2], local.A[3], local.A[4],
           local.A[5], local.A[6], local.A[7], local.W[i], kSha512[i] ); i++;
        P( local.A[7], local.A[0], local.A[1], local.A[2], local.A[3],
           local.A[4], local.A[5], local.A[6], local.W[i], kSha512[i] ); i++;
        P( local.A[6], local.A[7], local.A[0], local.A[1], local.A[2],
           local.A[3], local.A[4], local.A[5], local.W[i], kSha512[i] ); i++;
        P( local.A[5], local.A[6], local.A[7], local.A[0], local.A[1],
           local.A[2], local.A[3], local.A[4], local.W[i], kSha512[i] ); i++;
        P( local.A[4], local.A[5], local.A[6], local.A[7], local.A[0],
           local.A[1], local.A[2], local.A[3], local.W[i], kSha512[i] ); i++;
        P( local.A[3], local.A[4], local.A[5], local.A[6], local.A[7],
           local.A[0], local.A[1], local.A[2], local.W[i], kSha512[i] ); i++;
        P( local.A[2], local.A[3], local.A[4], local.A[5], local.A[6],
           local.A[7], local.A[0], local.A[1], local.W[i], kSha512[i] ); i++;
        P( local.A[1], local.A[2], local.A[3], local.A[4], local.A[5],
           local.A[6], local.A[7], local.A[0], local.W[i], kSha512[i] ); i++;
    }
    while( i < 80 );
#endif /* MBEDTLS_SHA512_SMALLER */
    for( i = 0; i < 8; i++ )
        ctx->state[i] += local.A[i];
    /* Zeroise buffers and variables to clear sensitive data from memory. */
    mbedtls_platform_zeroize( &local, sizeof( local ) );
    return( 0 );
}

#endif /* !MBEDTLS_SHA512_PROCESS_ALT */

/**
 * \brief          This function feeds an input buffer into an ongoing
 *                 SHA-512 checksum calculation.
 *
 * \param ctx      The SHA-512 context. This must be initialized
 *                 and have a hash operation started.
 * \param input    The buffer holding the input data. This must
 *                 be a readable buffer of length \p ilen Bytes.
 * \param ilen     The length of the input data in Bytes.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha512_update_ret( mbedtls_sha512_context *ctx,
                               const unsigned char *input,
                               size_t ilen )
{
    int ret = MBEDTLS_ERR_THIS_CORRUPTION;
    size_t fill;
    unsigned int left;
    SHA512_VALIDATE_RET( ctx != NULL );
    SHA512_VALIDATE_RET( ilen == 0 || input != NULL );
    if( ilen == 0 )
        return( 0 );
    left = (unsigned int) (ctx->total[0] & 0x7F);
    fill = 128 - left;
    ctx->total[0] += (uint64_t) ilen;
    if( ctx->total[0] < (uint64_t) ilen )
        ctx->total[1]++;
    if( left && ilen >= fill )
    {
        memcpy( (void *) (ctx->buffer + left), input, fill );
        if( ( ret = mbedtls_internal_sha512_process( ctx, ctx->buffer ) ) != 0 )
            return( ret );
        input += fill;
        ilen  -= fill;
        left = 0;
    }
    if (!IsTiny() && ilen >= 128 && X86_HAVE(AVX2)) {
        if (IsAsan()) __asan_verify(input, ilen / 128 * 128);
        sha512_transform_rorx(ctx, input, ilen / 128);
        input += ROUNDDOWN(ilen, 128);
        ilen  -= ROUNDDOWN(ilen, 128);
    }
    while( ilen >= 128 )
    {
        if( ( ret = mbedtls_internal_sha512_process( ctx, input ) ) != 0 )
            return( ret );
        input += 128;
        ilen  -= 128;
    }
    if( ilen > 0 )
        memcpy( (void *) (ctx->buffer + left), input, ilen );
    return( 0 );
}

/**
 * \brief          This function finishes the SHA-512 operation, and writes
 *                 the result to the output buffer.
 *
 * \param ctx      The SHA-512 context. This must be initialized
 *                 and have a hash operation started.
 * \param output   The SHA-384 or SHA-512 checksum result.
 *                 This must be a writable buffer of length \c 64 Bytes.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha512_finish_ret( mbedtls_sha512_context *ctx,
                               unsigned char output[64] )
{
    int ret = MBEDTLS_ERR_THIS_CORRUPTION;
    unsigned used;
    uint64_t high, low;
    SHA512_VALIDATE_RET( ctx != NULL );
    SHA512_VALIDATE_RET( (unsigned char *)output != NULL );
    /*
     * Add padding: 0x80 then 0x00 until 16 bytes remain for the length
     */
    used = ctx->total[0] & 0x7F;
    ctx->buffer[used++] = 0x80;
    if( used <= 112 )
    {
        /* Enough room for padding + length in current block */
        mbedtls_platform_zeroize( ctx->buffer + used, 112 - used );
    }
    else
    {
        /* We'll need an extra block */
        mbedtls_platform_zeroize( ctx->buffer + used, 128 - used );
        if( ( ret = mbedtls_internal_sha512_process( ctx, ctx->buffer ) ) != 0 )
            return( ret );
        mbedtls_platform_zeroize( ctx->buffer, 112 );
    }
    /*
     * Add message length
     */
    high = ( ctx->total[0] >> 61 )
         | ( ctx->total[1] <<  3 );
    low  = ( ctx->total[0] <<  3 );
    sha512_put_uint64_be( high, ctx->buffer, 112 );
    sha512_put_uint64_be( low,  ctx->buffer, 120 );
    if( ( ret = mbedtls_internal_sha512_process( ctx, ctx->buffer ) ) != 0 )
        return( ret );
    /*
     * Output final state
     */
    sha512_put_uint64_be( ctx->state[0], output,  0 );
    sha512_put_uint64_be( ctx->state[1], output,  8 );
    sha512_put_uint64_be( ctx->state[2], output, 16 );
    sha512_put_uint64_be( ctx->state[3], output, 24 );
    sha512_put_uint64_be( ctx->state[4], output, 32 );
    sha512_put_uint64_be( ctx->state[5], output, 40 );
#if !defined(MBEDTLS_SHA512_NO_SHA384)
    if( ctx->is384 == 0 )
#endif
    {
        sha512_put_uint64_be( ctx->state[6], output, 48 );
        sha512_put_uint64_be( ctx->state[7], output, 56 );
    }
    return( 0 );
}

#endif /* !MBEDTLS_SHA512_ALT */

/**
 * \brief          This function calculates the SHA-512 or SHA-384
 *                 checksum of a buffer.
 *
 *                 The function allocates the context, performs the
 *                 calculation, and frees the context.
 *
 *                 The SHA-512 result is calculated as
 *                 output = SHA-512(input buffer).
 *
 * \param input    The buffer holding the input data. This must be
 *                 a readable buffer of length \p ilen Bytes.
 * \param ilen     The length of the input data in Bytes.
 * \param output   The SHA-384 or SHA-512 checksum result.
 *                 This must be a writable buffer of length \c 64 Bytes.
 * \param is384    Determines which function to use. This must be either
 *                 \c 0 for SHA-512, or \c 1 for SHA-384.
 *
 * \note           When \c MBEDTLS_SHA512_NO_SHA384 is defined, \p is384 must
 *                 be \c 0, or the function will return
 *                 #MBEDTLS_ERR_SHA512_BAD_INPUT_DATA.
 *
 * \return         \c 0 on success.
 * \return         A negative error code on failure.
 */
int mbedtls_sha512_ret( const void *input,
                        size_t ilen,
                        unsigned char output[64],
                        int is384 )
{
    int ret = MBEDTLS_ERR_THIS_CORRUPTION;
    mbedtls_sha512_context ctx;
#if !defined(MBEDTLS_SHA512_NO_SHA384)
    SHA512_VALIDATE_RET( is384 == 0 || is384 == 1 );
#else
    SHA512_VALIDATE_RET( is384 == 0 );
#endif
    SHA512_VALIDATE_RET( ilen == 0 || input );
    SHA512_VALIDATE_RET( (unsigned char *)output );
    mbedtls_sha512_init( &ctx );
    MBEDTLS_CHK( mbedtls_sha512_starts_ret( &ctx, is384 ) );
    MBEDTLS_CHK( mbedtls_sha512_update_ret( &ctx, input, ilen ) );
    MBEDTLS_CHK( mbedtls_sha512_finish_ret( &ctx, output ) );
cleanup:
    mbedtls_sha512_free( &ctx );
    return( ret );
}

noinstrument int mbedtls_sha512_ret_384( const void *input, size_t ilen, unsigned char *output )
{
    return mbedtls_sha512_ret( input, ilen, output, true );
}

noinstrument int mbedtls_sha512_ret_512( const void *input, size_t ilen, unsigned char *output )
{
    return mbedtls_sha512_ret( input, ilen, output, false );
}

#if !defined(MBEDTLS_SHA512_NO_SHA384)
const mbedtls_md_info_t mbedtls_sha384_info = {
    "SHA384",
    MBEDTLS_MD_SHA384,
    48,
    128,
    (void *)mbedtls_sha512_starts_384,
    (void *)mbedtls_sha512_update_ret,
    (void *)mbedtls_internal_sha512_process,
    (void *)mbedtls_sha512_finish_ret,
    mbedtls_sha512_ret_384,
};
#endif

const mbedtls_md_info_t mbedtls_sha512_info = {
    "SHA512",
    MBEDTLS_MD_SHA512,
    64,
    128,
    (void *)mbedtls_sha512_starts_512,
    (void *)mbedtls_sha512_update_ret,
    (void *)mbedtls_internal_sha512_process,
    (void *)mbedtls_sha512_finish_ret,
    mbedtls_sha512_ret_512,
};

#if defined(MBEDTLS_SELF_TEST)

/*
 * FIPS-180-2 test vectors
 */
static const unsigned char sha512_test_buf[3][113] =
{
    { "abc" },
    { "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
      "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu" },
    { "" }
};

static const size_t sha512_test_buflen[3] =
{
    3, 112, 1000
};

static const unsigned char sha512_test_sum[][64] =
{
#if !defined(MBEDTLS_SHA512_NO_SHA384)
    /*
     * SHA-384 test vectors
     */
    { 0xCB, 0x00, 0x75, 0x3F, 0x45, 0xA3, 0x5E, 0x8B,
      0xB5, 0xA0, 0x3D, 0x69, 0x9A, 0xC6, 0x50, 0x07,
      0x27, 0x2C, 0x32, 0xAB, 0x0E, 0xDE, 0xD1, 0x63,
      0x1A, 0x8B, 0x60, 0x5A, 0x43, 0xFF, 0x5B, 0xED,
      0x80, 0x86, 0x07, 0x2B, 0xA1, 0xE7, 0xCC, 0x23,
      0x58, 0xBA, 0xEC, 0xA1, 0x34, 0xC8, 0x25, 0xA7 },
    { 0x09, 0x33, 0x0C, 0x33, 0xF7, 0x11, 0x47, 0xE8,
      0x3D, 0x19, 0x2F, 0xC7, 0x82, 0xCD, 0x1B, 0x47,
      0x53, 0x11, 0x1B, 0x17, 0x3B, 0x3B, 0x05, 0xD2,
      0x2F, 0xA0, 0x80, 0x86, 0xE3, 0xB0, 0xF7, 0x12,
      0xFC, 0xC7, 0xC7, 0x1A, 0x55, 0x7E, 0x2D, 0xB9,
      0x66, 0xC3, 0xE9, 0xFA, 0x91, 0x74, 0x60, 0x39 },
    { 0x9D, 0x0E, 0x18, 0x09, 0x71, 0x64, 0x74, 0xCB,
      0x08, 0x6E, 0x83, 0x4E, 0x31, 0x0A, 0x4A, 0x1C,
      0xED, 0x14, 0x9E, 0x9C, 0x00, 0xF2, 0x48, 0x52,
      0x79, 0x72, 0xCE, 0xC5, 0x70, 0x4C, 0x2A, 0x5B,
      0x07, 0xB8, 0xB3, 0xDC, 0x38, 0xEC, 0xC4, 0xEB,
      0xAE, 0x97, 0xDD, 0xD8, 0x7F, 0x3D, 0x89, 0x85 },
#endif /* !MBEDTLS_SHA512_NO_SHA384 */
    /*
     * SHA-512 test vectors
     */
    { 0xDD, 0xAF, 0x35, 0xA1, 0x93, 0x61, 0x7A, 0xBA,
      0xCC, 0x41, 0x73, 0x49, 0xAE, 0x20, 0x41, 0x31,
      0x12, 0xE6, 0xFA, 0x4E, 0x89, 0xA9, 0x7E, 0xA2,
      0x0A, 0x9E, 0xEE, 0xE6, 0x4B, 0x55, 0xD3, 0x9A,
      0x21, 0x92, 0x99, 0x2A, 0x27, 0x4F, 0xC1, 0xA8,
      0x36, 0xBA, 0x3C, 0x23, 0xA3, 0xFE, 0xEB, 0xBD,
      0x45, 0x4D, 0x44, 0x23, 0x64, 0x3C, 0xE8, 0x0E,
      0x2A, 0x9A, 0xC9, 0x4F, 0xA5, 0x4C, 0xA4, 0x9F },
    { 0x8E, 0x95, 0x9B, 0x75, 0xDA, 0xE3, 0x13, 0xDA,
      0x8C, 0xF4, 0xF7, 0x28, 0x14, 0xFC, 0x14, 0x3F,
      0x8F, 0x77, 0x79, 0xC6, 0xEB, 0x9F, 0x7F, 0xA1,
      0x72, 0x99, 0xAE, 0xAD, 0xB6, 0x88, 0x90, 0x18,
      0x50, 0x1D, 0x28, 0x9E, 0x49, 0x00, 0xF7, 0xE4,
      0x33, 0x1B, 0x99, 0xDE, 0xC4, 0xB5, 0x43, 0x3A,
      0xC7, 0xD3, 0x29, 0xEE, 0xB6, 0xDD, 0x26, 0x54,
      0x5E, 0x96, 0xE5, 0x5B, 0x87, 0x4B, 0xE9, 0x09 },
    { 0xE7, 0x18, 0x48, 0x3D, 0x0C, 0xE7, 0x69, 0x64,
      0x4E, 0x2E, 0x42, 0xC7, 0xBC, 0x15, 0xB4, 0x63,
      0x8E, 0x1F, 0x98, 0xB1, 0x3B, 0x20, 0x44, 0x28,
      0x56, 0x32, 0xA8, 0x03, 0xAF, 0xA9, 0x73, 0xEB,
      0xDE, 0x0F, 0xF2, 0x44, 0x87, 0x7E, 0xA6, 0x0A,
      0x4C, 0xB0, 0x43, 0x2C, 0xE5, 0x77, 0xC3, 0x1B,
      0xEB, 0x00, 0x9C, 0x5C, 0x2C, 0x49, 0xAA, 0x2E,
      0x4E, 0xAD, 0xB2, 0x17, 0xAD, 0x8C, 0xC0, 0x9B }
};

/**
 * \brief          The SHA-384 or SHA-512 checkup routine.
 *
 * \return         \c 0 on success.
 * \return         \c 1 on failure.
 */
int mbedtls_sha512_self_test( int verbose )
{
    int i, j, k, buflen, ret = 0;
    unsigned char *buf;
    unsigned char sha512sum[64];
    mbedtls_sha512_context ctx;
    buf = mbedtls_calloc( 1024, sizeof(unsigned char) );
    if( NULL == buf )
    {
        if( verbose != 0 )
            mbedtls_printf( "Buffer allocation failed\n" );
        return( 1 );
    }
    mbedtls_sha512_init( &ctx );
    for( i = 0; i < (int) ARRAYLEN(sha512_test_sum); i++ )
    {
        j = i % 3;
#ifndef MBEDTLS_SHA512_NO_SHA384
        k = i < 3;
#else
        k = 0;
#endif
        if( verbose != 0 )
            mbedtls_printf( "  SHA-%d test #%d: ", 512 - k * 128, j + 1 );
        if( ( ret = mbedtls_sha512_starts_ret( &ctx, k ) ) != 0 )
            goto fail;
        if( j == 2 )
        {
            memset( buf, 'a', buflen = 1000 );
            for( j = 0; j < 1000; j++ )
            {
                ret = mbedtls_sha512_update_ret( &ctx, buf, buflen );
                if( ret != 0 )
                    goto fail;
            }
        }
        else
        {
            ret = mbedtls_sha512_update_ret( &ctx, sha512_test_buf[j],
                                             sha512_test_buflen[j] );
            if( ret != 0 )
                goto fail;
        }
        if( ( ret = mbedtls_sha512_finish_ret( &ctx, sha512sum ) ) != 0 )
            goto fail;
        if( timingsafe_bcmp( sha512sum, sha512_test_sum[i], 64 - k * 16 ) != 0 )
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
    mbedtls_sha512_free( &ctx );
    mbedtls_free( buf );
    return( ret );
}

#endif /* MBEDTLS_SELF_TEST */

#endif /* MBEDTLS_SHA512_C */

/*
 * SPDX-License-Identifier: Apache-2.0
 * Compact SHA-1/HMAC/WPA PRF used by the AIC8800 WPA2 handshake.
 * SHA-1 follows the public-domain Steve Reid implementation.
 */
#include "aic8800_wpa2.h"

#include <string.h>

struct aic_sha1 {
    uint32_t state[5];
    uint64_t bytes;
    uint8_t buffer[64];
};

static uint32_t rotl32(uint32_t value, unsigned int bits)
{
    return (value << bits) | (value >> (32U - bits));
}

static uint32_t get_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | data[3];
}

static void put_be32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

static void secure_zero(void *data, size_t length)
{
    volatile uint8_t *cursor = data;

    while (length-- != 0U) {
        *cursor++ = 0U;
    }
}

static void sha1_transform(uint32_t state[5], const uint8_t block[64])
{
    uint32_t words[80];
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    unsigned int index;

    for (index = 0U; index < 16U; index++) {
        words[index] = get_be32(block + index * 4U);
    }
    for (; index < 80U; index++) {
        words[index] = rotl32(words[index - 3U] ^ words[index - 8U] ^
                              words[index - 14U] ^ words[index - 16U], 1U);
    }
    for (index = 0U; index < 80U; index++) {
        uint32_t function;
        uint32_t constant;
        uint32_t temporary;

        if (index < 20U) {
            function = (b & c) | ((~b) & d);
            constant = 0x5a827999U;
        } else if (index < 40U) {
            function = b ^ c ^ d;
            constant = 0x6ed9eba1U;
        } else if (index < 60U) {
            function = (b & c) | (b & d) | (c & d);
            constant = 0x8f1bbcdcU;
        } else {
            function = b ^ c ^ d;
            constant = 0xca62c1d6U;
        }
        temporary = rotl32(a, 5U) + function + e + constant + words[index];
        e = d;
        d = c;
        c = rotl32(b, 30U);
        b = a;
        a = temporary;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    secure_zero(words, sizeof(words));
}

static void sha1_init(struct aic_sha1 *context)
{
    context->state[0] = 0x67452301U;
    context->state[1] = 0xefcdab89U;
    context->state[2] = 0x98badcfeU;
    context->state[3] = 0x10325476U;
    context->state[4] = 0xc3d2e1f0U;
    context->bytes = 0U;
}

static void sha1_update(struct aic_sha1 *context, const uint8_t *data,
                        size_t length)
{
    size_t used = (size_t)(context->bytes & 63U);

    context->bytes += length;
    if (used != 0U) {
        size_t copy = 64U - used;

        if (copy > length) {
            copy = length;
        }
        memcpy(context->buffer + used, data, copy);
        used += copy;
        data += copy;
        length -= copy;
        if (used == 64U) {
            sha1_transform(context->state, context->buffer);
        }
    }
    while (length >= 64U) {
        sha1_transform(context->state, data);
        data += 64U;
        length -= 64U;
    }
    if (length != 0U) {
        memcpy(context->buffer, data, length);
    }
}

static void sha1_finish(struct aic_sha1 *context, uint8_t digest[20])
{
    uint8_t tail[72];
    uint64_t bits = context->bytes * 8U;
    size_t used = (size_t)(context->bytes & 63U);
    size_t padding = (used < 56U) ? (56U - used) : (120U - used);
    unsigned int index;

    memset(tail, 0, padding + 8U);
    tail[0] = 0x80U;
    for (index = 0U; index < 8U; index++) {
        tail[padding + index] = (uint8_t)(bits >> (56U - index * 8U));
    }
    sha1_update(context, tail, padding + 8U);
    for (index = 0U; index < 5U; index++) {
        put_be32(digest + index * 4U, context->state[index]);
    }
    secure_zero(tail, sizeof(tail));
    secure_zero(context, sizeof(*context));
}

void aic8800_hmac_sha1(const uint8_t *key, size_t key_length,
                       const uint8_t *data, size_t data_length,
                       uint8_t digest[20])
{
    struct aic_sha1 context;
    uint8_t key_block[64];
    uint8_t inner[20];
    size_t index;

    memset(key_block, 0, sizeof(key_block));
    if (key_length > sizeof(key_block)) {
        sha1_init(&context);
        sha1_update(&context, key, key_length);
        sha1_finish(&context, key_block);
    } else {
        memcpy(key_block, key, key_length);
    }
    for (index = 0U; index < sizeof(key_block); index++) {
        key_block[index] ^= 0x36U;
    }
    sha1_init(&context);
    sha1_update(&context, key_block, sizeof(key_block));
    sha1_update(&context, data, data_length);
    sha1_finish(&context, inner);
    for (index = 0U; index < sizeof(key_block); index++) {
        key_block[index] ^= 0x36U ^ 0x5cU;
    }
    sha1_init(&context);
    sha1_update(&context, key_block, sizeof(key_block));
    sha1_update(&context, inner, sizeof(inner));
    sha1_finish(&context, digest);
    secure_zero(inner, sizeof(inner));
    secure_zero(key_block, sizeof(key_block));
}

void aic8800_wpa_prf(const uint8_t *key, size_t key_length,
                     const char *label, const uint8_t *data,
                     size_t data_length, uint8_t *output,
                     size_t output_length)
{
    uint8_t input[128];
    uint8_t digest[20];
    size_t label_length = strlen(label);
    size_t generated = 0U;
    uint8_t counter = 0U;

    if (label_length + data_length + 2U > sizeof(input)) {
        return;
    }
    memcpy(input, label, label_length);
    input[label_length] = 0U;
    memcpy(input + label_length + 1U, data, data_length);
    while (generated < output_length) {
        size_t copy = output_length - generated;

        input[label_length + 1U + data_length] = counter++;
        aic8800_hmac_sha1(key, key_length, input,
                          label_length + data_length + 2U, digest);
        if (copy > sizeof(digest)) {
            copy = sizeof(digest);
        }
        memcpy(output + generated, digest, copy);
        generated += copy;
    }
    secure_zero(input, sizeof(input));
    secure_zero(digest, sizeof(digest));
}

struct aic_aes_key {
    uint32_t words[44];
};

static const uint8_t aes_sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static const uint8_t aes_inverse_sbox[256] = {
    0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
    0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
    0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
    0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
    0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
    0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
    0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
    0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
    0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
    0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
    0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
    0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
    0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
    0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
    0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
    0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
};

static uint32_t aes_sub_word(uint32_t value)
{
    return ((uint32_t)aes_sbox[value >> 24] << 24) |
           ((uint32_t)aes_sbox[(value >> 16) & 0xffU] << 16) |
           ((uint32_t)aes_sbox[(value >> 8) & 0xffU] << 8) |
           aes_sbox[value & 0xffU];
}

static void aes_set_key(struct aic_aes_key *schedule, const uint8_t key[16])
{
    static const uint32_t rcon[11] = { 0U,0x01000000U,0x02000000U,0x04000000U,
        0x08000000U,0x10000000U,0x20000000U,0x40000000U,0x80000000U,
        0x1b000000U,0x36000000U };
    unsigned int index;

    for (index = 0U; index < 4U; index++) {
        schedule->words[index] = get_be32(key + index * 4U);
    }
    for (; index < 44U; index++) {
        uint32_t temporary = schedule->words[index - 1U];

        if ((index & 3U) == 0U) {
            temporary = aes_sub_word((temporary >> 24) | (temporary << 8)) ^
                        rcon[index / 4U];
        }
        schedule->words[index] = schedule->words[index - 4U] ^ temporary;
    }
}

static uint8_t aes_multiply(uint8_t value, uint8_t factor)
{
    uint8_t result = 0U;

    while (factor != 0U) {
        if ((factor & 1U) != 0U) result ^= value;
        value = (uint8_t)((value << 1) ^ ((value >> 7) * 0x1bU));
        factor >>= 1;
    }
    return result;
}

static void aes_add_round_key(uint8_t state[16], const uint32_t *words)
{
    unsigned int index;

    for (index = 0U; index < 4U; index++) {
        state[index * 4U] ^= (uint8_t)(words[index] >> 24);
        state[index * 4U + 1U] ^= (uint8_t)(words[index] >> 16);
        state[index * 4U + 2U] ^= (uint8_t)(words[index] >> 8);
        state[index * 4U + 3U] ^= (uint8_t)words[index];
    }
}

static void aes_decrypt_block(const struct aic_aes_key *schedule,
                              const uint8_t input[16], uint8_t output[16])
{
    uint8_t state[16];
    uint8_t copy[16];
    unsigned int round;
    unsigned int index;

    memcpy(state, input, 16U);
    aes_add_round_key(state, schedule->words + 40U);
    for (round = 9U; round > 0U; round--) {
        memcpy(copy, state, 16U);
        state[0]=copy[0]; state[1]=copy[13]; state[2]=copy[10]; state[3]=copy[7];
        state[4]=copy[4]; state[5]=copy[1]; state[6]=copy[14]; state[7]=copy[11];
        state[8]=copy[8]; state[9]=copy[5]; state[10]=copy[2]; state[11]=copy[15];
        state[12]=copy[12]; state[13]=copy[9]; state[14]=copy[6]; state[15]=copy[3];
        for (index = 0U; index < 16U; index++) state[index] = aes_inverse_sbox[state[index]];
        aes_add_round_key(state, schedule->words + round * 4U);
        memcpy(copy, state, 16U);
        for (index = 0U; index < 4U; index++) {
            const uint8_t *in = copy + index * 4U;
            uint8_t *out = state + index * 4U;
            out[0]=aes_multiply(in[0],14)^aes_multiply(in[1],11)^aes_multiply(in[2],13)^aes_multiply(in[3],9);
            out[1]=aes_multiply(in[0],9)^aes_multiply(in[1],14)^aes_multiply(in[2],11)^aes_multiply(in[3],13);
            out[2]=aes_multiply(in[0],13)^aes_multiply(in[1],9)^aes_multiply(in[2],14)^aes_multiply(in[3],11);
            out[3]=aes_multiply(in[0],11)^aes_multiply(in[1],13)^aes_multiply(in[2],9)^aes_multiply(in[3],14);
        }
    }
    memcpy(copy, state, 16U);
    state[0]=copy[0]; state[1]=copy[13]; state[2]=copy[10]; state[3]=copy[7];
    state[4]=copy[4]; state[5]=copy[1]; state[6]=copy[14]; state[7]=copy[11];
    state[8]=copy[8]; state[9]=copy[5]; state[10]=copy[2]; state[11]=copy[15];
    state[12]=copy[12]; state[13]=copy[9]; state[14]=copy[6]; state[15]=copy[3];
    for (index = 0U; index < 16U; index++) state[index] = aes_inverse_sbox[state[index]];
    aes_add_round_key(state, schedule->words);
    memcpy(output, state, 16U);
    secure_zero(state, sizeof(state));
    secure_zero(copy, sizeof(copy));
}

void aic8800_pbkdf2_sha1(const uint8_t *password, size_t password_length,
                         const uint8_t *salt, size_t salt_length,
                         uint32_t iterations, uint8_t *output,
                         size_t output_length)
{
    uint8_t block[36];
    uint8_t digest[20];
    uint8_t accumulator[20];
    uint32_t block_index = 1U;
    size_t generated = 0U;
    uint32_t round;
    unsigned int byte;

    if ((password == NULL) || (salt == NULL) || (output == NULL) ||
        (salt_length == 0U) || (salt_length > 32U) ||
        (iterations == 0U) || (output_length == 0U)) {
        return;
    }

    while (generated < output_length) {
        size_t copy = output_length - generated;

        memcpy(block, salt, salt_length);
        block[salt_length] = (uint8_t)(block_index >> 24);
        block[salt_length + 1U] = (uint8_t)(block_index >> 16);
        block[salt_length + 2U] = (uint8_t)(block_index >> 8);
        block[salt_length + 3U] = (uint8_t)block_index;
        aic8800_hmac_sha1(password, password_length, block, salt_length + 4U,
                          digest);
        memcpy(accumulator, digest, sizeof(accumulator));
        for (round = 1U; round < iterations; round++) {
            aic8800_hmac_sha1(password, password_length, digest,
                              sizeof(digest), digest);
            for (byte = 0U; byte < sizeof(accumulator); byte++) {
                accumulator[byte] ^= digest[byte];
            }
        }
        if (copy > sizeof(accumulator)) {
            copy = sizeof(accumulator);
        }
        memcpy(output + generated, accumulator, copy);
        generated += copy;
        block_index++;
    }
    secure_zero(block, sizeof(block));
    secure_zero(digest, sizeof(digest));
    secure_zero(accumulator, sizeof(accumulator));
}

int aic8800_aes_unwrap(const uint8_t kek[16], const uint8_t *input,
                       size_t input_length, uint8_t *output,
                       size_t *output_length)
{
    static const uint8_t initial[8] = {0xa6,0xa6,0xa6,0xa6,0xa6,0xa6,0xa6,0xa6};
    struct aic_aes_key schedule;
    uint8_t accumulator[8];
    uint8_t block[16];
    size_t count;
    size_t index;
    unsigned int round;

    if ((kek == NULL) || (input == NULL) || (output == NULL) ||
        (output_length == NULL) || (input_length < 24U) ||
        ((input_length & 7U) != 0U)) return -1;
    count = input_length / 8U - 1U;
    memcpy(accumulator, input, 8U);
    memcpy(output, input + 8U, input_length - 8U);
    aes_set_key(&schedule, kek);
    for (round = 6U; round-- > 0U;) {
        for (index = count; index > 0U; index--) {
            uint64_t counter = (uint64_t)count * round + index;
            unsigned int byte;

            memcpy(block, accumulator, 8U);
            for (byte = 0U; byte < 8U; byte++)
                block[7U - byte] ^= (uint8_t)(counter >> (byte * 8U));
            memcpy(block + 8U, output + (index - 1U) * 8U, 8U);
            aes_decrypt_block(&schedule, block, block);
            memcpy(accumulator, block, 8U);
            memcpy(output + (index - 1U) * 8U, block + 8U, 8U);
        }
    }
    *output_length = input_length - 8U;
    index = memcmp(accumulator, initial, 8U) == 0 ? 0U : 1U;
    secure_zero(&schedule, sizeof(schedule));
    secure_zero(accumulator, sizeof(accumulator));
    secure_zero(block, sizeof(block));
    if (index != 0U) {
        secure_zero(output, *output_length);
        *output_length = 0U;
        return -1;
    }
    return 0;
}

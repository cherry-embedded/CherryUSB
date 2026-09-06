/* SPDX-License-Identifier: Apache-2.0 */
#ifndef AIC8800_WPA2_H
#define AIC8800_WPA2_H

#include <stddef.h>
#include <stdint.h>

void aic8800_hmac_sha1(const uint8_t *key, size_t key_length,
                       const uint8_t *data, size_t data_length,
                       uint8_t digest[20]);
void aic8800_wpa_prf(const uint8_t *key, size_t key_length,
                     const char *label, const uint8_t *data,
                     size_t data_length, uint8_t *output,
                     size_t output_length);
int aic8800_aes_unwrap(const uint8_t kek[16], const uint8_t *input,
                       size_t input_length, uint8_t *output,
                       size_t *output_length);
void aic8800_pbkdf2_sha1(const uint8_t *password, size_t password_length,
                         const uint8_t *salt, size_t salt_length,
                         uint32_t iterations, uint8_t *output,
                         size_t output_length);

#endif

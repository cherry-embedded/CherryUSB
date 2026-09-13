/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Board-independent AIC8800D80 U02 firmware source.
 *
 * The BootROM loader only needs five contiguous images. How they get into
 * CPU-visible memory is a board problem:
 *
 *   1. Memory-mapped AICFWPKG (this board: XSPI NOR):
 *        aic8800_fw_set_package((const void *)base, capacity);
 *      or keep AIC8800_FW_PACKAGE_BASE and use the weak aic8800_fw_prepare().
 *
 *   2. Five vendor binaries already linked / copied to RAM:
 *        aic8800_fw_set_images(blobs);
 *
 *   3. SPI / filesystem / custom Flash: override aic8800_fw_prepare() and
 *      either map/copy the package then call set_package(), or fill five
 *      blobs and call set_images(). Call this before usbh_initialize().
 *
 * Images must stay readable until firmware download finishes.
 */
#ifndef AIC8800_FW_H
#define AIC8800_FW_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum aic8800_fw_image {
    AIC8800_FW_PATCH_TABLE = 0,
    AIC8800_FW_ADID,
    AIC8800_FW_PATCH,
    AIC8800_FW_PATCH_EXT0,
    AIC8800_FW_FMAC,
    AIC8800_FW_IMAGE_COUNT
};

struct aic8800_fw_blob {
    const uint8_t *data;
    uint32_t length;
};

int aic8800_fw_set_package(const void *package, uint32_t capacity);
int aic8800_fw_set_images(const struct aic8800_fw_blob
                          images[AIC8800_FW_IMAGE_COUNT]);
int aic8800_fw_prepare(void);
bool aic8800_fw_is_ready(void);
int aic8800_fw_get_image(enum aic8800_fw_image id, struct aic8800_fw_blob *blob);
const char *aic8800_fw_image_name(enum aic8800_fw_image id);

#ifdef __cplusplus
}
#endif

#endif /* AIC8800_FW_H */

/*
 * Copyright (c) 2024 ~ 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_ADB_H
#define USBD_ADB_H

#include <stdint.h>
#include "usb_vfs.h"

#define ADB_VERSION       0x01000000u
#define ADB_MAX_PAYLOAD   (4u * 1024u)
#define ADB_TX_TIMEOUT_MS 5000u
#define ADB_MAX_SERVICE   4u

#define ADB_LOCALID_SHELL 0x01u
#define ADB_LOCALID_SYNC  0x02u

/* -------------------------------------------------------------------------
 * adb packet commands
 *
 * The 4cc is stored as a little endian integer, exactly like the official
 * adb implementation does (A_CNXN == 0x4e584e43 == "CNXN" on the wire).
 * ---------------------------------------------------------------------- */
#define ADB_FOURCC(c0, c1, c2, c3) ((uint32_t)(c0) | ((uint32_t)(c1) << 8) | \
                                 ((uint32_t)(c2) << 16) | ((uint32_t)(c3) << 24))

#define A_SYNC ADB_FOURCC('S', 'Y', 'N', 'C')
#define A_CNXN ADB_FOURCC('C', 'N', 'X', 'N')
#define A_OPEN ADB_FOURCC('O', 'P', 'E', 'N')
#define A_OKAY ADB_FOURCC('O', 'K', 'A', 'Y')
#define A_CLSE ADB_FOURCC('C', 'L', 'S', 'E')
#define A_WRTE ADB_FOURCC('W', 'R', 'T', 'E')
#define A_AUTH ADB_FOURCC('A', 'U', 'T', 'H')

struct adb_msg {
    uint32_t command;     /* A_CNXN / A_OPEN / A_OKAY / A_CLSE / A_WRTE  */
    uint32_t arg0;        /* first argument                              */
    uint32_t arg1;        /* second argument                             */
    uint32_t data_length; /* length of the payload that follows          */
    uint32_t data_crc32;  /* checksum of the payload (plain 32-bit sum)  */
    uint32_t magic;       /* command ^ 0xffffffff                        */
};

struct adb_packet {
    USB_MEM_ALIGNX struct adb_msg msg;
    USB_MEM_ALIGNX uint8_t payload[ADB_MAX_PAYLOAD];
};

struct adb_service {
    const char *name; /* destination prefix, e.g. "shell:" / "sync:" */
    uint32_t localid; /* ADB_LOCALID_SHELL / ADB_LOCALID_FILE / ...  */
    void (*on_open)(uint32_t remoteid);
    void (*on_close)(uint32_t remoteid);
    void (*on_write)(uint32_t remoteid, const uint8_t *data, uint32_t len);
};

// clang-format off
#define ADB_DESCRIPTOR_INIT(bFirstInterface, in_ep, out_ep, wMaxPacketSize)                   \
    USB_INTERFACE_DESCRIPTOR_INIT(bFirstInterface, 0x00, 0x02, 0xff, 0x42, 0x01, 0x02), \
    USB_ENDPOINT_DESCRIPTOR_INIT(in_ep, 0x02, wMaxPacketSize, 0x00),     \
    USB_ENDPOINT_DESCRIPTOR_INIT(out_ep, 0x02, wMaxPacketSize, 0x00)
// clang-format on

#ifdef __cplusplus
extern "C" {
#endif

struct usbd_interface *usbd_adb_init_intf(uint8_t busid, struct usbd_interface *intf, uint8_t in_ep, uint8_t out_ep);
int usbd_adb_write(uint32_t localid,
                   const uint8_t *data,
                   uint32_t len);
void usbd_adb_close(uint32_t localid);

int usbd_adb_service_register(const struct adb_service *svc);
void usbd_adb_shell_init(void);
void usbd_adb_sync_init(void);

#ifdef __cplusplus
}
#endif

#endif /* USBD_ADB_H */
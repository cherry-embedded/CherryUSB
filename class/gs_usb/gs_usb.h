/*
 * Copyright (c) 2016 Hubert Denkmair
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef GS_USB_H
#define GS_USB_H

/*
 * gs_usb wire protocol definitions.
 *
 * Names and layout follow Linux drivers/net/can/usb/gs_usb.c
 */

enum gs_usb_breq {
    GS_USB_BREQ_HOST_FORMAT = 0,
    GS_USB_BREQ_BITTIMING,
    GS_USB_BREQ_MODE,
    GS_USB_BREQ_BERR,
    GS_USB_BREQ_BT_CONST,
    GS_USB_BREQ_DEVICE_CONFIG,
    GS_USB_BREQ_TIMESTAMP,
    GS_USB_BREQ_IDENTIFY,
    GS_USB_BREQ_GET_USER_ID,
    GS_USB_BREQ_QUIRK_CANTACT_PRO_DATA_BITTIMING = GS_USB_BREQ_GET_USER_ID,
    GS_USB_BREQ_SET_USER_ID,
    GS_USB_BREQ_DATA_BITTIMING,
    GS_USB_BREQ_BT_CONST_EXT,
    GS_USB_BREQ_SET_TERMINATION,
    GS_USB_BREQ_GET_TERMINATION,
    GS_USB_BREQ_GET_STATE,
};

enum gs_can_mode {
    /* reset a channel. turns it off */
    GS_CAN_MODE_RESET = 0,
    /* starts a channel */
    GS_CAN_MODE_START
};

enum gs_can_state {
    GS_CAN_STATE_ERROR_ACTIVE = 0,
    GS_CAN_STATE_ERROR_WARNING,
    GS_CAN_STATE_ERROR_PASSIVE,
    GS_CAN_STATE_BUS_OFF,
    GS_CAN_STATE_STOPPED,
    GS_CAN_STATE_SLEEPING
};

enum gs_can_identify_mode {
    GS_CAN_IDENTIFY_OFF = 0,
    GS_CAN_IDENTIFY_ON
};

enum gs_can_termination_state {
    GS_CAN_TERMINATION_STATE_OFF = 0,
    GS_CAN_TERMINATION_STATE_ON
};

#define GS_CAN_MODE_NORMAL        0
#define GS_CAN_MODE_LISTEN_ONLY   (1U << 0)
#define GS_CAN_MODE_LOOP_BACK     (1U << 1)
#define GS_CAN_MODE_TRIPLE_SAMPLE (1U << 2)
#define GS_CAN_MODE_ONE_SHOT      (1U << 3)
#define GS_CAN_MODE_HW_TIMESTAMP  (1U << 4)
/* GS_CAN_FEATURE_IDENTIFY BIT(5) */
/* GS_CAN_FEATURE_USER_ID BIT(6) */
#define GS_CAN_MODE_PAD_PKTS_TO_MAX_PKT_SIZE (1U << 7)
#define GS_CAN_MODE_FD                       (1U << 8)
/* GS_CAN_FEATURE_REQ_USB_QUIRK_LPC546XX BIT(9) */
/* GS_CAN_FEATURE_BT_CONST_EXT BIT(10) */
/* GS_CAN_FEATURE_TERMINATION BIT(11) */
#define GS_CAN_MODE_BERR_REPORTING (1U << 12)
/* GS_CAN_FEATURE_GET_STATE BIT(13) */

#define GS_CAN_FEATURE_LISTEN_ONLY              (1U << 0)
#define GS_CAN_FEATURE_LOOP_BACK                (1U << 1)
#define GS_CAN_FEATURE_TRIPLE_SAMPLE            (1U << 2)
#define GS_CAN_FEATURE_ONE_SHOT                 (1U << 3)
#define GS_CAN_FEATURE_HW_TIMESTAMP             (1U << 4)
#define GS_CAN_FEATURE_IDENTIFY                 (1U << 5)
#define GS_CAN_FEATURE_USER_ID                  (1U << 6)
#define GS_CAN_FEATURE_PAD_PKTS_TO_MAX_PKT_SIZE (1U << 7)
#define GS_CAN_FEATURE_FD                       (1U << 8)
#define GS_CAN_FEATURE_REQ_USB_QUIRK_LPC546XX   (1U << 9)
#define GS_CAN_FEATURE_BT_CONST_EXT             (1U << 10)
#define GS_CAN_FEATURE_TERMINATION              (1U << 11)
#define GS_CAN_FEATURE_BERR_REPORTING           (1U << 12)
#define GS_CAN_FEATURE_GET_STATE                (1U << 13)
#define GS_CAN_FEATURE_MASK                     ((1U << 14) - 1)

/* internal quirks - keep in GS_CAN_FEATURE space for now */

/* CANtact Pro original firmware:
 * BREQ DATA_BITTIMING overlaps with GET_USER_ID
 */
#define GS_CAN_FEATURE_QUIRK_BREQ_CANTACT_PRO (1U << 31)

#define GS_CAN_FLAG_OVERFLOW (1U << 0)
#define GS_CAN_FLAG_FD       (1U << 1)
#define GS_CAN_FLAG_BRS      (1U << 2)
#define GS_CAN_FLAG_ESI      (1U << 3)

#define CAN_EFF_FLAG 0x80000000U /* EFF/SFF is set in the MSB */
#define CAN_RTR_FLAG 0x40000000U /* remote transmission request */
#define CAN_ERR_FLAG 0x20000000U /* error message frame */

#define CAN_ERR_DLC 8 /* dlc for error message frames */

/* error class (mask) in can_id */
#define CAN_ERR_TX_TIMEOUT 0x00000001U /* TX timeout (by netdevice driver) */
#define CAN_ERR_LOSTARB    0x00000002U /* lost arbitration    / data[0]    */
#define CAN_ERR_CRTL       0x00000004U /* controller problems / data[1]    */
#define CAN_ERR_PROT       0x00000008U /* protocol violations / data[2..3] */
#define CAN_ERR_TRX        0x00000010U /* transceiver status  / data[4]    */
#define CAN_ERR_ACK        0x00000020U /* received no ACK on transmission */
#define CAN_ERR_BUSOFF     0x00000040U /* bus off */
#define CAN_ERR_BUSERROR   0x00000080U /* bus error (may flood!) */
#define CAN_ERR_RESTARTED  0x00000100U /* controller restarted */
#define CAN_ERR_CNT        0x00000200U /* TX error counter / data[6] */
                                       /* RX error counter / data[7] */

/* arbitration lost in bit ... / data[0] */
#define CAN_ERR_LOSTARB_UNSPEC 0x00 /* unspecified */
/* else bit number in bitstream */

/* error status of CAN-controller / data[1] */
#define CAN_ERR_CRTL_UNSPEC      0x00 /* unspecified */
#define CAN_ERR_CRTL_RX_OVERFLOW 0x01 /* RX buffer overflow */
#define CAN_ERR_CRTL_TX_OVERFLOW 0x02 /* TX buffer overflow */
#define CAN_ERR_CRTL_RX_WARNING  0x04 /* reached warning level for RX errors */
#define CAN_ERR_CRTL_TX_WARNING  0x08 /* reached warning level for TX errors */
#define CAN_ERR_CRTL_RX_PASSIVE  0x10 /* reached error passive status RX */
#define CAN_ERR_CRTL_TX_PASSIVE  0x20 /* reached error passive status TX */
/* (at least one error counter exceeds */
/* the protocol-defined level of 127)  */
#define CAN_ERR_CRTL_ACTIVE 0x40 /* recovered to error active state */

/* error in CAN protocol (type) / data[2] */
#define CAN_ERR_PROT_UNSPEC   0x00 /* unspecified */
#define CAN_ERR_PROT_BIT      0x01 /* single bit error */
#define CAN_ERR_PROT_FORM     0x02 /* frame format error */
#define CAN_ERR_PROT_STUFF    0x04 /* bit stuffing error */
#define CAN_ERR_PROT_BIT0     0x08 /* unable to send dominant bit */
#define CAN_ERR_PROT_BIT1     0x10 /* unable to send recessive bit */
#define CAN_ERR_PROT_OVERLOAD 0x20 /* bus overload */
#define CAN_ERR_PROT_ACTIVE   0x40 /* active error announcement */
#define CAN_ERR_PROT_TX       0x80 /* error occurred on transmission */

/* error in CAN protocol (location) / data[3] */
#define CAN_ERR_PROT_LOC_UNSPEC  0x00 /* unspecified */
#define CAN_ERR_PROT_LOC_SOF     0x03 /* start of frame */
#define CAN_ERR_PROT_LOC_ID28_21 0x02 /* ID bits 28 - 21 (SFF: 10 - 3) */
#define CAN_ERR_PROT_LOC_ID20_18 0x06 /* ID bits 20 - 18 (SFF: 2 - 0 )*/
#define CAN_ERR_PROT_LOC_SRTR    0x04 /* substitute RTR (SFF: RTR) */
#define CAN_ERR_PROT_LOC_IDE     0x05 /* identifier extension */
#define CAN_ERR_PROT_LOC_ID17_13 0x07 /* ID bits 17-13 */
#define CAN_ERR_PROT_LOC_ID12_05 0x0F /* ID bits 12-5 */
#define CAN_ERR_PROT_LOC_ID04_00 0x0E /* ID bits 4-0 */
#define CAN_ERR_PROT_LOC_RTR     0x0C /* RTR */
#define CAN_ERR_PROT_LOC_RES1    0x0D /* reserved bit 1 */
#define CAN_ERR_PROT_LOC_RES0    0x09 /* reserved bit 0 */
#define CAN_ERR_PROT_LOC_DLC     0x0B /* data length code */
#define CAN_ERR_PROT_LOC_DATA    0x0A /* data section */
#define CAN_ERR_PROT_LOC_CRC_SEQ 0x08 /* CRC sequence */
#define CAN_ERR_PROT_LOC_CRC_DEL 0x18 /* CRC delimiter */
#define CAN_ERR_PROT_LOC_ACK     0x19 /* ACK slot */
#define CAN_ERR_PROT_LOC_ACK_DEL 0x1B /* ACK delimiter */
#define CAN_ERR_PROT_LOC_EOF     0x1A /* end of frame */
#define CAN_ERR_PROT_LOC_INTERM  0x12 /* intermission */

/* error status of CAN-transceiver / data[4] */
/*                                             CANH CANL */
#define CAN_ERR_TRX_UNSPEC             0x00 /* 0000 0000 */
#define CAN_ERR_TRX_CANH_NO_WIRE       0x04 /* 0000 0100 */
#define CAN_ERR_TRX_CANH_SHORT_TO_BAT  0x05 /* 0000 0101 */
#define CAN_ERR_TRX_CANH_SHORT_TO_VCC  0x06 /* 0000 0110 */
#define CAN_ERR_TRX_CANH_SHORT_TO_GND  0x07 /* 0000 0111 */
#define CAN_ERR_TRX_CANL_NO_WIRE       0x40 /* 0100 0000 */
#define CAN_ERR_TRX_CANL_SHORT_TO_BAT  0x50 /* 0101 0000 */
#define CAN_ERR_TRX_CANL_SHORT_TO_VCC  0x60 /* 0110 0000 */
#define CAN_ERR_TRX_CANL_SHORT_TO_GND  0x70 /* 0111 0000 */
#define CAN_ERR_TRX_CANL_SHORT_TO_CANH 0x80 /* 1000 0000 */

struct gs_host_config {
    uint32_t byte_order;
} __PACKED;

struct gs_device_config {
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t reserved3;
    uint8_t icount;
    uint32_t sw_version;
    uint32_t hw_version;
} __PACKED;

struct gs_device_mode {
    uint32_t mode;
    uint32_t flags;
} __PACKED;

struct gs_device_state {
    uint32_t state;
    uint32_t rxerr;
    uint32_t txerr;
} __PACKED;

struct gs_device_bittiming {
    uint32_t prop_seg;
    uint32_t phase_seg1;
    uint32_t phase_seg2;
    uint32_t sjw;
    uint32_t brp;
} __PACKED;

struct gs_identify_mode {
    uint32_t mode;
} __PACKED;

struct gs_device_termination_state {
    uint32_t state;
} __PACKED;

/* Hardware bit-timing ranges. Linux stores these fields inline in gs_device_bt_const. */
struct can_bt_const {
    uint32_t tseg1_min; /* Minimum of (prop_seg + phase_seg1) */
    uint32_t tseg1_max;
    uint32_t tseg2_min;
    uint32_t tseg2_max;
    uint32_t sjw_max;
    uint32_t brp_min;
    uint32_t brp_max;
    uint32_t brp_inc; /* Prescaler step (1 or 2 depending on hardware) */
} __PACKED;

struct gs_device_bt_const {
    uint32_t feature;             /* GS_CAN_FEATURE_* bit mask */
    uint32_t fclk_can;            /* CAN clock in Hz, used with brp to form bit time */
    struct can_bt_const bt_const; /* Classic / arbitration timing limits */
} __PACKED;

/* Layout must begin with the same three members as gs_device_bt_const (feature,
 * fclk_can, bt_const), in the same order/size, so GET_BT_CONST can reuse the
 * leading bytes of this extended struct.
 */
struct gs_device_bt_const_extended {
    uint32_t feature;
    uint32_t fclk_can;
    struct can_bt_const bt_const;  /* Classic / arbitration timing limits */
    struct can_bt_const dbt_const; /* CAN FD data-phase timing limits */
} __PACKED;

struct classic_can {
    uint8_t data[8];
} __PACKED;

struct classic_can_ts {
    uint8_t data[8];
    uint32_t timestamp_us;
} __PACKED;

struct classic_can_quirk {
    uint8_t data[8];
    uint8_t quirk;
} __PACKED;

struct canfd {
    uint8_t data[64];
} __PACKED;

struct canfd_ts {
    uint8_t data[64];
    uint32_t timestamp_us;
} __PACKED;

struct canfd_quirk {
    uint8_t data[64];
    uint8_t quirk;
} __PACKED;

/* struct gs_host_frame::echo_id == GS_HOST_FRAME_ECHO_ID_RX indicates
 * a regular RX'ed CAN frame
 */
#define GS_HOST_FRAME_ECHO_ID_RX 0xffffffff

#define DECLARE_FLEX_ARRAY(_t, _n) \
    struct {                       \
        struct {                   \
        } __dummy_##_n;            \
        _t _n[0];                  \
    }

struct gs_host_frame {
    uint32_t echo_id;
    uint32_t can_id;

    uint8_t can_dlc;
    uint8_t channel;
    uint8_t flags;
    uint8_t reserved;

    union {
        DECLARE_FLEX_ARRAY(struct classic_can, classic_can);
        DECLARE_FLEX_ARRAY(struct classic_can_ts, classic_can_ts);
        DECLARE_FLEX_ARRAY(struct classic_can_quirk, classic_can_quirk);
        DECLARE_FLEX_ARRAY(struct canfd, canfd);
        DECLARE_FLEX_ARRAY(struct canfd_ts, canfd_ts);
        DECLARE_FLEX_ARRAY(struct canfd_quirk, canfd_quirk);
    };
} __PACKED;

#define struct_size(ptr, field, num) \
    (offsetof(__typeof(*(ptr)), field) + sizeof((ptr)->field[0]) * (num))

#define GS_HOST_CANFD_FRAME_SIZE struct_size((struct gs_host_frame *)NULL, canfd_ts, 1)
#define GS_HOST_CAN_FRAME_SIZE   struct_size((struct gs_host_frame *)NULL, classic_can, 1)

#endif // GS_USB_H
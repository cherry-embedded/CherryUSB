// SPDX-License-Identifier: GPL-2.0
/*
 * Silicon Laboratories CP210x USB to RS232 serial adaptor driver
 *
 * Copyright (C) 2005 Craig Shelley (craig@microtron.org.uk)
 *
 * Support to set flow control line levels using TIOCMGET and TIOCMSET
 * thanks to Karl Hiramoto karl@hiramoto.org. RTSCTS hardware flow
 * control thanks to Munir Nassar nassarmu@real-time.com
 *
 */

#include <stdint.h>
#include <stdlib.h>

#include <string.h>
#include <limits.h>

#include "usbh_cp210x.h"

#define u16 uint16_t
#define u32 uint32_t
#define u8  uint8_t

#ifndef container_of
#define container_of(p, t, m) \
    ((t *)((char *)p - (char *)(&(((t *)0)->m))))
#endif

#define dev_dbg(...)  do {} while (0)
#define dev_err  USB_LOG_ERR
#define dev_warn USB_LOG_WRN

#warning FIXME: le32_to_cpu
#define le32_to_cpu(le32_val) (le32_val)
#define le16_to_cpu(v) (v)
#define cpu_to_le32(v) (v)
	

#define USB_CTRL_SET_TIMEOUT 1
#define USB_CTRL_GET_TIMEOUT 1

#define TIOCSTI    0x5412
#define TIOCMGET   0x5415
#define TIOCMBIS   0x5416
#define TIOCMBIC   0x5417
#define TIOCMSET   0x5418
#define TIOCM_LE   0x001
#define TIOCM_DTR  0x002
#define TIOCM_RTS  0x004
#define TIOCM_ST   0x008
#define TIOCM_SR   0x010
#define TIOCM_CTS  0x020
#define TIOCM_CAR  0x040
#define TIOCM_RNG  0x080
#define TIOCM_DSR  0x100
#define TIOCM_CD   TIOCM_CAR
#define TIOCM_RI   TIOCM_RNG
#define TIOCM_OUT1 0x2000
#define TIOCM_OUT2 0x4000
#define TIOCM_LOOP 0x8000

/* c_cc characters */
#define VEOF     0
#define VEOL     1
#define VEOL2    2
#define VERASE   3
#define VWERASE  4
#define VKILL    5
#define VREPRINT 6
#define VSWTC    7
#define VINTR    8
#define VQUIT    9
#define VSUSP    10
#define VSTART   12
#define VSTOP    13
#define VLNEXT   14
#define VDISCARD 15
#define VMIN     16
#define VTIME    17

/* c_iflag bits */
#define IGNBRK  0000001
#define BRKINT  0000002
#define IGNPAR  0000004
#define PARMRK  0000010
#define INPCK   0000020
#define ISTRIP  0000040
#define INLCR   0000100
#define IGNCR   0000200
#define ICRNL   0000400
#define IXON    0001000
#define IXOFF   0002000
#define IXANY   0004000
#define IUCLC   0010000
#define IMAXBEL 0020000
#define IUTF8   0040000

/* c_oflag bits */
#define OPOST 0000001
#define ONLCR 0000002
#define OLCUC 0000004

#define OCRNL  0000010
#define ONOCR  0000020
#define ONLRET 0000040

#define OFILL  00000100
#define OFDEL  00000200
#define NLDLY  00001400
#define NL0    00000000
#define NL1    00000400
#define NL2    00001000
#define NL3    00001400
#define TABDLY 00006000
#define TAB0   00000000
#define TAB1   00002000
#define TAB2   00004000
#define TAB3   00006000
#define CRDLY  00030000
#define CR0    00000000
#define CR1    00010000
#define CR2    00020000
#define CR3    00030000
#define FFDLY  00040000
#define FF0    00000000
#define FF1    00040000
#define BSDLY  00100000
#define BS0    00000000
#define BS1    00100000
#define VTDLY  00200000
#define VT0    00000000
#define VT1    00200000
/*
 * Should be equivalent to TAB3, see description of TAB3 in
 * POSIX.1-2008, Ch. 11.2.3 "Output Modes"
 */
#define XTABS TAB3

/* c_cflag bit meaning */
#define CBAUD    0000037
#define B0       0000000 /* hang up */
#define B50      0000001
#define B75      0000002
#define B110     0000003
#define B134     0000004
#define B150     0000005
#define B200     0000006
#define B300     0000007
#define B600     0000010
#define B1200    0000011
#define B1800    0000012
#define B2400    0000013
#define B4800    0000014
#define B9600    0000015
#define B19200   0000016
#define B38400   0000017
#define EXTA     B19200
#define EXTB     B38400
#define CBAUDEX  0000000
#define B57600   00020
#define B115200  00021
#define B230400  00022
#define B460800  00023
#define B500000  00024
#define B576000  00025
#define B921600  00026
#define B1000000 00027
#define B1152000 00030
#define B1500000 00031
#define B2000000 00032
#define B2500000 00033
#define B3000000 00034
#define B3500000 00035
#define B4000000 00036
#define BOTHER   00037

#define CSIZE 00001400
#define CS5   00000000
#define CS6   00000400
#define CS7   00001000
#define CS8   00001400

#define CSTOPB 00002000
#define CREAD  00004000
#define PARENB 00010000
#define PARODD 00020000
#define HUPCL  00040000

#define CLOCAL  00100000
#define CMSPAR  010000000000 /* mark or space (stick) parity */
#define CRTSCTS 020000000000 /* flow control */

#define CIBAUD  07600000
#define IBSHIFT 16

/* c_lflag bits */
#define ISIG    0x00000080
#define ICANON  0x00000100
#define XCASE   0x00004000
#define ECHO    0x00000008
#define ECHOE   0x00000002
#define ECHOK   0x00000004
#define ECHONL  0x00000010
#define NOFLSH  0x80000000
#define TOSTOP  0x00400000
#define ECHOCTL 0x00000040
#define ECHOPRT 0x00000020
#define ECHOKE  0x00000001
#define FLUSHO  0x00800000
#define PENDIN  0x20000000
#define IEXTEN  0x00000400
#define EXTPROC 0x10000000

/* Values for the ACTION argument to `tcflow'.  */
#define TCOOFF 0
#define TCOON  1
#define TCIOFF 2
#define TCION  3

/* Values for the QUEUE_SELECTOR argument to `tcflush'.  */
#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2

/* Values for the OPTIONAL_ACTIONS argument to `tcsetattr'.  */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

/* c_cc characters */
#define VEOF     0
#define VEOL     1
#define VEOL2    2
#define VERASE   3
#define VWERASE  4
#define VKILL    5
#define VREPRINT 6
#define VSWTC    7
#define VINTR    8
#define VQUIT    9
#define VSUSP    10
#define VSTART   12
#define VSTOP    13
#define VLNEXT   14
#define VDISCARD 15
#define VMIN     16
#define VTIME    17

/* c_iflag bits */
#define IGNBRK  0000001
#define BRKINT  0000002
#define IGNPAR  0000004
#define PARMRK  0000010
#define INPCK   0000020
#define ISTRIP  0000040
#define INLCR   0000100
#define IGNCR   0000200
#define ICRNL   0000400
#define IXON    0001000
#define IXOFF   0002000
#define IXANY   0004000
#define IUCLC   0010000
#define IMAXBEL 0020000
#define IUTF8   0040000

/* c_oflag bits */
#define OPOST 0000001
#define ONLCR 0000002
#define OLCUC 0000004

#define OCRNL  0000010
#define ONOCR  0000020
#define ONLRET 0000040

#define OFILL  00000100
#define OFDEL  00000200
#define NLDLY  00001400
#define NL0    00000000
#define NL1    00000400
#define NL2    00001000
#define NL3    00001400
#define TABDLY 00006000
#define TAB0   00000000
#define TAB1   00002000
#define TAB2   00004000
#define TAB3   00006000
#define CRDLY  00030000
#define CR0    00000000
#define CR1    00010000
#define CR2    00020000
#define CR3    00030000
#define FFDLY  00040000
#define FF0    00000000
#define FF1    00040000
#define BSDLY  00100000
#define BS0    00000000
#define BS1    00100000
#define VTDLY  00200000
#define VT0    00000000
#define VT1    00200000
/*
 * Should be equivalent to TAB3, see description of TAB3 in
 * POSIX.1-2008, Ch. 11.2.3 "Output Modes"
 */
#define XTABS TAB3

/* c_cflag bit meaning */
#define CBAUD    0000037
#define B0       0000000 /* hang up */
#define B50      0000001
#define B75      0000002
#define B110     0000003
#define B134     0000004
#define B150     0000005
#define B200     0000006
#define B300     0000007
#define B600     0000010
#define B1200    0000011
#define B1800    0000012
#define B2400    0000013
#define B4800    0000014
#define B9600    0000015
#define B19200   0000016
#define B38400   0000017
#define EXTA     B19200
#define EXTB     B38400
#define CBAUDEX  0000000
#define B57600   00020
#define B115200  00021
#define B230400  00022
#define B460800  00023
#define B500000  00024
#define B576000  00025
#define B921600  00026
#define B1000000 00027
#define B1152000 00030
#define B1500000 00031
#define B2000000 00032
#define B2500000 00033
#define B3000000 00034
#define B3500000 00035
#define B4000000 00036
#define BOTHER   00037

#define CSIZE 00001400
#define CS5   00000000
#define CS6   00000400
#define CS7   00001000
#define CS8   00001400

#define CSTOPB 00002000
#define CREAD  00004000
#define PARENB 00010000
#define PARODD 00020000
#define HUPCL  00040000

#define CLOCAL  00100000
#define CMSPAR  010000000000 /* mark or space (stick) parity */
#define CRTSCTS 020000000000 /* flow control */

#define CIBAUD  07600000
#define IBSHIFT 16

/* c_lflag bits */
#define ISIG    0x00000080
#define ICANON  0x00000100
#define XCASE   0x00004000
#define ECHO    0x00000008
#define ECHOE   0x00000002
#define ECHOK   0x00000004
#define ECHONL  0x00000010
#define NOFLSH  0x80000000
#define TOSTOP  0x00400000
#define ECHOCTL 0x00000040
#define ECHOPRT 0x00000020
#define ECHOKE  0x00000001
#define FLUSHO  0x00800000
#define PENDIN  0x20000000
#define IEXTEN  0x00000400
#define EXTPROC 0x10000000

/* Values for the ACTION argument to `tcflow'.  */
#define TCOOFF 0
#define TCOON  1
#define TCIOFF 2
#define TCION  3

/* Values for the QUEUE_SELECTOR argument to `tcflush'.  */
#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2

/* Values for the OPTIONAL_ACTIONS argument to `tcsetattr'.  */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

/* c_cc characters */
#define VEOF     0
#define VEOL     1
#define VEOL2    2
#define VERASE   3
#define VWERASE  4
#define VKILL    5
#define VREPRINT 6
#define VSWTC    7
#define VINTR    8
#define VQUIT    9
#define VSUSP    10
#define VSTART   12
#define VSTOP    13
#define VLNEXT   14
#define VDISCARD 15
#define VMIN     16
#define VTIME    17

/* c_iflag bits */
#define IGNBRK  0000001
#define BRKINT  0000002
#define IGNPAR  0000004
#define PARMRK  0000010
#define INPCK   0000020
#define ISTRIP  0000040
#define INLCR   0000100
#define IGNCR   0000200
#define ICRNL   0000400
#define IXON    0001000
#define IXOFF   0002000
#define IXANY   0004000
#define IUCLC   0010000
#define IMAXBEL 0020000
#define IUTF8   0040000

/* c_oflag bits */
#define OPOST 0000001
#define ONLCR 0000002
#define OLCUC 0000004

#define OCRNL  0000010
#define ONOCR  0000020
#define ONLRET 0000040

#define OFILL  00000100
#define OFDEL  00000200
#define NLDLY  00001400
#define NL0    00000000
#define NL1    00000400
#define NL2    00001000
#define NL3    00001400
#define TABDLY 00006000
#define TAB0   00000000
#define TAB1   00002000
#define TAB2   00004000
#define TAB3   00006000
#define CRDLY  00030000
#define CR0    00000000
#define CR1    00010000
#define CR2    00020000
#define CR3    00030000
#define FFDLY  00040000
#define FF0    00000000
#define FF1    00040000
#define BSDLY  00100000
#define BS0    00000000
#define BS1    00100000
#define VTDLY  00200000
#define VT0    00000000
#define VT1    00200000
/*
 * Should be equivalent to TAB3, see description of TAB3 in
 * POSIX.1-2008, Ch. 11.2.3 "Output Modes"
 */
#define XTABS TAB3

/* c_cflag bit meaning */
#define CBAUD    0000037
#define B0       0000000 /* hang up */
#define B50      0000001
#define B75      0000002
#define B110     0000003
#define B134     0000004
#define B150     0000005
#define B200     0000006
#define B300     0000007
#define B600     0000010
#define B1200    0000011
#define B1800    0000012
#define B2400    0000013
#define B4800    0000014
#define B9600    0000015
#define B19200   0000016
#define B38400   0000017
#define EXTA     B19200
#define EXTB     B38400
#define CBAUDEX  0000000
#define B57600   00020
#define B115200  00021
#define B230400  00022
#define B460800  00023
#define B500000  00024
#define B576000  00025
#define B921600  00026
#define B1000000 00027
#define B1152000 00030
#define B1500000 00031
#define B2000000 00032
#define B2500000 00033
#define B3000000 00034
#define B3500000 00035
#define B4000000 00036
#define BOTHER   00037

#define CSIZE 00001400
#define CS5   00000000
#define CS6   00000400
#define CS7   00001000
#define CS8   00001400

#define CSTOPB 00002000
#define CREAD  00004000
#define PARENB 00010000
#define PARODD 00020000
#define HUPCL  00040000

#define CLOCAL  00100000
#define CMSPAR  010000000000 /* mark or space (stick) parity */
#define CRTSCTS 020000000000 /* flow control */

#define CIBAUD  07600000
#define IBSHIFT 16

/* c_lflag bits */
#define ISIG    0x00000080
#define ICANON  0x00000100
#define XCASE   0x00004000
#define ECHO    0x00000008
#define ECHOE   0x00000002
#define ECHOK   0x00000004
#define ECHONL  0x00000010
#define NOFLSH  0x80000000
#define TOSTOP  0x00400000
#define ECHOCTL 0x00000040
#define ECHOPRT 0x00000020
#define ECHOKE  0x00000001
#define FLUSHO  0x00800000
#define PENDIN  0x20000000
#define IEXTEN  0x00000400
#define EXTPROC 0x10000000

/* Values for the ACTION argument to `tcflow'.  */
#define TCOOFF 0
#define TCOON  1
#define TCIOFF 2
#define TCION  3

/* Values for the QUEUE_SELECTOR argument to `tcflush'.  */
#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2

/* Values for the OPTIONAL_ACTIONS argument to `tcsetattr'.  */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

#define __packed

#undef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define clamp(val, lo, hi) min(max(val, lo), hi)

#define BITS_PER_LONG 32

#define GENMASK(h, l) \
    (((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define DIV_ROUND_CLOSEST(x, d) (((x) + ((d) / 2)) / (d))

#define true           1
#define false          0

static uint16_t swab16(uint16_t v)
{
    return ((v & 0xff) << 8) | ((v & 0xff00) >> 8);
}

int usb_rcvctrlpipe(struct usb_serial_port *port, int useless)
{
    return port->ctrlpipe_rx;
}
int usb_sndctrlpipe(struct usb_serial_port *port, int useless)
{
    return port->ctrlpipe_tx;
}

void cp210x_get_termios(struct tty_struct *tty);
void cp210x_change_speed(struct tty_struct *tty);

void cp210x_get_termios(struct tty_struct *tty);
void cp210x_get_termios_port(struct usb_serial_port *port, tcflag_t *cflagp, unsigned int *baudp);

int usb_control_msg(struct usb_serial_port *port, int pipe, int req, u8 type, u16 val, u8 interface_num, void *dmabuf, int bufsize, int flags);

/* Config request types */
#define REQTYPE_HOST_TO_INTERFACE 0x41
#define REQTYPE_INTERFACE_TO_HOST 0xc1
#define REQTYPE_HOST_TO_DEVICE    0x40
#define REQTYPE_DEVICE_TO_HOST    0xc0

/* Config request codes */
#define CP210X_IFC_ENABLE      0x00
#define CP210X_SET_BAUDDIV     0x01
#define CP210X_GET_BAUDDIV     0x02
#define CP210X_SET_LINE_CTL    0x03
#define CP210X_GET_LINE_CTL    0x04
#define CP210X_SET_BREAK       0x05
#define CP210X_IMM_CHAR        0x06
#define CP210X_SET_MHS         0x07
#define CP210X_GET_MDMSTS      0x08
#define CP210X_SET_XON         0x09
#define CP210X_SET_XOFF        0x0A
#define CP210X_SET_EVENTMASK   0x0B
#define CP210X_GET_EVENTMASK   0x0C
#define CP210X_SET_CHAR        0x0D
#define CP210X_GET_CHARS       0x0E
#define CP210X_GET_PROPS       0x0F
#define CP210X_GET_COMM_STATUS 0x10
#define CP210X_RESET           0x11
#define CP210X_PURGE           0x12
#define CP210X_SET_FLOW        0x13
#define CP210X_GET_FLOW        0x14
#define CP210X_EMBED_EVENTS    0x15
#define CP210X_GET_EVENTSTATE  0x16
#define CP210X_SET_CHARS       0x19
#define CP210X_GET_BAUDRATE    0x1D
#define CP210X_SET_BAUDRATE    0x1E
#define CP210X_VENDOR_SPECIFIC 0xFF

/* CP210X_IFC_ENABLE */
#define UART_ENABLE  0x0001
#define UART_DISABLE 0x0000

/* CP210X_(SET|GET)_BAUDDIV */
#define BAUD_RATE_GEN_FREQ 0x384000

/* CP210X_(SET|GET)_LINE_CTL */
#define BITS_DATA_MASK 0X0f00
#define BITS_DATA_5    0X0500
#define BITS_DATA_6    0X0600
#define BITS_DATA_7    0X0700
#define BITS_DATA_8    0X0800
#define BITS_DATA_9    0X0900

#define BITS_PARITY_MASK  0x00f0
#define BITS_PARITY_NONE  0x0000
#define BITS_PARITY_ODD   0x0010
#define BITS_PARITY_EVEN  0x0020
#define BITS_PARITY_MARK  0x0030
#define BITS_PARITY_SPACE 0x0040

#define BITS_STOP_MASK 0x000f
#define BITS_STOP_1    0x0000
#define BITS_STOP_1_5  0x0001
#define BITS_STOP_2    0x0002

/* CP210X_SET_BREAK */
#define BREAK_ON  0x0001
#define BREAK_OFF 0x0000

/* CP210X_(SET_MHS|GET_MDMSTS) */
#define CONTROL_DTR       0x0001
#define CONTROL_RTS       0x0002
#define CONTROL_CTS       0x0010
#define CONTROL_DSR       0x0020
#define CONTROL_RING      0x0040
#define CONTROL_DCD       0x0080
#define CONTROL_WRITE_DTR 0x0100
#define CONTROL_WRITE_RTS 0x0200

/* CP210X_VENDOR_SPECIFIC values */
#define CP210X_READ_2NCONFIG  0x000E
#define CP210X_READ_LATCH     0x00C2
#define CP210X_GET_PARTNUM    0x370B
#define CP210X_GET_PORTCONFIG 0x370C
#define CP210X_GET_DEVICEMODE 0x3711
#define CP210X_WRITE_LATCH    0x37E1

/* Part number definitions */
#define CP210X_PARTNUM_CP2101        0x01
#define CP210X_PARTNUM_CP2102        0x02
#define CP210X_PARTNUM_CP2103        0x03
#define CP210X_PARTNUM_CP2104        0x04
#define CP210X_PARTNUM_CP2105        0x05
#define CP210X_PARTNUM_CP2108        0x08
#define CP210X_PARTNUM_CP2102N_QFN28 0x20
#define CP210X_PARTNUM_CP2102N_QFN24 0x21
#define CP210X_PARTNUM_CP2102N_QFN20 0x22
#define CP210X_PARTNUM_UNKNOWN       0xFF

#define __le32 int32_t
/* CP210X_GET_COMM_STATUS returns these 0x13 bytes */
struct cp210x_comm_status {
    __le32 ulErrors;
    __le32 ulHoldReasons;
    __le32 ulAmountInInQueue;
    __le32 ulAmountInOutQueue;
    u8 bEofReceived;
    u8 bWaitForImmediate;
    u8 bReserved;
} __packed;

/*
 * CP210X_PURGE - 16 bits passed in wValue of USB request.
 * SiLabs app note AN571 gives a strange description of the 4 bits:
 * bit 0 or bit 2 clears the transmit queue and 1 or 3 receive.
 * writing 1 to all, however, purges cp2108 well enough to avoid the hang.
 */
#define PURGE_ALL 0x000f

/* CP210X_GET_FLOW/CP210X_SET_FLOW read/write these 0x10 bytes */
struct cp210x_flow_ctl {
    __le32 ulControlHandshake;
    __le32 ulFlowReplace;
    __le32 ulXonLimit;
    __le32 ulXoffLimit;
} __packed;

#undef BIT
#undef BIT_ULL
#undef BIT_MASK
#undef BIT_WORD
#undef BIT_ULL_MASK
#undef BITS_PER_BYTE
#define BIT(nr)          (1UL << (nr))
#define BIT_ULL(nr)      (1ULL << (nr))
#define BIT_MASK(nr)     (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)     ((nr) / BITS_PER_LONG)
#define BIT_ULL_MASK(nr) (1ULL << ((nr) % BITS_PER_LONG_LONG))
#define BIT_ULL_WORD(nr) ((nr) / BITS_PER_LONG_LONG)
#define BITS_PER_BYTE    8

/* cp210x_flow_ctl::ulControlHandshake */
#define CP210X_SERIAL_DTR_MASK         GENMASK(1, 0)
#define CP210X_SERIAL_DTR_SHIFT(_mode) (_mode)
#define CP210X_SERIAL_CTS_HANDSHAKE    BIT(3)
#define CP210X_SERIAL_DSR_HANDSHAKE    BIT(4)
#define CP210X_SERIAL_DCD_HANDSHAKE    BIT(5)
#define CP210X_SERIAL_DSR_SENSITIVITY  BIT(6)

/* values for cp210x_flow_ctl::ulControlHandshake::CP210X_SERIAL_DTR_MASK */
#define CP210X_SERIAL_DTR_INACTIVE 0
#define CP210X_SERIAL_DTR_ACTIVE   1
#define CP210X_SERIAL_DTR_FLOW_CTL 2

/* cp210x_flow_ctl::ulFlowReplace */
#define CP210X_SERIAL_AUTO_TRANSMIT    BIT(0)
#define CP210X_SERIAL_AUTO_RECEIVE     BIT(1)
#define CP210X_SERIAL_ERROR_CHAR       BIT(2)
#define CP210X_SERIAL_NULL_STRIPPING   BIT(3)
#define CP210X_SERIAL_BREAK_CHAR       BIT(4)
#define CP210X_SERIAL_RTS_MASK         GENMASK(7, 6)
#define CP210X_SERIAL_RTS_SHIFT(_mode) (_mode << 6)
#define CP210X_SERIAL_XOFF_CONTINUE    BIT(31)

/* values for cp210x_flow_ctl::ulFlowReplace::CP210X_SERIAL_RTS_MASK */
#define CP210X_SERIAL_RTS_INACTIVE 0
#define CP210X_SERIAL_RTS_ACTIVE   1
#define CP210X_SERIAL_RTS_FLOW_CTL 2

/* CP210X_VENDOR_SPECIFIC, CP210X_GET_DEVICEMODE call reads these 0x2 bytes. */
struct cp210x_pin_mode {
    u8 eci;
    u8 sci;
} __packed;

#define CP210X_PIN_MODE_MODEM 0
#define CP210X_PIN_MODE_GPIO  BIT(0)

/*
 * CP210X_VENDOR_SPECIFIC, CP210X_GET_PORTCONFIG call reads these 0xf bytes.
 * Structure needs padding due to unused/unspecified bytes.
 */
#define __le16 int16_t

struct cp210x_config {
    __le16 gpio_mode;
    u8 __pad0[2];
    __le16 reset_state;
    u8 __pad1[4];
    __le16 suspend_state;
    u8 sci_cfg;
    u8 eci_cfg;
    u8 device_cfg;
} __packed;

/* GPIO modes */
#define CP210X_SCI_GPIO_MODE_OFFSET 9
#define CP210X_SCI_GPIO_MODE_MASK   GENMASK(11, 9)

#define CP210X_ECI_GPIO_MODE_OFFSET 2
#define CP210X_ECI_GPIO_MODE_MASK   GENMASK(3, 2)

/* CP2105 port configuration values */
#define CP2105_GPIO0_TXLED_MODE BIT(0)
#define CP2105_GPIO1_RXLED_MODE BIT(1)
#define CP2105_GPIO1_RS485_MODE BIT(2)

/* CP2102N configuration array indices */
#define CP210X_2NCONFIG_CONFIG_VERSION_IDX 2
#define CP210X_2NCONFIG_GPIO_MODE_IDX      581
#define CP210X_2NCONFIG_GPIO_RSTLATCH_IDX  587
#define CP210X_2NCONFIG_GPIO_CONTROL_IDX   600

/* CP210X_VENDOR_SPECIFIC, CP210X_WRITE_LATCH call writes these 0x2 bytes. */
struct cp210x_gpio_write {
    u8 mask;
    u8 state;
} __packed;

/*
 * Helper to get interface number when we only have struct usb_serial.
 */
static u8 cp210x_interface_num(struct usb_serial_port *port)
{
    return port->bInterfaceNumber;
}

/*
 * Reads a variable-sized block of CP210X_ registers, identified by req.
 * Returns data into buf in native USB byte order.
 */
static int cp210x_read_reg_block(struct usb_serial_port *port, u8 req,
                                 void *buf, int bufsize)
{
    int result;

    result = usb_control_msg(port, usb_rcvctrlpipe(port, 0),
                             req, REQTYPE_INTERFACE_TO_HOST, 0,
                             port->bInterfaceNumber, buf, bufsize,
                             USB_CTRL_SET_TIMEOUT);
    if (result == bufsize) {
        result = 0;
    } else {
        dev_err("failed get req 0x%x size %d status: %d\n",
                (uint8_t)req, bufsize, result);
        if (result >= 0)
            result = -EIO;

        /*
		 * FIXME Some callers don't bother to check for error,
		 * at least give them consistent junk until they are fixed
		 */
        memset(buf, 0, bufsize);
    }

    return result;
}

/*
 * Reads any 32-bit CP210X_ register identified by req.
 */
static int cp210x_read_u32_reg(struct usb_serial_port *port, u8 req, u32 *val)
{
    __le32 le32_val;
    int err;

    err = cp210x_read_reg_block(port, req, &le32_val, sizeof(le32_val));
    if (err) {
        /*
		 * FIXME Some callers don't bother to check for error,
		 * at least give them consistent junk until they are fixed
		 */
        *val = 0;
        return err;
    }

    *val = le32_to_cpu(le32_val);

    return 0;
}

/*
 * Reads any 16-bit CP210X_ register identified by req.
 */
static int cp210x_read_u16_reg(struct usb_serial_port *port, u8 req, u16 *val)
{
    __le16 le16_val;
    int err;

    err = cp210x_read_reg_block(port, req, &le16_val, sizeof(le16_val));
    if (err)
        return err;

    *val = le16_to_cpu(le16_val);

    return 0;
}

/*
 * Reads any 8-bit CP210X_ register identified by req.
 */
static int cp210x_read_u8_reg(struct usb_serial_port *port, u8 req, u8 *val)
{
    return cp210x_read_reg_block(port, req, val, sizeof(*val));
}

/*
 * Reads a variable-sized vendor block of CP210X_ registers, identified by val.
 * Returns data into buf in native USB byte order.
 */
static int cp210x_read_vendor_block(struct usb_serial_port *port, u8 type, u16 val,
                                    void *buf, int bufsize)
{
    int result;

    result = usb_control_msg(port, usb_rcvctrlpipe(port, 0),
                             CP210X_VENDOR_SPECIFIC, type, val,
                             cp210x_interface_num(port), buf, bufsize,
                             USB_CTRL_GET_TIMEOUT);
    if (result == bufsize) {
        return 0;
    } else {
        if (result >= 0)
            return -1;
    }

    return -2;
}

/*
 * Writes any 16-bit CP210X_ register (req) whose value is passed
 * entirely in the wValue field of the USB request.
 */
static int cp210x_write_u16_reg(struct usb_serial_port *port, u8 req, u16 val)
{
    int result;

    result = usb_control_msg(port, usb_sndctrlpipe(port, 0),
                             req, REQTYPE_HOST_TO_INTERFACE, val,
                             port->bInterfaceNumber, NULL, 0,
                             USB_CTRL_SET_TIMEOUT);
    if (result < 0) {
        USB_LOG_ERR("failed set request 0x%x status: %d\n",
                    req, result);
    }

    return result;
}

/*
 * Writes a variable-sized block of CP210X_ registers, identified by req.
 * Data in buf must be in native USB byte order.
 */
static int cp210x_write_reg_block(struct usb_serial_port *port, u8 req,
                                  void *buf, int bufsize)
{
    int result;

    result = usb_control_msg(port, usb_sndctrlpipe(port, 0),
                             req, REQTYPE_HOST_TO_INTERFACE, 0,
                             port->bInterfaceNumber, buf, bufsize,
                             USB_CTRL_SET_TIMEOUT);

    if (result == bufsize) {
        result = 0;
    } else {
        USB_LOG_ERR("failed set req 0x%x size %d status: %d\n",
                    req, bufsize, result);
        if (result >= 0)
            result = -EIO;
    }

    return result;
}

/*
 * Writes any 32-bit CP210X_ register identified by req.
 */
static int cp210x_write_u32_reg(struct usb_serial_port *port, u8 req, u32 val)
{
    __le32 le32_val;

    le32_val = cpu_to_le32(val);

    return cp210x_write_reg_block(port, req, &le32_val, sizeof(le32_val));
}

/*
 * Detect CP2108 GET_LINE_CTL bug and activate workaround.
 * Write a known good value 0x800, read it back.
 * If it comes back swapped the bug is detected.
 * Preserve the original register value.
 */
static int cp210x_detect_swapped_line_ctl(struct usb_serial_port *port)
{
    u16 line_ctl_save;
    u16 line_ctl_test;
    int err;

    err = cp210x_read_u16_reg(port, CP210X_GET_LINE_CTL, &line_ctl_save);
    if (err) {
        USB_LOG_ERR("Error, read reg GET_LINE_CTL\n");
        return err;
    }

    err = cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, 0x800);
    if (err) {
        USB_LOG_ERR("Error, write reg SET_LINE_CTL\n");
        return err;
    }

    err = cp210x_read_u16_reg(port, CP210X_GET_LINE_CTL, &line_ctl_test);
    if (err) {
        USB_LOG_ERR("Error, read reg GET_LINE_CTL\n");
        return err;
    }

    if (line_ctl_test == 8) {
        port->has_swapped_line_ctl = true;
        line_ctl_save = swab16(line_ctl_save);
    }

    return cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, line_ctl_save);
}

/*
 * Must always be called instead of cp210x_read_u16_reg(CP210X_GET_LINE_CTL)
 * to workaround cp2108 bug and get correct value.
 */
static int cp210x_get_line_ctl(struct usb_serial_port *port, u16 *ctl)
{
    int err;

    err = cp210x_read_u16_reg(port, CP210X_GET_LINE_CTL, ctl);
    if (err)
        return err;

    /* Workaround swapped bytes in 16-bit value from CP210X_GET_LINE_CTL */
    if (port->has_swapped_line_ctl)
        *ctl = swab16(*ctl);

    return 0;
}

int cp210x_open(struct tty_struct *tty)
{
    int result;
    struct usb_serial_port *port = &tty->driver_data;

    result = cp210x_write_u16_reg(port, CP210X_IFC_ENABLE, UART_ENABLE);
    if (result) {
        USB_LOG_ERR("%s - Unable to enable UART\n", __func__);
        return result;
    }

    /* Configure the termios structure */
    cp210x_get_termios(tty);

    /* The baud rate must be initialised on cp2104 */
    if (tty)
        cp210x_change_speed(tty);

    return 0;
}

void cp210x_close(struct usb_serial_port *port)
{
    /* Clear both queues; cp2108 needs this to avoid an occasional hang */
    cp210x_write_u16_reg(port, CP210X_PURGE, PURGE_ALL);

    cp210x_write_u16_reg(port, CP210X_IFC_ENABLE, UART_DISABLE);
}

/*
 * Read how many bytes are waiting in the TX queue.
 */
static int cp210x_get_tx_queue_byte_count(struct usb_serial_port *port,
                                          u32 *count)
{
    struct cp210x_comm_status sts;
    int result;

    result = usb_control_msg(port, usb_rcvctrlpipe(port, 0),
                             CP210X_GET_COMM_STATUS, REQTYPE_INTERFACE_TO_HOST,
                             0, port->bInterfaceNumber, &sts, sizeof(sts),
                             USB_CTRL_GET_TIMEOUT);
    if (result == sizeof(sts)) {
        *count = le32_to_cpu(sts.ulAmountInOutQueue);
        result = 0;
    } else {
        dev_err("failed to get comm status: %d\n", result);
        if (result >= 0)
            result = -EIO;
    }

    return result;
}

bool cp210x_tx_empty(struct usb_serial_port *port)
{
    int err;
    u32 count;

    err = cp210x_get_tx_queue_byte_count(port, &count);
    if (err)
        return true;

    return !count;
}

/*
 * cp210x_get_termios
 * Reads the baud rate, data bits, parity, stop bits and flow control mode
 * from the device, corrects any unsupported values, and configures the
 * termios structure to reflect the state of the device
 */
void cp210x_get_termios(struct tty_struct *tty)
{
    unsigned int baud;
    struct usb_serial_port *port = &tty->driver_data;

    if (tty) {
        cp210x_get_termios_port(&tty->driver_data,
                                &tty->termios.c_cflag, &baud);
    } else {
        tcflag_t cflag;
        cflag = 0;
        cp210x_get_termios_port(port, &cflag, &baud);
    }
}

/*
 * cp210x_get_termios_port
 * This is the heart of cp210x_get_termios which always uses a &usb_serial_port.
 */
void cp210x_get_termios_port(struct usb_serial_port *port, tcflag_t *cflagp, unsigned int *baudp)
{
    tcflag_t cflag;
    struct cp210x_flow_ctl flow_ctl;
    u32 baud;
    u16 bits;
    u32 ctl_hs;

    cp210x_read_u32_reg(port, CP210X_GET_BAUDRATE, &baud);

    dev_dbg("%s - baud rate = %d\n", __func__, baud);
    *baudp = baud;

    cflag = *cflagp;

    cp210x_get_line_ctl(port, &bits);
    cflag &= ~CSIZE;
    switch (bits & BITS_DATA_MASK) {
        case BITS_DATA_5:
            dev_dbg("%s - data bits = 5\n", __func__);
            cflag |= CS5;
            break;
        case BITS_DATA_6:
            dev_dbg("%s - data bits = 6\n", __func__);
            cflag |= CS6;
            break;
        case BITS_DATA_7:
            dev_dbg("%s - data bits = 7\n", __func__);
            cflag |= CS7;
            break;
        case BITS_DATA_8:
            dev_dbg("%s - data bits = 8\n", __func__);
            cflag |= CS8;
            break;
        case BITS_DATA_9:
            dev_dbg("%s - data bits = 9 (not supported, using 8 data bits)\n", __func__);
            cflag |= CS8;
            bits &= ~BITS_DATA_MASK;
            bits |= BITS_DATA_8;
            cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits);
            break;
        default:
            dev_dbg("%s - Unknown number of data bits, using 8\n", __func__);
            cflag |= CS8;
            bits &= ~BITS_DATA_MASK;
            bits |= BITS_DATA_8;
            cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits);
            break;
    }

    switch (bits & BITS_PARITY_MASK) {
        case BITS_PARITY_NONE:
            dev_dbg("%s - parity = NONE\n", __func__);
            cflag &= ~PARENB;
            break;
        case BITS_PARITY_ODD:
            dev_dbg("%s - parity = ODD\n", __func__);
            cflag |= (PARENB | PARODD);
            break;
        case BITS_PARITY_EVEN:
            dev_dbg("%s - parity = EVEN\n", __func__);
            cflag &= ~PARODD;
            cflag |= PARENB;
            break;
        case BITS_PARITY_MARK:
            dev_dbg("%s - parity = MARK\n", __func__);
            cflag |= (PARENB | PARODD | CMSPAR);
            break;
        case BITS_PARITY_SPACE:
            dev_dbg("%s - parity = SPACE\n", __func__);
            cflag &= ~PARODD;
            cflag |= (PARENB | CMSPAR);
            break;
        default:
            dev_dbg("%s - Unknown parity mode, disabling parity\n", __func__);
            cflag &= ~PARENB;
            bits &= ~BITS_PARITY_MASK;
            cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits);
            break;
    }

    cflag &= ~CSTOPB;
    switch (bits & BITS_STOP_MASK) {
        case BITS_STOP_1:
            dev_dbg("%s - stop bits = 1\n", __func__);
            break;
        case BITS_STOP_1_5:
            dev_dbg("%s - stop bits = 1.5 (not supported, using 1 stop bit)\n", __func__);
            bits &= ~BITS_STOP_MASK;
            cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits);
            break;
        case BITS_STOP_2:
            dev_dbg("%s - stop bits = 2\n", __func__);
            cflag |= CSTOPB;
            break;
        default:
            dev_dbg("%s - Unknown number of stop bits, using 1 stop bit\n", __func__);
            bits &= ~BITS_STOP_MASK;
            cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits);
            break;
    }

    cp210x_read_reg_block(port, CP210X_GET_FLOW, &flow_ctl,
                          sizeof(flow_ctl));
    ctl_hs = le32_to_cpu(flow_ctl.ulControlHandshake);
    if (ctl_hs & CP210X_SERIAL_CTS_HANDSHAKE) {
        dev_dbg("%s - flow control = CRTSCTS\n", __func__);
        cflag |= CRTSCTS;
    } else {
        dev_dbg("%s - flow control = NONE\n", __func__);
        cflag &= ~CRTSCTS;
    }

    *cflagp = cflag;
}

struct cp210x_rate {
    speed_t rate;
    speed_t high;
};

static const struct cp210x_rate cp210x_an205_table1[] = {
    { 300, 300 },
    { 600, 600 },
    { 1200, 1200 },
    { 1800, 1800 },
    { 2400, 2400 },
    { 4000, 4000 },
    { 4800, 4803 },
    { 7200, 7207 },
    { 9600, 9612 },
    { 14400, 14428 },
    { 16000, 16062 },
    { 19200, 19250 },
    { 28800, 28912 },
    { 38400, 38601 },
    { 51200, 51558 },
    { 56000, 56280 },
    { 57600, 58053 },
    { 64000, 64111 },
    { 76800, 77608 },
    { 115200, 117028 },
    { 128000, 129347 },
    { 153600, 156868 },
    { 230400, 237832 },
    { 250000, 254234 },
    { 256000, 273066 },
    { 460800, 491520 },
    { 500000, 567138 },
    { 576000, 670254 },
    { 921600, UINT_MAX }
};

/*
 * Quantises the baud rate as per AN205 Table 1
 */
static speed_t cp210x_get_an205_rate(speed_t baud)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(cp210x_an205_table1); ++i) {
        if (baud <= cp210x_an205_table1[i].high)
            break;
    }

    return cp210x_an205_table1[i].rate;
}

static speed_t cp210x_get_actual_rate(struct usb_serial_port *port, speed_t baud)
{
    unsigned int prescale = 1;
    unsigned int div;

    baud = clamp(baud, 300u, port->max_speed);

    if (baud <= 365)
        prescale = 4;

    div = DIV_ROUND_CLOSEST(48000000, 2 * prescale * baud);
    baud = 48000000 / (2 * prescale * div);

    return baud;
}

/*
 * CP2101 supports the following baud rates:
 *
 *	300, 600, 1200, 1800, 2400, 4800, 7200, 9600, 14400, 19200, 28800,
 *	38400, 56000, 57600, 115200, 128000, 230400, 460800, 921600
 *
 * CP2102 and CP2103 support the following additional rates:
 *
 *	4000, 16000, 51200, 64000, 76800, 153600, 250000, 256000, 500000,
 *	576000
 *
 * The device will map a requested rate to a supported one, but the result
 * of requests for rates greater than 1053257 is undefined (see AN205).
 *
 * CP2104, CP2105 and CP2110 support most rates up to 2M, 921k and 1M baud,
 * respectively, with an error less than 1%. The actual rates are determined
 * by
 *
 *	div = round(freq / (2 x prescale x request))
 *	actual = freq / (2 x prescale x div)
 *
 * For CP2104 and CP2105 freq is 48Mhz and prescale is 4 for request <= 365bps
 * or 1 otherwise.
 * For CP2110 freq is 24Mhz and prescale is 4 for request <= 300bps or 1
 * otherwise.
 */
void cp210x_change_speed(struct tty_struct *tty)
{
    struct usb_serial_port *port = &tty->driver_data;
    u32 baud;

    baud = tty->termios.c_ospeed;

    /*
	 * This maps the requested rate to the actual rate, a valid rate on
	 * cp2102 or cp2103, or to an arbitrary rate in [1M, max_speed].
	 *
	 * NOTE: B0 is not implemented.
	 */
    if (port->use_actual_rate)
        baud = cp210x_get_actual_rate(port, baud);
    else if (baud < 1000000)
        baud = cp210x_get_an205_rate(baud);
    else if (baud > port->max_speed)
        baud = port->max_speed;

    dev_dbg("%s - setting baud rate to %u\n", __func__, baud);
    if (cp210x_write_u32_reg(port, CP210X_SET_BAUDRATE, baud)) {
        dev_warn("failed to set baud rate to %u\n", baud);
        baud = 9600;
    }
}

void cp210x_set_termios(struct tty_struct *tty)
{
    unsigned int cflag;
    u16 bits;
    struct usb_serial_port *port = &tty->driver_data;

    cflag = tty->termios.c_cflag;

    cp210x_change_speed(tty);

    /* If the number of data bits is to be updated */
    cp210x_get_line_ctl(port, &bits);
    bits &= ~BITS_DATA_MASK;
    switch (cflag & CSIZE) {
        case CS5:
            bits |= BITS_DATA_5;
            dev_dbg("%s - data bits = 5\n", __func__);
            break;
        case CS6:
            bits |= BITS_DATA_6;
            dev_dbg("%s - data bits = 6\n", __func__);
            break;
        case CS7:
            bits |= BITS_DATA_7;
            dev_dbg("%s - data bits = 7\n", __func__);
            break;
        case CS8:
        default:
            bits |= BITS_DATA_8;
            dev_dbg("%s - data bits = 8\n", __func__);
            break;
    }
    if (cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits))
        dev_dbg("Number of data bits requested not supported by device%s\n", "");

    cp210x_get_line_ctl(port, &bits);
    bits &= ~BITS_PARITY_MASK;
    if (cflag & PARENB) {
        if (cflag & CMSPAR) {
            if (cflag & PARODD) {
                bits |= BITS_PARITY_MARK;
                dev_dbg("%s - parity = MARK\n", __func__);
            } else {
                bits |= BITS_PARITY_SPACE;
                dev_dbg(, "%s - parity = SPACE\n", __func__);
            }
        } else {
            if (cflag & PARODD) {
                bits |= BITS_PARITY_ODD;
                dev_dbg("%s - parity = ODD\n", __func__);
            } else {
                bits |= BITS_PARITY_EVEN;
                dev_dbg("%s - parity = EVEN\n", __func__);
            }
        }
    }
    if (cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits))
        dev_dbg("Parity mode not supported by device%s\n", "");

    cp210x_get_line_ctl(port, &bits);
    bits &= ~BITS_STOP_MASK;
    if (cflag & CSTOPB) {
        bits |= BITS_STOP_2;
        dev_dbg("%s - stop bits = 2\n", __func__);
    } else {
        bits |= BITS_STOP_1;
        dev_dbg("%s - stop bits = 1\n", __func__);
    }
    if (cp210x_write_u16_reg(port, CP210X_SET_LINE_CTL, bits))
        dev_dbg("Number of stop bits requested not supported by device%s\n", "");

    {
        struct cp210x_flow_ctl flow_ctl;
        u32 ctl_hs;
        u32 flow_repl;

        cp210x_read_reg_block(port, CP210X_GET_FLOW, &flow_ctl,
                              sizeof(flow_ctl));
        ctl_hs = le32_to_cpu(flow_ctl.ulControlHandshake);
        flow_repl = le32_to_cpu(flow_ctl.ulFlowReplace);
        dev_dbg("%s - read ulControlHandshake=0x%08x, ulFlowReplace=0x%08x\n",
                __func__, ctl_hs, flow_repl);

        ctl_hs &= ~CP210X_SERIAL_DSR_HANDSHAKE;
        ctl_hs &= ~CP210X_SERIAL_DCD_HANDSHAKE;
        ctl_hs &= ~CP210X_SERIAL_DSR_SENSITIVITY;
        ctl_hs &= ~CP210X_SERIAL_DTR_MASK;
        ctl_hs |= CP210X_SERIAL_DTR_SHIFT(CP210X_SERIAL_DTR_ACTIVE);
        if (cflag & CRTSCTS) {
            ctl_hs |= CP210X_SERIAL_CTS_HANDSHAKE;

            flow_repl &= ~CP210X_SERIAL_RTS_MASK;
            flow_repl |= CP210X_SERIAL_RTS_SHIFT(
                CP210X_SERIAL_RTS_FLOW_CTL);
            dev_dbg("%s - flow control = CRTSCTS\n", __func__);
        } else {
            ctl_hs &= ~CP210X_SERIAL_CTS_HANDSHAKE;

            flow_repl &= ~CP210X_SERIAL_RTS_MASK;
            flow_repl |= CP210X_SERIAL_RTS_SHIFT(
                CP210X_SERIAL_RTS_ACTIVE);
            dev_dbg("%s - flow control = NONE\n", __func__);
        }

        dev_dbg("%s - write ulControlHandshake=0x%08x, ulFlowReplace=0x%08x\n",
                __func__, ctl_hs, flow_repl);
        flow_ctl.ulControlHandshake = cpu_to_le32(ctl_hs);
        flow_ctl.ulFlowReplace = cpu_to_le32(flow_repl);
        cp210x_write_reg_block(port, CP210X_SET_FLOW, &flow_ctl,
                               sizeof(flow_ctl));
    }
}

int cp210x_tiocmset_port(struct usb_serial_port *port, unsigned int set, unsigned int clear);

int cp210x_tiocmset(struct tty_struct *tty,
                    unsigned int set, unsigned int clear)
{
    struct usb_serial_port *port = 0;
    return cp210x_tiocmset_port(port, set, clear);
}

int cp210x_tiocmset_port(struct usb_serial_port *port, unsigned int set, unsigned int clear)
{
    u16 control = 0;

    if (set & TIOCM_RTS) {
        control |= CONTROL_RTS;
        control |= CONTROL_WRITE_RTS;
    }
    if (set & TIOCM_DTR) {
        control |= CONTROL_DTR;
        control |= CONTROL_WRITE_DTR;
    }
    if (clear & TIOCM_RTS) {
        control &= ~CONTROL_RTS;
        control |= CONTROL_WRITE_RTS;
    }
    if (clear & TIOCM_DTR) {
        control &= ~CONTROL_DTR;
        control |= CONTROL_WRITE_DTR;
    }

    return cp210x_write_u16_reg(port, CP210X_SET_MHS, control);
}

void cp210x_dtr_rts(struct usb_serial_port *p, int on)
{
    if (on)
        cp210x_tiocmset_port(p, TIOCM_DTR | TIOCM_RTS, 0);
    else
        cp210x_tiocmset_port(p, 0, TIOCM_DTR | TIOCM_RTS);
}

int cp210x_tiocmget(struct tty_struct *tty)
{
    struct usb_serial_port *port = &tty->driver_data;
    u8 control;
    int result;

    result = cp210x_read_u8_reg(port, CP210X_GET_MDMSTS, &control);
    if (result)
        return result;

    result = ((control & CONTROL_DTR) ? TIOCM_DTR : 0) | ((control & CONTROL_RTS) ? TIOCM_RTS : 0) | ((control & CONTROL_CTS) ? TIOCM_CTS : 0) | ((control & CONTROL_DSR) ? TIOCM_DSR : 0) | ((control & CONTROL_RING) ? TIOCM_RI : 0) | ((control & CONTROL_DCD) ? TIOCM_CD : 0);

    dev_dbg("%s - control = 0x%.2x\n", __func__, control);

    return result;
}

void cp210x_break_ctl(struct tty_struct *tty, int break_state)
{
    struct usb_serial_port *port = &tty->driver_data;
    u16 state;

    if (break_state == 0)
        state = BREAK_OFF;
    else
        state = BREAK_ON;
    dev_dbg("%s - turning break %s\n", __func__,
            state == BREAK_OFF ? "off" : "on");
    cp210x_write_u16_reg(port, CP210X_SET_BREAK, state);
}

int cp210x_port_probe(struct usb_serial_port *port)
{
    int ret;

    ret = cp210x_detect_swapped_line_ctl(port);
    if (ret) {
        return ret;
    }

    return 0;
}

void cp210x_init_max_speed(struct usb_serial_port *port)
{
    bool use_actual_rate = false;
    speed_t max;

    switch (port->partnum) {
        case CP210X_PARTNUM_CP2101:
            max = 921600;
            break;
        case CP210X_PARTNUM_CP2102:
        case CP210X_PARTNUM_CP2103:
            //max = 1000000;
            max = 9600;
            break;
        case CP210X_PARTNUM_CP2104:
            use_actual_rate = true;
            max = 2000000;
            break;
        case CP210X_PARTNUM_CP2108:
            max = 2000000;
            break;
        case CP210X_PARTNUM_CP2105:
            if (cp210x_interface_num(port) == 0) {
                use_actual_rate = true;
                max = 2000000; /* ECI */
            } else {
                max = 921600;  /* SCI */
            }
            break;
        case CP210X_PARTNUM_CP2102N_QFN28:
        case CP210X_PARTNUM_CP2102N_QFN24:
        case CP210X_PARTNUM_CP2102N_QFN20:
            use_actual_rate = true;
            max = 3000000;
            break;
        default:
            max = 2000000;
            break;
    }

    max = 9600;

    port->max_speed = max;
    port->use_actual_rate = use_actual_rate;
    dev_dbg("max_speed=%d\n", max);
    dev_dbg("use_actual_rate=%d\n", use_actual_rate);
}

int cp210x_attach(struct usb_serial_port *port)
{
    int result;
    port->partnum = CP210X_PARTNUM_UNKNOWN;

    result = cp210x_read_vendor_block(port, REQTYPE_DEVICE_TO_HOST,
                                      CP210X_GET_PARTNUM, &port->partnum,
                                      sizeof(port->partnum));
    if (result < 0) {
        dev_warn(
            "querying part number failed%s\n", "");
        port->partnum = CP210X_PARTNUM_UNKNOWN;
    }

    switch (port->partnum) {
        case CP210X_PARTNUM_CP2101:
        case CP210X_PARTNUM_CP2102:
        case CP210X_PARTNUM_CP2103:
        case CP210X_PARTNUM_CP2104:
        case CP210X_PARTNUM_CP2108:
        case CP210X_PARTNUM_CP2105:
        case CP210X_PARTNUM_CP2102N_QFN28:
        case CP210X_PARTNUM_CP2102N_QFN24:
        case CP210X_PARTNUM_CP2102N_QFN20:
            cp210x_init_max_speed(port);
            return 0;
        default:
            return -1;
    }
}

int usb_control_msg(struct usb_serial_port *port, int pipe, int req, u8 type, u16 val, u8 interface_num, void *dmabuf, int bufsize, int flags)
{
    struct usbh_cp210x *p_device = container_of(port, struct usbh_cp210x, drv_data.driver_data);
    struct usb_setup_packet *setup = p_device->hport->setup;

    setup->bmRequestType = type;
    setup->bRequest = req;
    setup->wValue = val;
    setup->wIndex = interface_num;
    setup->wLength = bufsize;
    int len = usbh_control_transfer(p_device->hport->ep0, setup, dmabuf);
    if (len > 0) {
        return len - 8;
    }
    return len;
}

static int __usbh_cp210x_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usb_endpoint_descriptor *ep_desc;
    struct usbh_cp210x *p_cp210x = usb_malloc(sizeof(struct usbh_cp210x));
    if (0 == p_cp210x) {
        return -ENOSYS;
    }
    memset(p_cp210x, 0x00, sizeof(*p_cp210x));
    p_cp210x->hport = hport;
    p_cp210x->intf = intf;
    p_cp210x->index = hport->port - 1;
    hport->config.intf[intf].priv = p_cp210x;

    p_cp210x->drv_data.driver_data.bInterfaceNumber = 1;
    p_cp210x->drv_data.driver_data.ctrlpipe_rx = 0x80;
    p_cp210x->drv_data.driver_data.ctrlpipe_tx = 0x00;
    p_cp210x->drv_data.driver_data.max_speed = 9600;
    p_cp210x->drv_data.driver_data.use_actual_rate = 1;
    p_cp210x->drv_data.driver_data.bInterfaceNumber = 1;
    p_cp210x->drv_data.driver_data.has_swapped_line_ctl = 0;

    USB_LOG_INFO("%s hub %d port %d\n", "---- attach ----", hport->parent->index, hport->port);
    if (0 != cp210x_attach(&p_cp210x->drv_data.driver_data)) {
        USB_LOG_INFO("%s\n", "Device NOT supported!");
        return -EIO;
    }
    cp210x_port_probe(&p_cp210x->drv_data.driver_data);
    cp210x_open(&p_cp210x->drv_data);
    cp210x_break_ctl(&p_cp210x->drv_data, 0);

    memset(&p_cp210x->drv_data.termios, 0, sizeof(p_cp210x->drv_data.termios));
    p_cp210x->drv_data.termios.c_iflag = 0;
    p_cp210x->drv_data.termios.c_oflag = 0;
    p_cp210x->drv_data.termios.c_cflag = CS8;
    p_cp210x->drv_data.termios.c_lflag = 0;
    p_cp210x->drv_data.termios.c_cc[0] = 0;
    p_cp210x->drv_data.termios.c_ospeed = 9600;
    cp210x_set_termios(&p_cp210x->drv_data);

    for (uint8_t i = 0; i < hport->config.intf[intf].altsetting[0].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf].altsetting[0].ep[i].ep_desc;
        if (ep_desc->bEndpointAddress & 0x80) {
            usbh_hport_activate_epx(&p_cp210x->bulkin, hport, ep_desc);
        } else {
            usbh_hport_activate_epx(&p_cp210x->bulkout, hport, ep_desc);
        }
    }

    drv_usbh_cp210x_run(p_cp210x);

    return 0;
}

__WEAK void drv_usbh_cp210x_run(struct usbh_cp210x *p_device)
{
}

__WEAK void drv_usbh_cp210x_stop(struct usbh_cp210x *p_device)
{
}

static int __usbh_cp210x_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usbh_cp210x *p_device = (struct usbh_cp210x *)hport->config.intf[intf].priv;
    if (p_device) {
        drv_usbh_cp210x_stop(p_device);
        if (p_device->bulkin) {
            usbh_pipe_free(p_device->bulkin);
        }
        if (p_device->bulkout) {
            usbh_pipe_free(p_device->bulkout);
        }
        memset(p_device, 0, sizeof(*p_device));
        usb_free(p_device);
    }
    return 0;
}

static const struct usbh_class_driver cp210x_class_driver = {
    .driver_name = "cp210x",
    .connect = __usbh_cp210x_connect,
    .disconnect = __usbh_cp210x_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info cp210x_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = 0xff,    // usbh_cp210x_static_device_CLASS_MASS_STORAGE,
    .subclass = 0x00, //MSC_SUBCLASS_SCSI,
    .protocol = 0x00, //MSC_PROTOCOL_BULK_ONLY,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &cp210x_class_driver
};

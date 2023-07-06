
#ifndef USBH_CP210X_H
#define USBH_CP210X_H

#include <stdint.h>

#include "usbh_core.h"

typedef int32_t speed_t;
typedef int32_t tcflag_t;

#define DRIVER_DESC "Silicon Labs CP210x RS232 serial adaptor driver"

struct usb_serial_port{
	uint8_t partnum;
	uint8_t ctrlpipe_rx;
	uint8_t ctrlpipe_tx;
	int32_t max_speed;
	speed_t use_actual_rate;
	uint8_t bInterfaceNumber;
	int     has_swapped_line_ctl;
};


typedef unsigned char cc_t;

#define NCCS 19
struct termios {
	tcflag_t c_iflag;		/* input mode flags */
	tcflag_t c_oflag;		/* output mode flags */
	tcflag_t c_cflag;		/* control mode flags */
	tcflag_t c_lflag;		/* local mode flags */
	cc_t c_cc[NCCS];		/* control characters */
	cc_t c_line;			/* line discipline (== c_cc[19]) */
	speed_t c_ispeed;		/* input speed */
	speed_t c_ospeed;		/* output speed */
};

/* Alpha has identical termios and termios2 */

struct termios2 {
	tcflag_t c_iflag;		/* input mode flags */
	tcflag_t c_oflag;		/* output mode flags */
	tcflag_t c_cflag;		/* control mode flags */
	tcflag_t c_lflag;		/* local mode flags */
	cc_t c_cc[NCCS];		/* control characters */
	cc_t c_line;			/* line discipline (== c_cc[19]) */
	speed_t c_ispeed;		/* input speed */
	speed_t c_ospeed;		/* output speed */
};

/* Alpha has matching termios and ktermios */

struct ktermios {
	tcflag_t c_iflag;		/* input mode flags */
	tcflag_t c_oflag;		/* output mode flags */
	tcflag_t c_cflag;		/* control mode flags */
	tcflag_t c_lflag;		/* local mode flags */
	cc_t c_cc[NCCS];		/* control characters */
	cc_t c_line;			/* line discipline (== c_cc[19]) */
	speed_t c_ispeed;		/* input speed */
	speed_t c_ospeed;		/* output speed */
};


struct tty_struct{
    struct usb_serial_port driver_data;
    struct termios termios;
};

int cp210x_attach(struct usb_serial_port *port);
int cp210x_port_probe(struct usb_serial_port *port);
void cp210x_break_ctl(struct tty_struct *tty, int break_state);
int cp210x_open(struct tty_struct *tty);
int cp210x_tiocmget(struct tty_struct *tty);
void cp210x_set_termios(struct tty_struct *tty);
void cp210x_change_speed(struct tty_struct *tty);
void cp210x_dtr_rts(struct usb_serial_port *port, int on);

struct usbh_cp210x
{
    struct usbh_hubport *hport;

    uint8_t intf;
    usbh_pipe_t bulkin;
    usbh_pipe_t bulkout;
    struct usbh_urb bulkin_urb;
    struct usbh_urb bulkout_urb;

    struct tty_struct drv_data;
    int index;
};


/* weak defined function */
void drv_usbh_cp210x_run(struct usbh_cp210x *p_device);
void drv_usbh_cp210x_stop(struct usbh_cp210x *p_device);

#endif

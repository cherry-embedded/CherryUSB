Serial Host
=================

本节主要介绍 Host serial 框架的使用。Serial 框架当前支持 cdc acm, ftdi, cp210x, ch34x, pl2303，gsm 驱动。

.. figure:: img/usbh_serial.png

当前支持两种使用方式，一种是使用源生 CherryUSB usbhost serial API 进行操作，另一种是基于平台封装的 API 操作，比如 rt-thread device API，nuttx posix API。

下面演示的是使用 CherryUSB usbhost serial API 进行串口回环测试，并且使用阻塞发送，异步读取的方式：

.. code-block:: C

    struct usbh_serial *serial;

    serial = usbh_serial_open("/dev/ttyACM0", USBH_SERIAL_O_RDWR | USBH_SERIAL_O_NONBLOCK);
    if (serial == NULL) {
        serial = usbh_serial_open("/dev/ttyUSB0", USBH_SERIAL_O_RDWR | USBH_SERIAL_O_NONBLOCK);
        if (serial == NULL) {
            USB_LOG_RAW("no serial device found\r\n");
            goto delete;
        }
    }

    struct usbh_serial_termios termios;

    memset(&termios, 0, sizeof(termios));
    termios.baudrate = 115200;
    termios.stopbits = 0;
    termios.parity = 0;
    termios.databits = 8;
    termios.rtscts = false;
    termios.rx_timeout = 0;
    ret = usbh_serial_control(serial, USBH_SERIAL_CMD_SET_ATTR, &termios);
    if (ret < 0) {
        USB_LOG_RAW("set serial attr error, ret:%d\r\n", ret);
        goto delete_with_close;
    }

    serial_tx_bytes = 0;
    while (1) {
        ret = usbh_serial_write(serial, serial_tx_buffer, sizeof(serial_tx_buffer));
        if (ret < 0) {
            USB_LOG_RAW("serial write error, ret:%d\r\n", ret);
            goto delete_with_close;
        } else {
            serial_tx_bytes += ret;

            if (serial_tx_bytes == SERIAL_TEST_LEN) {
                USB_LOG_RAW("send over\r\n");
                break;
            }
        }
    }

    volatile uint32_t wait_timeout = 0;
    serial_rx_bytes = 0;
    while (1) {
        ret = usbh_serial_read(serial, &serial_rx_data[serial_rx_bytes], SERIAL_TEST_LEN - serial_rx_bytes);
        if (ret < 0) {
            USB_LOG_RAW("serial read error, ret:%d\r\n", ret);
            goto delete_with_close;
        } else {
            serial_rx_bytes += ret;

            if (serial_rx_bytes == SERIAL_TEST_LEN) {
                USB_LOG_RAW("receive over\r\n");
                for (uint32_t i = 0; i < SERIAL_TEST_LEN; i++) {
                    if (serial_rx_data[i] != 0xa5) {
                        USB_LOG_RAW("serial loopback data error at index %d, data: 0x%02x\r\n", (unsigned int)i, serial_rx_data[i]);
                        goto delete_with_close;
                    }
                }
                serial_test_success = true;
                break;
            }
        }
        wait_timeout++;

        if (wait_timeout > 500) { // 5s
            USB_LOG_RAW("serial read timeout\r\n");
            goto delete_with_close;
        }

        usb_osal_msleep(10);
    }

    usbh_serial_close(serial);

.. caution:: 需要注意，例程中使用的是比较简单的先发送后读取的方式，因此发送的总长度不可以超过 CONFIG_USBHOST_SERIAL_RX_SIZE，正常使用 TX/RX 请分开进行。

用户需要考虑以下三种场景：

- USB2TTL 设备 + 启用了波特率（USB2TTL设备必须启用波特率），这种情况下需要使用 `usbh_serial_write` 和 `usbh_serial_read` 进行收发数据， **并且 read 操作需要及时，防止 ringbuf 数据溢出而丢包**。不可以使用 `usbh_serial_cdc_write_async` 和 `usbh_serial_cdc_read_async`

- 纯 USB 设备 + 未启动波特率，这种情况下可以使用 `usbh_serial_cdc_write_async` 和 `usbh_serial_cdc_read_async` 进行异步收发数据。阻塞则可以用 `usbh_serial_write` ，不可以使用 `usbh_serial_read`。

- 纯 USB 设备 + 启动波特率，同 1，但是接收速率会打折扣（因为多了一层 ringbuf）。此时也不可以使用 `usbh_serial_cdc_write_async` 和 `usbh_serial_cdc_read_async`。 **如果是 GSM 设备请使用第一种场景**。

.. note:: 简单来说就是，如果接收数据需要用到ringbuf转一层的，请使用第一种场景。

.. code-block:: C

    [I/usbh_hub] New full-speed device on Bus 0, Hub 1, Port 1 connected
    [I/usbh_core] New device found,idVendor:10c4,idProduct:ea60,bcdDevice:0100
    [I/usbh_core] The device has 1 bNumConfigurations
    [I/usbh_core] The device has 1 interfaces
    [I/usbh_core] Enumeration success, start loading class driver
    [I/usbh_core] Loading cp210x class driver on interface 0
    [I/usbh_cp210x] chip partnum: 0x02
    [I/usbh_cp210x] ulAmountInInQueue: 0, ulAmountInOutQueue: 0
    [I/usbh_serial] Ep=81 Attr=02 Mps=64 Interval=00 Mult=00
    [I/usbh_serial] Ep=01 Attr=02 Mps=64 Interval=00 Mult=00
    [I/usbh_serial] Register Serial Class: /dev/ttyUSB0 (cp210x)
    start serial loopback test, len: 1024
    send over
    receive over
    serial loopback test success
    [I/usbh_serial] Unregister Serial Class: /dev/ttyUSB0 (cp210x)
    [I/usbh_core] Device on Bus 0, Hub 1, Port 1 disconnected
    [I/usbh_hub] New high-speed device on Bus 0, Hub 1, Port 1 connected
    [I/usbh_core] New device found,idVendor:0403,idProduct:6010,bcdDevice:0700
    [I/usbh_core] The device has 1 bNumConfigurations
    [I/usbh_core] The device has 2 interfaces
    [I/usbh_core] Enumeration success, start loading class driver
    [I/usbh_core] Loading ftdi class driver on interface 0
    [I/usbh_ftdi] chip name: FT2232H
    [I/usbh_serial] Ep=81 Attr=02 Mps=512 Interval=00 Mult=00
    [I/usbh_serial] Ep=02 Attr=02 Mps=512 Interval=00 Mult=00
    [I/usbh_serial] Register Serial Class: /dev/ttyUSB0 (ftdi)
    [I/usbh_core] Loading ftdi class driver on interface 1
    [I/usbh_ftdi] chip name: FT2232H
    [I/usbh_serial] Ep=83 Attr=02 Mps=512 Interval=00 Mult=00
    [I/usbh_serial] Ep=04 Attr=02 Mps=512 Interval=00 Mult=00
    [I/usbh_serial] Register Serial Class: /dev/ttyUSB1 (ftdi)
    start serial loopback test, len: 1024
    send over
    receive over
    serial loopback test success
    [I/usbh_serial] Unregister Serial Class: /dev/ttyUSB0 (ftdi)
    [I/usbh_serial] Unregister Serial Class: /dev/ttyUSB1 (ftdi)
    [I/usbh_core] Device on Bus 0, Hub 1, Port 1 disconnected
    [I/usbh_hub] New full-speed device on Bus 0, Hub 1, Port 1 connected
    [I/usbh_core] New device found,idVendor:067b,idProduct:2303,bcdDevice:0300
    [I/usbh_core] The device has 1 bNumConfigurations
    [I/usbh_core] The device has 1 interfaces
    [I/usbh_core] Enumeration success, start loading class driver
    [I/usbh_core] Loading pl2303 class driver on interface 0
    [I/usbh_pl2303] Ep=81 Attr=03 Mps=10 Interval=01 Mult=00
    [I/usbh_pl2303] chip type: PL2303HX
    [I/usbh_serial] Ep=02 Attr=02 Mps=64 Interval=00 Mult=00
    [I/usbh_serial] Ep=83 Attr=02 Mps=64 Interval=00 Mult=00
    [I/usbh_serial] Register Serial Class: /dev/ttyUSB0 (pl2303)
    start serial loopback test, len: 1024
    send over
    receive over
    serial loopback test success
    [I/usbh_serial] Unregister Serial Class: /dev/ttyUSB0 (pl2303)
    [I/usbh_core] Device on Bus 0, Hub 1, Port 1 disconnected
    [W/usbh_hub] Failed to enable port 1
    [I/usbh_hub] New full-speed device on Bus 0, Hub 1, Port 1 connected
    [I/usbh_core] New device found,idVendor:1a86,idProduct:7523,bcdDevice:0264
    [I/usbh_core] The device has 1 bNumConfigurations
    [I/usbh_core] The device has 1 interfaces
    [I/usbh_core] Enumeration success, start loading class driver
    [I/usbh_core] Loading ch34x class driver on interface 0
    [I/usbh_ch43x] Ep=81 Attr=03 Mps=8 Interval=01 Mult=00
    [I/usbh_ch43x] chip version: 0x31
    [I/usbh_serial] Ep=82 Attr=02 Mps=32 Interval=00 Mult=00
    [I/usbh_serial] Ep=02 Attr=02 Mps=32 Interval=00 Mult=00
    [I/usbh_serial] Register Serial Class: /dev/ttyUSB0 (ch34x)
    start serial loopback test, len: 1024
    send over
    receive over
    serial loopback test success
    [I/usbh_serial] Unregister Serial Class: /dev/ttyUSB0 (ch34x)
    [I/usbh_core] Device on Bus 0, Hub 1, Port 1 disconnected
    [I/usbh_hub] New full-speed device on Bus 0, Hub 1, Port 1 connected
    [I/usbh_core] New device found,idVendor:42bf,idProduct:b210,bcdDevice:0217
    [I/usbh_core] The device has 1 bNumConfigurations
    [I/usbh_core] The device has 3 interfaces
    [I/usbh_core] Enumeration success, start loading class driver
    [E/usbh_core] Do not support Class:0xff, Subclass:0x01, Protocl:0x00 on interface 0
    [I/usbh_core] Loading cdc_acm class driver on interface 1
    [I/usbh_cdc_acm] Ep=85 Attr=03 Mps=64 Interval=00 Mult=00
    [I/usbh_serial] Ep=04 Attr=02 Mps=64 Interval=00 Mult=00
    [I/usbh_serial] Ep=83 Attr=02 Mps=64 Interval=00 Mult=00
    [I/usbh_serial] Register Serial Class: /dev/ttyACM0 (cdc_acm)
    [I/usbh_core] Loading cdc_data class driver on interface 2
    start serial loopback test, len: 1024
    send over
    receive over
    serial loopback test success
    [I/usbh_serial] Unregister Serial Class: /dev/ttyACM0 (cdc_acm)
    [I/usbh_core] Device on Bus 0, Hub 1, Port 1 disconnected


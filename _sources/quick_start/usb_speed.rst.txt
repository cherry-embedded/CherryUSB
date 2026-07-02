USB 极限速度测试
======================

在 tools 中我们提供了一个 USB 极限速度测试工具，帮助用户测试 USB 设备的传输速度。该工具可以测量 USB 设备在不同条件下的读写速度，并生成详细的测试报告。

- test_cdc_speed.c + libusb_winusb_device_for_speed.c

该工具主要使用 libusb 进行测试，因此，需要保证本地文件需要有 libusb 库和头文件，device 使用 winusb，因为如果使用 cdc acm，则需要进行驱动转换。使用如下：

.. code-block:: C

    #默认已经提供 test_cdc_speed.exe

    gcc -O2 -o test_cdc_speed.exe .\test_cdc_speed.c .\libusb-1.0.dll.a -I. -lm
    .\test_cdc_speed.exe --vendor 0xFFFF --product 0xFFFF --interface 0 --mode tx --chunk-length 4M
    .\test_cdc_speed.exe --vendor 0xFFFF --product 0xFFFF --interface 0 --mode rx --chunk-length 4M

- test_cdc_speed.py

该工具主要使用 pyserial 进行测试，因此，需要保证已经安装 pyserial 库。使用如下：

.. code-block:: C

    python test_cdc_speed.py --port COM6 --mode tx --chunk-length 4M
    python test_cdc_speed.py --port COM6 --mode rx --chunk-length 4M

.. note:: python 测试 TX 和 libusb 基本相同，rx 会略低于 libusb，主要原因是因为 pyserial 读取后会放到缓存中，多一次拷贝导致
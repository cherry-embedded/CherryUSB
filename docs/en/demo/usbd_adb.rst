ADB Device
=================

The adb device demo refers to the `demo/adb/usbd_adb_template.c` template. It adapts to **cherrysh** (`platform/demo/adb/cherrysh_port.c`) and **rt-thread msh** (`platform/rtthread/usbd_adb_shell.c`) by default. You only need to add the following initialization in main.

.. code-block:: C

    cherryadb_init(0, xxxxx);

If using rt-thread, you also need to enable adb device in menuconfig.

.. figure:: img/rtt_adb_shell1.png

Entering ADB
--------------

- When using **cherrysh**, automatically enters adb mode after enumeration is completed
- When using **msh**, you need to input ``adb_enter`` in **msh** to enter adb mode

Exiting ADB
--------------

- When using **cherrysh**, input ``exit`` to exit adb mode
- When using **msh**, you need to input ``adb_exit`` in **msh** to exit adb mode

.. figure:: img/cherryadb.png

.. figure:: img/rtt_adb_shell2.png

USB OTG
=================

If you need to use OTG functionality, first the chip you're using needs to support ID detection capability, then enable the ``CONFIG_USB_OTG_ENABLE`` macro, and replace ``usbh_initialize`` or ``usbd_initialize`` in previous examples with ``usbotg_initialize``.

The ID detection circuit varies depending on different USB interface types, with micro-USB and USB-C being the two common interface types.

- If it's a micro-USB interface, connect the ID line to the chip's ID pin and enable the ID function.
- If it's a USB-C interface, since there's no ID pin, you need to use CC circuit to convert to ID and then connect to the chip's ID pin. A common circuit diagram is shown below (DNP means Do Not Populate):

.. figure:: img/otg.png


.. note:: In addition to the ID pin, you also need to add VBUS output switch control. When working in host mode, enable VBUS power supply; when working in device mode, disable VBUS power supply.
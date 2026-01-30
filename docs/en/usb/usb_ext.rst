.. _usb_ext:

USB Knowledge Extension
=========================================

What is Packetization
-------------------------

Due to USB protocol specifications defining the maximum length of each packet, when we send data that exceeds the maximum packet length, we need to send it in packets - this is packetization. For example, if EP MPS is 64 and data length is 129, USB will transmit in the form of 64 + 64 + 1.
For USB IP, packetization is divided into software packetization and hardware packetization. Software packetization means user code handles packetization itself - this type of IP generally uses FIFO because FIFO depth is limited. The second type
uses hardware packetization. This type of USB IP generally comes with DMA or descriptor DMA functionality, making this type of IP undoubtedly the most efficient. CherryUSB fully utilizes this point, enabling USB speed to reach its maximum.

For software packetization, even if the transmission length is 16K at once, **it is internally handled through software packetization. In this case, the transmission length has no impact on speed improvement**.
For hardware packetization, the transmission length affects speed because hardware packetization is performed through DMA, **the larger the transmission length at one time, the higher the DMA efficiency and the faster the speed**. (Of course, although other protocol stacks use DMA, some code implementations still process one packet at a time, which is equivalent to not using it, and this is also a reason for low speed)

What is a Short Packet
----------------------------

Based on what we discussed about packetization above, a short packet is the last packet in the packetization (and its length is less than EP MPS). For example, when sending 129 bytes of data, USB will transmit it in the form of 64 + 64 + 1, where the last packet is 1 byte, and this 1 byte is the short packet.

What is ZLP
-------------

ZLP, as the name suggests, is a Zero-Length Packet, which is a short packet with data length of 0. It's used by USB devices at the end of data transmission. If the data length is exactly a multiple of the maximum packet length, then a ZLP packet needs to be sent to inform the other party that data transmission has ended.

.. caution:: ZLP functionality is limited to CONTROL and BULK transfers

When is an Interrupt Considered Complete
--------------------------------------------

Device reception: The received length equals the set length; the last received packet is a short packet.
Device transmission: The transmitted length equals the set length. If the transmitted length is a multiple of EP MPS, **usually** a ZLP needs to be sent additionally (limited to control and bulk transfers).

.. note:: For device reception and bulk transfers, the reception length is usually designed as EP MPS. The following three cases can be modified to multiple EP MPS: fixed length; custom protocol with length information (e.g., MSC); host manually sends ZLP or short packet (e.g., RNDIS)

.. note:: For device transmission and bulk transfers, there is no limit on transmission length, but if it is a multiple of EP MPS, ZLP usually needs to be sent. Custom protocols do not need to send ZLP, such as MSC.

Host reception: Same as device reception
Host transmission: The transmitted length equals the set length

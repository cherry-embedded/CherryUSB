其他
=========================

usb_malloc
""""""""""""""""""""""""""""""""""""

``usb_malloc``  用来申请内存。

.. code-block:: C

    void *usb_malloc(size_t size);

- **size** 要申请的内存大小
- **return** 申请的内存地址

usb_free
""""""""""""""""""""""""""""""""""""

``usb_free``  用来释放申请的内存。

.. code-block:: C

    void usb_free(void *ptr);

- **ptr** 要释放的内存地址

usb_iomalloc
""""""""""""""""""""""""""""""""""""

``usb_iomalloc``  用来申请内存，并按照 `CONFIG_DCACHE_LINE_SIZE` 对齐，一般使用到 dcache 和 dma 需要对齐操作的时候使用。

.. code-block:: C

    void *usb_iomalloc(size_t size);

- **size** 要申请的内存大小
- **return** 申请的内存地址

usb_iofree
""""""""""""""""""""""""""""""""""""

``usb_iofree``  用来释放申请的内存。

.. code-block:: C

    void usb_iofree(void *ptr);

- **ptr** 要释放的内存地址

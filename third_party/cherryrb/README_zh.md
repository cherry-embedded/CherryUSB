# CherryRingBuffer

[English](./README.md)

CherryRingBuffer 是一个的高效、易用的环形缓冲区，其原理与kfifo一致。

## 简介

### 1.创建并初始化一个RingBuffer
```c
chry_ringbuffer_t rb;
uint8_t mempool[1024];

int main(void)
{
    /**
     * 需要注意的点是，init 函数第三个参数是内存池的大小（字节为单位）
     * 也是ringbuffer的深度，必须为 2 的幂次！！！。
     * 例如 4、16、32、64、128、1024、8192、65536等
     */
    if(0 == chry_ringbuffer_init(&rb, mempool, 1024)){
        printf("success\r\n");
    }else{
        printf("error\r\n");
    }

    return 0;
}
```

### 2.生产者消费者模型的无锁使用
读和写的API除了overwrite外，如果满足ringbuffer只在一个线程里进行读，并且只在一个线程里面写，那么无须加锁，因为读和写操作只单独操作读或者写指针。
```c
void thread_producer(void* param)
{
    char *data = "hello world";
    while(1){
        uint32_t len = chry_ringbuffer_write(&rb, data, 11);
        if(11 == len){
            printf("[P] write success\r\n");
        }else{
            printf("[P] write faild, only write %d byte\r\n", len);
        }
        vTaskDelay(100);
    }
}

void thread_consumer(void* param)
{
    char data[1024];
    while(1){
        uint32_t len =chry_ringbuffer_read(&rb, data, 11);
        if (len){
            printf("[C] read success, read %d byte\r\n",len);
            data[11]='\0';
            printf("%s\r\n",data);
        }else{
            printf("[C] read faild, no data in ringbuffer\r\n");
        }
        vTaskDelay(100);
    }
}
```

### 3. API简介

```c

    /**
     * 用于清空，同时操作读写指针，多线程需要加锁
     */
    chry_ringbuffer_reset(&rb);

    /**
     * 用于清空，只操作读指针，在唯一的读线程调用无须加锁
     */
    chry_ringbuffer_reset_read(&rb);

    /**
     * 获取ringbuffer总大小（字节）
     */
    uint32_t size = chry_ringbuffer_get_size(&rb);

    /**
     * 获取ringbuffer使用大小（字节）
     */
    uint32_t used = chry_ringbuffer_get_used(&rb);

    /**
     * 获取ringbuffer空闲大小（字节）
     */
    uint32_t free = chry_ringbuffer_get_free(&rb);

    /**
     * 检查ringbuffer是否为满，满返回true
     */
    bool is_full = chry_ringbuffer_check_full(&rb);

    /**
     * 检查ringbuffer是否为空，空返回true
     */
    bool is_empty = chry_ringbuffer_check_empty(&rb);

    uint8_t data = 0x55;

    /**
     * 写一字节
     * 在唯一的写线程调用无须加锁
     * 写成功返回true，满返回false
     */
    chry_ringbuffer_write_byte(&rb, data);

    /**
     * 覆盖写一字节，当ringbuffer满的时候会覆盖最早的数据，
     * 非满情况与 chry_ringbuffer_write_byte 一致。
     * 此API可能会同时操作读写指针，所以多线程需要加锁
     * 始终返回true
     */
    chry_ringbuffer_overwrite_byte(&rb, data);

    /**
     * 从ringbuffer中复制出来一字节
     * 数据仍然保留在ringbuffer中
     * 在唯一的读线程调用无须加锁
     * 成功返回true，空返回false
     */
    chry_ringbuffer_peek_byte(&rb, &data);

    /**
     * 从ringbuffer中读取出来一字节
     * 数据从ringbuffer中取出
     * 在唯一的读线程调用无须加锁
     * 成功返回true，空返回false
     */
    chry_ringbuffer_read_byte(&rb, &data);

    /**
     * 丢弃ringbuffer中一字节
     * 数据从ringbuffer中取出
     * 在唯一的读线程调用无须加锁
     * 成功返回true，空返回false
     */
    chry_ringbuffer_drop_byte(&rb);

    /**
     * 写多字节，其他与单字节写相同
     * 返回实际写入的长度
     */
    chry_ringbuffer_write(&rb, data, 1);

    /**
     * 覆盖写多字节，其他与单字节覆盖写相同
     * 返回实际写入的长度
     */
    chry_ringbuffer_overwrite(&rb, data, 1);

    /**
     * 复制多字节，其他与单字节复制相同
     * 返回实际复制的长度
     */
    chry_ringbuffer_peek(&rb, data, 1);

    /**
     * 读取多字节，其他与单字节读取相同
     * 返回实际读取的长度
     */
    chry_ringbuffer_read(&rb, data, 1);

    /**
     * 丢弃多字节，其他与单字节丢弃相同
     * 返回实际丢弃的长度
     */
    chry_ringbuffer_drop(&rb, 1);

    void *pdata;
    uint32_t size;

    /**
     * 用于启动DMA，获取读取起始内存地址和最大线性可读取长度
     */
    pdata = chry_ringbuffer_linear_read_setup(&rb, &size);

    /**
     * 用于DMA完成，增加读指针
     * 返回实际增加长度
     */
    size = chry_ringbuffer_linear_read_done(&rb, 512);

    /**
     * 用于启动DMA，获取写入起始内存地址和最大线性可写入长度
     */
    pdata = chry_ringbuffer_linear_write_setup(&rb, &size);

    /**
     * 用于DMA完成，增加写指针
     * 返回实际增加长度
     */
    size = chry_ringbuffer_linear_write_done(&rb, 512);

```
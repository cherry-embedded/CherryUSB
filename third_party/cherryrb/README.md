# CherryRingBuffer

[中文版](./README_zh.md)

CherryRingBuffer is an efficient and easy-to-use ringbuffer that is based on the same principle as kfifo.

## Brief

### 1.Create and initialize a RingBuffer
```c
chry_ringbuffer_t rb;
uint8_t mempool[1024];

int main(void)
{
    /**
     * Note that the third parameter of the init function is 
     * the size of the memory pool (in bytes)
     * It is also the depth of the ringbuffer, 
     * which must be a power of 2 !!!
     * Example: 4, 16, 32, 64, 128, 1024, 8192, 65536, etc.
     */
    if(0==chry_ringbuffer_init(&rb, mempool, 1024)){
        printf("success\r\n");
    }else{
        printf("error\r\n");
    }

    return 0;
}
```

### 2.Lock-free use of the producer-consumer model
The read and write APIs, except for overwrite, do not require locking if they satisfy the requirement that the ringbuffer is read in only one thread and written in only one thread, because the read and write operations only operate on the read or write pointers separately.
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

### 3. API Introduction

```c

    /**
     * Used to clear ringbuffer, while operating read and write pointers, 
     * multi-threaded need to add lock
     */
    chry_ringbuffer_reset(&rb);       

    /**
     * Used to clear ringbuffer, only operates on read pointers, 
     * called without locking in the only read thread
     */
    chry_ringbuffer_reset_read(&rb);

    /**
     * Get the total ringbuffer size (bytes)
     */
    uint32_t size = chry_ringbuffer_get_size(&rb);

    /**
     * Get ringbuffer used size (bytes)
     */
    uint32_t used = chry_ringbuffer_get_used(&rb);

    /**
     * Get ringbuffer free size (bytes)
     */
    uint32_t free = chry_ringbuffer_get_free(&rb);

    /**
     * Check if ringbuffer is full, full returns true
     */
    bool is_full = chry_ringbuffer_check_full(&rb);

    /**
     * Check if ringbuffer is empty, empty returns true
     */
    bool is_empty = chry_ringbuffer_check_empty(&rb);

    uint8_t data = 0x55;

    /**
     * Write a byte
     * No locking on the only write thread call
     * Success returns true, full returns false
     */
    chry_ringbuffer_write_byte(&rb, data);

    /**
     * overwriting write one byte, 
     * when ringbuffer is full will overwrite the earliest data
     * The non-full case is the same as chry_ringbuffer_write_byte
     * This API may operate on read and write pointers at the same time, 
     * so multiple threads need to add locks
     * Always return true
     */
    chry_ringbuffer_overwrite_byte(&rb, data);

    /**
     * Peek one byte from ringbuffer
     * The data is still retained in the ringbuffer
     * No locking on the only read thread call
     * Success returns true, empty returns false
     */
    chry_ringbuffer_peek_byte(&rb, &data);

    /**
     * Read one byte from ringbuffer
     * Data retrieval from ringbuffer
     * No locking on the only read thread call
     * Success returns true, empty returns false
     */
    chry_ringbuffer_read_byte(&rb, &data);

    /**
     * Drop one byte from ringbuffer
     * Data retrieval from ringbuffer
     * No locking on the only read thread call
     * Success returns true, empty returns false
     */
    chry_ringbuffer_drop_byte(&rb);

    /**
     * Write multi-byte, otherwise same as single byte write
     * Returns the actual length of the write
     */
    chry_ringbuffer_write(&rb, data, 1);

    /**
     * Overwrite multi-byte, otherwise same as single byte overwrite
     * Returns the actual length of the write
     */
    chry_ringbuffer_overwrite(&rb, data, 1);

    /**
     * Peek multiple bytes, otherwise same as single byte peek
     * Returns the length of the actual peek
     */
    chry_ringbuffer_peek(&rb, data, 1);

    /**
     * Read multiple bytes, otherwise same as single byte read
     * Returns the length of the actual read
     */
    chry_ringbuffer_read(&rb, data, 1);

    /**
     * Drop multiple bytes, otherwise same as single byte Drop
     * Returns the length of the actual drop
     */
    chry_ringbuffer_drop(&rb, 1);

    void *pdata;
    uint32_t size;

    /**
     * For dma start, get read memory address and max linear read size
     */
    pdata = chry_ringbuffer_linear_read_setup(&rb, &size);

    /**
     * For dma done, add read pointer
     * Returns the length of the actual add
     */
    size = chry_ringbuffer_linear_read_done(&rb, 512);

    /**
     * For dma start, get write memory address and max linear write size
     */
    pdata = chry_ringbuffer_linear_write_setup(&rb, &size);

    /**
     * For dma done, add write pointer
     * Returns the length of the actual add
     */
    size = chry_ringbuffer_linear_write_done(&rb, 512);

```
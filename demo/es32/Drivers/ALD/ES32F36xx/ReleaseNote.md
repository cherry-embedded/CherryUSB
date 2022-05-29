# ES32F36xx ALD Release Note

## V1.00 2019-12-16

1. First release

## V1.01 2020-05-07

1. Delete invalid member variables in can_filter_t->bank_number
2. Support FIFO0/FIFO1 receiving in CAN module
3. Add parameter check in ald_trng_init()
4. Add ping-pong, scatter-gather modes in DMA module
5. Add automatic control function of flash read waiting cycle

## V1.02 2020-05-20

1. Modify the ADC module structure member variables
2. Update ADC module driver
3. Support ADC1 in PIS module
4. Add get_hclk2_clock() API in ald_cmu.h

## V1.03 2020-08-24

1. Add API: ald_cmu_current_clock_source_get()
2. Add "Set/Read HCLK2 Bus Frequency Division" interface
3. Modify the API related to DMA in the TIMER module
4. Optimize the control logic of the discontinuous-scan-mode in ADC module
5. Optimize FIFO function in UART module
6. Add API: ald_dma_descriptor_cplt_get() in DMA module
7. Optimize ald_dma_config_sg_per() in DMA module
8. Fixed bug in md_usart_set_smartcard_psc()
9. Add API to get UID/CHIPID in utils.h
10. Add LDO12/LDO18 configure function in PMU module
11. Delete the operation of writing SSEC register in RTC module
12. Make the code comply with "C-STAT(STDCHECKS)" specification
13. Fixed a bug in ald_can.c
14. Fixed a bug in ald_uart.c
15. Optimize I2S module library functions


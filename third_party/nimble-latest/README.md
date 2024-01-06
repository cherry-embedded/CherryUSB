## Note

- ble_hs_startup_set_evmask_tx 函数中 BLE_HCI_OCF_CB_SET_EVENT_MASK2 功能需要关闭

```
    // if (version >= BLE_HCI_VER_BCS_4_1) {
    //     /**
    //      * Enable the following events:
    //      *     0x0000000000800000 Authenticated Payload Timeout Event
    //      */
    //     cmd2.event_mask2 = htole64(0x0000000000800000);
    //     rc = ble_hs_hci_cmd_tx(BLE_HCI_OP(BLE_HCI_OGF_CTLR_BASEBAND,
    //                                       BLE_HCI_OCF_CB_SET_EVENT_MASK2),
    //                            &cmd2, sizeof(cmd2), NULL, 0);
    //     if (rc != 0) {
    //         return rc;
    //     }
    // }

```

- 如果使用 rt-thread 中的软件包，请删除软件包中 SConscript 文件以下内容

```
# if rtconfig.CROSS_TOOL == 'keil':
    #LOCAL_CCFLAGS += ' --gnu --diag_suppress=111'
    # __BYTE_ORDER__ & __ORDER_BIG_ENDIAN__ & __ORDER_LITTLE_ENDIAN__ is not defined in keil, the specific values comes from gcc.
    # CPPDEFINES.append('__ORDER_LITTLE_ENDIAN__=1234')
    # CPPDEFINES.append('__ORDER_BIG_ENDIAN__=4321')
    # CPPDEFINES.append('__BYTE_ORDER__=1234')
    
```
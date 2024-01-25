## Note

当前支持 nimble 主线 1.6.0 版本，**禁止使用其余软件包或者第三方包**



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

- 主线中头文件和源文件参与编译例子

```
源文件

nimble/host/src/ble_att_clt.c
nimble/host/src/ble_att_cmd.c
nimble/host/src/ble_att_svr.c
nimble/host/src/ble_att.c
nimble/host/src/ble_audio_broadcast.c
nimble/host/src/ble_dtm.c
nimble/host/src/ble_eatt.c
nimble/host/src/ble_eddystone.c
nimble/host/src/ble_gap.c
nimble/host/src/ble_gattc.c
nimble/host/src/ble_gatts.c
nimble/host/src/ble_gatts_lcl.c
nimble/host/src/ble_hs.c
nimble/host/src/ble_hs_adv.c
nimble/host/src/ble_hs_atomic.c
nimble/host/src/ble_hs_cfg.c
nimble/host/src/ble_hs_conn.c
nimble/host/src/ble_hs_flow.c
nimble/host/src/ble_hs_hci.c
nimble/host/src/ble_hs_hci_cmd.c
nimble/host/src/ble_hs_hci_evt.c
nimble/host/src/ble_hs_hci_util.c
nimble/host/src/ble_hs_id.c
nimble/host/src/ble_hs_log.c
nimble/host/src/ble_hs_mbuf.c
nimble/host/src/ble_hs_misc.c
nimble/host/src/ble_hs_mqueue.c
nimble/host/src/ble_hs_periodic_sync.c
nimble/host/src/ble_hs_pvcy.c
nimble/host/src/ble_hs_shutdown.c
nimble/host/src/ble_hs_startup.c
nimble/host/src/ble_hs_stop.c
nimble/host/src/ble_ibeacon.c
nimble/host/src/ble_iso.c
nimble/host/src/ble_l2cap.c
nimble/host/src/ble_l2cap_coc.c
nimble/host/src/ble_l2cap_sig.c
nimble/host/src/ble_l2cap_sig_cmd.c
nimble/host/src/ble_sm.c
nimble/host/src/ble_sm_alg.c
nimble/host/src/ble_sm_cmd.c
nimble/host/src/ble_sm_lgcy.c
nimble/host/src/ble_sm_sc.c
nimble/host/src/ble_store.c
nimble/host/src/ble_store_util.c
nimble/host/src/ble_uuid.c

nimble/host/services/gap/src/ble_svc_gap.c
nimble/host/services/gatt/src/ble_svc_gatt.c
nimble/host/services/bas/src/ble_svc_bas.c
nimble/host/services/dis/src/ble_svc_dis.c
# nimble/host/store/config/src/ble_store_config.c
# nimble/host/store/config/src/ble_store_config_conf.c
nimble/host/store/ram/src/ble_store_ram.c
nimble/host/util/src/addr.c

nimble/transport/common/hci_h4/src/hci_h4.c
nimble/transport/src/transport.c

ext/tinycrypt/src/aes_decrypt.c
ext/tinycrypt/src/aes_encrypt.c
ext/tinycrypt/src/cbc_mode.c
ext/tinycrypt/src/ccm_mode.c
ext/tinycrypt/src/cmac_mode.c
ext/tinycrypt/src/ctr_mode.c
ext/tinycrypt/src/ctr_prng.c
ext/tinycrypt/src/ecc.c
ext/tinycrypt/src/ecc_dh.c
ext/tinycrypt/src/ecc_dsa.c
ext/tinycrypt/src/ecc_platform_specific.c
ext/tinycrypt/src/hmac.c
ext/tinycrypt/src/hmac_prng.c
ext/tinycrypt/src/sha256.c
ext/tinycrypt/src/utils.c


===========头文件=================

nimble/include
nimble/host/include
nimble/host/services/ans/include
nimble/host/services/bas/include
nimble/host/services/dis/include
nimble/host/services/gap/include
nimble/host/services/gatt/include
nimble/host/services/ias/include
nimble/host/services/lls/include
nimble/host/services/tps/include
nimble/host/store/ram/include
nimble/host/util/include
nimble/transport/include
ext/tinycrypt/include
nimble/transport/common/hci_h4/include



```

- porting 请禁止使用 nimble 官方源码，请使用 porting 目录下的文件

```
头文件
porting/nimble/include
porting/npl/freertos/include

源文件
porting/nimble/src/endian.c
porting/nimble/src/mem.c
porting/nimble/src/nimble_port.c
porting/nimble/src/os_mbuf.c
porting/nimble/src/os_mempool.c
porting/nimble/src/os_msys_init.c

porting/npl/freertos/src/nimble_port_freertos.c
porting/npl/freertos/src/npl_os_freertos.c
```



- 初始化 nimble，请注意，必须在 usb bluetooth 识别以后才能初始化

```
#include "nimble/nimble_port.h"
#include "usbh_core.h"

void nimble_thread_entry(void *parameter)
{
    nimble_port_run();
}

void nimble_init(void)
{
    nimble_port_init();

    usb_osal_thread_create("nimble", 2048, CONFIG_USBHOST_PSC_PRIO + 1, nimble_thread_entry, NULL);
}

```


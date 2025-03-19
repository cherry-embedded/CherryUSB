**[English](README.md) | [简体中文](README_zh-CN.md) | 繁體中文**

<h1 align="center" style="margin: 30px 0 30px; font-weight: bold;">CherryUSB</h1>
<p align="center">
	<a href="https://github.com/cherry-embedded/CherryUSB/releases"><img src="https://img.shields.io/github/release/cherry-embedded/CherryUSB.svg"></a>
	<a href="https://github.com/cherry-embedded/CherryUSB/blob/master/LICENSE"><img src="https://img.shields.io/github/license/cherry-embedded/CherryUSB.svg?style=flat-square"></a>
    <a href="https://github.com/cherry-embedded/CherryUSB/actions/workflows/deploy-docs.yml"><img src="https://github.com/cherry-embedded/CherryUSB/actions/workflows/deploy-docs.yml/badge.svg"> </a>
    <a href="https://discord.com/invite/wFfvrSAey8"><img src="https://img.shields.io/badge/Discord-blue?logo=discord&style=flat-square"> </a>
</p>

CherryUSB 是一款小巧精美、可移植性高的高效能 USB 主從協議棧，適用於內建 USB IP 的嵌入式系統。

![CherryUSB](CherryUSB.svg)

## 為何選擇 CherryUSB

### 易於學習 USB

為了幫助使用者學習 USB 基礎知識、枚舉、驅動加載與 IP 驅動，因此，編寫的代碼具備以下優點：

- 代碼精簡，邏輯簡單，無複雜 C 語法
- 樹狀編程，層層遞進
- Class 驅動與 porting 驅動模板化、精簡化
- API 分類清晰（設備端：初始化、註冊類別、命令回調、數據收發；主機端：初始化、類別查找、數據收發）

### 易於使用 USB

為了簡化 USB 介面的使用，考慮到使用者可能已熟悉 uart 和 dma，因此，設計的數據收發接口具有以下優勢：

- 等同於使用 uart tx dma/uart rx dma
- 收發長度無限制，使用者無需關心 USB 分包過程（分包在 porting 層處理）

### 發揮 USB 的最佳效能

考慮到 USB 性能問題，盡量達到 USB 硬件理論帶寬，因此，設計的數據收發類接口具備以下優點：

- Porting 驅動直接對接寄存器，無抽象層封裝
- Memory zero copy
- IP 如果帶 DMA 則使用 DMA 模式（DMA 帶硬件分包功能）
- 長度無限制，方便對接硬件 DMA 並且發揮 DMA 的優勢
- 分包過程在中斷中執行

## 目錄結構

|   目錄名    |           描述           |
|:--------:|:----------------------:|
|  class   |    usb class 類主從驅動     |
|  common  | usb spec 定義、常用宏、標准接口定義 |
|   core   |     usb 主從協議棧核心實現      |
|   demo   |     主從 class demo      |
|   docs   |           文檔           |
|   osal   |         os 封裝層         |
| platform |      其他 os 全家桶適配       |
|   port   | usb 主從需要實現的 porting 接口 |
|  tools   |          工具鏈接          |

## Device 協議棧簡介

CherryUSB Device 協議棧對標准設備請求、CLASS 請求、VENDOR 請求以及 custom 特殊請求規範了壹套統壹的函數框架，采用面向對象和鏈表的方式，能夠使得用戶快速上手複合設備，不用管底層的邏輯。同時，規範了壹套標准的 dcd porting 接口，用于適配不同的 USB IP，達到面向 ip 編程。

CherryUSB Device 協議棧當前實現以下功能：

- 支持 USB2.0 全速和高速設備，USB3.0 超速設備
- 支持端點中斷注冊功能，porting 給用戶自己處理中斷裏的數據
- 支持複合設備
- 支持 Communication Device Class (CDC_ACM, CDC_ECM)
- 支持 Human Interface Device (HID)
- 支持 Mass Storage Class (MSC)
- 支持 USB VIDEO CLASS (UVC1.0、UVC1.5)
- 支持 USB AUDIO CLASS (UAC1.0、UAC2.0)
- 支持 Device Firmware Upgrade CLASS (DFU)
- 支持 USB MIDI CLASS (MIDI)
- 支持 Remote NDIS (RNDIS)
- 支持 WINUSB1.0、WINUSB2.0、WEBUSB、BOS
- 支持 Vendor 類 class
- 支持 UF2
- 支持 Android Debug Bridge (Only support shell)
- 支持相同 USB IP 的多從機

CherryUSB Device 協議棧資源占用說明（GCC 10.2 with -O2）：

|      file      | FLASH (Byte) |   No Cache RAM (Byte)   | RAM (Byte) | Heap (Byte) |
|:--------------:|:------------:|:-----------------------:|:----------:|:-----------:|
|  usbd_core.c   |    ~4400     |   512(default) + 320    |     0      |      0      |
| usbd_cdc_acm.c |     ~400     |            0            |     0      |      0      |
|   usbd_msc.c   |    ~3800     |   128 + 512(default)    |     16     |      0      |
|   usbd_hid.c   |     ~360     |            0            |     0      |      0      |
|  usbd_audio.c  |    ~1500     |            0            |     0      |      0      |
|  usbd_video.c  |    ~2600     |            0            |     84     |      0      |
|  usbd_rndis.c  |    ~2100     | 2 * 1580(default)+156+8 |     76     |      0      |

## Host 協議棧簡介

CherryUSB Host 協議棧對挂載在 root hub、外部 hub 上的設備規範了壹套標准的枚舉實現，對不同的 Class 類也規範了壹套標准接口，用來指示在枚舉後和斷開連接後該 Class 驅動需要做的事情。同時，規範了壹套標准的 hcd porting 接口，用于適配不同的 USB IP，達到面向 IP 編程。最後，協議棧使用 OS 管理，並提供了 osal 用來適配不同的 os。

CherryUSB Host 協議棧當前實現以下功能：

- 支持 low speed, full speed, high speed 和 super speed 設備
- 自動加載支持的Class 驅動
- 支持阻塞式傳輸和異步傳輸
- 支持複合設備
- 支持多級 HUB，最高可拓展到 7 級(目前測試 1拖 10 沒有問題，僅支持 dwc2/ehci/xhci/rp2040)
- 支持 Communication Device Class (CDC_ACM, CDC_ECM)
- 支持 Human Interface Device (HID)
- 支持 Mass Storage Class (MSC)
- Support USB Video CLASS (UVC1.0、UVC1.5)
- Support USB Audio CLASS (UAC1.0)
- 支持 Remote NDIS (RNDIS)
- 支持 USB Bluetooth (支持 nimble and zephyr bluetooth 協議棧，支持 **CLASS: 0xE0** 或者廠家自定義類，類似于 cdc acm 功能)
- 支持 Vendor 類 class (serial, net, wifi)
- 支持 USB modeswitch
- 支持 Android Open Accessory
- 支持相同 USB IP 的多主機

同時，CherryUSB Host 協議棧還提供了 lsusb 的功能，借助 shell 插件可以查看所有挂載設備的信息，包括外部 hub 上的設備的信息。

CherryUSB Host 協議棧資源占用說明（GCC 10.2 with -O2）：

|       file       | FLASH (Byte) |     No Cache RAM (Byte)     |              RAM (Byte)              |   Heap (Byte)   |
|:----------------:|:------------:|:---------------------------:|:------------------------------------:|:---------------:|
|   usbh_core.c    |    ~9000     |     512 + 8 * (1+x) *n      |                  28                  | raw_config_desc |
|    usbh_hub.c    |    ~6000     |       32 + 4 * (1+x)        | 12 + sizeof(struct usbh_hub) * (1+x) |        0        |
|  usbh_cdc_acm.c  |     ~900     |              7              | 4  + sizeof(struct usbh_cdc_acm) * x |        0        |
|    usbh_msc.c    |    ~2700     |             64              |   4  + sizeof(struct usbh_msc) * x   |        0        |
|    usbh_hid.c    |    ~1400     |             256             |   4  + sizeof(struct usbh_hid) * x   |        0        |
|   usbh_video.c   |    ~3800     |             128             |  4  + sizeof(struct usbh_video) * x  |        0        |
|   usbh_audio.c   |    ~4100     |             128             |  4  + sizeof(struct usbh_audio) * x  |        0        |
|   usbh_rndis.c   |    ~4200     |   512 + 2 * 2048(default)   |    sizeof(struct usbh_rndis) * 1     |        0        |
|  usbh_cdc_ecm.c  |    ~2200     |        2 * 1514 + 16        |   sizeof(struct usbh_cdc_ecm) * 1    |        0        |
|  usbh_cdc_ncm.c  |    ~3300     | 2 * 2048(default) + 16 + 32 |   sizeof(struct usbh_cdc_ncm) * 1    |        0        |
| usbh_bluetooth.c |    ~1000     |      2 * 2048(default)      |  sizeof(struct usbh_bluetooth) * 1   |        0        |

其中，`sizeof(struct usbh_hub)` 和 `sizeof(struct usbh_hubport)` 受以下宏影響：

```
#define CONFIG_USBHOST_MAX_EXTHUBS          1
#define CONFIG_USBHOST_MAX_EHPORTS          4
#define CONFIG_USBHOST_MAX_INTERFACES       8
#define CONFIG_USBHOST_MAX_INTF_ALTSETTINGS 8
#define CONFIG_USBHOST_MAX_ENDPOINTS        4
```

x 受以下宏影響：

```
#define CONFIG_USBHOST_MAX_CDC_ACM_CLASS 4
#define CONFIG_USBHOST_MAX_HID_CLASS     4
#define CONFIG_USBHOST_MAX_MSC_CLASS     2
#define CONFIG_USBHOST_MAX_AUDIO_CLASS   1
#define CONFIG_USBHOST_MAX_VIDEO_CLASS   1
```

## USB IP 支持情況

僅列舉標准 USB IP 和商業性 USB IP

|         IP         |  device  | host  | Support status |
|:------------------:|:--------:|:-----:|:--------------:|
|    OHCI(intel)     |   none   | OHCI  |       √        |
|    EHCI(intel)     |   none   | EHCI  |       √        |
|    XHCI(intel)     |   none   | XHCI  |       √        |
|    UHCI(intel)     |   none   | UHCI  |       ×        |
|   DWC2(synopsys)   |   DWC2   | DWC2  |       √        |
|    MUSB(mentor)    |   MUSB   | MUSB  |       √        |
|  FOTG210(faraday)  | FOTG210  | EHCI  |       √        |
| CHIPIDEA(synopsys) | CHIPIDEA | EHCI  |       √        |
|   CDNS2(cadence)   |  CDNS2   | CDNS2 |       √        |
|   CDNS3(cadence)   |  CDNS3   | XHCI  |       ×        |
|   DWC3(synopsys)   |   DWC3   | XHCI  |       ×        |

## 文檔教程

CherryUSB 快速入門、USB 基本概念、API 手冊、Class 基本概念和例程，參考 [CherryUSB Documentation Tutorial](https://cherryusb.readthedocs.io/)。

## 視頻教程

- USB 基本知識點與 CherryUSB Device 協議棧是如何編寫的（使用 v0.4.1 版本），參考 https://www.bilibili.com/video/BV1Ef4y1t73d 。
- CherryUSB 騰訊會議（使用 v1.1.0 版本），參考 https://www.bilibili.com/video/BV16x421y7mM 。

## 圖形化界面配置工具

[chryusb_configurator](https://github.com/Egahp/chryusb_configurator) 采用 **electron + vite2 + ts** 框架編寫，當前用于自動化生成描述符數組，後續會增加其他功能。

## 示例倉庫

| Manufacturer  |     CHIP or Series      |           USB IP            |                                  Repo Url                                   | Support version |   Support status   |
|:-------------:|:-----------------------:|:---------------------------:|:---------------------------------------------------------------------------:|:---------------:|:------------------:|
|  Bouffalolab  |    BL702/BL616/BL808    |      bouffalolab/ehci       |          [bouffalo_sdk](https://github.com/CherryUSB/bouffalo_sdk)          |    <= latest    |     Long-term      |
|      ST       |        STM32F1x         |            fsdev            |         [stm32_repo](https://github.com/CherryUSB/cherryusb_stm32)          |    <= latest    |     Long-term      |
|      ST       |     STM32F4/STM32H7     |            dwc2             |         [stm32_repo](https://github.com/CherryUSB/cherryusb_stm32)          |    <= latest    |     Long-term      |
|    HPMicro    |     HPM6000/HPM5000     |          hpm/ehci           |               [hpm_sdk](https://github.com/CherryUSB/hpm_sdk)               |    <= latest    |     Long-term      |
|    Essemi     |        ES32F36xx        |            musb             |        [es32f369_repo](https://github.com/CherryUSB/cherryusb_es32)         |    <= latest    |     Long-term      |
|    Phytium    |          e2000          |         pusb2/xhci          |  [phytium_repo](https://gitee.com/phytium_embedded/phytium-free-rtos-sdk)   |     >=1.4.0     |     Long-term      |
|   Artinchip   |     d12x/d13x/d21x      |        aic/ehci/ohci        |            [luban-lite](https://gitee.com/artinchip/luban-lite)             |    <= latest    |     Long-term      |
|   Espressif   | esp32s2/esp32s3/esp32p4 |            dwc2             |         [esp32_repo](https://github.com/CherryUSB/cherryusb_esp32)          |    <= latest    |     Long-term      |
|      NXP      |           mcx           |    kinetis/chipidea/ehci    |         [nxp_mcx_repo](https://github.com/CherryUSB/cherryusb_mcx)          |    <= latest    |     Long-term      |
|   Kendryte    |          k230           |            dwc2             |             [k230_repo](https://github.com/CherryUSB/k230_sdk)              |     v1.2.0      |     Long-term      |
| Raspberry pi  |      rp2040/rp2350      |           rp2040            |         [pico-examples](https://github.com/CherryUSB/pico-examples)         |    <= latest    |     Long-term      |
| AllwinnerTech |     F1C100S/F1C200S     |            musb             | [cherryusb_rtt_f1c100s](https://github.com/CherryUSB/cherryusb_rtt_f1c100s) |    <= latest    | the same with musb |
|   Bekencorp   |      bk7256/bk7258      |            musb             |                [bk_idk](https://github.com/CherryUSB/bk_idk)                |     v0.7.0      | the same with musb |
|    Sophgo     |         cv18xx          |            dwc2             |        [cvi_alios_open](https://github.com/CherryUSB/cvi_alios_open)        |     v0.7.0      |        TBD         |
|      WCH      |     CH32V307/ch58x      | ch32_usbfs/ch32_usbhs/ch58x |           [wch_repo](https://github.com/CherryUSB/cherryusb_wch)            |   <= v0.10.2    |        TBD         |

## 軟件包支持

CherryUSB 軟件包可以通過以下方式獲取：

- [RT-Thread](https://packages.rt-thread.org/detail.html?package=CherryUSB)
- [YOC](https://www.xrvm.cn/document?temp=usb-host-protocol-stack-device-driver-adaptation-instructions&slug=yocbook)
- [ESP-Registry](https://components.espressif.com/components/cherry-embedded/cherryusb)

## 商業支持

參考 https://cherryusb.readthedocs.io/zh-cn/latest/support/index.html 。

## 聯系

CherryUSB QQ群：642693751

CherryUSB 微信群：與我聯系後邀請加入

## 支持企業

感謝以下企業支持（順序不分先後）：

<img src="docs/assets/bouffalolab.jpg"  width="100" height="80"/> <img src="docs/assets/hpmicro.jpg"  width="100" height="80" /> <img src="docs/assets/eastsoft.jpg"  width="100" height="80" /> <img src="docs/assets/rtthread.jpg"  width="100" height="80" /> <img src="docs/assets/sophgo.jpg"  width="100" height="80" /> <img src="docs/assets/phytium.jpg"  width="100" height="80" /> <img src="docs/assets/thead.jpg"  width="100" height="80" /> <img src="docs/assets/nuvoton.jpg"  width="100" height="80" /> <img src="docs/assets/artinchip.jpg"  width="100" height="80" /> <img src="docs/assets/bekencorp.jpg"  width="100" height="80" /> <img src="docs/assets/nxp.png"  width="100" height="80" /> <img src="docs/assets/espressif.png"  width="100" height="80" /> <img src="docs/assets/canaan.jpg"  width="100" height="80" />

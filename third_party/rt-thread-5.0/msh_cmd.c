/**************************************************************************/ /**
*
* @copyright (C) 2019 Nuvoton Technology Corp. All rights reserved.
*
* SPDX-License-Identifier: Apache-2.0
*
* Change Logs:
* Date            Author       Notes
* 2023-8-9        Wayne        First version
*
******************************************************************************/

#include <rtthread.h>

#if defined(PKG_USING_CHERRYUSB)

#if defined(PKG_CHERRYUSB_DEVICE_CDC_TEMPLATE)
void cdc_acm_init(void);
MSH_CMD_EXPORT(cdc_acm_init, start cdc_acm_init);
#endif

#if defined(PKG_CHERRYUSB_DEVICE_HID_MOUSE_TEMPLATE)
void hid_mouse_init(void);
void hid_mouse_test(void);

MSH_CMD_EXPORT(hid_mouse_init, start hid_mouse_init);
MSH_CMD_EXPORT(hid_mouse_test, start hid_mouse_test);
#endif

#if defined(PKG_CHERRYUSB_DEVICE_HID_KEYBOARD_TEMPLATE)
void hid_keyboard_init(void);
void hid_keyboard_test(void);

MSH_CMD_EXPORT(hid_keyboard_init, start hid_keyboard_init);
MSH_CMD_EXPORT(hid_keyboard_test, start hid_keyboard_test);
#endif

#if defined(PKG_CHERRYUSB_DEVICE_MSC_TEMPLATE)
void msc_ram_init(void);
MSH_CMD_EXPORT(msc_ram_init, start msc_ram_init);
#endif

#if defined(PKG_CHERRYUSB_DEVICE_RNDIS_TEMPLATE)
void cdc_rndis_init(void);
MSH_CMD_EXPORT(cdc_rndis_init, start cdc_rndis_init);
#endif

#if defined(PKG_CHERRYUSB_DEVICE_VIDEO_TEMPLATE)
void video_init(void);
void video_test(void);

MSH_CMD_EXPORT(video_init, start video_init);
MSH_CMD_EXPORT(video_test, start video_test);
#endif

#if defined(PKG_CHERRYUSB_DEVICE_AUDIO_V1_TEMPLATE)
void audio_v1_init(void);
void audio_v1_test(void);

MSH_CMD_EXPORT(audio_v1_init, start audio_v1_init);
MSH_CMD_EXPORT(audio_v1_test, start audio_v1_test);
#endif

#if defined(PKG_CHERRYUSB_DEVICE_AUDIO_V2_TEMPLATE)
void audio_v2_init(void);
void audio_v2_test(void);

MSH_CMD_EXPORT(audio_v2_init, start audio_v2_init);
MSH_CMD_EXPORT(audio_v2_test, start audio_v2_test);
#endif

#if defined(PKG_CHERRYUSB_HOST)
void usbh_class_test(void);
MSH_CMD_EXPORT(usbh_class_test, start usbh_class_test);

int lsusb(int argc, char **argv);
MSH_CMD_EXPORT(lsusb, start lsusb);

int usbh_initialize(void);
//INIT_APP_EXPORT(usbh_initialize);
MSH_CMD_EXPORT(usbh_initialize, start usbh_initialize);
#endif

#endif

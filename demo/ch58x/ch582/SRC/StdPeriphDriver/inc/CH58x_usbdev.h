


#ifndef __CH58x_USBDEV_H__
#define __CH58x_USBDEV_H__

#ifdef __cplusplus
 extern "C" {
#endif


/* 以下缓存区是USB模块收发使用的数据缓冲区，总共9个通道（9块缓存），用户可根据实际使用的通道数定义相应缓存区 */
extern PUINT8  pEP0_RAM_Addr;		//ep0(64)+ep4_out(64)+ep4_in(64)
extern PUINT8  pEP1_RAM_Addr;		//ep1_out(64)+ep1_in(64)
extern PUINT8  pEP2_RAM_Addr;		//ep2_out(64)+ep2_in(64)
extern PUINT8  pEP3_RAM_Addr;		//ep3_out(64)+ep3_in(64)

extern PUINT8  pU2EP0_RAM_Addr;   //ep0(64)+ep4_out(64)+ep4_in(64)
extern PUINT8  pU2EP1_RAM_Addr;   //ep1_out(64)+ep1_in(64)
extern PUINT8  pU2EP2_RAM_Addr;   //ep2_out(64)+ep2_in(64)
extern PUINT8  pU2EP3_RAM_Addr;   //ep3_out(64)+ep3_in(64)

#define pSetupReqPak    ((PUSB_SETUP_REQ)pEP0_RAM_Addr)
#define pEP0_DataBuf    (pEP0_RAM_Addr)
#define pEP1_OUT_DataBuf  (pEP1_RAM_Addr)
#define pEP1_IN_DataBuf   (pEP1_RAM_Addr+64)
#define pEP2_OUT_DataBuf  (pEP2_RAM_Addr)
#define pEP2_IN_DataBuf   (pEP2_RAM_Addr+64)
#define pEP3_OUT_DataBuf  (pEP3_RAM_Addr)
#define pEP3_IN_DataBuf   (pEP3_RAM_Addr+64)
#define pEP4_OUT_DataBuf  (pEP0_RAM_Addr+64)
#define pEP4_IN_DataBuf   (pEP0_RAM_Addr+128)

#define pU2SetupReqPak    ((PUSB_SETUP_REQ)pU2EP0_RAM_Addr)
#define pU2EP0_DataBuf    (pU2EP0_RAM_Addr)
#define pU2EP1_OUT_DataBuf  (pU2EP1_RAM_Addr)
#define pU2EP1_IN_DataBuf   (pU2EP1_RAM_Addr+64)
#define pU2EP2_OUT_DataBuf  (pU2EP2_RAM_Addr)
#define pU2EP2_IN_DataBuf   (pU2EP2_RAM_Addr+64)
#define pU2EP3_OUT_DataBuf  (pU2EP3_RAM_Addr)
#define pU2EP3_IN_DataBuf   (pU2EP3_RAM_Addr+64)
#define pU2EP4_OUT_DataBuf  (pU2EP0_RAM_Addr+64)
#define pU2EP4_IN_DataBuf   (pU2EP0_RAM_Addr+128)


void USB_DeviceInit( void );			/* USB设备功能初始化，4个端点，8个通道 */	 
void USB_DevTransProcess( void );		/* USB设备应答传输处理 */	 
	 
void DevEP1_OUT_Deal( UINT8 l );		/* 设备端点1下传通道处理 */
void DevEP2_OUT_Deal( UINT8 l );		/* 设备端点2下传通道处理 */
void DevEP3_OUT_Deal( UINT8 l );		/* 设备端点3下传通道处理 */
void DevEP4_OUT_Deal( UINT8 l );		/* 设备端点4下传通道处理 */

void DevEP1_IN_Deal( UINT8 l );		/* 设备端点1上传通道处理 */
void DevEP2_IN_Deal( UINT8 l );		/* 设备端点2上传通道处理 */
void DevEP3_IN_Deal( UINT8 l );		/* 设备端点3上传通道处理 */
void DevEP4_IN_Deal( UINT8 l );		/* 设备端点4上传通道处理 */

// 0-未完成  (!0)-已完成
#define EP1_GetINSta()		(R8_UEP1_CTRL&UEP_T_RES_NAK)		/* 查询端点1是否上传完成 */
#define EP2_GetINSta()		(R8_UEP2_CTRL&UEP_T_RES_NAK)		/* 查询端点2是否上传完成 */
#define EP3_GetINSta()		(R8_UEP3_CTRL&UEP_T_RES_NAK)		/* 查询端点3是否上传完成 */
#define EP4_GetINSta()		(R8_UEP4_CTRL&UEP_T_RES_NAK)		/* 查询端点4是否上传完成 */

void USB2_DeviceInit( void );      /* USB2设备功能初始化，4个端点，8个通道 */
void USB2_DevTransProcess( void );   /* USB2设备应答传输处理 */

void U2DevEP1_OUT_Deal( UINT8 l );    /* 设备端点1下传通道处理 */
void U2DevEP2_OUT_Deal( UINT8 l );    /* 设备端点2下传通道处理 */
void U2DevEP3_OUT_Deal( UINT8 l );    /* 设备端点3下传通道处理 */
void U2DevEP4_OUT_Deal( UINT8 l );    /* 设备端点4下传通道处理 */

void U2DevEP1_IN_Deal( UINT8 l );   /* 设备端点1上传通道处理 */
void U2DevEP2_IN_Deal( UINT8 l );   /* 设备端点2上传通道处理 */
void U2DevEP3_IN_Deal( UINT8 l );   /* 设备端点3上传通道处理 */
void U2DevEP4_IN_Deal( UINT8 l );   /* 设备端点4上传通道处理 */

// 0-未完成  (!0)-已完成
#define U2EP1_GetINSta()    (R8_U2EP1_CTRL&UEP_T_RES_NAK)    /* 查询端点1是否上传完成 */
#define U2EP2_GetINSta()    (R8_U2EP2_CTRL&UEP_T_RES_NAK)    /* 查询端点2是否上传完成 */
#define U2EP3_GetINSta()    (R8_U2EP3_CTRL&UEP_T_RES_NAK)    /* 查询端点3是否上传完成 */
#define U2EP4_GetINSta()    (R8_U2EP4_CTRL&UEP_T_RES_NAK)    /* 查询端点4是否上传完成 */

	 
#ifdef __cplusplus
}
#endif

#endif  // __CH58x_USBDEV_H__


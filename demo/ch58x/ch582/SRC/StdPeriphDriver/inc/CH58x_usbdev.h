


#ifndef __CH58x_USBDEV_H__
#define __CH58x_USBDEV_H__

#ifdef __cplusplus
 extern "C" {
#endif


/* ���»�������USBģ���շ�ʹ�õ����ݻ��������ܹ�9��ͨ����9�黺�棩���û��ɸ���ʵ��ʹ�õ�ͨ����������Ӧ������ */
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


void USB_DeviceInit( void );			/* USB�豸���ܳ�ʼ����4���˵㣬8��ͨ�� */	 
void USB_DevTransProcess( void );		/* USB�豸Ӧ���䴦�� */	 
	 
void DevEP1_OUT_Deal( UINT8 l );		/* �豸�˵�1�´�ͨ������ */
void DevEP2_OUT_Deal( UINT8 l );		/* �豸�˵�2�´�ͨ������ */
void DevEP3_OUT_Deal( UINT8 l );		/* �豸�˵�3�´�ͨ������ */
void DevEP4_OUT_Deal( UINT8 l );		/* �豸�˵�4�´�ͨ������ */

void DevEP1_IN_Deal( UINT8 l );		/* �豸�˵�1�ϴ�ͨ������ */
void DevEP2_IN_Deal( UINT8 l );		/* �豸�˵�2�ϴ�ͨ������ */
void DevEP3_IN_Deal( UINT8 l );		/* �豸�˵�3�ϴ�ͨ������ */
void DevEP4_IN_Deal( UINT8 l );		/* �豸�˵�4�ϴ�ͨ������ */

// 0-δ���  (!0)-�����
#define EP1_GetINSta()		(R8_UEP1_CTRL&UEP_T_RES_NAK)		/* ��ѯ�˵�1�Ƿ��ϴ���� */
#define EP2_GetINSta()		(R8_UEP2_CTRL&UEP_T_RES_NAK)		/* ��ѯ�˵�2�Ƿ��ϴ���� */
#define EP3_GetINSta()		(R8_UEP3_CTRL&UEP_T_RES_NAK)		/* ��ѯ�˵�3�Ƿ��ϴ���� */
#define EP4_GetINSta()		(R8_UEP4_CTRL&UEP_T_RES_NAK)		/* ��ѯ�˵�4�Ƿ��ϴ���� */

void USB2_DeviceInit( void );      /* USB2�豸���ܳ�ʼ����4���˵㣬8��ͨ�� */
void USB2_DevTransProcess( void );   /* USB2�豸Ӧ���䴦�� */

void U2DevEP1_OUT_Deal( UINT8 l );    /* �豸�˵�1�´�ͨ������ */
void U2DevEP2_OUT_Deal( UINT8 l );    /* �豸�˵�2�´�ͨ������ */
void U2DevEP3_OUT_Deal( UINT8 l );    /* �豸�˵�3�´�ͨ������ */
void U2DevEP4_OUT_Deal( UINT8 l );    /* �豸�˵�4�´�ͨ������ */

void U2DevEP1_IN_Deal( UINT8 l );   /* �豸�˵�1�ϴ�ͨ������ */
void U2DevEP2_IN_Deal( UINT8 l );   /* �豸�˵�2�ϴ�ͨ������ */
void U2DevEP3_IN_Deal( UINT8 l );   /* �豸�˵�3�ϴ�ͨ������ */
void U2DevEP4_IN_Deal( UINT8 l );   /* �豸�˵�4�ϴ�ͨ������ */

// 0-δ���  (!0)-�����
#define U2EP1_GetINSta()    (R8_U2EP1_CTRL&UEP_T_RES_NAK)    /* ��ѯ�˵�1�Ƿ��ϴ���� */
#define U2EP2_GetINSta()    (R8_U2EP2_CTRL&UEP_T_RES_NAK)    /* ��ѯ�˵�2�Ƿ��ϴ���� */
#define U2EP3_GetINSta()    (R8_U2EP3_CTRL&UEP_T_RES_NAK)    /* ��ѯ�˵�3�Ƿ��ϴ���� */
#define U2EP4_GetINSta()    (R8_U2EP4_CTRL&UEP_T_RES_NAK)    /* ��ѯ�˵�4�Ƿ��ϴ���� */

	 
#ifdef __cplusplus
}
#endif

#endif  // __CH58x_USBDEV_H__


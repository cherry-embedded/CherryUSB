


#ifndef __CH58x_USBHOST_H__
#define __CH58x_USBHOST_H__

#ifdef __cplusplus
 extern "C" {
#endif

#if DISK_LIB_ENABLE
#if DISK_WITHOUT_USB_HUB
/***************************************** /* ��ʹ��U���ļ�ϵͳ�����U�̹���USBhub���棬��Ҫ�ر����涨�� */
#define	FOR_ROOT_UDISK_ONLY
#endif
/***************************************** /* ʹ��U���ļ�ϵͳ�⣬��Ҫ�������涨��, ��ʹ����ر� */
#define DISK_BASE_BUF_LEN		512	        /* Ĭ�ϵĴ������ݻ�������СΪ512�ֽ�,����ѡ��Ϊ2048����4096��֧��ĳЩ��������U��,Ϊ0���ֹ��.H�ļ��ж��建��������Ӧ�ó�����pDISK_BASE_BUF��ָ�� */
#endif
     
     
// ���ӳ��򷵻�״̬��
#define ERR_SUCCESS         0x00    // �����ɹ�
#define ERR_USB_CONNECT     0x15    /* ��⵽USB�豸�����¼�,�Ѿ����� */
#define ERR_USB_DISCON      0x16    /* ��⵽USB�豸�Ͽ��¼�,�Ѿ��Ͽ� */
#define ERR_USB_BUF_OVER    0x17    /* USB��������������������̫�໺������� */
#define ERR_USB_DISK_ERR    0x1F    /* USB�洢������ʧ��,�ڳ�ʼ��ʱ������USB�洢����֧��,�ڶ�д�����п����Ǵ����𻵻����Ѿ��Ͽ� */
#define ERR_USB_TRANSFER    0x20    /* NAK/STALL�ȸ����������0x20~0x2F */
#define ERR_USB_UNSUPPORT   0xFB    /* ��֧�ֵ�USB�豸*/
#define ERR_USB_UNKNOWN     0xFE    /* �豸��������*/
#define ERR_AOA_PROTOCOL    0x41    /* Э��汾���� */

/*USB�豸�����Ϣ��,���֧��1���豸*/
#define ROOT_DEV_DISCONNECT  0
#define ROOT_DEV_CONNECTED   1
#define ROOT_DEV_FAILED      2
#define ROOT_DEV_SUCCESS     3
#define DEV_TYPE_KEYBOARD   ( USB_DEV_CLASS_HID | 0x20 )
#define DEV_TYPE_MOUSE      ( USB_DEV_CLASS_HID | 0x30 )
#define DEF_AOA_DEVICE       0xF0
#define DEV_TYPE_UNKNOW 	 0xFF


/*
Լ��: USB�豸��ַ�������(�ο�USB_DEVICE_ADDR)
��ֵַ  �豸λ��
0x02    ����Root-HUB�µ�USB�豸���ⲿHUB
0x1x    ����Root-HUB�µ��ⲿHUB�Ķ˿�x�µ�USB�豸,xΪ1~n
*/
#define HUB_MAX_PORTS       	4
#define WAIT_USB_TOUT_200US     800   // �ȴ�USB�жϳ�ʱʱ��


typedef struct
{
    UINT8   DeviceStatus;              // �豸״̬,0-���豸,1-���豸����δ��ʼ��,2-���豸����ʼ��ö��ʧ��,3-���豸�ҳ�ʼ��ö�ٳɹ�
    UINT8   DeviceAddress;             // �豸�������USB��ַ
    UINT8   DeviceSpeed;               // 0Ϊ����,��0Ϊȫ��
    UINT8   DeviceType;                // �豸����
    UINT16  DeviceVID;
    UINT16  DevicePID;
    UINT8   GpVar[4];                    // ͨ�ñ�������Ŷ˵�
    UINT8   GpHUBPortNum;                // ͨ�ñ���,�����HUB����ʾHUB�˿���
} _RootHubDev;


extern _RootHubDev  ThisUsbDev;
extern UINT8  UsbDevEndp0Size;              // USB�豸�Ķ˵�0�������ߴ� */
extern UINT8  FoundNewDev;

extern PUINT8  pHOST_RX_RAM_Addr;
extern PUINT8  pHOST_TX_RAM_Addr;

extern _RootHubDev  ThisUsb2Dev;
extern UINT8  Usb2DevEndp0Size;              // USB�豸�Ķ˵�0�������ߴ� */
extern UINT8  FoundNewU2Dev;

extern PUINT8  pU2HOST_RX_RAM_Addr;
extern PUINT8  pU2HOST_TX_RAM_Addr;

#define pSetupReq   ((PUSB_SETUP_REQ)pHOST_TX_RAM_Addr)
#define pU2SetupReq   ((PUSB_SETUP_REQ)pU2HOST_TX_RAM_Addr)
extern UINT8  Com_Buffer[];
extern UINT8  U2Com_Buffer[];

/* ����ΪUSB��������� */
extern const UINT8  SetupGetDevDescr[];    // ��ȡ�豸������*/
extern const UINT8  SetupGetCfgDescr[];    // ��ȡ����������*/
extern const UINT8  SetupSetUsbAddr[];     // ����USB��ַ*/
extern const UINT8  SetupSetUsbConfig[];   // ����USB����*/
extern const UINT8  SetupSetUsbInterface[];// ����USB�ӿ�����*/
extern const UINT8  SetupClrEndpStall[];   // ����˵�STALL*/

extern const UINT8  SetupGetU2DevDescr[];    // ��ȡ�豸������*/
extern const UINT8  SetupGetU2CfgDescr[];    // ��ȡ����������*/
extern const UINT8  SetupSetUsb2Addr[];     // ����USB��ַ*/
extern const UINT8  SetupSetUsb2Config[];   // ����USB����*/
extern const UINT8  SetupSetUsb2Interface[];// ����USB�ӿ�����*/
extern const UINT8  SetupClrU2EndpStall[];   // ����˵�STALL*/

void  DisableRootHubPort(void)  ;                   // �ر�ROOT-HUB�˿�,ʵ����Ӳ���Ѿ��Զ��ر�,�˴�ֻ�����һЩ�ṹ״̬
UINT8   AnalyzeRootHub( void ) ;         // ����ROOT-HUB״̬,����ROOT-HUB�˿ڵ��豸����¼�
// ����ERR_SUCCESSΪû�����,����ERR_USB_CONNECTΪ��⵽������,����ERR_USB_DISCONΪ��⵽�Ͽ�
void    SetHostUsbAddr( UINT8 addr );                 // ����USB������ǰ������USB�豸��ַ
void    SetUsbSpeed( UINT8 FullSpeed );               // ���õ�ǰUSB�ٶ�
void    ResetRootHubPort(void);                          // ��⵽�豸��,��λ��Ӧ�˿ڵ�����,Ϊö���豸׼��,����ΪĬ��Ϊȫ��
UINT8   EnableRootHubPort(void);                          // ʹ��ROOT-HUB�˿�,��Ӧ��bUH_PORT_EN��1�����˿�,�豸�Ͽ����ܵ��·���ʧ��
void    SelectHubPort( UINT8 HubPortIndex );// HubPortIndex=0ѡ�����ָ����ROOT-HUB�˿�,����ѡ�����ָ����ROOT-HUB�˿ڵ��ⲿHUB��ָ���˿�
UINT8   WaitUSB_Interrupt( void );                    // �ȴ�USB�ж�
// ��������,����Ŀ�Ķ˵��ַ/PID����,ͬ����־,��20uSΪ��λ��NAK������ʱ��(0������,0xFFFF��������),����0�ɹ�,��ʱ/��������
UINT8   USBHostTransact( UINT8 endp_pid, UINT8 tog, UINT32 timeout );  // endp_pid: ��4λ��token_pid����, ��4λ�Ƕ˵��ַ
UINT8   HostCtrlTransfer( PUINT8 DataBuf, PUINT8 RetLen );  // ִ�п��ƴ���,8�ֽ���������pSetupReq��,DataBufΪ��ѡ���շ�������
// �����Ҫ���պͷ�������,��ôDataBuf��ָ����Ч���������ڴ�ź�������,ʵ�ʳɹ��շ����ܳ��ȷ��ر�����ReqLenָ����ֽڱ�����


void CopySetupReqPkg( PCCHAR pReqPkt );            // ���ƿ��ƴ���������
UINT8 CtrlGetDeviceDescr( void );                    // ��ȡ�豸������,������ pHOST_TX_RAM_Addr ��
UINT8 CtrlGetConfigDescr( void );                    // ��ȡ����������,������ pHOST_TX_RAM_Addr ��
UINT8 CtrlSetUsbAddress( UINT8 addr );                         // ����USB�豸��ַ
UINT8 CtrlSetUsbConfig( UINT8 cfg );                           // ����USB�豸����
UINT8 CtrlClearEndpStall( UINT8 endp ) ;                       // ����˵�STALL
UINT8 CtrlSetUsbIntercace( UINT8 cfg );					       // ����USB�豸�ӿ�


void    USB_HostInit( void );                                  // ��ʼ��USB����


void  DisableRootU2HubPort(void)  ;                   // �ر�ROOT-U2HUB�˿�,ʵ����Ӳ���Ѿ��Զ��ر�,�˴�ֻ�����һЩ�ṹ״̬
UINT8   AnalyzeRootU2Hub( void ) ;         // ����ROOT-U2HUB״̬,����ROOT-U2HUB�˿ڵ��豸����¼�
// ����ERR_SUCCESSΪû�����,����ERR_USB_CONNECTΪ��⵽������,����ERR_USB_DISCONΪ��⵽�Ͽ�
void    SetHostUsb2Addr( UINT8 addr );                 // ����USB������ǰ������USB�豸��ַ
void    SetUsb2Speed( UINT8 FullSpeed );               // ���õ�ǰUSB�ٶ�
void    ResetRootU2HubPort(void);                          // ��⵽�豸��,��λ��Ӧ�˿ڵ�����,Ϊö���豸׼��,����ΪĬ��Ϊȫ��
UINT8   EnableRootU2HubPort(void);                          // ʹ��ROOT-HUB�˿�,��Ӧ��bUH_PORT_EN��1�����˿�,�豸�Ͽ����ܵ��·���ʧ��
void    SelectU2HubPort( UINT8 HubPortIndex );// HubPortIndex=0ѡ�����ָ����ROOT-HUB�˿�,����ѡ�����ָ����ROOT-HUB�˿ڵ��ⲿHUB��ָ���˿�
UINT8   WaitUSB2_Interrupt( void );                    // �ȴ�USB�ж�
// ��������,����Ŀ�Ķ˵��ַ/PID����,ͬ����־,��20uSΪ��λ��NAK������ʱ��(0������,0xFFFF��������),����0�ɹ�,��ʱ/��������
UINT8   USB2HostTransact( UINT8 endp_pid, UINT8 tog, UINT32 timeout );  // endp_pid: ��4λ��token_pid����, ��4λ�Ƕ˵��ַ
UINT8   U2HostCtrlTransfer( PUINT8 DataBuf, PUINT8 RetLen );  // ִ�п��ƴ���,8�ֽ���������pSetupReq��,DataBufΪ��ѡ���շ�������
// �����Ҫ���պͷ�������,��ôDataBuf��ָ����Ч���������ڴ�ź�������,ʵ�ʳɹ��շ����ܳ��ȷ��ر�����ReqLenָ����ֽڱ�����


void CopyU2SetupReqPkg( PCCHAR pReqPkt );            // ���ƿ��ƴ���������
UINT8 CtrlGetU2DeviceDescr( void );                    // ��ȡ�豸������,������ pHOST_TX_RAM_Addr ��
UINT8 CtrlGetU2ConfigDescr( void );                    // ��ȡ����������,������ pHOST_TX_RAM_Addr ��
UINT8 CtrlSetUsb2Address( UINT8 addr );                         // ����USB�豸��ַ
UINT8 CtrlSetUsb2Config( UINT8 cfg );                           // ����USB�豸����
UINT8 CtrlClearU2EndpStall( UINT8 endp ) ;                       // ����˵�STALL
UINT8 CtrlSetUsb2Intercace( UINT8 cfg );                // ����USB�豸�ӿ�


void    USB2_HostInit( void );                                  // ��ʼ��USB����

/*************************************************************/


UINT8 InitRootDevice( void );

UINT8   CtrlGetHIDDeviceReport( UINT8 infc );          // HID�����SET_IDLE��GET_REPORT
UINT8   CtrlGetHubDescr( void );                       // ��ȡHUB������,������TxBuffer��
UINT8   HubGetPortStatus( UINT8 HubPortIndex );        // ��ѯHUB�˿�״̬,������TxBuffer��
UINT8   HubSetPortFeature( UINT8 HubPortIndex, UINT8 FeatureSelt );  // ����HUB�˿�����
UINT8   HubClearPortFeature( UINT8 HubPortIndex, UINT8 FeatureSelt );  // ���HUB�˿�����
	 	 
UINT8 InitRootU2Device( void );

UINT8   CtrlGetU2HIDDeviceReport( UINT8 infc );          // HID�����SET_IDLE��GET_REPORT
UINT8   CtrlGetU2HubDescr( void );                       // ��ȡHUB������,������TxBuffer��
UINT8   U2HubGetPortStatus( UINT8 HubPortIndex );        // ��ѯHUB�˿�״̬,������TxBuffer��
UINT8   U2HubSetPortFeature( UINT8 HubPortIndex, UINT8 FeatureSelt );  // ����HUB�˿�����
UINT8   U2HubClearPortFeature( UINT8 HubPortIndex, UINT8 FeatureSelt );  // ���HUB�˿�����


	 
#ifdef __cplusplus
}
#endif

#endif  // __CH58x_USBHOST_H__


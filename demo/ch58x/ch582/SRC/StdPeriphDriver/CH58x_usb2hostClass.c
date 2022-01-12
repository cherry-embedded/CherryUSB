/********************************** (C) COPYRIGHT *******************************
* File Name          : CH58x_usbhost.c
* Author             : WCH
* Version            : V1.0
* Date               : 2018/12/15
* Description 
*******************************************************************************/

#include "CH58x_common.h"
#if DISK_LIB_ENABLE
#include "CHRV3UFI.H"
#endif

/* ����HID�ϴ����� */
__attribute__((aligned(4))) const UINT8  SetupSetU2HIDIdle[] = { 0x21, HID_SET_IDLE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
/* ��ȡHID�豸���������� */
__attribute__((aligned(4))) const UINT8  SetupGetU2HIDDevReport[] = { 0x81, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_REPORT, 0x00, 0x00, 0x41, 0x00 };
/* ��ȡHUB������ */
__attribute__((aligned(4))) const UINT8  SetupGetU2HubDescr[] = { HUB_GET_HUB_DESCRIPTOR, HUB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_HUB, 0x00, 0x00, sizeof( USB_HUB_DESCR ), 0x00 };


__attribute__((aligned(4))) UINT8  U2Com_Buffer[ 128 ];            // �����û���ʱ������,ö��ʱ���ڴ���������,ö�ٽ���Ҳ����������ͨ��ʱ������
    
/*****************************************************************************
* Function Name  : InitRootU2Device
* Description    : ��ʼ��ָ��ROOT-U2HUB�˿ڵ�USB�豸
* Input          : DataBuf: ö�ٹ����д�ŵ���������Ϣ������
* Return         :
*******************************************************************************/
UINT8 InitRootU2Device( void )
{
    UINT8  i, s;
    UINT8  cfg, dv_cls, if_cls;
	
    PRINT( "Reset U2 host port\n" );
    ResetRootU2HubPort( );  		// ��⵽�豸��,��λ��Ӧ�˿ڵ�USB����
    for ( i = 0, s = 0; i < 100; i ++ ) {  				// �ȴ�USB�豸��λ����������,100mS��ʱ
        mDelaymS( 1 );
        if ( EnableRootU2HubPort( ) == ERR_SUCCESS ) {  // ʹ�ܶ˿�
            i = 0;
            s ++;  					
            if ( s > 100 ) break;  	// �Ѿ��ȶ�����100mS
        }
    }
    if ( i ) {  										// ��λ���豸û������
        DisableRootU2HubPort( );
        PRINT( "Disable U2 host port because of disconnect\n" );
        return( ERR_USB_DISCON );
    }
    SetUsb2Speed( ThisUsb2Dev.DeviceSpeed );  // ���õ�ǰUSB�ٶ�
	
    PRINT( "GetU2DevDescr: " );
    s = CtrlGetU2DeviceDescr();  		// ��ȡ�豸������
    if ( s == ERR_SUCCESS )
    {
        for ( i = 0; i < ((PUSB_SETUP_REQ)SetupGetU2DevDescr)->wLength; i ++ )
       PRINT( "x%02X ", (UINT16)( U2Com_Buffer[i] ) );
       PRINT( "\n" ); 
		
       ThisUsb2Dev.DeviceVID = ((PUSB_DEV_DESCR)U2Com_Buffer)->idVendor; //����VID PID��Ϣ
       ThisUsb2Dev.DevicePID = ((PUSB_DEV_DESCR)U2Com_Buffer)->idProduct;
       dv_cls = ( (PUSB_DEV_DESCR)U2Com_Buffer ) -> bDeviceClass;
		
       s = CtrlSetUsb2Address( ((PUSB_SETUP_REQ)SetupSetUsb2Addr)->wValue );
       if ( s == ERR_SUCCESS )
       {
           ThisUsb2Dev.DeviceAddress = ( (PUSB_SETUP_REQ)SetupSetUsb2Addr )->wValue;  // ����USB��ַ
    
           PRINT( "GetU2CfgDescr: " );
           s = CtrlGetU2ConfigDescr( );
           if ( s == ERR_SUCCESS ) 
           {
               for ( i = 0; i < ( (PUSB_CFG_DESCR)U2Com_Buffer )->wTotalLength; i ++ )
               PRINT( "x%02X ", (UINT16)( U2Com_Buffer[i] ) );
               PRINT("\n");
/* ��������������,��ȡ�˵�����/���˵��ַ/���˵��С��,���±���endp_addr��endp_size�� */				
               cfg = ( (PUSB_CFG_DESCR)U2Com_Buffer )->bConfigurationValue;
               if_cls = ( (PUSB_CFG_DESCR_LONG)U2Com_Buffer )->itf_descr.bInterfaceClass;  // �ӿ������
                              
               if ( (dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_STORAGE)) {  // ��USB�洢���豸,������ȷ����U��
#ifdef	FOR_ROOT_UDISK_ONLY
                   CHRV3DiskStatus = DISK_USB_ADDR;
                   return( ERR_SUCCESS );
               }
               else 	return( ERR_USB_UNSUPPORT );
#else
                  s = CtrlSetUsb2Config( cfg );  // ����USB�豸����
                  if ( s == ERR_SUCCESS )
                  {
                      ThisUsb2Dev.DeviceStatus = ROOT_DEV_SUCCESS;
                      ThisUsb2Dev.DeviceType = USB_DEV_CLASS_STORAGE;
                      PRINT( "U2 USB-Disk Ready\n" );
                      SetUsb2Speed( 1 );  // Ĭ��Ϊȫ��
                      return( ERR_SUCCESS );
                  }
               }
               else if ( (dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_PRINTER) && ((PUSB_CFG_DESCR_LONG)U2Com_Buffer)->itf_descr.bInterfaceSubClass == 0x01 ) {  // �Ǵ�ӡ�����豸
                   s = CtrlSetUsb2Config( cfg );  // ����USB�豸����
                   if ( s == ERR_SUCCESS ) {
//	�豣��˵���Ϣ�Ա����������USB����
                       ThisUsb2Dev.DeviceStatus = ROOT_DEV_SUCCESS;
                       ThisUsb2Dev.DeviceType = USB_DEV_CLASS_PRINTER;
                       PRINT( "U2 USB-Print Ready\n" );
                       SetUsb2Speed( 1 );  // Ĭ��Ϊȫ��
                       return( ERR_SUCCESS );
                   }
               }
               else if ( (dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_HID) && ((PUSB_CFG_DESCR_LONG)U2Com_Buffer)->itf_descr.bInterfaceSubClass <= 0x01 ) {  // ��HID���豸,����/����
//  ���������з�����HID�ж϶˵�ĵ�ַ

//  �����ж϶˵�ĵ�ַ,λ7����ͬ����־λ,��0
                   if_cls = ( (PUSB_CFG_DESCR_LONG)U2Com_Buffer ) -> itf_descr.bInterfaceProtocol;
                   s = CtrlSetUsb2Config( cfg );  // ����USB�豸����
                   if ( s == ERR_SUCCESS ) {
//	    					Set_Idle( );
//	�豣��˵���Ϣ�Ա����������USB����
                       ThisUsb2Dev.DeviceStatus = ROOT_DEV_SUCCESS;
                       if ( if_cls == 1 ) {
                       ThisUsb2Dev.DeviceType = DEV_TYPE_KEYBOARD;
//	��һ����ʼ��,�����豸����ָʾ��LED��
                       PRINT( "U2 USB-Keyboard Ready\n" );
                       SetUsb2Speed( 1 );  // Ĭ��Ϊȫ��
                       return( ERR_SUCCESS );
                       }
                       else if ( if_cls == 2 ) {
                           ThisUsb2Dev.DeviceType = DEV_TYPE_MOUSE;
//	Ϊ���Ժ��ѯ���״̬,Ӧ�÷���������,ȡ���ж϶˿ڵĵ�ַ,���ȵ���Ϣ
                           PRINT( "U2 USB-Mouse Ready\n" );
                            SetUsb2Speed( 1 );  // Ĭ��Ϊȫ��
                           return( ERR_SUCCESS );
                       }
                       s = ERR_USB_UNSUPPORT;
                   }
                }
                else if(dv_cls == USB_DEV_CLASS_HUB){ // ��HUB���豸,��������
                    s = CtrlGetU2HubDescr( );
                    if(s==ERR_SUCCESS){
                        printf( "Max Port:%02X ",(((PXUSB_HUB_DESCR)U2Com_Buffer)->bNbrPorts) );
                        s = CtrlSetUsb2Config( cfg );               // ����USB�豸����
                        if(s == ERR_SUCCESS){
                            ThisUsb2Dev.DeviceStatus = ROOT_DEV_SUCCESS;
                            ThisUsb2Dev.DeviceType = USB_DEV_CLASS_HUB;
                            PRINT( "U2 USB-HUB Ready\n" );
                        }
                    }
                }
                else {   // ���Խ�һ������
                    s = CtrlSetUsb2Config( cfg );  // ����USB�豸����
                    if ( s == ERR_SUCCESS ) {
//	�豣��˵���Ϣ�Ա����������USB����
                        ThisUsb2Dev.DeviceStatus = ROOT_DEV_SUCCESS;
                        ThisUsb2Dev.DeviceType = DEV_TYPE_UNKNOW;
                        SetUsb2Speed( 1 );  // Ĭ��Ϊȫ��
                        return( ERR_SUCCESS );  /* δ֪�豸��ʼ���ɹ� */
                   }
               }
#endif			
            }
       }
    }
	
    PRINT( "InitRootU2Dev Err = %02X\n", (UINT16)s );
#ifdef	FOR_ROOT_UDISK_ONLY
    CHRV3DiskStatus = DISK_CONNECT;
#else
    ThisUsb2Dev.DeviceStatus = ROOT_DEV_FAILED;
#endif
    SetUsb2Speed( 1 );  // Ĭ��Ϊȫ��
    return( s );		
}


/*******************************************************************************
* Function Name  : CtrlGetU2HIDDeviceReport
* Description    : ��ȡHID�豸����������,������U2TxBuffer��
* Input          : None
* Output         : None
* Return         : ERR_SUCCESS �ɹ�
                   ����        ����
*******************************************************************************/
UINT8   CtrlGetU2HIDDeviceReport( UINT8 infc )
{
    UINT8   s;
    UINT8  len;    

	CopyU2SetupReqPkg( (PCHAR)SetupSetU2HIDIdle );
	pU2SetupReq -> wIndex = infc;
    s = U2HostCtrlTransfer( U2Com_Buffer, &len );                            // ִ�п��ƴ���
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }	
	
	CopyU2SetupReqPkg( (PCHAR)SetupGetU2HIDDevReport );
	pU2SetupReq -> wIndex = infc;
    s = U2HostCtrlTransfer( U2Com_Buffer, &len );                            // ִ�п��ƴ���
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
	
    return( ERR_SUCCESS );
}

/*******************************************************************************
* Function Name  : CtrlGetU2HubDescr
* Description    : ��ȡU2HUB������,������U2Com_Buffer��
* Input          : None
* Output         : None
* Return         : ERR_SUCCESS �ɹ�
                   ERR_USB_BUF_OVER ���ȴ���
*******************************************************************************/
UINT8   CtrlGetU2HubDescr( void )
{
    UINT8   s;
    UINT8  len;
    
    CopyU2SetupReqPkg( (PCHAR)SetupGetU2HubDescr );
    s = U2HostCtrlTransfer( U2Com_Buffer, &len );                          // ִ�п��ƴ���
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
    if ( len < ( (PUSB_SETUP_REQ)SetupGetU2HubDescr ) -> wLength )
    {
        return( ERR_USB_BUF_OVER );                                            // ���������ȴ���
    }
//  if ( len < 4 ) return( ERR_USB_BUF_OVER );                                 // ���������ȴ���
    return( ERR_SUCCESS );
}


/*******************************************************************************
* Function Name  : U2HubGetPortStatus
* Description    : ��ѯU2HUB�˿�״̬,������U2Com_Buffer��
* Input          : UINT8 HubPortIndex 
* Output         : None
* Return         : ERR_SUCCESS �ɹ�
                   ERR_USB_BUF_OVER ���ȴ���
*******************************************************************************/
UINT8   U2HubGetPortStatus( UINT8 HubPortIndex )
{
    UINT8   s;
    UINT8  len;
    
    pU2SetupReq -> bRequestType = HUB_GET_PORT_STATUS;
    pU2SetupReq -> bRequest = HUB_GET_STATUS;
    pU2SetupReq -> wValue = 0x0000;
    pU2SetupReq -> wIndex = 0x0000|HubPortIndex;
    pU2SetupReq -> wLength = 0x0004;
    s = U2HostCtrlTransfer( U2Com_Buffer, &len );                           // ִ�п��ƴ���
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
    if ( len < 4 )
    {
        return( ERR_USB_BUF_OVER );                                             // ���������ȴ���
    }
    return( ERR_SUCCESS );
}

/*******************************************************************************
* Function Name  : U2HubSetPortFeature
* Description    : ����U2HUB�˿�����
* Input          : UINT8 HubPortIndex    //HUB�˿�
                   UINT8 FeatureSelt     //HUB�˿�����
* Output         : None
* Return         : ERR_SUCCESS �ɹ�
                   ����        ����
*******************************************************************************/
UINT8   U2HubSetPortFeature( UINT8 HubPortIndex, UINT8 FeatureSelt )
{
    pU2SetupReq -> bRequestType = HUB_SET_PORT_FEATURE;
    pU2SetupReq -> bRequest = HUB_SET_FEATURE;
    pU2SetupReq -> wValue = 0x0000|FeatureSelt;
    pU2SetupReq -> wIndex = 0x0000|HubPortIndex;
    pU2SetupReq -> wLength = 0x0000;
    return( U2HostCtrlTransfer( NULL, NULL ) );                                 // ִ�п��ƴ���
}

/*******************************************************************************
* Function Name  : U2HubClearPortFeature
* Description    : ���U2HUB�˿�����
* Input          : UINT8 HubPortIndex                                         //HUB�˿�
                   UINT8 FeatureSelt                                          //HUB�˿�����
* Output         : None
* Return         : ERR_SUCCESS �ɹ�
                   ����        ����
*******************************************************************************/
UINT8   U2HubClearPortFeature( UINT8 HubPortIndex, UINT8 FeatureSelt )
{
    pU2SetupReq -> bRequestType = HUB_CLEAR_PORT_FEATURE;
    pU2SetupReq -> bRequest = HUB_CLEAR_FEATURE;
    pU2SetupReq -> wValue = 0x0000|FeatureSelt;
    pU2SetupReq -> wIndex = 0x0000|HubPortIndex;
    pU2SetupReq -> wLength = 0x0000;
    return( U2HostCtrlTransfer( NULL, NULL ) );                                // ִ�п��ƴ���
}












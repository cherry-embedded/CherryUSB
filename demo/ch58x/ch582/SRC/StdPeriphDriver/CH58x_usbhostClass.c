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
__attribute__((aligned(4))) const UINT8  SetupSetHIDIdle[] = { 0x21, HID_SET_IDLE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
/* ��ȡHID�豸���������� */
__attribute__((aligned(4))) const UINT8  SetupGetHIDDevReport[] = { 0x81, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_REPORT, 0x00, 0x00, 0x41, 0x00 };
/* ��ȡHUB������ */
__attribute__((aligned(4))) const UINT8  SetupGetHubDescr[] = { HUB_GET_HUB_DESCRIPTOR, HUB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_HUB, 0x00, 0x00, sizeof( USB_HUB_DESCR ), 0x00 };


__attribute__((aligned(4))) UINT8  Com_Buffer[ 128 ];            // �����û���ʱ������,ö��ʱ���ڴ���������,ö�ٽ���Ҳ����������ͨ��ʱ������
    
/*****************************************************************************
* Function Name  : InitRootDevice
* Description    : ��ʼ��ָ��ROOT-HUB�˿ڵ�USB�豸
* Input          : DataBuf: ö�ٹ����д�ŵ���������Ϣ������
* Return         :
*******************************************************************************/
UINT8 InitRootDevice( void ) 
{
    UINT8  i, s;
    UINT8  cfg, dv_cls, if_cls;
	
    PRINT( "Reset host port\n" );
    ResetRootHubPort( );  		// ��⵽�豸��,��λ��Ӧ�˿ڵ�USB����
    for ( i = 0, s = 0; i < 100; i ++ ) {  				// �ȴ�USB�豸��λ����������,100mS��ʱ
        mDelaymS( 1 );
        if ( EnableRootHubPort( ) == ERR_SUCCESS ) {  // ʹ�ܶ˿�
            i = 0;
            s ++;  					
            if ( s > 100 ) break;  	// �Ѿ��ȶ�����100mS
        }
    }
    if ( i ) {  										// ��λ���豸û������
        DisableRootHubPort( );
        PRINT( "Disable host port because of disconnect\n" );
        return( ERR_USB_DISCON );
    }
    SetUsbSpeed( ThisUsbDev.DeviceSpeed );  // ���õ�ǰUSB�ٶ�
	
    PRINT( "GetDevDescr: " );
    s = CtrlGetDeviceDescr();  		// ��ȡ�豸������
    if ( s == ERR_SUCCESS )
    {
        for ( i = 0; i < ((PUSB_SETUP_REQ)SetupGetDevDescr)->wLength; i ++ ) 		
       PRINT( "x%02X ", (UINT16)( Com_Buffer[i] ) );
       PRINT( "\n" ); 
		
       ThisUsbDev.DeviceVID = ((PUSB_DEV_DESCR)Com_Buffer)->idVendor; //����VID PID��Ϣ
       ThisUsbDev.DevicePID = ((PUSB_DEV_DESCR)Com_Buffer)->idProduct;
       dv_cls = ( (PUSB_DEV_DESCR)Com_Buffer ) -> bDeviceClass;
		
       s = CtrlSetUsbAddress( ((PUSB_SETUP_REQ)SetupSetUsbAddr)->wValue );  
       if ( s == ERR_SUCCESS )
       {
           ThisUsbDev.DeviceAddress = ( (PUSB_SETUP_REQ)SetupSetUsbAddr )->wValue;  // ����USB��ַ
    
           PRINT( "GetCfgDescr: " );
           s = CtrlGetConfigDescr( );
           if ( s == ERR_SUCCESS ) 
           {
               for ( i = 0; i < ( (PUSB_CFG_DESCR)Com_Buffer )->wTotalLength; i ++ ) 
               PRINT( "x%02X ", (UINT16)( Com_Buffer[i] ) );
               PRINT("\n");
/* ��������������,��ȡ�˵�����/���˵��ַ/���˵��С��,���±���endp_addr��endp_size�� */				
               cfg = ( (PUSB_CFG_DESCR)Com_Buffer )->bConfigurationValue;
               if_cls = ( (PUSB_CFG_DESCR_LONG)Com_Buffer )->itf_descr.bInterfaceClass;  // �ӿ������
                              
               if ( (dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_STORAGE)) {  // ��USB�洢���豸,������ȷ����U��
#ifdef	FOR_ROOT_UDISK_ONLY
                   CHRV3DiskStatus = DISK_USB_ADDR;
                   return( ERR_SUCCESS );
               }
               else 	return( ERR_USB_UNSUPPORT );
#else
                  s = CtrlSetUsbConfig( cfg );  // ����USB�豸����
                  if ( s == ERR_SUCCESS )
                  {
                      ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                      ThisUsbDev.DeviceType = USB_DEV_CLASS_STORAGE;
                      PRINT( "USB-Disk Ready\n" );
                      SetUsbSpeed( 1 );  // Ĭ��Ϊȫ��
                      return( ERR_SUCCESS );
                  }
               }
               else if ( (dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_PRINTER) && ((PUSB_CFG_DESCR_LONG)Com_Buffer)->itf_descr.bInterfaceSubClass == 0x01 ) {  // �Ǵ�ӡ�����豸
                   s = CtrlSetUsbConfig( cfg );  // ����USB�豸����
                   if ( s == ERR_SUCCESS ) {
//	�豣��˵���Ϣ�Ա����������USB����
                       ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                       ThisUsbDev.DeviceType = USB_DEV_CLASS_PRINTER;
                       PRINT( "USB-Print Ready\n" );
                       SetUsbSpeed( 1 );  // Ĭ��Ϊȫ��    
                       return( ERR_SUCCESS );
                   }
               }
               else if ( (dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_HID) && ((PUSB_CFG_DESCR_LONG)Com_Buffer)->itf_descr.bInterfaceSubClass <= 0x01 ) {  // ��HID���豸,����/����
//  ���������з�����HID�ж϶˵�ĵ�ַ
//  �����ж϶˵�ĵ�ַ,λ7����ͬ����־λ,��0
                   if_cls = ( (PUSB_CFG_DESCR_LONG)Com_Buffer ) -> itf_descr.bInterfaceProtocol;
                   s = CtrlSetUsbConfig( cfg );  // ����USB�豸����
                   if ( s == ERR_SUCCESS ) {
//	    					Set_Idle( );
//	�豣��˵���Ϣ�Ա����������USB����
                       ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                       if ( if_cls == 1 ) {
                       ThisUsbDev.DeviceType = DEV_TYPE_KEYBOARD;
//	��һ����ʼ��,�����豸����ָʾ��LED��
                       PRINT( "USB-Keyboard Ready\n" );
                       SetUsbSpeed( 1 );  // Ĭ��Ϊȫ��
                       return( ERR_SUCCESS );
                       }
                       else if ( if_cls == 2 ) {
                           ThisUsbDev.DeviceType = DEV_TYPE_MOUSE;
//	Ϊ���Ժ��ѯ���״̬,Ӧ�÷���������,ȡ���ж϶˿ڵĵ�ַ,���ȵ���Ϣ
                           PRINT( "USB-Mouse Ready\n" );
                            SetUsbSpeed( 1 );  // Ĭ��Ϊȫ��
                           return( ERR_SUCCESS );
                       }
                       s = ERR_USB_UNSUPPORT;
                   }
                }
                else if(dv_cls == USB_DEV_CLASS_HUB){ // ��HUB���豸,��������
                    s = CtrlGetHubDescr( );
                    if(s==ERR_SUCCESS){
                        printf( "Max Port:%02X ",(((PXUSB_HUB_DESCR)Com_Buffer)->bNbrPorts) );
                        s = CtrlSetUsbConfig( cfg );               // ����USB�豸����
                        if(s == ERR_SUCCESS){
                            ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                            ThisUsbDev.DeviceType = USB_DEV_CLASS_HUB;
                            PRINT( "USB-HUB Ready\n" );
                        }
                    }
                }
                else {   // ���Խ�һ������
                    s = CtrlSetUsbConfig( cfg );  // ����USB�豸����
                    if ( s == ERR_SUCCESS ) {
//	�豣��˵���Ϣ�Ա����������USB����
                        ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                        ThisUsbDev.DeviceType = DEV_TYPE_UNKNOW;
                        SetUsbSpeed( 1 );  // Ĭ��Ϊȫ��
                        return( ERR_SUCCESS );  /* δ֪�豸��ʼ���ɹ� */
                   }
               }
#endif			
            }
       }
    }
	
    PRINT( "InitRootDev Err = %02X\n", (UINT16)s );
#ifdef	FOR_ROOT_UDISK_ONLY
    CHRV3DiskStatus = DISK_CONNECT;
#else
    ThisUsbDev.DeviceStatus = ROOT_DEV_FAILED;
#endif
    SetUsbSpeed( 1 );  // Ĭ��Ϊȫ��	
    return( s );		
}


/*******************************************************************************
* Function Name  : CtrlGetHIDDeviceReport
* Description    : ��ȡHID�豸����������,������TxBuffer��
* Input          : None
* Output         : None
* Return         : ERR_SUCCESS �ɹ�
                   ����        ����
*******************************************************************************/
UINT8   CtrlGetHIDDeviceReport( UINT8 infc )  
{
    UINT8   s;
    UINT8  len;    

	CopySetupReqPkg( (PCHAR)SetupSetHIDIdle );		
	pSetupReq -> wIndex = infc;
    s = HostCtrlTransfer( Com_Buffer, &len );                            // ִ�п��ƴ���
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }	
	
	CopySetupReqPkg( (PCHAR)SetupGetHIDDevReport );	
	pSetupReq -> wIndex = infc;	
    s = HostCtrlTransfer( Com_Buffer, &len );                            // ִ�п��ƴ���
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
	
    return( ERR_SUCCESS );
}

/*******************************************************************************
* Function Name  : CtrlGetHubDescr
* Description    : ��ȡHUB������,������Com_Buffer��
* Input          : None
* Output         : None
* Return         : ERR_SUCCESS �ɹ�
                   ERR_USB_BUF_OVER ���ȴ���
*******************************************************************************/
UINT8   CtrlGetHubDescr( void )  
{
    UINT8   s;
    UINT8  len;
    
    CopySetupReqPkg( (PCHAR)SetupGetHubDescr );
    s = HostCtrlTransfer( Com_Buffer, &len );                          // ִ�п��ƴ���
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
    if ( len < ( (PUSB_SETUP_REQ)SetupGetHubDescr ) -> wLength )
    {
        return( ERR_USB_BUF_OVER );                                            // ���������ȴ���
    }
//  if ( len < 4 ) return( ERR_USB_BUF_OVER );                                 // ���������ȴ���
    return( ERR_SUCCESS );
}


/*******************************************************************************
* Function Name  : HubGetPortStatus
* Description    : ��ѯHUB�˿�״̬,������Com_Buffer��
* Input          : UINT8 HubPortIndex 
* Output         : None
* Return         : ERR_SUCCESS �ɹ�
                   ERR_USB_BUF_OVER ���ȴ���
*******************************************************************************/
UINT8   HubGetPortStatus( UINT8 HubPortIndex )   
{
    UINT8   s;
    UINT8  len;
    
    pSetupReq -> bRequestType = HUB_GET_PORT_STATUS;
    pSetupReq -> bRequest = HUB_GET_STATUS;
    pSetupReq -> wValue = 0x0000;
    pSetupReq -> wIndex = 0x0000|HubPortIndex;
    pSetupReq -> wLength = 0x0004;
    s = HostCtrlTransfer( Com_Buffer, &len );                           // ִ�п��ƴ���
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
* Function Name  : HubSetPortFeature
* Description    : ����HUB�˿�����
* Input          : UINT8 HubPortIndex    //HUB�˿�
                   UINT8 FeatureSelt     //HUB�˿�����
* Output         : None
* Return         : ERR_SUCCESS �ɹ�
                   ����        ����
*******************************************************************************/
UINT8   HubSetPortFeature( UINT8 HubPortIndex, UINT8 FeatureSelt ) 
{
    pSetupReq -> bRequestType = HUB_SET_PORT_FEATURE;
    pSetupReq -> bRequest = HUB_SET_FEATURE;
    pSetupReq -> wValue = 0x0000|FeatureSelt;
    pSetupReq -> wIndex = 0x0000|HubPortIndex;
    pSetupReq -> wLength = 0x0000;
    return( HostCtrlTransfer( NULL, NULL ) );                                 // ִ�п��ƴ���
}

/*******************************************************************************
* Function Name  : HubClearPortFeature
* Description    : ���HUB�˿�����
* Input          : UINT8 HubPortIndex                                         //HUB�˿�
                   UINT8 FeatureSelt                                          //HUB�˿�����
* Output         : None
* Return         : ERR_SUCCESS �ɹ�
                   ����        ����
*******************************************************************************/
UINT8   HubClearPortFeature( UINT8 HubPortIndex, UINT8 FeatureSelt ) 
{
    pSetupReq -> bRequestType = HUB_CLEAR_PORT_FEATURE;
    pSetupReq -> bRequest = HUB_CLEAR_FEATURE;
    pSetupReq -> wValue = 0x0000|FeatureSelt;
    pSetupReq -> wIndex = 0x0000|HubPortIndex;
    pSetupReq -> wLength = 0x0000;
    return( HostCtrlTransfer( NULL, NULL ) );                                // ִ�п��ƴ���
}












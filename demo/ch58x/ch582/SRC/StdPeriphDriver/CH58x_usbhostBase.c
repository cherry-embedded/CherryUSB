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

UINT8  UsbDevEndp0Size;			// USB�豸�Ķ˵�0�������ߴ� 
UINT8  FoundNewDev;
_RootHubDev   ThisUsbDev;                            //ROOT��

PUINT8  pHOST_RX_RAM_Addr;
PUINT8  pHOST_TX_RAM_Addr;

/*��ȡ�豸������*/
__attribute__((aligned(4))) const UINT8  SetupGetDevDescr[] = { USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_DEVICE, 0x00, 0x00, sizeof( USB_DEV_DESCR ), 0x00 };
/*��ȡ����������*/
__attribute__((aligned(4))) const UINT8  SetupGetCfgDescr[] = { USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_CONFIG, 0x00, 0x00, 0x04, 0x00 };
/*����USB��ַ*/
__attribute__((aligned(4))) const UINT8  SetupSetUsbAddr[] = { USB_REQ_TYP_OUT, USB_SET_ADDRESS, USB_DEVICE_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00 };
/*����USB����*/
__attribute__((aligned(4))) const UINT8  SetupSetUsbConfig[] = { USB_REQ_TYP_OUT, USB_SET_CONFIGURATION, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
/*����USB�ӿ�����*/
__attribute__((aligned(4))) const UINT8  SetupSetUsbInterface[] = { USB_REQ_RECIP_INTERF, USB_SET_INTERFACE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
/*����˵�STALL*/
__attribute__((aligned(4))) const UINT8  SetupClrEndpStall[] = { USB_REQ_TYP_OUT | USB_REQ_RECIP_ENDP, USB_CLEAR_FEATURE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };


/*******************************************************************************
* Function Name  : DisableRootHubPort( )
* Description    : �ر�HUB�˿�
* Input          : None
* Return         : None
*******************************************************************************/
void  DisableRootHubPort(void)          
{
#ifdef	FOR_ROOT_UDISK_ONLY
  CHRV3DiskStatus = DISK_DISCONNECT;
#endif
#ifndef	DISK_BASE_BUF_LEN
    ThisUsbDev.DeviceStatus = ROOT_DEV_DISCONNECT;
    ThisUsbDev.DeviceAddress = 0x00;
#endif
}

/*******************************************************************************
* Function Name  : AnalyzeRootHub(void)
* Description    : ����ROOT-HUB״̬,����ROOT-HUB�˿ڵ��豸����¼�
                   ����豸�γ�,�����е���DisableRootHubPort()����,���˿ڹر�,�����¼�,����Ӧ�˿ڵ�״̬λ
* Input          : None
* Return         : ����ERR_SUCCESSΪû�����,����ERR_USB_CONNECTΪ��⵽������,����ERR_USB_DISCONΪ��⵽�Ͽ�
*******************************************************************************/
UINT8   AnalyzeRootHub( void ) 
{ 
	UINT8	s;
	
    s = ERR_SUCCESS;
	
    if ( R8_USB_MIS_ST & RB_UMS_DEV_ATTACH ) {                                        // �豸����
#ifdef DISK_BASE_BUF_LEN
        if ( CHRV3DiskStatus == DISK_DISCONNECT
#else
        if ( ThisUsbDev.DeviceStatus == ROOT_DEV_DISCONNECT                          // ��⵽���豸����
#endif
            || ( R8_UHOST_CTRL & RB_UH_PORT_EN ) == 0x00 ) {                         // ��⵽���豸����,����δ����,˵���Ǹղ���
            DisableRootHubPort( );                                                   // �رն˿�
#ifdef DISK_BASE_BUF_LEN
            CHRV3DiskStatus = DISK_CONNECT;
#else
            ThisUsbDev.DeviceSpeed = R8_USB_MIS_ST & RB_UMS_DM_LEVEL ? 0 : 1;
            ThisUsbDev.DeviceStatus = ROOT_DEV_CONNECTED;                            //�����ӱ�־
#endif
            PRINT( "USB dev in\n" );
            s = ERR_USB_CONNECT;
        }
    }
	
#ifdef DISK_BASE_BUF_LEN
    else if ( CHRV3DiskStatus >= DISK_CONNECT ) {
#else
    else if ( ThisUsbDev.DeviceStatus >= ROOT_DEV_CONNECTED ) {                    //��⵽�豸�γ�
#endif
        DisableRootHubPort( );                                                     // �رն˿�
        PRINT( "USB dev out\n" );
        if ( s == ERR_SUCCESS ) s = ERR_USB_DISCON;
    }
//	R8_USB_INT_FG = RB_UIF_DETECT;                                                  // ���жϱ�־
    return( s );
}

/*******************************************************************************
* Function Name  : SetHostUsbAddr
* Description    : ����USB������ǰ������USB�豸��ַ
* Input          : UINT8 addr
* Return         : None
*******************************************************************************/
void    SetHostUsbAddr( UINT8 addr )
{
    R8_USB_DEV_AD = (R8_USB_DEV_AD&RB_UDA_GP_BIT) | (addr&MASK_USB_ADDR);
}

/*******************************************************************************
* Function Name  : SetUsbSpeed
* Description    : ���õ�ǰUSB�ٶ�
* Input          : UINT8 FullSpeed
* Return         : None
*******************************************************************************/
void    SetUsbSpeed( UINT8 FullSpeed )  
{
#ifndef	DISK_BASE_BUF_LEN    
    if ( FullSpeed )                                                           // ȫ��
    {
        R8_USB_CTRL &= ~ RB_UC_LOW_SPEED;                                           // ȫ��
        R8_UH_SETUP &= ~ RB_UH_PRE_PID_EN;                                          // ��ֹPRE PID
    }
    else
    {
        R8_USB_CTRL |= RB_UC_LOW_SPEED;                                             // ����		
    }
#endif   
    ( void ) FullSpeed;
}

/*******************************************************************************
* Function Name  : ResetRootHubPort( )
* Description    : ��⵽�豸��,��λ����,Ϊö���豸׼��,����ΪĬ��Ϊȫ��
* Input          : None  
* Return         : None
*******************************************************************************/
void  ResetRootHubPort( void )
{
    UsbDevEndp0Size = DEFAULT_ENDP0_SIZE;                                      //USB�豸�Ķ˵�0�������ߴ�	
    SetHostUsbAddr( 0x00 );
    R8_UHOST_CTRL &= ~RB_UH_PORT_EN;                                             // �ص��˿�
    SetUsbSpeed( 1 );                                                            // Ĭ��Ϊȫ��
    R8_UHOST_CTRL = (R8_UHOST_CTRL & ~RB_UH_LOW_SPEED) | RB_UH_BUS_RESET;        // Ĭ��Ϊȫ��,��ʼ��λ
    mDelaymS( 15 );                                                            // ��λʱ��10mS��20mS
    R8_UHOST_CTRL = R8_UHOST_CTRL & ~ RB_UH_BUS_RESET;                            // ������λ
    mDelayuS( 250 );
    R8_USB_INT_FG = RB_UIF_DETECT;                                               // ���жϱ�־
}

/*******************************************************************************
* Function Name  : EnableRootHubPort( )
* Description    : ʹ��ROOT-HUB�˿�,��Ӧ��bUH_PORT_EN��1�����˿�,�豸�Ͽ����ܵ��·���ʧ��
* Input          : None
* Return         : ����ERR_SUCCESSΪ��⵽������,����ERR_USB_DISCONΪ������
*******************************************************************************/
UINT8   EnableRootHubPort(void) 
{
#ifdef DISK_BASE_BUF_LEN
    if ( CHRV3DiskStatus < DISK_CONNECT ) CHRV3DiskStatus = DISK_CONNECT;
#else
    if ( ThisUsbDev.DeviceStatus < ROOT_DEV_CONNECTED ) ThisUsbDev.DeviceStatus = ROOT_DEV_CONNECTED;
#endif
    if ( R8_USB_MIS_ST & RB_UMS_DEV_ATTACH ) {                                        // ���豸
#ifndef DISK_BASE_BUF_LEN
        if ( ( R8_UHOST_CTRL & RB_UH_PORT_EN ) == 0x00 ) {                              // ��δʹ��
            ThisUsbDev.DeviceSpeed = (R8_USB_MIS_ST & RB_UMS_DM_LEVEL) ? 0 : 1;
            if ( ThisUsbDev.DeviceSpeed == 0 ) R8_UHOST_CTRL |= RB_UH_LOW_SPEED;          // ����
        }
#endif
        R8_UHOST_CTRL |= RB_UH_PORT_EN;                                                 //ʹ��HUB�˿�
        return( ERR_SUCCESS );
    }
    return( ERR_USB_DISCON );
}


/*******************************************************************************
* Function Name  : WaitUSB_Interrupt
* Description    : �ȴ�USB�ж�
* Input          : None
* Return         : ����ERR_SUCCESS ���ݽ��ջ��߷��ͳɹ�
                   ERR_USB_UNKNOWN ���ݽ��ջ��߷���ʧ��
*******************************************************************************/
UINT8 WaitUSB_Interrupt( void )
{
    UINT16  i;
    for ( i = WAIT_USB_TOUT_200US; i != 0 && (R8_USB_INT_FG&RB_UIF_TRANSFER) == 0; i -- ){;}
    return( (R8_USB_INT_FG&RB_UIF_TRANSFER)  ? ERR_SUCCESS : ERR_USB_UNKNOWN );
}


/*******************************************************************************
* Function Name  : USBHostTransact
* Description    : ��������,����Ŀ�Ķ˵��ַ/PID����,ͬ����־,��20uSΪ��λ��NAK������ʱ��(0������,0xFFFF��������),����0�ɹ�,��ʱ/��������
                   ���ӳ��������������,����ʵ��Ӧ����,Ϊ���ṩ�����ٶ�,Ӧ�öԱ��ӳ����������Ż�
* Input          : endp_pid:���ƺ͵�ַ  endp_pid: ��4λ��token_pid����, ��4λ�Ƕ˵��ַ
                   tog     : ͬ����־
                   timeout: ��ʱʱ��
* Return         : ERR_USB_UNKNOWN ��ʱ������Ӳ���쳣
                   ERR_USB_DISCON  �豸�Ͽ�
                   ERR_USB_CONNECT �豸����
                   ERR_SUCCESS     �������
*******************************************************************************/
UINT8   USBHostTransact( UINT8 endp_pid, UINT8 tog, UINT32 timeout )
{
    UINT8	TransRetry;

    UINT8	s, r;
    UINT16	i;

    R8_UH_RX_CTRL = R8_UH_TX_CTRL = tog;
    TransRetry = 0;

    do {
        R8_UH_EP_PID = endp_pid;                                                      // ָ������PID��Ŀ�Ķ˵��        
        R8_USB_INT_FG = RB_UIF_TRANSFER;                                                       
        for ( i = WAIT_USB_TOUT_200US; i != 0 && (R8_USB_INT_FG&RB_UIF_TRANSFER) == 0; i -- );
        R8_UH_EP_PID = 0x00;                                                          // ֹͣUSB����
        if ( (R8_USB_INT_FG&RB_UIF_TRANSFER) == 0 )     {return( ERR_USB_UNKNOWN );}

        if ( R8_USB_INT_FG & RB_UIF_DETECT ) {                                       // USB�豸����¼�
//			mDelayuS( 200 );                                                       // �ȴ��������
            R8_USB_INT_FG = RB_UIF_DETECT;                                                          
            s = AnalyzeRootHub( );                                                   // ����ROOT-HUB״̬

            if ( s == ERR_USB_CONNECT ) 			FoundNewDev = 1;
#ifdef DISK_BASE_BUF_LEN
            if ( CHRV3DiskStatus == DISK_DISCONNECT ) {return( ERR_USB_DISCON );}      // USB�豸�Ͽ��¼�
            if ( CHRV3DiskStatus == DISK_CONNECT ) {return( ERR_USB_CONNECT );}        // USB�豸�����¼�
#else
            if ( ThisUsbDev.DeviceStatus == ROOT_DEV_DISCONNECT ) {return( ERR_USB_DISCON );}// USB�豸�Ͽ��¼�
            if ( ThisUsbDev.DeviceStatus == ROOT_DEV_CONNECTED ) {return( ERR_USB_CONNECT );}// USB�豸�����¼�
#endif
            mDelayuS( 200 );  // �ȴ��������
        }
		
        if ( R8_USB_INT_FG & RB_UIF_TRANSFER ) 										// ��������¼�
        {  
            if ( R8_USB_INT_ST & RB_UIS_TOG_OK )        {return( ERR_SUCCESS );}
            r = R8_USB_INT_ST & MASK_UIS_H_RES;  // USB�豸Ӧ��״̬
            if ( r == USB_PID_STALL )                   {return( r | ERR_USB_TRANSFER );}
            if ( r == USB_PID_NAK ) 
            {
                if ( timeout == 0 )                     {return( r | ERR_USB_TRANSFER );}
                if ( timeout < 0xFFFFFFFF ) timeout --;
                -- TransRetry;
            }
            else switch ( endp_pid >> 4 ) {
                case USB_PID_SETUP:
                case USB_PID_OUT:
                    if ( r ) {return( r | ERR_USB_TRANSFER );}  // ���ǳ�ʱ/����,����Ӧ��
                    break;  // ��ʱ����
                case USB_PID_IN:
                    if ( r == USB_PID_DATA0 || r == USB_PID_DATA1 ) {  // ��ͬ�����趪��������
                    }  // ��ͬ������
                    else if ( r ) {return( r | ERR_USB_TRANSFER );}  // ���ǳ�ʱ/����,����Ӧ��
                    break;  // ��ʱ����
                default:
                    return( ERR_USB_UNKNOWN );  // �����ܵ����
                    break;
            }
        }
        else {  // �����ж�,��Ӧ�÷��������
            R8_USB_INT_FG = 0xFF;  /* ���жϱ�־ */
        }
        mDelayuS( 15 );	
    } while ( ++ TransRetry < 3 );
    return( ERR_USB_TRANSFER );  // Ӧ��ʱ
}

/*******************************************************************************
* Function Name  : HostCtrlTransfer
* Description    : ִ�п��ƴ���,8�ֽ���������pSetupReq��,DataBufΪ��ѡ���շ�������
* Input          : DataBuf :�����Ҫ���պͷ�������,��ôDataBuf��ָ����Ч���������ڴ�ź�������
                   RetLen  :ʵ�ʳɹ��շ����ܳ��ȱ�����RetLenָ����ֽڱ�����
* Return         : ERR_USB_BUF_OVER IN״̬�׶γ���
                   ERR_SUCCESS     ���ݽ����ɹ�
                   ��������״̬
*******************************************************************************/
UINT8 HostCtrlTransfer( PUINT8 DataBuf, PUINT8 RetLen )  
{
    UINT16  RemLen  = 0;
    UINT8   s, RxLen, RxCnt, TxCnt;
    PUINT8  pBuf;
    PUINT8   pLen;

    pBuf = DataBuf;
    pLen = RetLen;
    mDelayuS( 200 );
    if ( pLen )				*pLen = 0;  		// ʵ�ʳɹ��շ����ܳ���

    R8_UH_TX_LEN = sizeof( USB_SETUP_REQ );
    s = USBHostTransact( USB_PID_SETUP << 4 | 0x00, 0x00, 200000/20 );          // SETUP�׶�,200mS��ʱ
    if ( s != ERR_SUCCESS )             return( s );
    R8_UH_RX_CTRL = R8_UH_TX_CTRL = RB_UH_R_TOG | RB_UH_R_AUTO_TOG | RB_UH_T_TOG | RB_UH_T_AUTO_TOG;// Ĭ��DATA1
    R8_UH_TX_LEN = 0x01;                                                        // Ĭ�������ݹ�״̬�׶�ΪIN
    RemLen = pSetupReq -> wLength;
    if ( RemLen && pBuf )                                                       // ��Ҫ�շ�����
    {
        if ( pSetupReq -> bRequestType & USB_REQ_TYP_IN )                       // ��
        {
            while ( RemLen )
            {
                mDelayuS( 200 );
                s = USBHostTransact( USB_PID_IN << 4 | 0x00, R8_UH_RX_CTRL, 200000/20 );// IN����
                if ( s != ERR_SUCCESS )         return( s );
                RxLen = R8_USB_RX_LEN < RemLen ? R8_USB_RX_LEN : RemLen;
                RemLen -= RxLen;
                if ( pLen )				*pLen += RxLen;                       // ʵ�ʳɹ��շ����ܳ���
                for ( RxCnt = 0; RxCnt != RxLen; RxCnt ++ )
                {
                    *pBuf = pHOST_RX_RAM_Addr[ RxCnt ];
                    pBuf ++;
                }
                if ( R8_USB_RX_LEN == 0 || ( R8_USB_RX_LEN & ( UsbDevEndp0Size - 1 ) ) ) break;    // �̰�
            }
            R8_UH_TX_LEN = 0x00;                                                    // ״̬�׶�ΪOUT
        }
        else                                                                     // ��
        {
            while ( RemLen )
            {
                mDelayuS( 200 );
                R8_UH_TX_LEN = RemLen >= UsbDevEndp0Size ? UsbDevEndp0Size : RemLen;
                for ( TxCnt = 0; TxCnt != R8_UH_TX_LEN; TxCnt ++ )
                {
                    pHOST_TX_RAM_Addr[ TxCnt ] = *pBuf;
                    pBuf ++;
                }
                s = USBHostTransact( USB_PID_OUT << 4 | 0x00, R8_UH_TX_CTRL, 200000/20 );// OUT����
                if ( s != ERR_SUCCESS )         return( s );
                RemLen -= R8_UH_TX_LEN;
                if ( pLen )					   *pLen += R8_UH_TX_LEN;   		// ʵ�ʳɹ��շ����ܳ���
            }
//          R8_UH_TX_LEN = 0x01;                                                     // ״̬�׶�ΪIN
        }
    }
    mDelayuS( 200 );
    s = USBHostTransact( ( R8_UH_TX_LEN ? USB_PID_IN << 4 | 0x00: USB_PID_OUT << 4 | 0x00 ), RB_UH_R_TOG | RB_UH_T_TOG, 200000/20 );  // STATUS�׶�
    if ( s != ERR_SUCCESS )         return( s );    
    if ( R8_UH_TX_LEN == 0 )        return( ERR_SUCCESS );      // ״̬OUT
    if ( R8_USB_RX_LEN == 0 )       return( ERR_SUCCESS );     // ״̬IN,���IN״̬�������ݳ���
    return( ERR_USB_BUF_OVER );                                                   // IN״̬�׶δ���
}

/*******************************************************************************
* Function Name  : CopySetupReqPkg
* Description    : ���ƿ��ƴ���������
* Input          : pReqPkt: �����������ַ
* Return         : None
*******************************************************************************/
void CopySetupReqPkg( PCCHAR pReqPkt )                                        // ���ƿ��ƴ���������
{
    UINT8   i;
    for ( i = 0; i != sizeof( USB_SETUP_REQ ); i ++ )
    {
        ((PUINT8)pSetupReq)[ i ] = *pReqPkt;
        pReqPkt++;
    }			
}

/*******************************************************************************
* Function Name  : CtrlGetDeviceDescr
* Description    : ��ȡ�豸������,������ pHOST_TX_RAM_Addr ��
* Input          : None
* Return         : ERR_USB_BUF_OVER ���������ȴ���
                   ERR_SUCCESS      �ɹ�
                   ����
*******************************************************************************/
UINT8 CtrlGetDeviceDescr( void )  
{
    UINT8   s;
    UINT8  len;

    UsbDevEndp0Size = DEFAULT_ENDP0_SIZE;
    CopySetupReqPkg( (PCHAR)SetupGetDevDescr );
    s = HostCtrlTransfer( Com_Buffer, &len );                            // ִ�п��ƴ���
    if ( s != ERR_SUCCESS ) 		return( s );
    UsbDevEndp0Size = ( (PUSB_DEV_DESCR)Com_Buffer ) -> bMaxPacketSize0;        // �˵�0��������,���Ǽ򻯴���,����Ӧ���Ȼ�ȡǰ8�ֽں���������UsbDevEndp0Size�ټ���
    if ( len < ((PUSB_SETUP_REQ)SetupGetDevDescr)->wLength )        return( ERR_USB_BUF_OVER );   // ���������ȴ���
    return( ERR_SUCCESS );
}

/*******************************************************************************
* Function Name  : CtrlGetConfigDescr
* Description    : ��ȡ����������,������ pHOST_TX_RAM_Addr ��
* Input          : None
* Return         : ERR_USB_BUF_OVER ���������ȴ���
                   ERR_SUCCESS      �ɹ�
                   ����
*******************************************************************************/
UINT8 CtrlGetConfigDescr( void )
{
    UINT8   s;
    UINT8  len;
      
    CopySetupReqPkg( (PCHAR)SetupGetCfgDescr );
    s = HostCtrlTransfer( Com_Buffer, &len );                            // ִ�п��ƴ���
    if ( s != ERR_SUCCESS )	        return( s );
    if ( len < ( (PUSB_SETUP_REQ)SetupGetCfgDescr ) -> wLength ) return( ERR_USB_BUF_OVER );  // ���س��ȴ���

    len = ( (PUSB_CFG_DESCR)Com_Buffer ) -> wTotalLength;	
    CopySetupReqPkg( (PCHAR)SetupGetCfgDescr );
    pSetupReq ->wLength = len;                                                 // �����������������ܳ���		
    s = HostCtrlTransfer( Com_Buffer, &len );                            // ִ�п��ƴ���
    if ( s != ERR_SUCCESS )         return( s );
 
    
#ifdef DISK_BASE_BUF_LEN
	if(len>64) len = 64;
	memcpy( TxBuffer, Com_Buffer, len);                                             //U�̲���ʱ����Ҫ������TxBuffer
#endif
     
    return( ERR_SUCCESS );
}

/*******************************************************************************
* Function Name  : CtrlSetUsbAddress
* Description    : ����USB�豸��ַ
* Input          : addr: �豸��ַ
* Return         : ERR_SUCCESS      �ɹ�
                   ����
*******************************************************************************/
UINT8 CtrlSetUsbAddress( UINT8 addr ) 
{
    UINT8   s;

    CopySetupReqPkg( (PCHAR)SetupSetUsbAddr );	
    pSetupReq -> wValue = addr;                                       // USB�豸��ַ	
    s = HostCtrlTransfer( NULL, NULL );                               // ִ�п��ƴ���
    if ( s != ERR_SUCCESS )         return( s );
    SetHostUsbAddr( addr );                                                     // ����USB������ǰ������USB�豸��ַ
    mDelaymS( 10 );                                                             // �ȴ�USB�豸��ɲ���
    return( ERR_SUCCESS );
    }

/*******************************************************************************
* Function Name  : CtrlSetUsbConfig
* Description    : ����USB�豸����
* Input          : cfg: ����ֵ
* Return         : ERR_SUCCESS      �ɹ�
                   ����
*******************************************************************************/
UINT8 CtrlSetUsbConfig( UINT8 cfg )                   
{    
    CopySetupReqPkg( (PCHAR)SetupSetUsbConfig );	
    pSetupReq -> wValue = cfg;                                        // USB�豸����		
    return( HostCtrlTransfer( NULL, NULL ) );                         // ִ�п��ƴ���
}

/*******************************************************************************
* Function Name  : CtrlClearEndpStall
* Description    : ����˵�STALL
* Input          : endp: �˵��ַ
* Return         : ERR_SUCCESS      �ɹ�
                   ����
*******************************************************************************/
UINT8 CtrlClearEndpStall( UINT8 endp )  
{
    CopySetupReqPkg( (PCHAR)SetupClrEndpStall );                              // ����˵�Ĵ���
    pSetupReq -> wIndex = endp;                                        // �˵��ַ	
    return( HostCtrlTransfer( NULL, NULL ) );                          // ִ�п��ƴ���
}

/*******************************************************************************
* Function Name  : CtrlSetUsbIntercace
* Description    : ����USB�豸�ӿ�
* Input          : cfg: ����ֵ
* Return         : ERR_SUCCESS      �ɹ�
                   ����
*******************************************************************************/
UINT8 CtrlSetUsbIntercace( UINT8 cfg )                   
{
    CopySetupReqPkg( (PCHAR)SetupSetUsbInterface );		
    pSetupReq -> wValue = cfg;                                          // USB�豸����	
    return( HostCtrlTransfer( NULL, NULL ) );                           // ִ�п��ƴ���
}

/*******************************************************************************
* Function Name  : USB_HostInit
* Description    : USB�������ܳ�ʼ��
* Input          : None			   				
* Return         : None
*******************************************************************************/
void  USB_HostInit( void )
{	
    R8_USB_CTRL = RB_UC_HOST_MODE; 
    R8_UHOST_CTRL = 0;                           
    R8_USB_DEV_AD = 0x00;

    R8_UH_EP_MOD = RB_UH_EP_TX_EN | RB_UH_EP_RX_EN;
    R16_UH_RX_DMA = (UINT16)(UINT32)pHOST_RX_RAM_Addr;
    R16_UH_TX_DMA = (UINT16)(UINT32)pHOST_TX_RAM_Addr;

    R8_UH_RX_CTRL = 0x00;
    R8_UH_TX_CTRL = 0x00;
    R8_USB_CTRL =  RB_UC_HOST_MODE | RB_UC_INT_BUSY | RB_UC_DMA_EN;
    R8_UH_SETUP = RB_UH_SOF_EN;	
    R8_USB_INT_FG = 0xFF;
    DisableRootHubPort( );
    R8_USB_INT_EN = RB_UIE_TRANSFER|RB_UIE_DETECT;	

    FoundNewDev = 0;
}














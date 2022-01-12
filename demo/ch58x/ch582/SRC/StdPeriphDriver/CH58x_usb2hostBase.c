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

UINT8  Usb2DevEndp0Size;     // USB2�豸�Ķ˵�0�������ߴ�
UINT8  FoundNewU2Dev;
_RootHubDev   ThisUsb2Dev;                            //ROOT��

PUINT8  pU2HOST_RX_RAM_Addr;
PUINT8  pU2HOST_TX_RAM_Addr;

/*��ȡ�豸������*/
__attribute__((aligned(4))) const UINT8  SetupGetU2DevDescr[] = { USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_DEVICE, 0x00, 0x00, sizeof( USB_DEV_DESCR ), 0x00 };
/*��ȡ����������*/
__attribute__((aligned(4))) const UINT8  SetupGetU2CfgDescr[] = { USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_CONFIG, 0x00, 0x00, 0x04, 0x00 };
/*����USB��ַ*/
__attribute__((aligned(4))) const UINT8  SetupSetUsb2Addr[] = { USB_REQ_TYP_OUT, USB_SET_ADDRESS, USB_DEVICE_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00 };
/*����USB����*/
__attribute__((aligned(4))) const UINT8  SetupSetUsb2Config[] = { USB_REQ_TYP_OUT, USB_SET_CONFIGURATION, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
/*����USB�ӿ�����*/
__attribute__((aligned(4))) const UINT8  SetupSetUsb2Interface[] = { USB_REQ_RECIP_INTERF, USB_SET_INTERFACE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
/*����˵�STALL*/
__attribute__((aligned(4))) const UINT8  SetupClrU2EndpStall[] = { USB_REQ_TYP_OUT | USB_REQ_RECIP_ENDP, USB_CLEAR_FEATURE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/*******************************************************************************
* Function Name  : DisableRootU2HubPort( )
* Description    : �ر�U2HUB�˿�
* Input          : None
* Return         : None
*******************************************************************************/
void  DisableRootU2HubPort(void)
{
#ifdef  FOR_ROOT_UDISK_ONLY
  CHRV3DiskStatus = DISK_DISCONNECT;
#endif
#ifndef DISK_BASE_BUF_LEN
    ThisUsb2Dev.DeviceStatus = ROOT_DEV_DISCONNECT;
    ThisUsb2Dev.DeviceAddress = 0x00;
#endif
}


/*******************************************************************************
* Function Name  : AnalyzeRootU2Hub(void)
* Description    : ����ROOT-U2HUB״̬,����ROOT-U2HUB�˿ڵ��豸����¼�
                   ����豸�γ�,�����е���DisableRootU2HubPort()����,���˿ڹر�,�����¼�,����Ӧ�˿ڵ�״̬λ
* Input          : None
* Return         : ����ERR_SUCCESSΪû�����,����ERR_USB_CONNECTΪ��⵽������,����ERR_USB_DISCONΪ��⵽�Ͽ�
*******************************************************************************/
UINT8   AnalyzeRootU2Hub( void )
{
  UINT8 s;

    s = ERR_SUCCESS;

    if ( R8_USB2_MIS_ST & RB_UMS_DEV_ATTACH ) {                                        // �豸����
#ifdef DISK_BASE_BUF_LEN
        if ( CHRV3DiskStatus == DISK_DISCONNECT
#else
        if ( ThisUsb2Dev.DeviceStatus == ROOT_DEV_DISCONNECT                          // ��⵽���豸����
#endif
            || ( R8_U2HOST_CTRL & RB_UH_PORT_EN ) == 0x00 ) {                         // ��⵽���豸����,����δ����,˵���Ǹղ���
            DisableRootU2HubPort( );                                                   // �رն˿�
#ifdef DISK_BASE_BUF_LEN
            CHRV3DiskStatus = DISK_CONNECT;
#else
            ThisUsb2Dev.DeviceSpeed = R8_USB2_MIS_ST & RB_UMS_DM_LEVEL ? 0 : 1;
            ThisUsb2Dev.DeviceStatus = ROOT_DEV_CONNECTED;                            //�����ӱ�־
#endif
            PRINT( "USB2 dev in\n" );
            s = ERR_USB_CONNECT;

        }
    }

#ifdef DISK_BASE_BUF_LEN
    else if ( CHRV3DiskStatus >= DISK_CONNECT ) {
#else
    else if ( ThisUsb2Dev.DeviceStatus >= ROOT_DEV_CONNECTED ) {                    //��⵽�豸�γ�
#endif
        DisableRootU2HubPort( );                                                     // �رն˿�
        PRINT( "USB2 dev out\n" );
        if ( s == ERR_SUCCESS ) s = ERR_USB_DISCON;
    }
//  R8_USB_INT_FG = RB_UIF_DETECT;                                                  // ���жϱ�־
    return( s );
}

/*******************************************************************************
* Function Name  : SetHostUsb2Addr
* Description    : ����USB2������ǰ������USB�豸��ַ
* Input          : UINT8 addr
* Return         : None
*******************************************************************************/
void    SetHostUsb2Addr( UINT8 addr )
{
    R8_USB2_DEV_AD = (R8_USB2_DEV_AD&RB_UDA_GP_BIT) | (addr&MASK_USB_ADDR);
}

/*******************************************************************************
* Function Name  : SetUsb2Speed
* Description    : ���õ�ǰUSB2�ٶ�
* Input          : UINT8 FullSpeed
* Return         : None
*******************************************************************************/
void    SetUsb2Speed( UINT8 FullSpeed )
{
#ifndef DISK_BASE_BUF_LEN
    if ( FullSpeed )                                                           // ȫ��
    {
        R8_USB2_CTRL &= ~ RB_UC_LOW_SPEED;                                           // ȫ��
        R8_U2H_SETUP &= ~ RB_UH_PRE_PID_EN;                                          // ��ֹPRE PID
    }
    else
    {
        R8_USB2_CTRL |= RB_UC_LOW_SPEED;                                             // ����
    }
#endif
    ( void ) FullSpeed;
}

/*******************************************************************************
* Function Name  : ResetRootU2HubPort( )
* Description    : ��⵽�豸��,��λ����,Ϊö���豸׼��,����ΪĬ��Ϊȫ��
* Input          : None
* Return         : None
*******************************************************************************/
void  ResetRootU2HubPort( void )
{
    Usb2DevEndp0Size = DEFAULT_ENDP0_SIZE;                                      //USB2�豸�Ķ˵�0�������ߴ�
    SetHostUsb2Addr( 0x00 );
    R8_U2HOST_CTRL &= ~RB_UH_PORT_EN;                                             // �ص��˿�
    SetUsb2Speed( 1 );                                                            // Ĭ��Ϊȫ��
    R8_U2HOST_CTRL = (R8_U2HOST_CTRL & ~RB_UH_LOW_SPEED) | RB_UH_BUS_RESET;        // Ĭ��Ϊȫ��,��ʼ��λ
    mDelaymS( 15 );                                                            // ��λʱ��10mS��20mS
    R8_U2HOST_CTRL = R8_U2HOST_CTRL & ~ RB_UH_BUS_RESET;                            // ������λ
    mDelayuS( 250 );
    R8_USB2_INT_FG = RB_UIF_DETECT;                                               // ���жϱ�־
}

/*******************************************************************************
* Function Name  : EnableRootU2HubPort( )
* Description    : ʹ��ROOT-U2HUB�˿�,��Ӧ��bU2H_PORT_EN��1�����˿�,�豸�Ͽ����ܵ��·���ʧ��
* Input          : None
* Return         : ����ERR_SUCCESSΪ��⵽������,����ERR_USB_DISCONΪ������
*******************************************************************************/
UINT8   EnableRootU2HubPort(void)
{
#ifdef DISK_BASE_BUF_LEN
    if ( CHRV3DiskStatus < DISK_CONNECT ) CHRV3DiskStatus = DISK_CONNECT;
#else
    if ( ThisUsb2Dev.DeviceStatus < ROOT_DEV_CONNECTED ) ThisUsb2Dev.DeviceStatus = ROOT_DEV_CONNECTED;
#endif
    if ( R8_USB2_MIS_ST & RB_UMS_DEV_ATTACH ) {                                        // ���豸
#ifndef DISK_BASE_BUF_LEN
        if ( ( R8_U2HOST_CTRL & RB_UH_PORT_EN ) == 0x00 ) {                              // ��δʹ��
            ThisUsb2Dev.DeviceSpeed = (R8_USB2_MIS_ST & RB_UMS_DM_LEVEL) ? 0 : 1;
            if ( ThisUsb2Dev.DeviceSpeed == 0 ) R8_U2HOST_CTRL |= RB_UH_LOW_SPEED;          // ����
        }
#endif
        R8_U2HOST_CTRL |= RB_UH_PORT_EN;                                                 //ʹ��HUB�˿�
        return( ERR_SUCCESS );
    }
    return( ERR_USB_DISCON );
}


/*******************************************************************************
* Function Name  : WaitUSB2_Interrupt
* Description    : �ȴ�USB2�ж�
* Input          : None
* Return         : ����ERR_SUCCESS ���ݽ��ջ��߷��ͳɹ�
                   ERR_USB_UNKNOWN ���ݽ��ջ��߷���ʧ��
*******************************************************************************/
UINT8 WaitUSB2_Interrupt( void )
{
    UINT16  i;
    for ( i = WAIT_USB_TOUT_200US; i != 0 && (R8_USB2_INT_FG&RB_UIF_TRANSFER) == 0; i -- ){;}
    return( (R8_USB2_INT_FG&RB_UIF_TRANSFER)  ? ERR_SUCCESS : ERR_USB_UNKNOWN );
}

/*******************************************************************************
* Function Name  : USB2HostTransact
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
UINT8   USB2HostTransact( UINT8 endp_pid, UINT8 tog, UINT32 timeout )
{
    UINT8 TransRetry;

    UINT8 s, r;
    UINT16  i;

    R8_U2H_RX_CTRL = R8_U2H_TX_CTRL = tog;
    TransRetry = 0;

    do {
        R8_U2H_EP_PID = endp_pid;                                                      // ָ������PID��Ŀ�Ķ˵��
        R8_USB2_INT_FG = RB_UIF_TRANSFER;
        for ( i = WAIT_USB_TOUT_200US; i != 0 && (R8_USB2_INT_FG&RB_UIF_TRANSFER) == 0; i -- );
        R8_U2H_EP_PID = 0x00;                                                          // ֹͣUSB����
        if ( (R8_USB2_INT_FG&RB_UIF_TRANSFER) == 0 )     {printf("!!!!!!!!\n");return( ERR_USB_UNKNOWN );}

        if ( R8_USB2_INT_FG & RB_UIF_DETECT ) {                                       // USB�豸����¼�
//      mDelayuS( 200 );                                                       // �ȴ��������
            R8_USB2_INT_FG = RB_UIF_DETECT;
            s = AnalyzeRootU2Hub( );                                                   // ����ROOT-U2HUB״̬

            if ( s == ERR_USB_CONNECT )       FoundNewU2Dev = 1;
#ifdef DISK_BASE_BUF_LEN
            if ( CHRV3DiskStatus == DISK_DISCONNECT ) {return( ERR_USB_DISCON );}      // USB�豸�Ͽ��¼�
            if ( CHRV3DiskStatus == DISK_CONNECT ) {return( ERR_USB_CONNECT );}        // USB�豸�����¼�
#else
            if ( ThisUsb2Dev.DeviceStatus == ROOT_DEV_DISCONNECT ) {return( ERR_USB_DISCON );}// USB�豸�Ͽ��¼�
            if ( ThisUsb2Dev.DeviceStatus == ROOT_DEV_CONNECTED ) {return( ERR_USB_CONNECT );}// USB�豸�����¼�
#endif
            mDelayuS( 200 );  // �ȴ��������
        }

        if ( R8_USB2_INT_FG & RB_UIF_TRANSFER )                    // ��������¼�
        {
            if ( R8_USB2_INT_ST & RB_UIS_TOG_OK )        {return( ERR_SUCCESS );}
            r = R8_USB2_INT_ST & MASK_UIS_H_RES;  // USB�豸Ӧ��״̬
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
            R8_USB2_INT_FG = 0xFF;  /* ���жϱ�־ */
        }
        mDelayuS( 15 );
    } while ( ++ TransRetry < 3 );
    return( ERR_USB_TRANSFER );  // Ӧ��ʱ
}

/*******************************************************************************
* Function Name  : U2HostCtrlTransfer
* Description    : ִ�п��ƴ���,8�ֽ���������pSetupReq��,DataBufΪ��ѡ���շ�������
* Input          : DataBuf :�����Ҫ���պͷ�������,��ôDataBuf��ָ����Ч���������ڴ�ź�������
                   RetLen  :ʵ�ʳɹ��շ����ܳ��ȱ�����RetLenָ����ֽڱ�����
* Return         : ERR_USB_BUF_OVER IN״̬�׶γ���
                   ERR_SUCCESS     ���ݽ����ɹ�
                   ��������״̬
*******************************************************************************/
UINT8 U2HostCtrlTransfer( PUINT8 DataBuf, PUINT8 RetLen )
{
    UINT16  RemLen  = 0;
    UINT8   s, RxLen, RxCnt, TxCnt;
    PUINT8  pBuf;
    PUINT8   pLen;

    pBuf = DataBuf;
    pLen = RetLen;
    mDelayuS( 200 );
    if ( pLen )       *pLen = 0;      // ʵ�ʳɹ��շ����ܳ���

    R8_U2H_TX_LEN = sizeof( USB_SETUP_REQ );
    s = USB2HostTransact( USB_PID_SETUP << 4 | 0x00, 0x00, 200000/20 );          // SETUP�׶�,200mS��ʱ
    if ( s != ERR_SUCCESS )             return( s );
    R8_U2H_RX_CTRL = R8_U2H_TX_CTRL = RB_UH_R_TOG | RB_UH_R_AUTO_TOG | RB_UH_T_TOG | RB_UH_T_AUTO_TOG;// Ĭ��DATA1
    R8_U2H_TX_LEN = 0x01;                                                        // Ĭ�������ݹ�״̬�׶�ΪIN
    RemLen = pU2SetupReq -> wLength;
    if ( RemLen && pBuf )                                                       // ��Ҫ�շ�����
    {
        if ( pU2SetupReq -> bRequestType & USB_REQ_TYP_IN )                       // ��
        {
            while ( RemLen )
            {
                mDelayuS( 200 );
                s = USB2HostTransact( USB_PID_IN << 4 | 0x00, R8_U2H_RX_CTRL, 200000/20 );// IN����
                if ( s != ERR_SUCCESS )         return( s );
                RxLen = R8_USB2_RX_LEN < RemLen ? R8_USB2_RX_LEN : RemLen;
                RemLen -= RxLen;
                if ( pLen )       *pLen += RxLen;                       // ʵ�ʳɹ��շ����ܳ���
                for ( RxCnt = 0; RxCnt != RxLen; RxCnt ++ )
                {
                    *pBuf = pU2HOST_RX_RAM_Addr[ RxCnt ];
                    pBuf ++;
                }
                if ( R8_USB2_RX_LEN == 0 || ( R8_USB2_RX_LEN & ( Usb2DevEndp0Size - 1 ) ) ) break;    // �̰�
            }
            R8_U2H_TX_LEN = 0x00;                                                    // ״̬�׶�ΪOUT
        }
        else                                                                     // ��
        {
            while ( RemLen )
            {
                mDelayuS( 200 );
                R8_U2H_TX_LEN = RemLen >= Usb2DevEndp0Size ? Usb2DevEndp0Size : RemLen;
                for ( TxCnt = 0; TxCnt != R8_U2H_TX_LEN; TxCnt ++ )
                {
                    pU2HOST_TX_RAM_Addr[ TxCnt ] = *pBuf;
                    pBuf ++;
                }
                s = USB2HostTransact( USB_PID_OUT << 4 | 0x00, R8_U2H_TX_CTRL, 200000/20 );// OUT����
                if ( s != ERR_SUCCESS )         return( s );
                RemLen -= R8_U2H_TX_LEN;
                if ( pLen )            *pLen += R8_U2H_TX_LEN;       // ʵ�ʳɹ��շ����ܳ���
            }
//          R8_U2H_TX_LEN = 0x01;                                                     // ״̬�׶�ΪIN
        }
    }
    mDelayuS( 200 );
    s = USB2HostTransact( ( R8_U2H_TX_LEN ? USB_PID_IN << 4 | 0x00: USB_PID_OUT << 4 | 0x00 ), RB_UH_R_TOG | RB_UH_T_TOG, 200000/20 );  // STATUS�׶�
    if ( s != ERR_SUCCESS )         return( s );
    if ( R8_U2H_TX_LEN == 0 )        return( ERR_SUCCESS );      // ״̬OUT
    if ( R8_USB2_RX_LEN == 0 )       return( ERR_SUCCESS );     // ״̬IN,���IN״̬�������ݳ���
    return( ERR_USB_BUF_OVER );                                                   // IN״̬�׶δ���
}

/*******************************************************************************
* Function Name  : CopyU2SetupReqPkg
* Description    : ����U2���ƴ���������
* Input          : pU2ReqPkt: �����������ַ
* Return         : None
*******************************************************************************/
void CopyU2SetupReqPkg( PCCHAR pReqPkt )                                        // ���ƿ��ƴ���������
{
    UINT8   i;
    for ( i = 0; i != sizeof( USB_SETUP_REQ ); i ++ )
    {
        ((PUINT8)pU2SetupReq)[ i ] = *pReqPkt;
        pReqPkt++;
    }
}

/*******************************************************************************
* Function Name  : CtrlGeU2tDeviceDescr
* Description    : ��ȡU2�豸������,������ pU2HOST_TX_RAM_Addr ��
* Input          : None
* Return         : ERR_USB_BUF_OVER ���������ȴ���
                   ERR_SUCCESS      �ɹ�
                   ����
*******************************************************************************/
UINT8 CtrlGetU2DeviceDescr( void )
{
    UINT8   s;
    UINT8  len;

    Usb2DevEndp0Size = DEFAULT_ENDP0_SIZE;
    CopyU2SetupReqPkg( (PCHAR)SetupGetU2DevDescr );
    s = U2HostCtrlTransfer( U2Com_Buffer, &len );                            // ִ�п��ƴ���
    if ( s != ERR_SUCCESS ) 		return( s );
    Usb2DevEndp0Size = ( (PUSB_DEV_DESCR)U2Com_Buffer ) -> bMaxPacketSize0;        // �˵�0��������,���Ǽ򻯴���,����Ӧ���Ȼ�ȡǰ8�ֽں���������UsbDevEndp0Size�ټ���
    if ( len < ((PUSB_SETUP_REQ)SetupGetU2DevDescr)->wLength )        return( ERR_USB_BUF_OVER );   // ���������ȴ���
    return( ERR_SUCCESS );
}

/*******************************************************************************
* Function Name  : CtrlGetU2ConfigDescr
* Description    : ��ȡU2����������,������ pU2HOST_TX_RAM_Addr ��
* Input          : None
* Return         : ERR_USB_BUF_OVER ���������ȴ���
                   ERR_SUCCESS      �ɹ�
                   ����
*******************************************************************************/
UINT8 CtrlGetU2ConfigDescr( void )
{
    UINT8   s;
    UINT8  len;
      
    CopyU2SetupReqPkg( (PCHAR)SetupGetU2CfgDescr );
    s = U2HostCtrlTransfer( U2Com_Buffer, &len );                            // ִ�п��ƴ���
    if ( s != ERR_SUCCESS )	        return( s );
    if ( len < ( (PUSB_SETUP_REQ)SetupGetU2CfgDescr ) -> wLength ) return( ERR_USB_BUF_OVER );  // ���س��ȴ���

    len = ( (PUSB_CFG_DESCR)U2Com_Buffer ) -> wTotalLength;
    CopyU2SetupReqPkg( (PCHAR)SetupGetU2CfgDescr );
    pU2SetupReq ->wLength = len;                                                 // �����������������ܳ���
    s = U2HostCtrlTransfer( U2Com_Buffer, &len );                            // ִ�п��ƴ���
    if ( s != ERR_SUCCESS )         return( s );
 
    
#ifdef DISK_BASE_BUF_LEN
	if(len>64) len = 64;
	memcpy( TxBuffer, U2Com_Buffer, len);                                             //U�̲���ʱ����Ҫ������TxBuffer
#endif
     
    return( ERR_SUCCESS );
}

/*******************************************************************************
* Function Name  : CtrlSetUsb2Address
* Description    : ����USB2�豸��ַ
* Input          : addr: �豸��ַ
* Return         : ERR_SUCCESS      �ɹ�
                   ����
*******************************************************************************/
UINT8 CtrlSetUsb2Address( UINT8 addr )
{
    UINT8   s;

    CopyU2SetupReqPkg( (PCHAR)SetupSetUsb2Addr );
    pU2SetupReq -> wValue = addr;                                       // USB�豸��ַ
    s = U2HostCtrlTransfer( NULL, NULL );                               // ִ�п��ƴ���
    if ( s != ERR_SUCCESS )         return( s );
    SetHostUsb2Addr( addr );                                                     // ����USB������ǰ������USB�豸��ַ
    mDelaymS( 10 );                                                             // �ȴ�USB�豸��ɲ���
    return( ERR_SUCCESS );
    }

/*******************************************************************************
* Function Name  : CtrlSetUsb2Config
* Description    : ����USB2�豸����
* Input          : cfg: ����ֵ
* Return         : ERR_SUCCESS      �ɹ�
                   ����
*******************************************************************************/
UINT8 CtrlSetUsb2Config( UINT8 cfg )
{    
    CopyU2SetupReqPkg( (PCHAR)SetupSetUsb2Config );
    pU2SetupReq -> wValue = cfg;                                        // USB�豸����
    return( U2HostCtrlTransfer( NULL, NULL ) );                         // ִ�п��ƴ���
}

/*******************************************************************************
* Function Name  : CtrlClearU2EndpStall
* Description    : ���U2�˵�STALL
* Input          : endp: �˵��ַ
* Return         : ERR_SUCCESS      �ɹ�
                   ����
*******************************************************************************/
UINT8 CtrlClearU2EndpStall( UINT8 endp )
{
    CopyU2SetupReqPkg( (PCHAR)SetupClrU2EndpStall );                              // ����˵�Ĵ���
    pU2SetupReq -> wIndex = endp;                                        // �˵��ַ
    return( U2HostCtrlTransfer( NULL, NULL ) );                          // ִ�п��ƴ���
}

/*******************************************************************************
* Function Name  : CtrlSetUsb2Intercace
* Description    : ����USB2�豸�ӿ�
* Input          : cfg: ����ֵ
* Return         : ERR_SUCCESS      �ɹ�
                   ����
*******************************************************************************/
UINT8 CtrlSetUsb2Intercace( UINT8 cfg )
{
    CopyU2SetupReqPkg( (PCHAR)SetupSetUsb2Interface );
    pU2SetupReq -> wValue = cfg;                                          // USB�豸����
    return( U2HostCtrlTransfer( NULL, NULL ) );                           // ִ�п��ƴ���
}

/*******************************************************************************
* Function Name  : USB2_HostInit
* Description    : USB2�������ܳ�ʼ��
* Input          : None
* Return         : None
*******************************************************************************/
void  USB2_HostInit( void )
{
    R8_USB2_CTRL = RB_UC_HOST_MODE;
    R8_U2HOST_CTRL = 0;
    R8_USB2_DEV_AD = 0x00;

    R8_U2H_EP_MOD = RB_UH_EP_TX_EN | RB_UH_EP_RX_EN;
    R16_U2H_RX_DMA = (UINT16)(UINT32)pU2HOST_RX_RAM_Addr;
    R16_U2H_TX_DMA = (UINT16)(UINT32)pU2HOST_TX_RAM_Addr;

    R8_U2H_RX_CTRL = 0x00;
    R8_U2H_TX_CTRL = 0x00;
    R8_USB2_CTRL =  RB_UC_HOST_MODE | RB_UC_INT_BUSY | RB_UC_DMA_EN;
    R8_U2H_SETUP = RB_UH_SOF_EN;
    R8_USB2_INT_FG = 0xFF;
    DisableRootU2HubPort( );
    R8_USB2_INT_EN = RB_UIE_TRANSFER|RB_UIE_DETECT;

    FoundNewU2Dev = 0;
}













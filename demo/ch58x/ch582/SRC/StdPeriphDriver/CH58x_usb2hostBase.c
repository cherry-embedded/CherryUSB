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

UINT8  Usb2DevEndp0Size;     // USB2设备的端点0的最大包尺寸
UINT8  FoundNewU2Dev;
_RootHubDev   ThisUsb2Dev;                            //ROOT口

PUINT8  pU2HOST_RX_RAM_Addr;
PUINT8  pU2HOST_TX_RAM_Addr;

/*获取设备描述符*/
__attribute__((aligned(4))) const UINT8  SetupGetU2DevDescr[] = { USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_DEVICE, 0x00, 0x00, sizeof( USB_DEV_DESCR ), 0x00 };
/*获取配置描述符*/
__attribute__((aligned(4))) const UINT8  SetupGetU2CfgDescr[] = { USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_CONFIG, 0x00, 0x00, 0x04, 0x00 };
/*设置USB地址*/
__attribute__((aligned(4))) const UINT8  SetupSetUsb2Addr[] = { USB_REQ_TYP_OUT, USB_SET_ADDRESS, USB_DEVICE_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00 };
/*设置USB配置*/
__attribute__((aligned(4))) const UINT8  SetupSetUsb2Config[] = { USB_REQ_TYP_OUT, USB_SET_CONFIGURATION, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
/*设置USB接口配置*/
__attribute__((aligned(4))) const UINT8  SetupSetUsb2Interface[] = { USB_REQ_RECIP_INTERF, USB_SET_INTERFACE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
/*清除端点STALL*/
__attribute__((aligned(4))) const UINT8  SetupClrU2EndpStall[] = { USB_REQ_TYP_OUT | USB_REQ_RECIP_ENDP, USB_CLEAR_FEATURE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/*******************************************************************************
* Function Name  : DisableRootU2HubPort( )
* Description    : 关闭U2HUB端口
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
* Description    : 分析ROOT-U2HUB状态,处理ROOT-U2HUB端口的设备插拔事件
                   如果设备拔出,函数中调用DisableRootU2HubPort()函数,将端口关闭,插入事件,置相应端口的状态位
* Input          : None
* Return         : 返回ERR_SUCCESS为没有情况,返回ERR_USB_CONNECT为检测到新连接,返回ERR_USB_DISCON为检测到断开
*******************************************************************************/
UINT8   AnalyzeRootU2Hub( void )
{
  UINT8 s;

    s = ERR_SUCCESS;

    if ( R8_USB2_MIS_ST & RB_UMS_DEV_ATTACH ) {                                        // 设备存在
#ifdef DISK_BASE_BUF_LEN
        if ( CHRV3DiskStatus == DISK_DISCONNECT
#else
        if ( ThisUsb2Dev.DeviceStatus == ROOT_DEV_DISCONNECT                          // 检测到有设备插入
#endif
            || ( R8_U2HOST_CTRL & RB_UH_PORT_EN ) == 0x00 ) {                         // 检测到有设备插入,但尚未允许,说明是刚插入
            DisableRootU2HubPort( );                                                   // 关闭端口
#ifdef DISK_BASE_BUF_LEN
            CHRV3DiskStatus = DISK_CONNECT;
#else
            ThisUsb2Dev.DeviceSpeed = R8_USB2_MIS_ST & RB_UMS_DM_LEVEL ? 0 : 1;
            ThisUsb2Dev.DeviceStatus = ROOT_DEV_CONNECTED;                            //置连接标志
#endif
            PRINT( "USB2 dev in\n" );
            s = ERR_USB_CONNECT;

        }
    }

#ifdef DISK_BASE_BUF_LEN
    else if ( CHRV3DiskStatus >= DISK_CONNECT ) {
#else
    else if ( ThisUsb2Dev.DeviceStatus >= ROOT_DEV_CONNECTED ) {                    //检测到设备拔出
#endif
        DisableRootU2HubPort( );                                                     // 关闭端口
        PRINT( "USB2 dev out\n" );
        if ( s == ERR_SUCCESS ) s = ERR_USB_DISCON;
    }
//  R8_USB_INT_FG = RB_UIF_DETECT;                                                  // 清中断标志
    return( s );
}

/*******************************************************************************
* Function Name  : SetHostUsb2Addr
* Description    : 设置USB2主机当前操作的USB设备地址
* Input          : UINT8 addr
* Return         : None
*******************************************************************************/
void    SetHostUsb2Addr( UINT8 addr )
{
    R8_USB2_DEV_AD = (R8_USB2_DEV_AD&RB_UDA_GP_BIT) | (addr&MASK_USB_ADDR);
}

/*******************************************************************************
* Function Name  : SetUsb2Speed
* Description    : 设置当前USB2速度
* Input          : UINT8 FullSpeed
* Return         : None
*******************************************************************************/
void    SetUsb2Speed( UINT8 FullSpeed )
{
#ifndef DISK_BASE_BUF_LEN
    if ( FullSpeed )                                                           // 全速
    {
        R8_USB2_CTRL &= ~ RB_UC_LOW_SPEED;                                           // 全速
        R8_U2H_SETUP &= ~ RB_UH_PRE_PID_EN;                                          // 禁止PRE PID
    }
    else
    {
        R8_USB2_CTRL |= RB_UC_LOW_SPEED;                                             // 低速
    }
#endif
    ( void ) FullSpeed;
}

/*******************************************************************************
* Function Name  : ResetRootU2HubPort( )
* Description    : 检测到设备后,复位总线,为枚举设备准备,设置为默认为全速
* Input          : None
* Return         : None
*******************************************************************************/
void  ResetRootU2HubPort( void )
{
    Usb2DevEndp0Size = DEFAULT_ENDP0_SIZE;                                      //USB2设备的端点0的最大包尺寸
    SetHostUsb2Addr( 0x00 );
    R8_U2HOST_CTRL &= ~RB_UH_PORT_EN;                                             // 关掉端口
    SetUsb2Speed( 1 );                                                            // 默认为全速
    R8_U2HOST_CTRL = (R8_U2HOST_CTRL & ~RB_UH_LOW_SPEED) | RB_UH_BUS_RESET;        // 默认为全速,开始复位
    mDelaymS( 15 );                                                            // 复位时间10mS到20mS
    R8_U2HOST_CTRL = R8_U2HOST_CTRL & ~ RB_UH_BUS_RESET;                            // 结束复位
    mDelayuS( 250 );
    R8_USB2_INT_FG = RB_UIF_DETECT;                                               // 清中断标志
}

/*******************************************************************************
* Function Name  : EnableRootU2HubPort( )
* Description    : 使能ROOT-U2HUB端口,相应的bU2H_PORT_EN置1开启端口,设备断开可能导致返回失败
* Input          : None
* Return         : 返回ERR_SUCCESS为检测到新连接,返回ERR_USB_DISCON为无连接
*******************************************************************************/
UINT8   EnableRootU2HubPort(void)
{
#ifdef DISK_BASE_BUF_LEN
    if ( CHRV3DiskStatus < DISK_CONNECT ) CHRV3DiskStatus = DISK_CONNECT;
#else
    if ( ThisUsb2Dev.DeviceStatus < ROOT_DEV_CONNECTED ) ThisUsb2Dev.DeviceStatus = ROOT_DEV_CONNECTED;
#endif
    if ( R8_USB2_MIS_ST & RB_UMS_DEV_ATTACH ) {                                        // 有设备
#ifndef DISK_BASE_BUF_LEN
        if ( ( R8_U2HOST_CTRL & RB_UH_PORT_EN ) == 0x00 ) {                              // 尚未使能
            ThisUsb2Dev.DeviceSpeed = (R8_USB2_MIS_ST & RB_UMS_DM_LEVEL) ? 0 : 1;
            if ( ThisUsb2Dev.DeviceSpeed == 0 ) R8_U2HOST_CTRL |= RB_UH_LOW_SPEED;          // 低速
        }
#endif
        R8_U2HOST_CTRL |= RB_UH_PORT_EN;                                                 //使能HUB端口
        return( ERR_SUCCESS );
    }
    return( ERR_USB_DISCON );
}


/*******************************************************************************
* Function Name  : WaitUSB2_Interrupt
* Description    : 等待USB2中断
* Input          : None
* Return         : 返回ERR_SUCCESS 数据接收或者发送成功
                   ERR_USB_UNKNOWN 数据接收或者发送失败
*******************************************************************************/
UINT8 WaitUSB2_Interrupt( void )
{
    UINT16  i;
    for ( i = WAIT_USB_TOUT_200US; i != 0 && (R8_USB2_INT_FG&RB_UIF_TRANSFER) == 0; i -- ){;}
    return( (R8_USB2_INT_FG&RB_UIF_TRANSFER)  ? ERR_SUCCESS : ERR_USB_UNKNOWN );
}

/*******************************************************************************
* Function Name  : USB2HostTransact
* Description    : 传输事务,输入目的端点地址/PID令牌,同步标志,以20uS为单位的NAK重试总时间(0则不重试,0xFFFF无限重试),返回0成功,超时/出错重试
                   本子程序着重于易理解,而在实际应用中,为了提供运行速度,应该对本子程序代码进行优化
* Input          : endp_pid:令牌和地址  endp_pid: 高4位是token_pid令牌, 低4位是端点地址
                   tog     : 同步标志
                   timeout: 超时时间
* Return         : ERR_USB_UNKNOWN 超时，可能硬件异常
                   ERR_USB_DISCON  设备断开
                   ERR_USB_CONNECT 设备连接
                   ERR_SUCCESS     传输完成
*******************************************************************************/
UINT8   USB2HostTransact( UINT8 endp_pid, UINT8 tog, UINT32 timeout )
{
    UINT8 TransRetry;

    UINT8 s, r;
    UINT16  i;

    R8_U2H_RX_CTRL = R8_U2H_TX_CTRL = tog;
    TransRetry = 0;

    do {
        R8_U2H_EP_PID = endp_pid;                                                      // 指定令牌PID和目的端点号
        R8_USB2_INT_FG = RB_UIF_TRANSFER;
        for ( i = WAIT_USB_TOUT_200US; i != 0 && (R8_USB2_INT_FG&RB_UIF_TRANSFER) == 0; i -- );
        R8_U2H_EP_PID = 0x00;                                                          // 停止USB传输
        if ( (R8_USB2_INT_FG&RB_UIF_TRANSFER) == 0 )     {printf("!!!!!!!!\n");return( ERR_USB_UNKNOWN );}

        if ( R8_USB2_INT_FG & RB_UIF_DETECT ) {                                       // USB设备插拔事件
//      mDelayuS( 200 );                                                       // 等待传输完成
            R8_USB2_INT_FG = RB_UIF_DETECT;
            s = AnalyzeRootU2Hub( );                                                   // 分析ROOT-U2HUB状态

            if ( s == ERR_USB_CONNECT )       FoundNewU2Dev = 1;
#ifdef DISK_BASE_BUF_LEN
            if ( CHRV3DiskStatus == DISK_DISCONNECT ) {return( ERR_USB_DISCON );}      // USB设备断开事件
            if ( CHRV3DiskStatus == DISK_CONNECT ) {return( ERR_USB_CONNECT );}        // USB设备连接事件
#else
            if ( ThisUsb2Dev.DeviceStatus == ROOT_DEV_DISCONNECT ) {return( ERR_USB_DISCON );}// USB设备断开事件
            if ( ThisUsb2Dev.DeviceStatus == ROOT_DEV_CONNECTED ) {return( ERR_USB_CONNECT );}// USB设备连接事件
#endif
            mDelayuS( 200 );  // 等待传输完成
        }

        if ( R8_USB2_INT_FG & RB_UIF_TRANSFER )                    // 传输完成事件
        {
            if ( R8_USB2_INT_ST & RB_UIS_TOG_OK )        {return( ERR_SUCCESS );}
            r = R8_USB2_INT_ST & MASK_UIS_H_RES;  // USB设备应答状态
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
                    if ( r ) {return( r | ERR_USB_TRANSFER );}  // 不是超时/出错,意外应答
                    break;  // 超时重试
                case USB_PID_IN:
                    if ( r == USB_PID_DATA0 || r == USB_PID_DATA1 ) {  // 不同步则需丢弃后重试
                    }  // 不同步重试
                    else if ( r ) {return( r | ERR_USB_TRANSFER );}  // 不是超时/出错,意外应答
                    break;  // 超时重试
                default:
                    return( ERR_USB_UNKNOWN );  // 不可能的情况
                    break;
            }
        }
        else {  // 其它中断,不应该发生的情况
            R8_USB2_INT_FG = 0xFF;  /* 清中断标志 */
        }
        mDelayuS( 15 );
    } while ( ++ TransRetry < 3 );
    return( ERR_USB_TRANSFER );  // 应答超时
}

/*******************************************************************************
* Function Name  : U2HostCtrlTransfer
* Description    : 执行控制传输,8字节请求码在pSetupReq中,DataBuf为可选的收发缓冲区
* Input          : DataBuf :如果需要接收和发送数据,那么DataBuf需指向有效缓冲区用于存放后续数据
                   RetLen  :实际成功收发的总长度保存在RetLen指向的字节变量中
* Return         : ERR_USB_BUF_OVER IN状态阶段出错
                   ERR_SUCCESS     数据交换成功
                   其他错误状态
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
    if ( pLen )       *pLen = 0;      // 实际成功收发的总长度

    R8_U2H_TX_LEN = sizeof( USB_SETUP_REQ );
    s = USB2HostTransact( USB_PID_SETUP << 4 | 0x00, 0x00, 200000/20 );          // SETUP阶段,200mS超时
    if ( s != ERR_SUCCESS )             return( s );
    R8_U2H_RX_CTRL = R8_U2H_TX_CTRL = RB_UH_R_TOG | RB_UH_R_AUTO_TOG | RB_UH_T_TOG | RB_UH_T_AUTO_TOG;// 默认DATA1
    R8_U2H_TX_LEN = 0x01;                                                        // 默认无数据故状态阶段为IN
    RemLen = pU2SetupReq -> wLength;
    if ( RemLen && pBuf )                                                       // 需要收发数据
    {
        if ( pU2SetupReq -> bRequestType & USB_REQ_TYP_IN )                       // 收
        {
            while ( RemLen )
            {
                mDelayuS( 200 );
                s = USB2HostTransact( USB_PID_IN << 4 | 0x00, R8_U2H_RX_CTRL, 200000/20 );// IN数据
                if ( s != ERR_SUCCESS )         return( s );
                RxLen = R8_USB2_RX_LEN < RemLen ? R8_USB2_RX_LEN : RemLen;
                RemLen -= RxLen;
                if ( pLen )       *pLen += RxLen;                       // 实际成功收发的总长度
                for ( RxCnt = 0; RxCnt != RxLen; RxCnt ++ )
                {
                    *pBuf = pU2HOST_RX_RAM_Addr[ RxCnt ];
                    pBuf ++;
                }
                if ( R8_USB2_RX_LEN == 0 || ( R8_USB2_RX_LEN & ( Usb2DevEndp0Size - 1 ) ) ) break;    // 短包
            }
            R8_U2H_TX_LEN = 0x00;                                                    // 状态阶段为OUT
        }
        else                                                                     // 发
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
                s = USB2HostTransact( USB_PID_OUT << 4 | 0x00, R8_U2H_TX_CTRL, 200000/20 );// OUT数据
                if ( s != ERR_SUCCESS )         return( s );
                RemLen -= R8_U2H_TX_LEN;
                if ( pLen )            *pLen += R8_U2H_TX_LEN;       // 实际成功收发的总长度
            }
//          R8_U2H_TX_LEN = 0x01;                                                     // 状态阶段为IN
        }
    }
    mDelayuS( 200 );
    s = USB2HostTransact( ( R8_U2H_TX_LEN ? USB_PID_IN << 4 | 0x00: USB_PID_OUT << 4 | 0x00 ), RB_UH_R_TOG | RB_UH_T_TOG, 200000/20 );  // STATUS阶段
    if ( s != ERR_SUCCESS )         return( s );
    if ( R8_U2H_TX_LEN == 0 )        return( ERR_SUCCESS );      // 状态OUT
    if ( R8_USB2_RX_LEN == 0 )       return( ERR_SUCCESS );     // 状态IN,检查IN状态返回数据长度
    return( ERR_USB_BUF_OVER );                                                   // IN状态阶段错误
}

/*******************************************************************************
* Function Name  : CopyU2SetupReqPkg
* Description    : 复制U2控制传输的请求包
* Input          : pU2ReqPkt: 控制请求包地址
* Return         : None
*******************************************************************************/
void CopyU2SetupReqPkg( PCCHAR pReqPkt )                                        // 复制控制传输的请求包
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
* Description    : 获取U2设备描述符,返回在 pU2HOST_TX_RAM_Addr 中
* Input          : None
* Return         : ERR_USB_BUF_OVER 描述符长度错误
                   ERR_SUCCESS      成功
                   其他
*******************************************************************************/
UINT8 CtrlGetU2DeviceDescr( void )
{
    UINT8   s;
    UINT8  len;

    Usb2DevEndp0Size = DEFAULT_ENDP0_SIZE;
    CopyU2SetupReqPkg( (PCHAR)SetupGetU2DevDescr );
    s = U2HostCtrlTransfer( U2Com_Buffer, &len );                            // 执行控制传输
    if ( s != ERR_SUCCESS ) 		return( s );
    Usb2DevEndp0Size = ( (PUSB_DEV_DESCR)U2Com_Buffer ) -> bMaxPacketSize0;        // 端点0最大包长度,这是简化处理,正常应该先获取前8字节后立即更新UsbDevEndp0Size再继续
    if ( len < ((PUSB_SETUP_REQ)SetupGetU2DevDescr)->wLength )        return( ERR_USB_BUF_OVER );   // 描述符长度错误
    return( ERR_SUCCESS );
}

/*******************************************************************************
* Function Name  : CtrlGetU2ConfigDescr
* Description    : 获取U2配置描述符,返回在 pU2HOST_TX_RAM_Addr 中
* Input          : None
* Return         : ERR_USB_BUF_OVER 描述符长度错误
                   ERR_SUCCESS      成功
                   其他
*******************************************************************************/
UINT8 CtrlGetU2ConfigDescr( void )
{
    UINT8   s;
    UINT8  len;
      
    CopyU2SetupReqPkg( (PCHAR)SetupGetU2CfgDescr );
    s = U2HostCtrlTransfer( U2Com_Buffer, &len );                            // 执行控制传输
    if ( s != ERR_SUCCESS )	        return( s );
    if ( len < ( (PUSB_SETUP_REQ)SetupGetU2CfgDescr ) -> wLength ) return( ERR_USB_BUF_OVER );  // 返回长度错误

    len = ( (PUSB_CFG_DESCR)U2Com_Buffer ) -> wTotalLength;
    CopyU2SetupReqPkg( (PCHAR)SetupGetU2CfgDescr );
    pU2SetupReq ->wLength = len;                                                 // 完整配置描述符的总长度
    s = U2HostCtrlTransfer( U2Com_Buffer, &len );                            // 执行控制传输
    if ( s != ERR_SUCCESS )         return( s );
 
    
#ifdef DISK_BASE_BUF_LEN
	if(len>64) len = 64;
	memcpy( TxBuffer, U2Com_Buffer, len);                                             //U盘操作时，需要拷贝到TxBuffer
#endif
     
    return( ERR_SUCCESS );
}

/*******************************************************************************
* Function Name  : CtrlSetUsb2Address
* Description    : 设置USB2设备地址
* Input          : addr: 设备地址
* Return         : ERR_SUCCESS      成功
                   其他
*******************************************************************************/
UINT8 CtrlSetUsb2Address( UINT8 addr )
{
    UINT8   s;

    CopyU2SetupReqPkg( (PCHAR)SetupSetUsb2Addr );
    pU2SetupReq -> wValue = addr;                                       // USB设备地址
    s = U2HostCtrlTransfer( NULL, NULL );                               // 执行控制传输
    if ( s != ERR_SUCCESS )         return( s );
    SetHostUsb2Addr( addr );                                                     // 设置USB主机当前操作的USB设备地址
    mDelaymS( 10 );                                                             // 等待USB设备完成操作
    return( ERR_SUCCESS );
    }

/*******************************************************************************
* Function Name  : CtrlSetUsb2Config
* Description    : 设置USB2设备配置
* Input          : cfg: 配置值
* Return         : ERR_SUCCESS      成功
                   其他
*******************************************************************************/
UINT8 CtrlSetUsb2Config( UINT8 cfg )
{    
    CopyU2SetupReqPkg( (PCHAR)SetupSetUsb2Config );
    pU2SetupReq -> wValue = cfg;                                        // USB设备配置
    return( U2HostCtrlTransfer( NULL, NULL ) );                         // 执行控制传输
}

/*******************************************************************************
* Function Name  : CtrlClearU2EndpStall
* Description    : 清除U2端点STALL
* Input          : endp: 端点地址
* Return         : ERR_SUCCESS      成功
                   其他
*******************************************************************************/
UINT8 CtrlClearU2EndpStall( UINT8 endp )
{
    CopyU2SetupReqPkg( (PCHAR)SetupClrU2EndpStall );                              // 清除端点的错误
    pU2SetupReq -> wIndex = endp;                                        // 端点地址
    return( U2HostCtrlTransfer( NULL, NULL ) );                          // 执行控制传输
}

/*******************************************************************************
* Function Name  : CtrlSetUsb2Intercace
* Description    : 设置USB2设备接口
* Input          : cfg: 配置值
* Return         : ERR_SUCCESS      成功
                   其他
*******************************************************************************/
UINT8 CtrlSetUsb2Intercace( UINT8 cfg )
{
    CopyU2SetupReqPkg( (PCHAR)SetupSetUsb2Interface );
    pU2SetupReq -> wValue = cfg;                                          // USB设备配置
    return( U2HostCtrlTransfer( NULL, NULL ) );                           // 执行控制传输
}

/*******************************************************************************
* Function Name  : USB2_HostInit
* Description    : USB2主机功能初始化
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













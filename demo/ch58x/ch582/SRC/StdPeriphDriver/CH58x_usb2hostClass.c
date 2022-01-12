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

/* 设置HID上传速率 */
__attribute__((aligned(4))) const UINT8  SetupSetU2HIDIdle[] = { 0x21, HID_SET_IDLE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
/* 获取HID设备报表描述符 */
__attribute__((aligned(4))) const UINT8  SetupGetU2HIDDevReport[] = { 0x81, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_REPORT, 0x00, 0x00, 0x41, 0x00 };
/* 获取HUB描述符 */
__attribute__((aligned(4))) const UINT8  SetupGetU2HubDescr[] = { HUB_GET_HUB_DESCRIPTOR, HUB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_HUB, 0x00, 0x00, sizeof( USB_HUB_DESCR ), 0x00 };


__attribute__((aligned(4))) UINT8  U2Com_Buffer[ 128 ];            // 定义用户临时缓冲区,枚举时用于处理描述符,枚举结束也可以用作普通临时缓冲区
    
/*****************************************************************************
* Function Name  : InitRootU2Device
* Description    : 初始化指定ROOT-U2HUB端口的USB设备
* Input          : DataBuf: 枚举过程中存放的描述符信息缓存区
* Return         :
*******************************************************************************/
UINT8 InitRootU2Device( void )
{
    UINT8  i, s;
    UINT8  cfg, dv_cls, if_cls;
	
    PRINT( "Reset U2 host port\n" );
    ResetRootU2HubPort( );  		// 检测到设备后,复位相应端口的USB总线
    for ( i = 0, s = 0; i < 100; i ++ ) {  				// 等待USB设备复位后重新连接,100mS超时
        mDelaymS( 1 );
        if ( EnableRootU2HubPort( ) == ERR_SUCCESS ) {  // 使能端口
            i = 0;
            s ++;  					
            if ( s > 100 ) break;  	// 已经稳定连接100mS
        }
    }
    if ( i ) {  										// 复位后设备没有连接
        DisableRootU2HubPort( );
        PRINT( "Disable U2 host port because of disconnect\n" );
        return( ERR_USB_DISCON );
    }
    SetUsb2Speed( ThisUsb2Dev.DeviceSpeed );  // 设置当前USB速度
	
    PRINT( "GetU2DevDescr: " );
    s = CtrlGetU2DeviceDescr();  		// 获取设备描述符
    if ( s == ERR_SUCCESS )
    {
        for ( i = 0; i < ((PUSB_SETUP_REQ)SetupGetU2DevDescr)->wLength; i ++ )
       PRINT( "x%02X ", (UINT16)( U2Com_Buffer[i] ) );
       PRINT( "\n" ); 
		
       ThisUsb2Dev.DeviceVID = ((PUSB_DEV_DESCR)U2Com_Buffer)->idVendor; //保存VID PID信息
       ThisUsb2Dev.DevicePID = ((PUSB_DEV_DESCR)U2Com_Buffer)->idProduct;
       dv_cls = ( (PUSB_DEV_DESCR)U2Com_Buffer ) -> bDeviceClass;
		
       s = CtrlSetUsb2Address( ((PUSB_SETUP_REQ)SetupSetUsb2Addr)->wValue );
       if ( s == ERR_SUCCESS )
       {
           ThisUsb2Dev.DeviceAddress = ( (PUSB_SETUP_REQ)SetupSetUsb2Addr )->wValue;  // 保存USB地址
    
           PRINT( "GetU2CfgDescr: " );
           s = CtrlGetU2ConfigDescr( );
           if ( s == ERR_SUCCESS ) 
           {
               for ( i = 0; i < ( (PUSB_CFG_DESCR)U2Com_Buffer )->wTotalLength; i ++ )
               PRINT( "x%02X ", (UINT16)( U2Com_Buffer[i] ) );
               PRINT("\n");
/* 分析配置描述符,获取端点数据/各端点地址/各端点大小等,更新变量endp_addr和endp_size等 */				
               cfg = ( (PUSB_CFG_DESCR)U2Com_Buffer )->bConfigurationValue;
               if_cls = ( (PUSB_CFG_DESCR_LONG)U2Com_Buffer )->itf_descr.bInterfaceClass;  // 接口类代码
                              
               if ( (dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_STORAGE)) {  // 是USB存储类设备,基本上确认是U盘
#ifdef	FOR_ROOT_UDISK_ONLY
                   CHRV3DiskStatus = DISK_USB_ADDR;
                   return( ERR_SUCCESS );
               }
               else 	return( ERR_USB_UNSUPPORT );
#else
                  s = CtrlSetUsb2Config( cfg );  // 设置USB设备配置
                  if ( s == ERR_SUCCESS )
                  {
                      ThisUsb2Dev.DeviceStatus = ROOT_DEV_SUCCESS;
                      ThisUsb2Dev.DeviceType = USB_DEV_CLASS_STORAGE;
                      PRINT( "U2 USB-Disk Ready\n" );
                      SetUsb2Speed( 1 );  // 默认为全速
                      return( ERR_SUCCESS );
                  }
               }
               else if ( (dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_PRINTER) && ((PUSB_CFG_DESCR_LONG)U2Com_Buffer)->itf_descr.bInterfaceSubClass == 0x01 ) {  // 是打印机类设备
                   s = CtrlSetUsb2Config( cfg );  // 设置USB设备配置
                   if ( s == ERR_SUCCESS ) {
//	需保存端点信息以便主程序进行USB传输
                       ThisUsb2Dev.DeviceStatus = ROOT_DEV_SUCCESS;
                       ThisUsb2Dev.DeviceType = USB_DEV_CLASS_PRINTER;
                       PRINT( "U2 USB-Print Ready\n" );
                       SetUsb2Speed( 1 );  // 默认为全速
                       return( ERR_SUCCESS );
                   }
               }
               else if ( (dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_HID) && ((PUSB_CFG_DESCR_LONG)U2Com_Buffer)->itf_descr.bInterfaceSubClass <= 0x01 ) {  // 是HID类设备,键盘/鼠标等
//  从描述符中分析出HID中断端点的地址

//  保存中断端点的地址,位7用于同步标志位,清0
                   if_cls = ( (PUSB_CFG_DESCR_LONG)U2Com_Buffer ) -> itf_descr.bInterfaceProtocol;
                   s = CtrlSetUsb2Config( cfg );  // 设置USB设备配置
                   if ( s == ERR_SUCCESS ) {
//	    					Set_Idle( );
//	需保存端点信息以便主程序进行USB传输
                       ThisUsb2Dev.DeviceStatus = ROOT_DEV_SUCCESS;
                       if ( if_cls == 1 ) {
                       ThisUsb2Dev.DeviceType = DEV_TYPE_KEYBOARD;
//	进一步初始化,例如设备键盘指示灯LED等
                       PRINT( "U2 USB-Keyboard Ready\n" );
                       SetUsb2Speed( 1 );  // 默认为全速
                       return( ERR_SUCCESS );
                       }
                       else if ( if_cls == 2 ) {
                           ThisUsb2Dev.DeviceType = DEV_TYPE_MOUSE;
//	为了以后查询鼠标状态,应该分析描述符,取得中断端口的地址,长度等信息
                           PRINT( "U2 USB-Mouse Ready\n" );
                            SetUsb2Speed( 1 );  // 默认为全速
                           return( ERR_SUCCESS );
                       }
                       s = ERR_USB_UNSUPPORT;
                   }
                }
                else if(dv_cls == USB_DEV_CLASS_HUB){ // 是HUB类设备,集线器等
                    s = CtrlGetU2HubDescr( );
                    if(s==ERR_SUCCESS){
                        printf( "Max Port:%02X ",(((PXUSB_HUB_DESCR)U2Com_Buffer)->bNbrPorts) );
                        s = CtrlSetUsb2Config( cfg );               // 设置USB设备配置
                        if(s == ERR_SUCCESS){
                            ThisUsb2Dev.DeviceStatus = ROOT_DEV_SUCCESS;
                            ThisUsb2Dev.DeviceType = USB_DEV_CLASS_HUB;
                            PRINT( "U2 USB-HUB Ready\n" );
                        }
                    }
                }
                else {   // 可以进一步分析
                    s = CtrlSetUsb2Config( cfg );  // 设置USB设备配置
                    if ( s == ERR_SUCCESS ) {
//	需保存端点信息以便主程序进行USB传输
                        ThisUsb2Dev.DeviceStatus = ROOT_DEV_SUCCESS;
                        ThisUsb2Dev.DeviceType = DEV_TYPE_UNKNOW;
                        SetUsb2Speed( 1 );  // 默认为全速
                        return( ERR_SUCCESS );  /* 未知设备初始化成功 */
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
    SetUsb2Speed( 1 );  // 默认为全速
    return( s );		
}


/*******************************************************************************
* Function Name  : CtrlGetU2HIDDeviceReport
* Description    : 获取HID设备报表描述符,返回在U2TxBuffer中
* Input          : None
* Output         : None
* Return         : ERR_SUCCESS 成功
                   其他        错误
*******************************************************************************/
UINT8   CtrlGetU2HIDDeviceReport( UINT8 infc )
{
    UINT8   s;
    UINT8  len;    

	CopyU2SetupReqPkg( (PCHAR)SetupSetU2HIDIdle );
	pU2SetupReq -> wIndex = infc;
    s = U2HostCtrlTransfer( U2Com_Buffer, &len );                            // 执行控制传输
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }	
	
	CopyU2SetupReqPkg( (PCHAR)SetupGetU2HIDDevReport );
	pU2SetupReq -> wIndex = infc;
    s = U2HostCtrlTransfer( U2Com_Buffer, &len );                            // 执行控制传输
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
	
    return( ERR_SUCCESS );
}

/*******************************************************************************
* Function Name  : CtrlGetU2HubDescr
* Description    : 获取U2HUB描述符,返回在U2Com_Buffer中
* Input          : None
* Output         : None
* Return         : ERR_SUCCESS 成功
                   ERR_USB_BUF_OVER 长度错误
*******************************************************************************/
UINT8   CtrlGetU2HubDescr( void )
{
    UINT8   s;
    UINT8  len;
    
    CopyU2SetupReqPkg( (PCHAR)SetupGetU2HubDescr );
    s = U2HostCtrlTransfer( U2Com_Buffer, &len );                          // 执行控制传输
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
    if ( len < ( (PUSB_SETUP_REQ)SetupGetU2HubDescr ) -> wLength )
    {
        return( ERR_USB_BUF_OVER );                                            // 描述符长度错误
    }
//  if ( len < 4 ) return( ERR_USB_BUF_OVER );                                 // 描述符长度错误
    return( ERR_SUCCESS );
}


/*******************************************************************************
* Function Name  : U2HubGetPortStatus
* Description    : 查询U2HUB端口状态,返回在U2Com_Buffer中
* Input          : UINT8 HubPortIndex 
* Output         : None
* Return         : ERR_SUCCESS 成功
                   ERR_USB_BUF_OVER 长度错误
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
    s = U2HostCtrlTransfer( U2Com_Buffer, &len );                           // 执行控制传输
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
    if ( len < 4 )
    {
        return( ERR_USB_BUF_OVER );                                             // 描述符长度错误
    }
    return( ERR_SUCCESS );
}

/*******************************************************************************
* Function Name  : U2HubSetPortFeature
* Description    : 设置U2HUB端口特性
* Input          : UINT8 HubPortIndex    //HUB端口
                   UINT8 FeatureSelt     //HUB端口特性
* Output         : None
* Return         : ERR_SUCCESS 成功
                   其他        错误
*******************************************************************************/
UINT8   U2HubSetPortFeature( UINT8 HubPortIndex, UINT8 FeatureSelt )
{
    pU2SetupReq -> bRequestType = HUB_SET_PORT_FEATURE;
    pU2SetupReq -> bRequest = HUB_SET_FEATURE;
    pU2SetupReq -> wValue = 0x0000|FeatureSelt;
    pU2SetupReq -> wIndex = 0x0000|HubPortIndex;
    pU2SetupReq -> wLength = 0x0000;
    return( U2HostCtrlTransfer( NULL, NULL ) );                                 // 执行控制传输
}

/*******************************************************************************
* Function Name  : U2HubClearPortFeature
* Description    : 清除U2HUB端口特性
* Input          : UINT8 HubPortIndex                                         //HUB端口
                   UINT8 FeatureSelt                                          //HUB端口特性
* Output         : None
* Return         : ERR_SUCCESS 成功
                   其他        错误
*******************************************************************************/
UINT8   U2HubClearPortFeature( UINT8 HubPortIndex, UINT8 FeatureSelt )
{
    pU2SetupReq -> bRequestType = HUB_CLEAR_PORT_FEATURE;
    pU2SetupReq -> bRequest = HUB_CLEAR_FEATURE;
    pU2SetupReq -> wValue = 0x0000|FeatureSelt;
    pU2SetupReq -> wIndex = 0x0000|HubPortIndex;
    pU2SetupReq -> wLength = 0x0000;
    return( U2HostCtrlTransfer( NULL, NULL ) );                                // 执行控制传输
}












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
__attribute__((aligned(4))) const UINT8  SetupSetHIDIdle[] = { 0x21, HID_SET_IDLE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
/* 获取HID设备报表描述符 */
__attribute__((aligned(4))) const UINT8  SetupGetHIDDevReport[] = { 0x81, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_REPORT, 0x00, 0x00, 0x41, 0x00 };
/* 获取HUB描述符 */
__attribute__((aligned(4))) const UINT8  SetupGetHubDescr[] = { HUB_GET_HUB_DESCRIPTOR, HUB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_HUB, 0x00, 0x00, sizeof( USB_HUB_DESCR ), 0x00 };


__attribute__((aligned(4))) UINT8  Com_Buffer[ 128 ];            // 定义用户临时缓冲区,枚举时用于处理描述符,枚举结束也可以用作普通临时缓冲区
    
/*****************************************************************************
* Function Name  : InitRootDevice
* Description    : 初始化指定ROOT-HUB端口的USB设备
* Input          : DataBuf: 枚举过程中存放的描述符信息缓存区
* Return         :
*******************************************************************************/
UINT8 InitRootDevice( void ) 
{
    UINT8  i, s;
    UINT8  cfg, dv_cls, if_cls;
	
    PRINT( "Reset host port\n" );
    ResetRootHubPort( );  		// 检测到设备后,复位相应端口的USB总线
    for ( i = 0, s = 0; i < 100; i ++ ) {  				// 等待USB设备复位后重新连接,100mS超时
        mDelaymS( 1 );
        if ( EnableRootHubPort( ) == ERR_SUCCESS ) {  // 使能端口
            i = 0;
            s ++;  					
            if ( s > 100 ) break;  	// 已经稳定连接100mS
        }
    }
    if ( i ) {  										// 复位后设备没有连接
        DisableRootHubPort( );
        PRINT( "Disable host port because of disconnect\n" );
        return( ERR_USB_DISCON );
    }
    SetUsbSpeed( ThisUsbDev.DeviceSpeed );  // 设置当前USB速度
	
    PRINT( "GetDevDescr: " );
    s = CtrlGetDeviceDescr();  		// 获取设备描述符
    if ( s == ERR_SUCCESS )
    {
        for ( i = 0; i < ((PUSB_SETUP_REQ)SetupGetDevDescr)->wLength; i ++ ) 		
       PRINT( "x%02X ", (UINT16)( Com_Buffer[i] ) );
       PRINT( "\n" ); 
		
       ThisUsbDev.DeviceVID = ((PUSB_DEV_DESCR)Com_Buffer)->idVendor; //保存VID PID信息
       ThisUsbDev.DevicePID = ((PUSB_DEV_DESCR)Com_Buffer)->idProduct;
       dv_cls = ( (PUSB_DEV_DESCR)Com_Buffer ) -> bDeviceClass;
		
       s = CtrlSetUsbAddress( ((PUSB_SETUP_REQ)SetupSetUsbAddr)->wValue );  
       if ( s == ERR_SUCCESS )
       {
           ThisUsbDev.DeviceAddress = ( (PUSB_SETUP_REQ)SetupSetUsbAddr )->wValue;  // 保存USB地址
    
           PRINT( "GetCfgDescr: " );
           s = CtrlGetConfigDescr( );
           if ( s == ERR_SUCCESS ) 
           {
               for ( i = 0; i < ( (PUSB_CFG_DESCR)Com_Buffer )->wTotalLength; i ++ ) 
               PRINT( "x%02X ", (UINT16)( Com_Buffer[i] ) );
               PRINT("\n");
/* 分析配置描述符,获取端点数据/各端点地址/各端点大小等,更新变量endp_addr和endp_size等 */				
               cfg = ( (PUSB_CFG_DESCR)Com_Buffer )->bConfigurationValue;
               if_cls = ( (PUSB_CFG_DESCR_LONG)Com_Buffer )->itf_descr.bInterfaceClass;  // 接口类代码
                              
               if ( (dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_STORAGE)) {  // 是USB存储类设备,基本上确认是U盘
#ifdef	FOR_ROOT_UDISK_ONLY
                   CHRV3DiskStatus = DISK_USB_ADDR;
                   return( ERR_SUCCESS );
               }
               else 	return( ERR_USB_UNSUPPORT );
#else
                  s = CtrlSetUsbConfig( cfg );  // 设置USB设备配置
                  if ( s == ERR_SUCCESS )
                  {
                      ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                      ThisUsbDev.DeviceType = USB_DEV_CLASS_STORAGE;
                      PRINT( "USB-Disk Ready\n" );
                      SetUsbSpeed( 1 );  // 默认为全速
                      return( ERR_SUCCESS );
                  }
               }
               else if ( (dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_PRINTER) && ((PUSB_CFG_DESCR_LONG)Com_Buffer)->itf_descr.bInterfaceSubClass == 0x01 ) {  // 是打印机类设备
                   s = CtrlSetUsbConfig( cfg );  // 设置USB设备配置
                   if ( s == ERR_SUCCESS ) {
//	需保存端点信息以便主程序进行USB传输
                       ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                       ThisUsbDev.DeviceType = USB_DEV_CLASS_PRINTER;
                       PRINT( "USB-Print Ready\n" );
                       SetUsbSpeed( 1 );  // 默认为全速    
                       return( ERR_SUCCESS );
                   }
               }
               else if ( (dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_HID) && ((PUSB_CFG_DESCR_LONG)Com_Buffer)->itf_descr.bInterfaceSubClass <= 0x01 ) {  // 是HID类设备,键盘/鼠标等
//  从描述符中分析出HID中断端点的地址
//  保存中断端点的地址,位7用于同步标志位,清0
                   if_cls = ( (PUSB_CFG_DESCR_LONG)Com_Buffer ) -> itf_descr.bInterfaceProtocol;
                   s = CtrlSetUsbConfig( cfg );  // 设置USB设备配置
                   if ( s == ERR_SUCCESS ) {
//	    					Set_Idle( );
//	需保存端点信息以便主程序进行USB传输
                       ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                       if ( if_cls == 1 ) {
                       ThisUsbDev.DeviceType = DEV_TYPE_KEYBOARD;
//	进一步初始化,例如设备键盘指示灯LED等
                       PRINT( "USB-Keyboard Ready\n" );
                       SetUsbSpeed( 1 );  // 默认为全速
                       return( ERR_SUCCESS );
                       }
                       else if ( if_cls == 2 ) {
                           ThisUsbDev.DeviceType = DEV_TYPE_MOUSE;
//	为了以后查询鼠标状态,应该分析描述符,取得中断端口的地址,长度等信息
                           PRINT( "USB-Mouse Ready\n" );
                            SetUsbSpeed( 1 );  // 默认为全速
                           return( ERR_SUCCESS );
                       }
                       s = ERR_USB_UNSUPPORT;
                   }
                }
                else if(dv_cls == USB_DEV_CLASS_HUB){ // 是HUB类设备,集线器等
                    s = CtrlGetHubDescr( );
                    if(s==ERR_SUCCESS){
                        printf( "Max Port:%02X ",(((PXUSB_HUB_DESCR)Com_Buffer)->bNbrPorts) );
                        s = CtrlSetUsbConfig( cfg );               // 设置USB设备配置
                        if(s == ERR_SUCCESS){
                            ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                            ThisUsbDev.DeviceType = USB_DEV_CLASS_HUB;
                            PRINT( "USB-HUB Ready\n" );
                        }
                    }
                }
                else {   // 可以进一步分析
                    s = CtrlSetUsbConfig( cfg );  // 设置USB设备配置
                    if ( s == ERR_SUCCESS ) {
//	需保存端点信息以便主程序进行USB传输
                        ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                        ThisUsbDev.DeviceType = DEV_TYPE_UNKNOW;
                        SetUsbSpeed( 1 );  // 默认为全速
                        return( ERR_SUCCESS );  /* 未知设备初始化成功 */
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
    SetUsbSpeed( 1 );  // 默认为全速	
    return( s );		
}


/*******************************************************************************
* Function Name  : CtrlGetHIDDeviceReport
* Description    : 获取HID设备报表描述符,返回在TxBuffer中
* Input          : None
* Output         : None
* Return         : ERR_SUCCESS 成功
                   其他        错误
*******************************************************************************/
UINT8   CtrlGetHIDDeviceReport( UINT8 infc )  
{
    UINT8   s;
    UINT8  len;    

	CopySetupReqPkg( (PCHAR)SetupSetHIDIdle );		
	pSetupReq -> wIndex = infc;
    s = HostCtrlTransfer( Com_Buffer, &len );                            // 执行控制传输
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }	
	
	CopySetupReqPkg( (PCHAR)SetupGetHIDDevReport );	
	pSetupReq -> wIndex = infc;	
    s = HostCtrlTransfer( Com_Buffer, &len );                            // 执行控制传输
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
	
    return( ERR_SUCCESS );
}

/*******************************************************************************
* Function Name  : CtrlGetHubDescr
* Description    : 获取HUB描述符,返回在Com_Buffer中
* Input          : None
* Output         : None
* Return         : ERR_SUCCESS 成功
                   ERR_USB_BUF_OVER 长度错误
*******************************************************************************/
UINT8   CtrlGetHubDescr( void )  
{
    UINT8   s;
    UINT8  len;
    
    CopySetupReqPkg( (PCHAR)SetupGetHubDescr );
    s = HostCtrlTransfer( Com_Buffer, &len );                          // 执行控制传输
    if ( s != ERR_SUCCESS )
    {
        return( s );
    }
    if ( len < ( (PUSB_SETUP_REQ)SetupGetHubDescr ) -> wLength )
    {
        return( ERR_USB_BUF_OVER );                                            // 描述符长度错误
    }
//  if ( len < 4 ) return( ERR_USB_BUF_OVER );                                 // 描述符长度错误
    return( ERR_SUCCESS );
}


/*******************************************************************************
* Function Name  : HubGetPortStatus
* Description    : 查询HUB端口状态,返回在Com_Buffer中
* Input          : UINT8 HubPortIndex 
* Output         : None
* Return         : ERR_SUCCESS 成功
                   ERR_USB_BUF_OVER 长度错误
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
    s = HostCtrlTransfer( Com_Buffer, &len );                           // 执行控制传输
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
* Function Name  : HubSetPortFeature
* Description    : 设置HUB端口特性
* Input          : UINT8 HubPortIndex    //HUB端口
                   UINT8 FeatureSelt     //HUB端口特性
* Output         : None
* Return         : ERR_SUCCESS 成功
                   其他        错误
*******************************************************************************/
UINT8   HubSetPortFeature( UINT8 HubPortIndex, UINT8 FeatureSelt ) 
{
    pSetupReq -> bRequestType = HUB_SET_PORT_FEATURE;
    pSetupReq -> bRequest = HUB_SET_FEATURE;
    pSetupReq -> wValue = 0x0000|FeatureSelt;
    pSetupReq -> wIndex = 0x0000|HubPortIndex;
    pSetupReq -> wLength = 0x0000;
    return( HostCtrlTransfer( NULL, NULL ) );                                 // 执行控制传输
}

/*******************************************************************************
* Function Name  : HubClearPortFeature
* Description    : 清除HUB端口特性
* Input          : UINT8 HubPortIndex                                         //HUB端口
                   UINT8 FeatureSelt                                          //HUB端口特性
* Output         : None
* Return         : ERR_SUCCESS 成功
                   其他        错误
*******************************************************************************/
UINT8   HubClearPortFeature( UINT8 HubPortIndex, UINT8 FeatureSelt ) 
{
    pSetupReq -> bRequestType = HUB_CLEAR_PORT_FEATURE;
    pSetupReq -> bRequest = HUB_CLEAR_FEATURE;
    pSetupReq -> wValue = 0x0000|FeatureSelt;
    pSetupReq -> wIndex = 0x0000|HubPortIndex;
    pSetupReq -> wLength = 0x0000;
    return( HostCtrlTransfer( NULL, NULL ) );                                // 执行控制传输
}












/******************** (C) COPYRIGHT 2018 STMicroelectronics ********************
* File Name          : version.txt
* Author             : MCD Application Team
* Version            : V3.0.6
* Date               : 01-June-2018
* Description        : read me file for DfuSe Demonstrator
********************************************************************************
* THE PRESENT SOFTWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE TIME.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*******************************************************************************/

Last version 
***************

        - V3.0.6 - 01-June-2018

Package content
***************
       + Binaries :([INSTALLATION PATH]\BIN\) and ([INSTALLATION PATH]\BIN\STM32L) Variant with extra time for Erase.

         - DfuFileMgr.exe             : DFU File Manager aplication,
         - DfuSeDemo.exe              : DfuSe Demo application,
         - DfuSeCommand.exe           : DfuSe Command line application,
         - STDFUTester.exe            : DfuSe Tester application ,  
         - STDFUFiles.dll             : Dll that implements .dfu files
         - STDFUPRT.dll               : Dll that implements Protocol for upload and download,
         - STDFU.dll                  : Dll that issues basic DFU requests,
         - STTubeDevice30.dll         : Dll layer for easier driver access
		  + Doc                       : Documentation directory
			- UM0384 : DfuSe Application Programming Interface  
            - UM0391 : DfuSe File Format Specification          
            - UM0392 : DfuSe Application Programming Guide
            - UM0412 : DfuSe getting started
		  + Driver([INSTALLATION PATH]\Driver)

       + Sources :([INSTALLATION PATH]\Sources\)
	   
         - Binary
         - DfuFileMgr 
         - DfuSeDemo 
         - DfuseCommand 
         - Include
         - STDFU 
         - STDFUFiles
         - STDFUPRT 
         - STTubeDevice		 
         - Tools
		 - GUID Generator application
Supported OS
***************

       + Windows 98SE, 2000, XP, Vista, Seven , 8, 8.1, 10  (x86 & x64 Windows platforms).

How to use 
***************

       1- Uninstall previous versions (Start-> Settings-> Control Panel-> Add or remove programs)

       2- run DfuSe setup.

       3- Install your device with the driver and the inf file, go to [Driver] directory
          
       4- Use it !


******************* (C) COPYRIGHT 2018 STMicroelectronics *****END OF FILE******


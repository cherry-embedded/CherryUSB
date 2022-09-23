#pragma once



#define     __IM     volatile const      /*! Defines 'read only' structure member permissions */
#define     __OM     volatile            /*! Defines 'write only' structure member permissions */
#define     __IOM    volatile            /*! Defines 'read / write' structure member permissions */

/**
  * @brief USBD_HALTED [HALTED] (Unspecified)
  */
typedef struct {
  __IM  uint32_t  EPIN[8];                      /*!< (@ 0x00000000) Description collection: IN endpoint halted status.
                                                                    Can be used as is as response to a GetStatus()
                                                                    request to endpoint.                                       */
  __IM  uint32_t  RESERVED;
  __IM  uint32_t  EPOUT[8];                     /*!< (@ 0x00000024) Description collection: OUT endpoint halted status.
                                                                    Can be used as is as response to a GetStatus()
                                                                    request to endpoint.                                       */
} USBD_HALTED_Type;                             /*!< Size = 68 (0x44)                                                          */


/**
  * @brief USBD_SIZE [SIZE] (Unspecified)
  */
typedef struct {
  __IOM uint32_t  EPOUT[8];                     /*!< (@ 0x00000000) Description collection: Number of bytes received
                                                                    last in the data stage of this OUT endpoint                */
  __IM  uint32_t  ISOOUT;                       /*!< (@ 0x00000020) Number of bytes received last on this ISO OUT
                                                                    data endpoint                                              */
} USBD_SIZE_Type;                               /*!< Size = 36 (0x24)                                                          */


/**
  * @brief USBD_EPIN [EPIN] (Unspecified)
  */
typedef struct {
  __IOM uint32_t  PTR;                          /*!< (@ 0x00000000) Description cluster: Data pointer                          */
  __IOM uint32_t  MAXCNT;                       /*!< (@ 0x00000004) Description cluster: Maximum number of bytes
                                                                    to transfer                                                */
  __IM  uint32_t  AMOUNT;                       /*!< (@ 0x00000008) Description cluster: Number of bytes transferred
                                                                    in the last transaction                                    */
  __IM  uint32_t  RESERVED[2];
} USBD_EPIN_Type;                               /*!< Size = 20 (0x14)                                                          */


/**
  * @brief USBD_ISOIN [ISOIN] (Unspecified)
  */
typedef struct {
  __IOM uint32_t  PTR;                          /*!< (@ 0x00000000) Data pointer                                               */
  __IOM uint32_t  MAXCNT;                       /*!< (@ 0x00000004) Maximum number of bytes to transfer                        */
  __IM  uint32_t  AMOUNT;                       /*!< (@ 0x00000008) Number of bytes transferred in the last transaction        */
} USBD_ISOIN_Type;                              /*!< Size = 12 (0xc)                                                           */


/**
  * @brief USBD_EPOUT [EPOUT] (Unspecified)
  */
typedef struct {
  __IOM uint32_t  PTR;                          /*!< (@ 0x00000000) Description cluster: Data pointer                          */
  __IOM uint32_t  MAXCNT;                       /*!< (@ 0x00000004) Description cluster: Maximum number of bytes
                                                                    to transfer                                                */
  __IM  uint32_t  AMOUNT;                       /*!< (@ 0x00000008) Description cluster: Number of bytes transferred
                                                                    in the last transaction                                    */
  __IM  uint32_t  RESERVED[2];
} USBD_EPOUT_Type;                              /*!< Size = 20 (0x14)                                                          */


/**
  * @brief USBD_ISOOUT [ISOOUT] (Unspecified)
  */
typedef struct {
  __IOM uint32_t  PTR;                          /*!< (@ 0x00000000) Data pointer                                               */
  __IOM uint32_t  MAXCNT;                       /*!< (@ 0x00000004) Maximum number of bytes to transfer                        */
  __IM  uint32_t  AMOUNT;                       /*!< (@ 0x00000008) Number of bytes transferred in the last transaction        */
} USBD_ISOOUT_Type;                             /*!< Size = 12 (0xc)                                                           */

typedef struct {                                /*!< (@ 0x40027000) USBD Structure                                             */
  __IM  uint32_t  RESERVED;
  __OM  uint32_t  TASKS_STARTEPIN[8];           /*!< (@ 0x00000004) Description collection: Captures the EPIN[n].PTR
                                                                    and EPIN[n].MAXCNT registers values, and
                                                                    enables endpoint IN n to respond to traffic
                                                                    from host                                                  */
  __OM  uint32_t  TASKS_STARTISOIN;             /*!< (@ 0x00000024) Captures the ISOIN.PTR and ISOIN.MAXCNT registers
                                                                    values, and enables sending data on ISO
                                                                    endpoint                                                   */
  __OM  uint32_t  TASKS_STARTEPOUT[8];          /*!< (@ 0x00000028) Description collection: Captures the EPOUT[n].PTR
                                                                    and EPOUT[n].MAXCNT registers values, and
                                                                    enables endpoint n to respond to traffic
                                                                    from host                                                  */
  __OM  uint32_t  TASKS_STARTISOOUT;            /*!< (@ 0x00000048) Captures the ISOOUT.PTR and ISOOUT.MAXCNT registers
                                                                    values, and enables receiving of data on
                                                                    ISO endpoint                                               */
  __OM  uint32_t  TASKS_EP0RCVOUT;              /*!< (@ 0x0000004C) Allows OUT data stage on control endpoint 0                */
  __OM  uint32_t  TASKS_EP0STATUS;              /*!< (@ 0x00000050) Allows status stage on control endpoint 0                  */
  __OM  uint32_t  TASKS_EP0STALL;               /*!< (@ 0x00000054) Stalls data and status stage on control endpoint
                                                                    0                                                          */
  __OM  uint32_t  TASKS_DPDMDRIVE;              /*!< (@ 0x00000058) Forces D+ and D- lines into the state defined
                                                                    in the DPDMVALUE register                                  */
  __OM  uint32_t  TASKS_DPDMNODRIVE;            /*!< (@ 0x0000005C) Stops forcing D+ and D- lines into any state
                                                                    (USB engine takes control)                                 */
  __IM  uint32_t  RESERVED1[40];
  __IOM uint32_t  EVENTS_USBRESET;              /*!< (@ 0x00000100) Signals that a USB reset condition has been detected
                                                                    on USB lines                                               */
  __IOM uint32_t  EVENTS_STARTED;               /*!< (@ 0x00000104) Confirms that the EPIN[n].PTR and EPIN[n].MAXCNT,
                                                                    or EPOUT[n].PTR and EPOUT[n].MAXCNT registers
                                                                    have been captured on all endpoints reported
                                                                    in the EPSTATUS register                                   */
  __IOM uint32_t  EVENTS_ENDEPIN[8];            /*!< (@ 0x00000108) Description collection: The whole EPIN[n] buffer
                                                                    has been consumed. The RAM buffer can be
                                                                    accessed safely by software.                               */
  __IOM uint32_t  EVENTS_EP0DATADONE;           /*!< (@ 0x00000128) An acknowledged data transfer has taken place
                                                                    on the control endpoint                                    */
  __IOM uint32_t  EVENTS_ENDISOIN;              /*!< (@ 0x0000012C) The whole ISOIN buffer has been consumed. The
                                                                    RAM buffer can be accessed safely by software.             */
  __IOM uint32_t  EVENTS_ENDEPOUT[8];           /*!< (@ 0x00000130) Description collection: The whole EPOUT[n] buffer
                                                                    has been consumed. The RAM buffer can be
                                                                    accessed safely by software.                               */
  __IOM uint32_t  EVENTS_ENDISOOUT;             /*!< (@ 0x00000150) The whole ISOOUT buffer has been consumed. The
                                                                    RAM buffer can be accessed safely by software.             */
  __IOM uint32_t  EVENTS_SOF;                   /*!< (@ 0x00000154) Signals that a SOF (start of frame) condition
                                                                    has been detected on USB lines                             */
  __IOM uint32_t  EVENTS_USBEVENT;              /*!< (@ 0x00000158) An event or an error not covered by specific
                                                                    events has occurred. Check EVENTCAUSE register
                                                                    to find the cause.                                         */
  __IOM uint32_t  EVENTS_EP0SETUP;              /*!< (@ 0x0000015C) A valid SETUP token has been received (and acknowledged)
                                                                    on the control endpoint                                    */
  __IOM uint32_t  EVENTS_EPDATA;                /*!< (@ 0x00000160) A data transfer has occurred on a data endpoint,
                                                                    indicated by the EPDATASTATUS register                     */
  __IM  uint32_t  RESERVED2[39];
  __IOM uint32_t  SHORTS;                       /*!< (@ 0x00000200) Shortcuts between local events and tasks                   */
  __IM  uint32_t  RESERVED3[63];
  __IOM uint32_t  INTEN;                        /*!< (@ 0x00000300) Enable or disable interrupt                                */
  __IOM uint32_t  INTENSET;                     /*!< (@ 0x00000304) Enable interrupt                                           */
  __IOM uint32_t  INTENCLR;                     /*!< (@ 0x00000308) Disable interrupt                                          */
  __IM  uint32_t  RESERVED4[61];
  __IOM uint32_t  EVENTCAUSE;                   /*!< (@ 0x00000400) Details on what caused the USBEVENT event                  */
  __IM  uint32_t  RESERVED5[7];
  __IOM USBD_HALTED_Type HALTED;                /*!< (@ 0x00000420) Unspecified                                                */
  __IM  uint32_t  RESERVED6;
  __IOM uint32_t  EPSTATUS;                     /*!< (@ 0x00000468) Provides information on which endpoint's EasyDMA
                                                                    registers have been captured                               */
  __IOM uint32_t  EPDATASTATUS;                 /*!< (@ 0x0000046C) Provides information on which endpoint(s) an
                                                                    acknowledged data transfer has occurred
                                                                    (EPDATA event)                                             */
  __IM  uint32_t  USBADDR;                      /*!< (@ 0x00000470) Device USB address                                         */
  __IM  uint32_t  RESERVED7[3];
  __IM  uint32_t  BMREQUESTTYPE;                /*!< (@ 0x00000480) SETUP data, byte 0, bmRequestType                          */
  __IM  uint32_t  BREQUEST;                     /*!< (@ 0x00000484) SETUP data, byte 1, bRequest                               */
  __IM  uint32_t  WVALUEL;                      /*!< (@ 0x00000488) SETUP data, byte 2, LSB of wValue                          */
  __IM  uint32_t  WVALUEH;                      /*!< (@ 0x0000048C) SETUP data, byte 3, MSB of wValue                          */
  __IM  uint32_t  WINDEXL;                      /*!< (@ 0x00000490) SETUP data, byte 4, LSB of wIndex                          */
  __IM  uint32_t  WINDEXH;                      /*!< (@ 0x00000494) SETUP data, byte 5, MSB of wIndex                          */
  __IM  uint32_t  WLENGTHL;                     /*!< (@ 0x00000498) SETUP data, byte 6, LSB of wLength                         */
  __IM  uint32_t  WLENGTHH;                     /*!< (@ 0x0000049C) SETUP data, byte 7, MSB of wLength                         */
  __IOM USBD_SIZE_Type SIZE;                    /*!< (@ 0x000004A0) Unspecified                                                */
  __IM  uint32_t  RESERVED8[15];
  __IOM uint32_t  ENABLE;                       /*!< (@ 0x00000500) Enable USB                                                 */
  __IOM uint32_t  USBPULLUP;                    /*!< (@ 0x00000504) Control of the USB pull-up                                 */
  __IOM uint32_t  DPDMVALUE;                    /*!< (@ 0x00000508) State D+ and D- lines will be forced into by
                                                                    the DPDMDRIVE task. The DPDMNODRIVE task
                                                                    reverts the control of the lines to MAC
                                                                    IP (no forcing).                                           */
  __IOM uint32_t  DTOGGLE;                      /*!< (@ 0x0000050C) Data toggle control and status                             */
  __IOM uint32_t  EPINEN;                       /*!< (@ 0x00000510) Endpoint IN enable                                         */
  __IOM uint32_t  EPOUTEN;                      /*!< (@ 0x00000514) Endpoint OUT enable                                        */
  __OM  uint32_t  EPSTALL;                      /*!< (@ 0x00000518) STALL endpoints                                            */
  __IOM uint32_t  ISOSPLIT;                     /*!< (@ 0x0000051C) Controls the split of ISO buffers                          */
  __IM  uint32_t  FRAMECNTR;                    /*!< (@ 0x00000520) Returns the current value of the start of frame
                                                                    counter                                                    */
  __IM  uint32_t  RESERVED9[2];
  __IOM uint32_t  LOWPOWER;                     /*!< (@ 0x0000052C) Controls USBD peripheral low power mode during
                                                                    USB suspend                                                */
  __IOM uint32_t  ISOINCONFIG;                  /*!< (@ 0x00000530) Controls the response of the ISO IN endpoint
                                                                    to an IN token when no data is ready to
                                                                    be sent                                                    */
  __IM  uint32_t  RESERVED10[51];
  __IOM USBD_EPIN_Type EPIN[8];                 /*!< (@ 0x00000600) Unspecified                                                */
  __IOM USBD_ISOIN_Type ISOIN;                  /*!< (@ 0x000006A0) Unspecified                                                */
  __IM  uint32_t  RESERVED11[21];
  __IOM USBD_EPOUT_Type EPOUT[8];               /*!< (@ 0x00000700) Unspecified                                                */
  __IOM USBD_ISOOUT_Type ISOOUT;                /*!< (@ 0x000007A0) Unspecified                                                */
} NRF_USBD_Type;                                /*!< Size = 1964 (0x7ac)                                                       */


/* Register: USBD_EPINEN */
/* Description: Endpoint IN enable */

/* Bit 8 : Enable ISO IN endpoint */
#define USBD_EPINEN_ISOIN_Pos (8UL)                            /*!< Position of ISOIN field. */
#define USBD_EPINEN_ISOIN_Msk (0x1UL << USBD_EPINEN_ISOIN_Pos) /*!< Bit mask of ISOIN field. */
#define USBD_EPINEN_ISOIN_Disable (0UL)                        /*!< Disable ISO IN endpoint 8 */
#define USBD_EPINEN_ISOIN_Enable (1UL)                         /*!< Enable ISO IN endpoint 8 */

/* Bit 7 : Enable IN endpoint 7 */
#define USBD_EPINEN_IN7_Pos (7UL)                          /*!< Position of IN7 field. */
#define USBD_EPINEN_IN7_Msk (0x1UL << USBD_EPINEN_IN7_Pos) /*!< Bit mask of IN7 field. */
#define USBD_EPINEN_IN7_Disable (0UL)                      /*!< Disable IN endpoint 7 */
#define USBD_EPINEN_IN7_Enable (1UL)                       /*!< Enable IN endpoint 7 */

/* Bit 6 : Enable IN endpoint 6 */
#define USBD_EPINEN_IN6_Pos (6UL)                          /*!< Position of IN6 field. */
#define USBD_EPINEN_IN6_Msk (0x1UL << USBD_EPINEN_IN6_Pos) /*!< Bit mask of IN6 field. */
#define USBD_EPINEN_IN6_Disable (0UL)                      /*!< Disable endpoint IN 6 (no response to IN tokens) */
#define USBD_EPINEN_IN6_Enable (1UL)                       /*!< Enable endpoint IN 6 (response to IN tokens) */

/* Bit 5 : Enable IN endpoint 5 */
#define USBD_EPINEN_IN5_Pos (5UL)                          /*!< Position of IN5 field. */
#define USBD_EPINEN_IN5_Msk (0x1UL << USBD_EPINEN_IN5_Pos) /*!< Bit mask of IN5 field. */
#define USBD_EPINEN_IN5_Disable (0UL)                      /*!< Disable endpoint IN 5 (no response to IN tokens) */
#define USBD_EPINEN_IN5_Enable (1UL)                       /*!< Enable endpoint IN 5 (response to IN tokens) */

/* Bit 4 : Enable IN endpoint 4 */
#define USBD_EPINEN_IN4_Pos (4UL)                          /*!< Position of IN4 field. */
#define USBD_EPINEN_IN4_Msk (0x1UL << USBD_EPINEN_IN4_Pos) /*!< Bit mask of IN4 field. */
#define USBD_EPINEN_IN4_Disable (0UL)                      /*!< Disable endpoint IN 4 (no response to IN tokens) */
#define USBD_EPINEN_IN4_Enable (1UL)                       /*!< Enable endpoint IN 4 (response to IN tokens) */

/* Bit 3 : Enable IN endpoint 3 */
#define USBD_EPINEN_IN3_Pos (3UL)                          /*!< Position of IN3 field. */
#define USBD_EPINEN_IN3_Msk (0x1UL << USBD_EPINEN_IN3_Pos) /*!< Bit mask of IN3 field. */
#define USBD_EPINEN_IN3_Disable (0UL)                      /*!< Disable endpoint IN 3 (no response to IN tokens) */
#define USBD_EPINEN_IN3_Enable (1UL)                       /*!< Enable endpoint IN 3 (response to IN tokens) */

/* Bit 2 : Enable IN endpoint 2 */
#define USBD_EPINEN_IN2_Pos (2UL)                          /*!< Position of IN2 field. */
#define USBD_EPINEN_IN2_Msk (0x1UL << USBD_EPINEN_IN2_Pos) /*!< Bit mask of IN2 field. */
#define USBD_EPINEN_IN2_Disable (0UL)                      /*!< Disable endpoint IN 2 (no response to IN tokens) */
#define USBD_EPINEN_IN2_Enable (1UL)                       /*!< Enable endpoint IN 2 (response to IN tokens) */

/* Bit 1 : Enable IN endpoint 1 */
#define USBD_EPINEN_IN1_Pos (1UL)                          /*!< Position of IN1 field. */
#define USBD_EPINEN_IN1_Msk (0x1UL << USBD_EPINEN_IN1_Pos) /*!< Bit mask of IN1 field. */
#define USBD_EPINEN_IN1_Disable (0UL)                      /*!< Disable endpoint IN 1 (no response to IN tokens) */
#define USBD_EPINEN_IN1_Enable (1UL)                       /*!< Enable endpoint IN 1 (response to IN tokens) */

/* Bit 0 : Enable IN endpoint 0 */
#define USBD_EPINEN_IN0_Pos (0UL)                          /*!< Position of IN0 field. */
#define USBD_EPINEN_IN0_Msk (0x1UL << USBD_EPINEN_IN0_Pos) /*!< Bit mask of IN0 field. */
#define USBD_EPINEN_IN0_Disable (0UL)                      /*!< Disable endpoint IN 0 (no response to IN tokens) */
#define USBD_EPINEN_IN0_Enable (1UL)                       /*!< Enable endpoint IN 0 (response to IN tokens) */

/* Register: USBD_EPOUTEN */
/* Description: Endpoint OUT enable */

/* Bit 8 : Enable ISO OUT endpoint 8 */
#define USBD_EPOUTEN_ISOOUT_Pos (8UL)                              /*!< Position of ISOOUT field. */
#define USBD_EPOUTEN_ISOOUT_Msk (0x1UL << USBD_EPOUTEN_ISOOUT_Pos) /*!< Bit mask of ISOOUT field. */
#define USBD_EPOUTEN_ISOOUT_Disable (0UL)                          /*!< Disable ISO OUT endpoint 8 */
#define USBD_EPOUTEN_ISOOUT_Enable (1UL)                           /*!< Enable ISO OUT endpoint 8 */

/* Bit 7 : Enable OUT endpoint 7 */
#define USBD_EPOUTEN_OUT7_Pos (7UL)                            /*!< Position of OUT7 field. */
#define USBD_EPOUTEN_OUT7_Msk (0x1UL << USBD_EPOUTEN_OUT7_Pos) /*!< Bit mask of OUT7 field. */
#define USBD_EPOUTEN_OUT7_Disable (0UL)                        /*!< Disable endpoint OUT 7 (no response to OUT tokens) */
#define USBD_EPOUTEN_OUT7_Enable (1UL)                         /*!< Enable endpoint OUT 7 (response to OUT tokens) */

/* Bit 6 : Enable OUT endpoint 6 */
#define USBD_EPOUTEN_OUT6_Pos (6UL)                            /*!< Position of OUT6 field. */
#define USBD_EPOUTEN_OUT6_Msk (0x1UL << USBD_EPOUTEN_OUT6_Pos) /*!< Bit mask of OUT6 field. */
#define USBD_EPOUTEN_OUT6_Disable (0UL)                        /*!< Disable endpoint OUT 6 (no response to OUT tokens) */
#define USBD_EPOUTEN_OUT6_Enable (1UL)                         /*!< Enable endpoint OUT 6 (response to OUT tokens) */

/* Bit 5 : Enable OUT endpoint 5 */
#define USBD_EPOUTEN_OUT5_Pos (5UL)                            /*!< Position of OUT5 field. */
#define USBD_EPOUTEN_OUT5_Msk (0x1UL << USBD_EPOUTEN_OUT5_Pos) /*!< Bit mask of OUT5 field. */
#define USBD_EPOUTEN_OUT5_Disable (0UL)                        /*!< Disable endpoint OUT 5 (no response to OUT tokens) */
#define USBD_EPOUTEN_OUT5_Enable (1UL)                         /*!< Enable endpoint OUT 5 (response to OUT tokens) */

/* Bit 4 : Enable OUT endpoint 4 */
#define USBD_EPOUTEN_OUT4_Pos (4UL)                            /*!< Position of OUT4 field. */
#define USBD_EPOUTEN_OUT4_Msk (0x1UL << USBD_EPOUTEN_OUT4_Pos) /*!< Bit mask of OUT4 field. */
#define USBD_EPOUTEN_OUT4_Disable (0UL)                        /*!< Disable endpoint OUT 4 (no response to OUT tokens) */
#define USBD_EPOUTEN_OUT4_Enable (1UL)                         /*!< Enable endpoint OUT 4 (response to OUT tokens) */

/* Bit 3 : Enable OUT endpoint 3 */
#define USBD_EPOUTEN_OUT3_Pos (3UL)                            /*!< Position of OUT3 field. */
#define USBD_EPOUTEN_OUT3_Msk (0x1UL << USBD_EPOUTEN_OUT3_Pos) /*!< Bit mask of OUT3 field. */
#define USBD_EPOUTEN_OUT3_Disable (0UL)                        /*!< Disable endpoint OUT 3 (no response to OUT tokens) */
#define USBD_EPOUTEN_OUT3_Enable (1UL)                         /*!< Enable endpoint OUT 3 (response to OUT tokens) */

/* Bit 2 : Enable OUT endpoint 2 */
#define USBD_EPOUTEN_OUT2_Pos (2UL)                            /*!< Position of OUT2 field. */
#define USBD_EPOUTEN_OUT2_Msk (0x1UL << USBD_EPOUTEN_OUT2_Pos) /*!< Bit mask of OUT2 field. */
#define USBD_EPOUTEN_OUT2_Disable (0UL)                        /*!< Disable endpoint OUT 2 (no response to OUT tokens) */
#define USBD_EPOUTEN_OUT2_Enable (1UL)                         /*!< Enable endpoint OUT 2 (response to OUT tokens) */

/* Bit 1 : Enable OUT endpoint 1 */
#define USBD_EPOUTEN_OUT1_Pos (1UL)                            /*!< Position of OUT1 field. */
#define USBD_EPOUTEN_OUT1_Msk (0x1UL << USBD_EPOUTEN_OUT1_Pos) /*!< Bit mask of OUT1 field. */
#define USBD_EPOUTEN_OUT1_Disable (0UL)                        /*!< Disable endpoint OUT 1 (no response to OUT tokens) */
#define USBD_EPOUTEN_OUT1_Enable (1UL)                         /*!< Enable endpoint OUT 1 (response to OUT tokens) */

/* Bit 0 : Enable OUT endpoint 0 */
#define USBD_EPOUTEN_OUT0_Pos (0UL)                            /*!< Position of OUT0 field. */
#define USBD_EPOUTEN_OUT0_Msk (0x1UL << USBD_EPOUTEN_OUT0_Pos) /*!< Bit mask of OUT0 field. */
#define USBD_EPOUTEN_OUT0_Disable (0UL)                        /*!< Disable endpoint OUT 0 (no response to OUT tokens) */
#define USBD_EPOUTEN_OUT0_Enable (1UL)                         /*!< Enable endpoint OUT 0 (response to OUT tokens) */

/* Register: USBD_INTEN */
/* Description: Enable or disable interrupt */

/* Bit 24 : Enable or disable interrupt for event EPDATA */
#define USBD_INTEN_EPDATA_Pos (24UL)                           /*!< Position of EPDATA field. */
#define USBD_INTEN_EPDATA_Msk (0x1UL << USBD_INTEN_EPDATA_Pos) /*!< Bit mask of EPDATA field. */
#define USBD_INTEN_EPDATA_Disabled (0UL)                       /*!< Disable */
#define USBD_INTEN_EPDATA_Enabled (1UL)                        /*!< Enable */

/* Bit 23 : Enable or disable interrupt for event EP0SETUP */
#define USBD_INTEN_EP0SETUP_Pos (23UL)                             /*!< Position of EP0SETUP field. */
#define USBD_INTEN_EP0SETUP_Msk (0x1UL << USBD_INTEN_EP0SETUP_Pos) /*!< Bit mask of EP0SETUP field. */
#define USBD_INTEN_EP0SETUP_Disabled (0UL)                         /*!< Disable */
#define USBD_INTEN_EP0SETUP_Enabled (1UL)                          /*!< Enable */

/* Bit 22 : Enable or disable interrupt for event USBEVENT */
#define USBD_INTEN_USBEVENT_Pos (22UL)                             /*!< Position of USBEVENT field. */
#define USBD_INTEN_USBEVENT_Msk (0x1UL << USBD_INTEN_USBEVENT_Pos) /*!< Bit mask of USBEVENT field. */
#define USBD_INTEN_USBEVENT_Disabled (0UL)                         /*!< Disable */
#define USBD_INTEN_USBEVENT_Enabled (1UL)                          /*!< Enable */

/* Bit 21 : Enable or disable interrupt for event SOF */
#define USBD_INTEN_SOF_Pos (21UL)                        /*!< Position of SOF field. */
#define USBD_INTEN_SOF_Msk (0x1UL << USBD_INTEN_SOF_Pos) /*!< Bit mask of SOF field. */
#define USBD_INTEN_SOF_Disabled (0UL)                    /*!< Disable */
#define USBD_INTEN_SOF_Enabled (1UL)                     /*!< Enable */

/* Bit 20 : Enable or disable interrupt for event ENDISOOUT */
#define USBD_INTEN_ENDISOOUT_Pos (20UL)                              /*!< Position of ENDISOOUT field. */
#define USBD_INTEN_ENDISOOUT_Msk (0x1UL << USBD_INTEN_ENDISOOUT_Pos) /*!< Bit mask of ENDISOOUT field. */
#define USBD_INTEN_ENDISOOUT_Disabled (0UL)                          /*!< Disable */
#define USBD_INTEN_ENDISOOUT_Enabled (1UL)                           /*!< Enable */

/* Bit 19 : Enable or disable interrupt for event ENDEPOUT[7] */
#define USBD_INTEN_ENDEPOUT7_Pos (19UL)                              /*!< Position of ENDEPOUT7 field. */
#define USBD_INTEN_ENDEPOUT7_Msk (0x1UL << USBD_INTEN_ENDEPOUT7_Pos) /*!< Bit mask of ENDEPOUT7 field. */
#define USBD_INTEN_ENDEPOUT7_Disabled (0UL)                          /*!< Disable */
#define USBD_INTEN_ENDEPOUT7_Enabled (1UL)                           /*!< Enable */

/* Bit 13 : Enable or disable interrupt for event ENDEPOUT[1] */
#define USBD_INTEN_ENDEPOUT1_Pos (13UL)                              /*!< Position of ENDEPOUT1 field. */
#define USBD_INTEN_ENDEPOUT1_Msk (0x1UL << USBD_INTEN_ENDEPOUT1_Pos) /*!< Bit mask of ENDEPOUT1 field. */
#define USBD_INTEN_ENDEPOUT1_Disabled (0UL)                          /*!< Disable */
#define USBD_INTEN_ENDEPOUT1_Enabled (1UL)                           /*!< Enable */

/* Bit 12 : Enable or disable interrupt for event ENDEPOUT[0] */
#define USBD_INTEN_ENDEPOUT0_Pos (12UL)                              /*!< Position of ENDEPOUT0 field. */
#define USBD_INTEN_ENDEPOUT0_Msk (0x1UL << USBD_INTEN_ENDEPOUT0_Pos) /*!< Bit mask of ENDEPOUT0 field. */
#define USBD_INTEN_ENDEPOUT0_Disabled (0UL)                          /*!< Disable */
#define USBD_INTEN_ENDEPOUT0_Enabled (1UL)                           /*!< Enable */

/* Bit 11 : Enable or disable interrupt for event ENDISOIN */
#define USBD_INTEN_ENDISOIN_Pos (11UL)                             /*!< Position of ENDISOIN field. */
#define USBD_INTEN_ENDISOIN_Msk (0x1UL << USBD_INTEN_ENDISOIN_Pos) /*!< Bit mask of ENDISOIN field. */
#define USBD_INTEN_ENDISOIN_Disabled (0UL)                         /*!< Disable */
#define USBD_INTEN_ENDISOIN_Enabled (1UL)                          /*!< Enable */

/* Bit 10 : Enable or disable interrupt for event EP0DATADONE */
#define USBD_INTEN_EP0DATADONE_Pos (10UL)                                /*!< Position of EP0DATADONE field. */
#define USBD_INTEN_EP0DATADONE_Msk (0x1UL << USBD_INTEN_EP0DATADONE_Pos) /*!< Bit mask of EP0DATADONE field. */
#define USBD_INTEN_EP0DATADONE_Disabled (0UL)                            /*!< Disable */
#define USBD_INTEN_EP0DATADONE_Enabled (1UL)                             /*!< Enable */

/* Bit 3 : Enable or disable interrupt for event ENDEPIN[1] */
#define USBD_INTEN_ENDEPIN1_Pos (3UL)                              /*!< Position of ENDEPIN1 field. */
#define USBD_INTEN_ENDEPIN1_Msk (0x1UL << USBD_INTEN_ENDEPIN1_Pos) /*!< Bit mask of ENDEPIN1 field. */
#define USBD_INTEN_ENDEPIN1_Disabled (0UL)                         /*!< Disable */
#define USBD_INTEN_ENDEPIN1_Enabled (1UL)                          /*!< Enable */

/* Bit 2 : Enable or disable interrupt for event ENDEPIN[0] */
#define USBD_INTEN_ENDEPIN0_Pos (2UL)                              /*!< Position of ENDEPIN0 field. */
#define USBD_INTEN_ENDEPIN0_Msk (0x1UL << USBD_INTEN_ENDEPIN0_Pos) /*!< Bit mask of ENDEPIN0 field. */
#define USBD_INTEN_ENDEPIN0_Disabled (0UL)                         /*!< Disable */
#define USBD_INTEN_ENDEPIN0_Enabled (1UL)                          /*!< Enable */

/* Bit 1 : Enable or disable interrupt for event STARTED */
#define USBD_INTEN_STARTED_Pos (1UL)                             /*!< Position of STARTED field. */
#define USBD_INTEN_STARTED_Msk (0x1UL << USBD_INTEN_STARTED_Pos) /*!< Bit mask of STARTED field. */
#define USBD_INTEN_STARTED_Disabled (0UL)                        /*!< Disable */
#define USBD_INTEN_STARTED_Enabled (1UL)                         /*!< Enable */

/* Bit 0 : Enable or disable interrupt for event USBRESET */
#define USBD_INTEN_USBRESET_Pos (0UL)                              /*!< Position of USBRESET field. */
#define USBD_INTEN_USBRESET_Msk (0x1UL << USBD_INTEN_USBRESET_Pos) /*!< Bit mask of USBRESET field. */
#define USBD_INTEN_USBRESET_Disabled (0UL)                         /*!< Disable */
#define USBD_INTEN_USBRESET_Enabled (1UL)                          /*!< Enable */

/* Register: USBD_INTENSET */
/* Description: Enable interrupt */

/* Bit 21 : Write '1' to enable interrupt for event SOF */
#define USBD_INTENSET_SOF_Pos (21UL)                           /*!< Position of SOF field. */
#define USBD_INTENSET_SOF_Msk (0x1UL << USBD_INTENSET_SOF_Pos) /*!< Bit mask of SOF field. */
#define USBD_INTENSET_SOF_Disabled (0UL)                       /*!< Read: Disabled */
#define USBD_INTENSET_SOF_Enabled (1UL)                        /*!< Read: Enabled */
#define USBD_INTENSET_SOF_Set (1UL)                            /*!< Enable */

/* Bit 20 : Write '1' to enable interrupt for event ENDISOOUT */
#define USBD_INTENSET_ENDISOOUT_Pos (20UL)                                 /*!< Position of ENDISOOUT field. */
#define USBD_INTENSET_ENDISOOUT_Msk (0x1UL << USBD_INTENSET_ENDISOOUT_Pos) /*!< Bit mask of ENDISOOUT field. */
#define USBD_INTENSET_ENDISOOUT_Disabled (0UL)                             /*!< Read: Disabled */
#define USBD_INTENSET_ENDISOOUT_Enabled (1UL)                              /*!< Read: Enabled */
#define USBD_INTENSET_ENDISOOUT_Set (1UL)                                  /*!< Enable */

/* Bit 11 : Write '1' to enable interrupt for event ENDISOIN */
#define USBD_INTENSET_ENDISOIN_Pos (11UL)                                /*!< Position of ENDISOIN field. */
#define USBD_INTENSET_ENDISOIN_Msk (0x1UL << USBD_INTENSET_ENDISOIN_Pos) /*!< Bit mask of ENDISOIN field. */
#define USBD_INTENSET_ENDISOIN_Disabled (0UL)                            /*!< Read: Disabled */
#define USBD_INTENSET_ENDISOIN_Enabled (1UL)                             /*!< Read: Enabled */
#define USBD_INTENSET_ENDISOIN_Set (1UL)                                 /*!< Enable */

/* Register: USBD_INTENCLR */
/* Description: Disable interrupt */

/* Bit 21 : Write '1' to disable interrupt for event SOF */
#define USBD_INTENCLR_SOF_Pos (21UL)                           /*!< Position of SOF field. */
#define USBD_INTENCLR_SOF_Msk (0x1UL << USBD_INTENCLR_SOF_Pos) /*!< Bit mask of SOF field. */
#define USBD_INTENCLR_SOF_Disabled (0UL)                       /*!< Read: Disabled */
#define USBD_INTENCLR_SOF_Enabled (1UL)                        /*!< Read: Enabled */
#define USBD_INTENCLR_SOF_Clear (1UL)                          /*!< Disable */

/* Bit 20 : Write '1' to disable interrupt for event ENDISOOUT */
#define USBD_INTENCLR_ENDISOOUT_Pos (20UL)                                 /*!< Position of ENDISOOUT field. */
#define USBD_INTENCLR_ENDISOOUT_Msk (0x1UL << USBD_INTENCLR_ENDISOOUT_Pos) /*!< Bit mask of ENDISOOUT field. */
#define USBD_INTENCLR_ENDISOOUT_Disabled (0UL)                             /*!< Read: Disabled */
#define USBD_INTENCLR_ENDISOOUT_Enabled (1UL)                              /*!< Read: Enabled */
#define USBD_INTENCLR_ENDISOOUT_Clear (1UL)                                /*!< Disable */

/* Bit 11 : Write '1' to disable interrupt for event ENDISOIN */
#define USBD_INTENCLR_ENDISOIN_Pos (11UL)                                /*!< Position of ENDISOIN field. */
#define USBD_INTENCLR_ENDISOIN_Msk (0x1UL << USBD_INTENCLR_ENDISOIN_Pos) /*!< Bit mask of ENDISOIN field. */
#define USBD_INTENCLR_ENDISOIN_Disabled (0UL)                            /*!< Read: Disabled */
#define USBD_INTENCLR_ENDISOIN_Enabled (1UL)                             /*!< Read: Enabled */
#define USBD_INTENCLR_ENDISOIN_Clear (1UL)                               /*!< Disable */

/* Register: USBD_ISOSPLIT */
/* Description: Controls the split of ISO buffers */

/* Bits 15..0 : Controls the split of ISO buffers */
#define USBD_ISOSPLIT_SPLIT_Pos (0UL)                                 /*!< Position of SPLIT field. */
#define USBD_ISOSPLIT_SPLIT_Msk (0xFFFFUL << USBD_ISOSPLIT_SPLIT_Pos) /*!< Bit mask of SPLIT field. */
#define USBD_ISOSPLIT_SPLIT_OneDir (0x0000UL)                         /*!< Full buffer dedicated to either iso IN or OUT */
#define USBD_ISOSPLIT_SPLIT_HalfIN (0x0080UL)                         /*!< Lower half for IN, upper half for OUT */

/* Register: USBD_EPSTALL */
/* Description: STALL endpoints */

/* Bit 8 : Stall selected endpoint */
#define USBD_EPSTALL_STALL_Pos (8UL)                             /*!< Position of STALL field. */
#define USBD_EPSTALL_STALL_Msk (0x1UL << USBD_EPSTALL_STALL_Pos) /*!< Bit mask of STALL field. */
#define USBD_EPSTALL_STALL_UnStall (0UL)                         /*!< Don't stall selected endpoint */
#define USBD_EPSTALL_STALL_Stall (1UL)                           /*!< Stall selected endpoint */

/* Register: USBD_DPDMVALUE */
/* Description: State D+ and D- lines will be forced into by the DPDMDRIVE task. The DPDMNODRIVE task reverts the control of the lines to MAC IP (no forcing). */

/* Bits 4..0 : State D+ and D- lines will be forced into by the DPDMDRIVE task */
#define USBD_DPDMVALUE_STATE_Pos (0UL)                                /*!< Position of STATE field. */
#define USBD_DPDMVALUE_STATE_Msk (0x1FUL << USBD_DPDMVALUE_STATE_Pos) /*!< Bit mask of STATE field. */
#define USBD_DPDMVALUE_STATE_Resume (1UL)                             /*!< D+ forced low, D- forced high (K state) for a timing preset in hardware (50 us or 5 ms, depending on bus state) */
#define USBD_DPDMVALUE_STATE_J (2UL)                                  /*!< D+ forced high, D- forced low (J state) */
#define USBD_DPDMVALUE_STATE_K (4UL)                                  /*!< D+ forced low, D- forced high (K state) */

/* Register: USBD_DTOGGLE */
/* Description: Data toggle control and status */

/* Bits 9..8 : Data toggle value */
#define USBD_DTOGGLE_VALUE_Pos (8UL)                             /*!< Position of VALUE field. */
#define USBD_DTOGGLE_VALUE_Msk (0x3UL << USBD_DTOGGLE_VALUE_Pos) /*!< Bit mask of VALUE field. */
#define USBD_DTOGGLE_VALUE_Nop (0UL)                             /*!< No action on data toggle when writing the register with this value */
#define USBD_DTOGGLE_VALUE_Data0 (1UL)                           /*!< Data toggle is DATA0 on endpoint set by EP and IO */
#define USBD_DTOGGLE_VALUE_Data1 (2UL)                           /*!< Data toggle is DATA1 on endpoint set by EP and IO */

/* Register: USBD_EVENTCAUSE */
/* Description: Details on what caused the USBEVENT event */

/* Bit 10 : USB MAC has been woken up and operational. Write '1' to clear. */
#define USBD_EVENTCAUSE_USBWUALLOWED_Pos (10UL)                                      /*!< Position of USBWUALLOWED field. */
#define USBD_EVENTCAUSE_USBWUALLOWED_Msk (0x1UL << USBD_EVENTCAUSE_USBWUALLOWED_Pos) /*!< Bit mask of USBWUALLOWED field. */
#define USBD_EVENTCAUSE_USBWUALLOWED_NotAllowed (0UL)                                /*!< Wake up not allowed */
#define USBD_EVENTCAUSE_USBWUALLOWED_Allowed (1UL)                                   /*!< Wake up allowed */

/* Bit 9 : Signals that a RESUME condition (K state or activity restart) has been detected on USB lines. Write '1' to clear. */
#define USBD_EVENTCAUSE_RESUME_Pos (9UL)                                 /*!< Position of RESUME field. */
#define USBD_EVENTCAUSE_RESUME_Msk (0x1UL << USBD_EVENTCAUSE_RESUME_Pos) /*!< Bit mask of RESUME field. */
#define USBD_EVENTCAUSE_RESUME_NotDetected (0UL)                         /*!< Resume not detected */
#define USBD_EVENTCAUSE_RESUME_Detected (1UL)                            /*!< Resume detected */

/* Bit 8 : Signals that USB lines have been idle long enough for the device to enter suspend. Write '1' to clear. */
#define USBD_EVENTCAUSE_SUSPEND_Pos (8UL)                                  /*!< Position of SUSPEND field. */
#define USBD_EVENTCAUSE_SUSPEND_Msk (0x1UL << USBD_EVENTCAUSE_SUSPEND_Pos) /*!< Bit mask of SUSPEND field. */
#define USBD_EVENTCAUSE_SUSPEND_NotDetected (0UL)                          /*!< Suspend not detected */
#define USBD_EVENTCAUSE_SUSPEND_Detected (1UL)                             /*!< Suspend detected */

/* Register: USBD_SIZE_ISOOUT */
/* Description: Number of bytes received last on this ISO OUT data endpoint */

/* Bit 16 : Zero-length data packet received */
#define USBD_SIZE_ISOOUT_ZERO_Pos (16UL)                               /*!< Position of ZERO field. */
#define USBD_SIZE_ISOOUT_ZERO_Msk (0x1UL << USBD_SIZE_ISOOUT_ZERO_Pos) /*!< Bit mask of ZERO field. */
#define USBD_SIZE_ISOOUT_ZERO_Normal (0UL)                             /*!< No zero-length data received, use value in SIZE */
#define USBD_SIZE_ISOOUT_ZERO_ZeroData (1UL)                           /*!< Zero-length data received, ignore value in SIZE */

/* Register: USBD_EVENTCAUSE */
/* Description: Details on what caused the USBEVENT event */

/* Bit 11 : USB device is ready for normal operation. Write '1' to clear. */
#define USBD_EVENTCAUSE_READY_Pos (11UL)                               /*!< Position of READY field. */
#define USBD_EVENTCAUSE_READY_Msk (0x1UL << USBD_EVENTCAUSE_READY_Pos) /*!< Bit mask of READY field. */
#define USBD_EVENTCAUSE_READY_NotDetected (0UL)                        /*!< USBEVENT was not issued due to USBD peripheral ready */
#define USBD_EVENTCAUSE_READY_Ready (1UL)                              /*!< USBD peripheral is ready */

/* Register: USBD_ISOINCONFIG */
/* Description: Controls the response of the ISO IN endpoint to an IN token when no data is ready to be sent */

/* Bit 0 : Controls the response of the ISO IN endpoint to an IN token when no data is ready to be sent */
#define USBD_ISOINCONFIG_RESPONSE_Pos (0UL)                                    /*!< Position of RESPONSE field. */
#define USBD_ISOINCONFIG_RESPONSE_Msk (0x1UL << USBD_ISOINCONFIG_RESPONSE_Pos) /*!< Bit mask of RESPONSE field. */
#define USBD_ISOINCONFIG_RESPONSE_NoResp (0UL)                                 /*!< Endpoint does not respond in that case */
#define USBD_ISOINCONFIG_RESPONSE_ZeroData (1UL)                               /*!< Endpoint responds with a zero-length data packet in that case */




/**
  * @brief Clock control (CLOCK)
  */

typedef struct {                                /*!< (@ 0x40000000) CLOCK Structure                                            */
  __OM  uint32_t  TASKS_HFCLKSTART;             /*!< (@ 0x00000000) Start HFXO crystal oscillator                              */
  __OM  uint32_t  TASKS_HFCLKSTOP;              /*!< (@ 0x00000004) Stop HFXO crystal oscillator                               */
  __OM  uint32_t  TASKS_LFCLKSTART;             /*!< (@ 0x00000008) Start LFCLK                                                */
  __OM  uint32_t  TASKS_LFCLKSTOP;              /*!< (@ 0x0000000C) Stop LFCLK                                                 */
  __OM  uint32_t  TASKS_CAL;                    /*!< (@ 0x00000010) Start calibration of LFRC                                  */
  __OM  uint32_t  TASKS_CTSTART;                /*!< (@ 0x00000014) Start calibration timer                                    */
  __OM  uint32_t  TASKS_CTSTOP;                 /*!< (@ 0x00000018) Stop calibration timer                                     */
  __IM  uint32_t  RESERVED[57];
  __IOM uint32_t  EVENTS_HFCLKSTARTED;          /*!< (@ 0x00000100) HFXO crystal oscillator started                            */
  __IOM uint32_t  EVENTS_LFCLKSTARTED;          /*!< (@ 0x00000104) LFCLK started                                              */
  __IM  uint32_t  RESERVED1;
  __IOM uint32_t  EVENTS_DONE;                  /*!< (@ 0x0000010C) Calibration of LFRC completed                              */
  __IOM uint32_t  EVENTS_CTTO;                  /*!< (@ 0x00000110) Calibration timer timeout                                  */
  __IM  uint32_t  RESERVED2[5];
  __IOM uint32_t  EVENTS_CTSTARTED;             /*!< (@ 0x00000128) Calibration timer has been started and is ready
                                                                    to process new tasks                                       */
  __IOM uint32_t  EVENTS_CTSTOPPED;             /*!< (@ 0x0000012C) Calibration timer has been stopped and is ready
                                                                    to process new tasks                                       */
  __IM  uint32_t  RESERVED3[117];
  __IOM uint32_t  INTENSET;                     /*!< (@ 0x00000304) Enable interrupt                                           */
  __IOM uint32_t  INTENCLR;                     /*!< (@ 0x00000308) Disable interrupt                                          */
  __IM  uint32_t  RESERVED4[63];
  __IM  uint32_t  HFCLKRUN;                     /*!< (@ 0x00000408) Status indicating that HFCLKSTART task has been
                                                                    triggered                                                  */
  __IM  uint32_t  HFCLKSTAT;                    /*!< (@ 0x0000040C) HFCLK status                                               */
  __IM  uint32_t  RESERVED5;
  __IM  uint32_t  LFCLKRUN;                     /*!< (@ 0x00000414) Status indicating that LFCLKSTART task has been
                                                                    triggered                                                  */
  __IM  uint32_t  LFCLKSTAT;                    /*!< (@ 0x00000418) LFCLK status                                               */
  __IM  uint32_t  LFCLKSRCCOPY;                 /*!< (@ 0x0000041C) Copy of LFCLKSRC register, set when LFCLKSTART
                                                                    task was triggered                                         */
  __IM  uint32_t  RESERVED6[62];
  __IOM uint32_t  LFCLKSRC;                     /*!< (@ 0x00000518) Clock source for the LFCLK                                 */
  __IM  uint32_t  RESERVED7[3];
  __IOM uint32_t  HFXODEBOUNCE;                 /*!< (@ 0x00000528) HFXO debounce time. The HFXO is started by triggering
                                                                    the TASKS_HFCLKSTART task.                                 */
  __IM  uint32_t  RESERVED8[3];
  __IOM uint32_t  CTIV;                         /*!< (@ 0x00000538) Calibration timer interval                                 */
  __IM  uint32_t  RESERVED9[8];
  __IOM uint32_t  TRACECONFIG;                  /*!< (@ 0x0000055C) Clocking options for the trace port debug interface        */
  __IM  uint32_t  RESERVED10[21];
  __IOM uint32_t  LFRCMODE;                     /*!< (@ 0x000005B4) LFRC mode configuration                                    */
} NRF_CLOCK_Type;                               /*!< Size = 1464 (0x5b8)                                                       */


#define CLOCK_HFCLKSTAT_STATE_Pos (16UL) /*!< Position of STATE field. */
#define CLOCK_HFCLKSTAT_STATE_Msk (0x1UL << CLOCK_HFCLKSTAT_STATE_Pos) /*!< Bit mask of STATE field. */
#define CLOCK_HFCLKSTAT_STATE_NotRunning (0UL) /*!< HFCLK not running */
#define CLOCK_HFCLKSTAT_STATE_Running (1UL) /*!< HFCLK running */


/* Bit 0 : Source of HFCLK */
#define CLOCK_HFCLKSTAT_SRC_Pos (0UL) /*!< Position of SRC field. */
#define CLOCK_HFCLKSTAT_SRC_Msk (0x1UL << CLOCK_HFCLKSTAT_SRC_Pos) /*!< Bit mask of SRC field. */
#define CLOCK_HFCLKSTAT_SRC_RC (0UL) /*!< 64 MHz internal oscillator (HFINT) */
#define CLOCK_HFCLKSTAT_SRC_Xtal (1UL) /*!< 64 MHz crystal oscillator (HFXO) */



typedef struct
{
  __IOM uint32_t ISER[8U];               /*!< Offset: 0x000 (R/W)  Interrupt Set Enable Register */
        uint32_t RESERVED0[24U];
  __IOM uint32_t ICER[8U];               /*!< Offset: 0x080 (R/W)  Interrupt Clear Enable Register */
        uint32_t RESERVED1[24U];
  __IOM uint32_t ISPR[8U];               /*!< Offset: 0x100 (R/W)  Interrupt Set Pending Register */
        uint32_t RESERVED2[24U];
  __IOM uint32_t ICPR[8U];               /*!< Offset: 0x180 (R/W)  Interrupt Clear Pending Register */
        uint32_t RESERVED3[24U];
  __IOM uint32_t IABR[8U];               /*!< Offset: 0x200 (R/W)  Interrupt Active bit Register */
        uint32_t RESERVED4[56U];
  __IOM uint8_t  IP[240U];               /*!< Offset: 0x300 (R/W)  Interrupt Priority Register (8Bit wide) */
        uint32_t RESERVED5[644U];
  __OM  uint32_t STIR;                   /*!< Offset: 0xE00 ( /W)  Software Trigger Interrupt Register */
}  NVIC_Type;


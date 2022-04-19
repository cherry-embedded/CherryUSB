#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "usb_dc.h"
#include "usbd_core.h"
#include "./nrf5x_regs.h"

#define __ISB()           \
  do                      \
  {                       \
    __schedule_barrier(); \
    __isb(0xF);           \
    __schedule_barrier(); \
  } while (0U)

#define __DSB()           \
  do                      \
  {                       \
    __schedule_barrier(); \
    __dsb(0xF);           \
    __schedule_barrier(); \
  } while (0U)

#ifndef USBD_IRQHandler
#define USBD_IRQHandler USBD_IRQHandler /*!< use actual usb irq name instead */
#endif

#ifndef USBD_CONFIG_ISO_IN_ZLP
#define USBD_CONFIG_ISO_IN_ZLP 0
#endif

/*!< ep dir in */
#define EP_DIR_IN 1
/*!< ep dir out */
#define EP_DIR_OUT 0
/*!< get ep id by epadd */
#define GET_EP_ID(ep_add) (uint8_t)(ep_add & 0x7f)
/*!< get ep dir by epadd */
#define GET_EP_DIR(ep_add) (uint8_t)(ep_add & 0x80)
/*!< ep nums */
#define EP_NUMS 9
/*!< ep mps */
#define EP_MPS 64
/*!< nrf5x special */
#define EP_ISO_NUM 8

/*!< Peripheral address base */
#define NRF_USBD_BASE 0x40027000UL
#define NRF_USBD ((NRF_USBD_Type *)NRF_USBD_BASE)

#ifndef EP_ISO_MPS
#define EP_ISO_MPS 64
#endif

__attribute__((aligned(4))) uint8_t ep_iso_tx[EP_ISO_MPS];
__attribute__((aligned(4))) uint8_t ep_iso_rx[EP_ISO_MPS + EP_ISO_MPS];

/**
 * @brief   Endpoint information structure
 */
typedef struct _usbd_ep_info
{
  uint8_t mps;          /*!< Maximum packet length of endpoint */
  uint8_t eptype;       /*!< Endpoint Type */
  uint8_t *ep_ram_addr; /*!< Endpoint buffer address */
  /*!< Other endpoint parameters that may be used */
  volatile uint8_t is_using_dma;
} usbd_ep_info;

/*!< nrf52840 usb */
struct _nrf52840_core_prvi
{
  uint8_t address;               /*!< address */
  usbd_ep_info ep_in[EP_NUMS];   /*!< ep in */
  usbd_ep_info ep_out[EP_NUMS];  /*!< ep out */
  struct usb_setup_packet setup; /*!< Setup package that may be used in interrupt processing (outside the protocol stack) */
  volatile uint8_t dma_running;
  int8_t in_count;
  volatile uint8_t iso_turn;
  volatile uint8_t iso_tx_is_ready;
  /**
   * For nrf5x, easydma will not move the setup packet into RAM.
   * We use a flag bit to judge whether the host sends setup,
   * and then notify usbd_ep_read to and from the register to read the setup package
   */
  volatile uint8_t is_setup_packet;
} usb_dc_cfg;

__WEAK void usb_dc_low_level_init(void)
{
}

__WEAK void usb_dc_low_level_deinit(void)
{
}

static inline void nrf_usbd_enable(void)
{
  /*!< Prepare for READY event receiving */
  NRF_USBD->EVENTCAUSE = USBD_EVENTCAUSE_READY_Msk;
  __ISB();
  __DSB();

  NRF_USBD->ENABLE = 0x01;

  while (0 == (USBD_EVENTCAUSE_READY_Msk & (NRF_USBD->EVENTCAUSE)))
  {
    /*!< Empty loop */
  }

  NRF_USBD->EVENTCAUSE = USBD_EVENTCAUSE_READY_Msk;
  __ISB();
  __DSB();

  NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_HalfIN << USBD_ISOSPLIT_SPLIT_Pos;
  if (USBD_CONFIG_ISO_IN_ZLP)
  {
    NRF_USBD->ISOINCONFIG = ((uint32_t)USBD_ISOINCONFIG_RESPONSE_ZeroData) << USBD_ISOINCONFIG_RESPONSE_Pos;
  }
  else
  {
    NRF_USBD->ISOINCONFIG = ((uint32_t)USBD_ISOINCONFIG_RESPONSE_NoResp) << USBD_ISOINCONFIG_RESPONSE_Pos;
  }
  
  usb_dc_low_level_init();
}

/**
 * @brief            Get setup package
 * @pre              None
 * @param[in]        setup Pointer to the address where the setup package is stored
 * @retval           None
 */
static inline void get_setup_packet(struct usb_setup_packet *setup)
{
  setup->bmRequestType = (uint8_t)(NRF_USBD->BMREQUESTTYPE);
  setup->bRequest = (uint8_t)(NRF_USBD->BREQUEST);
  setup->wIndex = (uint16_t)(NRF_USBD->WINDEXL | ((NRF_USBD->WINDEXH) << 8));
  setup->wLength = (uint16_t)(NRF_USBD->WLENGTHL | ((NRF_USBD->WLENGTHH) << 8));
  setup->wValue = (uint16_t)(NRF_USBD->WVALUEL | ((NRF_USBD->WVALUEH) << 8));
}

/**
 * @brief            Set tx easydma
 * @pre              None
 * @param[in]        ep      End point address
 * @param[in]        ptr     Data ram ptr
 * @param[in]        maxcnt  Max length
 * @retval           None
 */
static void nrf_usbd_ep_easydma_set_tx(uint8_t ep, uint32_t ptr, uint32_t maxcnt)
{
  uint8_t epid = GET_EP_ID(ep);
  if (epid == EP_ISO_NUM)
  {
    NRF_USBD->ISOIN.PTR = ptr;
    NRF_USBD->ISOIN.MAXCNT = maxcnt;
    return;
  }
  NRF_USBD->EPIN[epid].PTR = ptr;
  NRF_USBD->EPIN[epid].MAXCNT = maxcnt;
}

/**
 * @brief            Set rx easydma
 * @pre              None
 * @param[in]        ep      End point address
 * @param[in]        ptr     Data ram ptr
 * @param[in]        maxcnt  Max length
 * @retval           None
 */
static void nrf_usbd_ep_easydma_set_rx(uint8_t ep, uint32_t ptr, uint32_t maxcnt)
{
  uint8_t epid = GET_EP_ID(ep);
  if (epid == EP_ISO_NUM)
  {
    NRF_USBD->ISOOUT.PTR = ptr;
    NRF_USBD->ISOOUT.MAXCNT = maxcnt;
    return;
  }
  NRF_USBD->EPOUT[epid].PTR = ptr;
  NRF_USBD->EPOUT[epid].MAXCNT = maxcnt;
}

/**
 * @brief            Set address
 * @pre              None
 * @param[in]        address 8-bit valid address
 * @retval           >=0 success otherwise failure
 */
int usbd_set_address(const uint8_t address)
{
  if (address == 0)
  {
    /*!< init 0 address */
  }
  else
  {
    /*!< For non-0 addresses, write the address to the register in the state phase of setting the address */
  }

  NRF_USBD->EVENTCAUSE |= NRF_USBD->EVENTCAUSE;
  NRF_USBD->EVENTS_USBEVENT = 0;

  NRF_USBD->INTENSET = USBD_INTEN_USBEVENT_Msk;
  /*!< nothing to do, handled by hardware; but don't STALL */
  usb_dc_cfg.address = address;
  return 0;
}

/**
 * @brief            Open endpoint
 * @pre              None
 * @param[in]        ep_cfg : Endpoint configuration structure pointer
 * @retval           >=0 success otherwise failure
 */
int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
  /*!< ep id */
  uint8_t epid = GET_EP_ID(ep_cfg->ep_addr);
  /*!< ep dir */
  bool dir = GET_EP_DIR(ep_cfg->ep_addr);
  /*!< ep max packet length */
  uint8_t mps = ep_cfg->ep_mps;
  if (dir == EP_DIR_IN)
  {
    /*!< In */
    usb_dc_cfg.ep_in[epid].mps = mps;
    usb_dc_cfg.ep_in[epid].eptype = ep_cfg->ep_type;
    /*!< Open ep */
    if (ep_cfg->ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS)
    {
      /*!< Allocate memory to endpoints */
      usb_dc_cfg.ep_in[epid].ep_ram_addr = (uint8_t *)malloc(usb_dc_cfg.ep_in[epid].mps);
      /*!< Enable endpoint interrupt */
      NRF_USBD->INTENSET = (1 << (USBD_INTEN_ENDEPIN0_Pos + epid));
      /*!< Enable the in endpoint host to respond when sending in token */
      NRF_USBD->EPINEN |= (1 << (epid));
      __ISB();
      __DSB();
    }
    else
    {
      /*!< Allocate memory to endpoints */
      usb_dc_cfg.ep_in[EP_ISO_NUM].ep_ram_addr = ep_iso_tx;
      NRF_USBD->EVENTS_ENDISOIN = 0;
      /*!< SPLIT ISO buffer when ISO OUT endpoint is already opened. */
      if (usb_dc_cfg.ep_out[EP_ISO_NUM].mps)
        NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_HalfIN;

      /*!< Clear SOF event in case interrupt was not enabled yet. */
      if ((NRF_USBD->INTEN & USBD_INTEN_SOF_Msk) == 0)
        NRF_USBD->EVENTS_SOF = 0;

      /*!< Enable SOF and ISOIN interrupts, and ISOIN endpoint. */
      NRF_USBD->INTENSET = USBD_INTENSET_ENDISOIN_Msk | USBD_INTENSET_SOF_Msk;
      NRF_USBD->EPINEN |= USBD_EPINEN_ISOIN_Msk;
    }
  }
  else if (dir == EP_DIR_OUT)
  {
    /*!< Out */
    usb_dc_cfg.ep_out[epid].mps = mps;
    usb_dc_cfg.ep_out[epid].eptype = ep_cfg->ep_type;
    /*!< Open ep */
    if (ep_cfg->ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS)
    {
      /*!< Allocate memory to endpoints */
      usb_dc_cfg.ep_out[epid].ep_ram_addr = (uint8_t *)malloc(usb_dc_cfg.ep_out[epid].mps);
      NRF_USBD->INTENSET = (1 << (USBD_INTEN_ENDEPOUT0_Pos + epid));
      NRF_USBD->EPOUTEN |= (1 << (epid));
      __ISB();
      __DSB();
      /*!< Write any value to SIZE register will allow nRF to ACK/accept data */
      NRF_USBD->SIZE.EPOUT[epid] = 0;
    }
    else
    {
      /*!< Allocate memory to endpoints */
      usb_dc_cfg.ep_out[EP_ISO_NUM].ep_ram_addr = ep_iso_rx;
      /*!< SPLIT ISO buffer when ISO IN endpoint is already opened. */
      if (usb_dc_cfg.ep_in[EP_ISO_NUM].mps)
        NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_HalfIN;

      /*!< Clear old events */
      NRF_USBD->EVENTS_ENDISOOUT = 0;

      /*!< Clear SOF event in case interrupt was not enabled yet. */
      if ((NRF_USBD->INTEN & USBD_INTEN_SOF_Msk) == 0)
        NRF_USBD->EVENTS_SOF = 0;

      /*!< Enable SOF and ISOOUT interrupts, and ISOOUT endpoint. */
      NRF_USBD->INTENSET = USBD_INTENSET_ENDISOOUT_Msk | USBD_INTENSET_SOF_Msk;
      NRF_USBD->EPOUTEN |= USBD_EPOUTEN_ISOOUT_Msk;
    }
  }

  /*!< Clear stall and reset DataToggle */
  NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_UnStall << USBD_EPSTALL_STALL_Pos) | (ep_cfg->ep_addr);
  NRF_USBD->DTOGGLE = (USBD_DTOGGLE_VALUE_Data0 << USBD_DTOGGLE_VALUE_Pos) | (ep_cfg->ep_addr);

  __ISB();
  __DSB();

  return 0;
}

/**
 * @brief            Close endpoint
 * @pre              None
 * @param[in]        ep ： Endpoint address
 * @retval           >=0 success otherwise failure
 */
int usbd_ep_close(const uint8_t ep)
{
  /*!< ep id */
  uint8_t epid = GET_EP_ID(ep);
  /*!< ep dir */
  bool dir = GET_EP_DIR(ep);
  if (epid != EP_ISO_NUM)
  {
    if (dir == EP_DIR_OUT)
    {
      free(usb_dc_cfg.ep_out[epid].ep_ram_addr);
      NRF_USBD->INTENCLR = (1 << (USBD_INTEN_ENDEPOUT0_Pos + epid));
      NRF_USBD->EPOUTEN &= ~(1 << (epid));
    }
    else
    {
      free(usb_dc_cfg.ep_in[epid].ep_ram_addr);
      NRF_USBD->INTENCLR = (1 << (USBD_INTEN_ENDEPIN0_Pos + epid));
      NRF_USBD->EPINEN &= ~(1 << (epid));
    }
  }
  else
  {
    /*!< ISO EP */
    if (dir == EP_DIR_OUT)
    {
      usb_dc_cfg.ep_out[EP_ISO_NUM].mps = 0;
      NRF_USBD->INTENCLR = USBD_INTENCLR_ENDISOOUT_Msk;
      NRF_USBD->EPOUTEN &= ~USBD_EPOUTEN_ISOOUT_Msk;
      NRF_USBD->EVENTS_ENDISOOUT = 0;
    }
    else
    {
      usb_dc_cfg.ep_in[EP_ISO_NUM].mps = 0;
      NRF_USBD->INTENCLR = USBD_INTENCLR_ENDISOIN_Msk;
      NRF_USBD->EPINEN &= ~USBD_EPINEN_ISOIN_Msk;
    }
    /*!< One of the ISO endpoints closed, no need to split buffers any more. */
    NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_OneDir;
    /*!< When both ISO endpoint are close there is no need for SOF any more. */
    if (usb_dc_cfg.ep_in[EP_ISO_NUM].mps + usb_dc_cfg.ep_out[EP_ISO_NUM].mps == 0)
    {
      NRF_USBD->INTENCLR = USBD_INTENCLR_SOF_Msk;
    }
  }
  __ISB();
  __DSB();

  return 0;
}

/**
 * @brief            Write send buffer
 * @pre              None
 * @param[in]        ep ： Endpoint address
 * @param[in]        data ： First address of data buffer to be written
 * @param[in]        data_len ： Write total length
 * @param[in]        ret_bytes ： Length actually written
 * @retval           >=0 success otherwise failure
 */
int usbd_ep_write(const uint8_t ep, const uint8_t *data, uint32_t data_len, uint32_t *ret_bytes)
{
  /*!< ep id */
  uint8_t epid = GET_EP_ID(ep);
  /*!< real write byte nums */
  uint32_t real_wt_nums = 0;
  /*!< ep mps */
  uint8_t ep_mps = usb_dc_cfg.ep_in[epid].mps;
  /*!< Analyze bytes actually written */
  if (data == NULL && data_len > 0)
  {
    return -1;
  }

  if (data_len == 0)
  {
    /*!< write 0 len data */
    memset(usb_dc_cfg.ep_in[epid].ep_ram_addr, 0, ep_mps);
    nrf_usbd_ep_easydma_set_tx(epid, (uint32_t)usb_dc_cfg.ep_in[epid].ep_ram_addr, 0);
    NRF_USBD->TASKS_STARTEPIN[epid] = 1;
    return 0;
  }

  if (data_len > ep_mps)
  {
    /*!< The data length is greater than the maximum packet length of the endpoint */
    real_wt_nums = ep_mps;
  }
  else
  {
    real_wt_nums = data_len;
  }

  /*!< write buff start */
  memcpy(usb_dc_cfg.ep_in[epid].ep_ram_addr, data, real_wt_nums);
  nrf_usbd_ep_easydma_set_tx(epid, (uint32_t)usb_dc_cfg.ep_in[epid].ep_ram_addr, real_wt_nums);
  /*!< write buff over */

  /**
   * Note that starting DMA transmission is to transmit data to USB peripherals,
   * and then wait for the host to get it
   */
  /*!< Start dma transfer */
  if (epid != EP_ISO_NUM)
  {
    NRF_USBD->TASKS_STARTEPIN[epid] = 1;
  }
  else
  {
    // NRF_USBD->TASKS_STARTISOIN = 1;
    usb_dc_cfg.iso_tx_is_ready = 1;
  }

  if (ret_bytes != NULL)
  {
    *ret_bytes = real_wt_nums;
  }

  return 0;
}

/**
 * @brief            Read receive buffer
 * @pre              None
 * @param[in]        ep ： Endpoint address
 * @param[in]        data ： Read the first address of the buffer where the data is stored
 * @param[in]        max_data_len ： Maximum readout length
 * @param[in]        read_bytes ： Actual read length
 * @retval           >=0 success otherwise failure
 */
int usbd_ep_read(const uint8_t ep, uint8_t *data, uint32_t max_data_len, uint32_t *read_bytes)
{
  /*!< ep id */
  uint8_t epid = GET_EP_ID(ep);
  /*!< real read byte nums */
  uint32_t real_rd_nums = 0;
  /*!< ep mps */
  uint8_t ep_mps = usb_dc_cfg.ep_out[epid].mps;

  if (data == NULL && max_data_len > 0)
  {
    return -1;
  }

  if (max_data_len == 0)
  {
    // if (epid != 0)
    NRF_USBD->SIZE.EPOUT[epid] = EP_MPS;
    return 0;
  }

  /*!< Nrf5x special place */
  /*!< Start */
  if ((usb_dc_cfg.is_setup_packet == 1) &&
      (max_data_len == sizeof(struct usb_setup_packet)))
  {
    /*!< Read setup packet */
    get_setup_packet((struct usb_setup_packet *)data);
    usb_dc_cfg.is_setup_packet = 0;
    if (read_bytes != NULL)
    {
      *read_bytes = real_rd_nums;
    }
    return 0;
  }
  /*!< Over */

  if (max_data_len > ep_mps)
    max_data_len = ep_mps;

  real_rd_nums = NRF_USBD->SIZE.EPOUT[epid];
  real_rd_nums = MIN(real_rd_nums, max_data_len);

  /*!< read buff start */
  memcpy(data, (uint8_t *)usb_dc_cfg.ep_out[epid].ep_ram_addr, real_rd_nums);
  /**
   * The reason why DMA transmission is not started here is that when the endpoint receives data, the host sends an out token and then transmits the data.
   * Nrf5x after receiving the data and responding to the host ACK, an EPDATA event will be generated.
   * In that event, we query the flag bit to obtain which endpoint received the data successfully,
   * and then set the target address of easydma to move the data from the USB peripheral to ram.
   * Easydma will trigger an ENDEPOUT[epid] event after moving data,
   * in which EP in the protocol stack is called ep_out_handler.So When reading, the RAM buffer of the endpoint has received the data.
   * We only need to copy the data from the buffer.
   */
  /*!< read buff over */

  if (read_bytes != NULL)
  {
    *read_bytes = real_rd_nums;
  }

  return 0;
}

/**
 * @brief            Endpoint setting pause
 * @pre              None
 * @param[in]        ep ： Endpoint address
 * @retval           >=0 success otherwise failure
 */
int usbd_ep_set_stall(const uint8_t ep)
{
  /*!< ep id */
  uint8_t epid = GET_EP_ID(ep);
  bool dir = GET_EP_DIR(ep);

  if (epid == 0)
  {
    NRF_USBD->TASKS_EP0STALL = 1;
  }
  else if (epid != EP_ISO_NUM)
  {
    NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_Stall << USBD_EPSTALL_STALL_Pos) | (ep);
  }
  __ISB();
  __DSB();
  return 0;
}

/**
 * @brief            Endpoint clear pause
 * @pre              None
 * @param[in]        ep ： Endpoint address
 * @retval           >=0 success otherwise failure
 */
int usbd_ep_clear_stall(const uint8_t ep)
{
  /*!< ep id */
  uint8_t epid = GET_EP_ID(ep);
  uint8_t dir = GET_EP_DIR(ep);

  if (epid != 0 && epid != EP_ISO_NUM)
  {
    /**
     * reset data toggle to DATA0
     * First write this register with VALUE=Nop to select the endpoint, then either read it to get the status from
     * VALUE, or write it again with VALUE=Data0 or Data1
     */
    NRF_USBD->DTOGGLE = ep;
    NRF_USBD->DTOGGLE = (USBD_DTOGGLE_VALUE_Data0 << USBD_DTOGGLE_VALUE_Pos) | ep;

    /*!< Clear stall */
    NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_UnStall << USBD_EPSTALL_STALL_Pos) | ep;

    /*!< Write any value to SIZE register will allow nRF to ACK/accept data */
    if (dir == EP_DIR_OUT)
      NRF_USBD->SIZE.EPOUT[epid] = 0;

    __ISB();
    __DSB();
  }

  return 0;
}

/**
 * @brief            Check endpoint status
 * @pre              None
 * @param[in]        ep ： Endpoint address
 * @param[out]       stalled ： Outgoing endpoint status
 * @retval           >=0 success otherwise failure
 */
int usbd_ep_is_stalled(const uint8_t ep, uint8_t *stalled)
{
  return 0;
}

/**
 * @brief            USB initialization
 * @pre              None
 * @param[in]        None
 * @retval           >=0 success otherwise failure
 */
int usb_dc_init(void)
{
  /*!< dc init */
  memset(&usb_dc_cfg, 0, sizeof(usb_dc_cfg));

  /*!< Clear USB Event Interrupt */
  NRF_USBD->EVENTS_USBEVENT = 0;
  NRF_USBD->EVENTCAUSE |= NRF_USBD->EVENTCAUSE;

  /*!< Reset interrupt */
  NRF_USBD->INTENCLR = NRF_USBD->INTEN;
  NRF_USBD->INTENSET = USBD_INTEN_USBRESET_Msk | USBD_INTEN_USBEVENT_Msk | USBD_INTEN_EPDATA_Msk |
                       USBD_INTEN_EP0SETUP_Msk | USBD_INTEN_EP0DATADONE_Msk | USBD_INTEN_ENDEPIN0_Msk | USBD_INTEN_ENDEPOUT0_Msk | USBD_INTEN_STARTED_Msk;
  nrf_usbd_enable();
  return 0;
}

/**
 * @brief            USB interrupt processing function
 * @pre              None
 * @param[in]        None
 * @retval           None
 */
void USBD_IRQHandler(void)
{
  uint32_t const inten = NRF_USBD->INTEN;
  uint32_t int_status = 0;
  volatile uint32_t usb_event = 0;
  volatile uint32_t *regevt = &NRF_USBD->EVENTS_USBRESET;

  /*!< Traverse USB events */
  for (uint8_t i = 0; i < USBD_INTEN_EPDATA_Pos + 1; i++)
  {
    if ((inten & (1 << i)) && regevt[i])
    {
      int_status |= (1 << (i));
      /*!< event clear */
      regevt[i] = 0;
      __ISB();
      __DSB();
    }
  }

  /*!< bit 24 */
  if (int_status & USBD_INTEN_EPDATA_Msk)
  {
    /*!< out ep */
    for (uint8_t ep_out_ct = 1; ep_out_ct <= 7; ep_out_ct++)
    {
      if ((NRF_USBD->EPDATASTATUS) & (1 << (16 + ep_out_ct)))
      {
        NRF_USBD->EPDATASTATUS |= (1 << (16 + ep_out_ct));
        nrf_usbd_ep_easydma_set_rx(ep_out_ct, (uint32_t)usb_dc_cfg.ep_out[ep_out_ct].ep_ram_addr, NRF_USBD->SIZE.EPOUT[ep_out_ct]);
        NRF_USBD->TASKS_STARTEPOUT[ep_out_ct] = 1;
      }
    }
    /*!< in ep */
    for (uint8_t ep_in_ct = 1; ep_in_ct <= 7; ep_in_ct++)
    {
      if ((NRF_USBD->EPDATASTATUS) & (1 << (0 + ep_in_ct)))
      {
        /*!< in ep tranfer to host successfully */
        NRF_USBD->EPDATASTATUS |= (1 << (0 + ep_in_ct));
        usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (uint32_t *)((ep_in_ct) | 0x80));
      }
    }
  }

  /*!< bit 23 */
  if (int_status & USBD_INTEN_EP0SETUP_Msk)
  {
    /* Setup */
    /*!< Storing this setup package will help the following procedures */
    get_setup_packet(&(usb_dc_cfg.setup));
    usb_dc_cfg.is_setup_packet = 1;
    usb_dc_cfg.in_count = usb_dc_cfg.setup.wLength / 64;
    /*!< Nrf52840 will set the address automatically by hardware,
         so the protocol stack of the address setting command sent by the host does not need to be processed */
    if ((usb_dc_cfg.setup.bmRequestType & USB_REQUEST_DIR_MASK) == USB_REQUEST_DIR_OUT &&
        usb_dc_cfg.setup.wLength > 0)
    {
      NRF_USBD->TASKS_EP0RCVOUT = 1;
      nrf_usbd_ep_easydma_set_rx(0, (uint32_t)usb_dc_cfg.ep_out[0].ep_ram_addr, usb_dc_cfg.ep_out[0].mps);
    }

    if (usb_dc_cfg.setup.wLength == 0)
    {
      NRF_USBD->TASKS_EP0STATUS = 1;
    }

    if (usb_dc_cfg.setup.bRequest != USB_REQUEST_SET_ADDRESS)
    {
      usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);
    }
  }

  /*!< bit 22 */
  if (int_status & USBD_INTEN_USBEVENT_Msk)
  {
    usb_event = NRF_USBD->EVENTCAUSE;
    NRF_USBD->EVENTCAUSE = usb_event;
    if (usb_event & USBD_EVENTCAUSE_SUSPEND_Msk)
    {
      NRF_USBD->LOWPOWER = 1;
      usbd_event_notify_handler(USBD_EVENT_SUSPEND, NULL);
    }
    if (usb_event & USBD_EVENTCAUSE_RESUME_Msk)
    {
      usbd_event_notify_handler(USBD_EVENT_RESUME, NULL);
    }
    if (usb_event & USBD_EVENTCAUSE_USBWUALLOWED_Msk)
    {
      NRF_USBD->DPDMVALUE = USBD_DPDMVALUE_STATE_Resume;
      NRF_USBD->TASKS_DPDMDRIVE = 1;
      /**
       * There is no Resume interrupt for remote wakeup, enable SOF for to report bus ready state
       * Clear SOF event in case interrupt was not enabled yet.
       */
      if ((NRF_USBD->INTEN & USBD_INTEN_SOF_Msk) == 0)
        NRF_USBD->EVENTS_SOF = 0;
      NRF_USBD->INTENSET = USBD_INTENSET_SOF_Msk;
    }
  }

  /*!< bit 21 */
  if (int_status & USBD_INTEN_SOF_Msk)
  {
    bool iso_enabled = false;
    /*!< ISOOUT: Transfer data gathered in previous frame from buffer to RAM */
    if (NRF_USBD->EPOUTEN & USBD_EPOUTEN_ISOOUT_Msk)
    {
      iso_enabled = true;
      /*!< If ZERO bit is set, ignore ISOOUT length */
      if (usb_dc_cfg.iso_turn < 2)
      {
        usb_dc_cfg.iso_turn++;
        if ((NRF_USBD->SIZE.ISOOUT) & USBD_SIZE_ISOOUT_ZERO_Msk)
        {
          /*!<  */
        }
        else
        {
          /*!< Prepare */
          NRF_USBD->ISOOUT.PTR = (uint32_t)usb_dc_cfg.ep_out[EP_ISO_NUM].ep_ram_addr;
          NRF_USBD->ISOOUT.MAXCNT = NRF_USBD->SIZE.ISOOUT;
        }
      }
      if (usb_dc_cfg.iso_turn == 2)
      {
        NRF_USBD->ISOOUT.PTR = (uint32_t)usb_dc_cfg.ep_out[EP_ISO_NUM].ep_ram_addr;
        NRF_USBD->ISOOUT.MAXCNT = NRF_USBD->SIZE.ISOOUT;
        NRF_USBD->TASKS_STARTISOOUT = 1;
        /*!< EP_ISO_NUM out using dma */
        usb_dc_cfg.ep_out[EP_ISO_NUM].is_using_dma = 1;
      }
    }

    /*!< ISOIN: Notify client that data was transferred */
    if (NRF_USBD->EPINEN & USBD_EPINEN_ISOIN_Msk)
    {
      iso_enabled = true;
      if (usb_dc_cfg.iso_tx_is_ready == 1)
      {
        usb_dc_cfg.iso_tx_is_ready = 0;
        NRF_USBD->TASKS_STARTISOIN = 1;
      }
    }

    if (!iso_enabled)
    {
      /**
       * ISO endpoint is not used, SOF is only enabled one-time for remote wakeup
       * so we disable it now
       */
      NRF_USBD->INTENCLR = USBD_INTENSET_SOF_Msk;
    }
  }

  /*!< bit 20 */
  if (int_status & USBD_INTEN_ENDISOOUT_Msk)
  {
    if (usb_dc_cfg.ep_out[EP_ISO_NUM].is_using_dma == 1)
    {
      usb_dc_cfg.ep_out[EP_ISO_NUM].is_using_dma = 0;
      usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (uint32_t *)(EP_ISO_NUM | 0x00));
    }
  }

  /**
   * Traverse ordinary out endpoint events, starting from endpoint 1 to endpoint 7,
   * and end 0 for special processing
   */
  for (uint8_t offset = 0; offset < 7; offset++)
  {
    if (int_status & (USBD_INTEN_ENDEPOUT1_Msk << offset))
    {
      /*!< Out 'offset' transfer complete */
      usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (uint32_t *)((offset + 1) & 0x7f));
    }
  }

  /*!< bit 12 */
  if (int_status & USBD_INTEN_ENDEPOUT0_Msk)
  {
    if (NRF_USBD->SIZE.EPOUT[0] == 64)
    {
      /*!< Enable the data phase of the endpoint */
      NRF_USBD->TASKS_EP0RCVOUT = 1;
    }
    else
    {
      /*!< Enable the state phase of endpoint 0 */
      NRF_USBD->TASKS_EP0STATUS = 1;
    }
    usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
  }

  /*!< bit 11 */
  if (int_status & USBD_INTEN_ENDISOIN_Msk)
  {
  }

  /*!< bit 10 */
  if (int_status & USBD_INTEN_EP0DATADONE_Msk)
  {
    switch (usb_dc_cfg.setup.bmRequestType >> USB_REQUEST_DIR_SHIFT)
    {
    case 1:
      /*!< IN */
      if ((usb_dc_cfg.setup.wLength == 64 && usb_dc_cfg.setup.bRequest == 0x06 && (usb_dc_cfg.setup.wValue >> 8) == 1) ||   /*!< Get device descriptor for the first time */
          (usb_dc_cfg.setup.wLength == 0xff && usb_dc_cfg.setup.bRequest == 0x06 && (usb_dc_cfg.setup.wValue >> 8) == 2) || /*!< Get configuration descriptor for the first time */
          (usb_dc_cfg.setup.wLength == 0xff && usb_dc_cfg.setup.bRequest == 0x06 && (usb_dc_cfg.setup.wValue >> 8) == 3) || /*!< Get string descriptor for the first time */
          (usb_dc_cfg.setup.wLength == 64))
      {
        usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
        NRF_USBD->TASKS_EP0STATUS = 1;
      }
      else if (usb_dc_cfg.setup.wLength > 64)
      {
        usb_dc_cfg.in_count--;
        usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
        if (usb_dc_cfg.in_count == -1)
        {
          NRF_USBD->TASKS_EP0STATUS = 1;
        }
      }
      else
      {
        usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
        NRF_USBD->TASKS_EP0STATUS = 1;
      }
      break;
    case 0:
      if (usb_dc_cfg.setup.bRequest != USB_REQUEST_SET_ADDRESS)
      {
        NRF_USBD->TASKS_STARTEPOUT[0] = 1;
      }
      break;
    }
  }

  /**
   * Traversing ordinary in endpoint events, starting from endpoint 1 to endpoint 7,
   * endpoint 0 special processing
   */
  for (uint8_t offset = 0; offset < 7; offset++)
  {
    if (int_status & (USBD_INTEN_ENDEPIN1_Msk << offset))
    {
      /*!< DMA move data completed */
    }
  }

  /*!< bit 2 */
  if (int_status & USBD_INTEN_ENDEPIN0_Msk)
  {
  }

  /*!< bit 1 */
  if (int_status & USBD_INTEN_STARTED_Msk)
  {
    if (usb_dc_cfg.ep_out[EP_ISO_NUM].is_using_dma == 1)
    {
      NRF_USBD->ISOOUT.PTR = (uint32_t)usb_dc_cfg.ep_out[EP_ISO_NUM].ep_ram_addr + EP_NUMS;
      NRF_USBD->ISOOUT.MAXCNT = NRF_USBD->SIZE.ISOOUT;
    }
  }

  /*!< bit 0 */
  if (int_status & USBD_INTEN_USBRESET_Msk)
  {
    NRF_USBD->EPOUTEN = 1UL;
    NRF_USBD->EPINEN = 1UL;

    for (int i = 0; i < 8; i++)
    {
      NRF_USBD->TASKS_STARTEPIN[i] = 0;
      NRF_USBD->TASKS_STARTEPOUT[i] = 0;
    }

    NRF_USBD->TASKS_STARTISOIN = 0;
    NRF_USBD->TASKS_STARTISOOUT = 0;

    /*!< Clear USB Event Interrupt */
    NRF_USBD->EVENTS_USBEVENT = 0;
    NRF_USBD->EVENTCAUSE |= NRF_USBD->EVENTCAUSE;

    /*!< Reset interrupt */
    NRF_USBD->INTENCLR = NRF_USBD->INTEN;
    NRF_USBD->INTENSET = USBD_INTEN_USBRESET_Msk | USBD_INTEN_USBEVENT_Msk | USBD_INTEN_EPDATA_Msk |
                         USBD_INTEN_EP0SETUP_Msk | USBD_INTEN_EP0DATADONE_Msk | USBD_INTEN_ENDEPIN0_Msk | USBD_INTEN_ENDEPOUT0_Msk | USBD_INTEN_STARTED_Msk;

    usbd_event_notify_handler(USBD_EVENT_RESET, NULL);
  }
}

#include "usbd_core.h"
#include "fsl_common.h"
#include "usb_dc_imxrt_port.h"
#include "usb_imxrt_reg.h"

#define USB_GET_INDEX(ep)       ((USB_EP_GET_IDX(ep) << 1U) | USB_EP_DIR_IS_IN(ep))
#define USB_GET_PRIMEBIT(ep)    (1UL << (USB_EP_GET_IDX(ep) + (USB_EP_GET_DIR(ep) >> 0x03U)))

/*
 * alloc the temporary memory to store the status
 */
#define OSA_SR_ALLOC() uint32_t osaCurrentSr = 0U;
/*
 * Enter critical mode
 */
#define OSA_ENTER_CRITICAL() (osaCurrentSr = DisableGlobalIRQ())
/*
 * Exit critical mode and retore the previous mode
 */
#define OSA_EXIT_CRITICAL() EnableGlobalIRQ(osaCurrentSr)

/* Apply for dtd buffer list */
USB_RAM_ADDRESS_ALIGNMENT(4)
static usb_dtd_buffer_t g_UsbDtdBufList[USB_DEVICE_USE_PORT][USB_DEVICE_MAX_DTD] = {0};

/* Apply for QH buffer, 2048-byte alignment */
USB_RAM_ADDRESS_ALIGNMENT(2048)
USB_CONTROLLER_DATA static uint8_t qh_buffer[(USB_DEVICE_USE_PORT - 1) * 2048 +
                                             2 * USB_DEVICE_ENDPOINTS * sizeof(usb_device_ehci_qh_struct_t)];

/* Apply for DTD buffer, 32-byte alignment */
USB_RAM_ADDRESS_ALIGNMENT(32)
USB_CONTROLLER_DATA static usb_device_ehci_dtd_struct_t s_UsbDeviceEhciDtd[USB_DEVICE_USE_PORT][USB_DEVICE_MAX_DTD];

/* Apply for ehci device state structure */
static usb_device_ehci_state_struct_t g_UsbDeviceEhciState[USB_DEVICE_USE_PORT];

/* device function */
static void usb_clock_init(uint8_t instance)
{
    USBPHY_Type *usbPhyBase;

    if(instance == 0)
    {
        CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_Usbphy480M, 480000000U);
        CLOCK_EnableUsbhs0Clock(kCLOCK_Usb480M, 480000000U);
        usbPhyBase = USBPHY1;
    }
    else if(instance == 1)
    {
        CLOCK_EnableUsbhs1PhyPllClock(kCLOCK_Usbphy480M, 480000000U);
        CLOCK_EnableUsbhs1Clock(kCLOCK_Usb480M, 480000000U);
        usbPhyBase = USBPHY2;
    }
    else
    {
        while(1);
    }

    USB_ANALOG->INSTANCE[instance].CHRG_DETECT_SET =
        USB_ANALOG_CHRG_DETECT_CHK_CHRG_B(1) | USB_ANALOG_CHRG_DETECT_EN_B(1);

#if ((!(defined FSL_FEATURE_SOC_CCM_ANALOG_COUNT)) && (!(defined FSL_FEATURE_SOC_ANATOP_COUNT)))
    usbPhyBase->TRIM_OVERRIDE_EN = 0x001fU; /* override IFR value */
#endif
    usbPhyBase->CTRL |= USBPHY_CTRL_SET_ENUTMILEVEL2_MASK; /* support LS device. */
    usbPhyBase->CTRL |= USBPHY_CTRL_SET_ENUTMILEVEL3_MASK; /* support external FS Hub with LS device connected. */
    /* PWD register provides overall control of the PHY power state */
    usbPhyBase->PWD = 0U;

    /* Decode to trim the nominal 17.78mA current source for the High Speed TX drivers on USB_DP and USB_DM. */
    usbPhyBase->TX =
        ((usbPhyBase->TX & (~(USBPHY_TX_D_CAL_MASK | USBPHY_TX_TXCAL45DM_MASK | USBPHY_TX_TXCAL45DP_MASK))) |
            (USBPHY_TX_D_CAL(BOARD_USB_PHY_D_CAL) | USBPHY_TX_TXCAL45DP(BOARD_USB_PHY_TXCAL45DP) |
            USBPHY_TX_TXCAL45DM(BOARD_USB_PHY_TXCAL45DM)));
}

/*!
 * @brief Get dtds and link to QH.
 *
 * The function is used to get dtds and link to QH.
 *
 * @param ehciState       Pointer of the device EHCI state structure.
 * @param endpointAddress The endpoint address, Bit7, 0U - USB_OUT, 1U - USB_IN.
 * @param buffer           The memory address needed to be transferred.
 * @param length           Data length.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
static usb_status_t USB_DeviceEhciTransfer(usb_device_ehci_state_struct_t *ehciState,
                                           uint8_t endpointAddress,
                                           uint8_t *buffer,
                                           uint32_t length)
{
    usb_device_ehci_dtd_struct_t *dtd;
    usb_device_ehci_dtd_struct_t *dtdHead;
    uint32_t index = USB_GET_INDEX(endpointAddress);
    uint32_t primeBit = USB_GET_PRIMEBIT(endpointAddress);
    uint32_t epStatus = primeBit;
    uint32_t sendLength;
    uint32_t currentIndex       = 0U;
    uint32_t dtdRequestCount    = (length + USB_DEVICE_ECHI_DTD_TOTAL_BYTES - 1U) / USB_DEVICE_ECHI_DTD_TOTAL_BYTES;
    uint8_t qhIdle              = 0U;
    uint8_t waitingSafelyAccess = 1U;
    uint32_t primeTimesCount    = 0U;
    OSA_SR_ALLOC();

    if (NULL == ehciState)
    {
        return kStatus_USB_InvalidHandle;
    }

    if (0U == ehciState->qh[index].endpointStatusUnion.endpointStatusBitmap.isOpened)
    {
        return kStatus_USB_Error;
    }
    /* Return error when ehci is doing reset */
    if (0U != ehciState->isResetting)
    {
        return kStatus_USB_Error;
    }

    if (0U == dtdRequestCount)
    {
        dtdRequestCount = 1U;
    }

    OSA_ENTER_CRITICAL();
    /* The free dtd count need to not less than the transfer requests. */
    if (dtdRequestCount > (uint32_t)ehciState->dtdCount)
    {
        OSA_EXIT_CRITICAL();
        return kStatus_USB_Busy;
    }

    do
    {
        /* The transfer length need to not more than USB_DEVICE_ECHI_DTD_TOTAL_BYTES for each dtd. */
        sendLength =  (length > USB_DEVICE_ECHI_DTD_TOTAL_BYTES) ? USB_DEVICE_ECHI_DTD_TOTAL_BYTES : length;
        length -= sendLength;

        /* Get a free dtd */
        dtd = ehciState->dtdFree;

        ehciState->dtdFree = (usb_device_ehci_dtd_struct_t *)dtd->nextDtdPointer;
        ehciState->dtdCount--;

        /* Save the dtd head when current active buffer offset is zero. */
        if (0U == currentIndex)
        {
            dtdHead = dtd;
        }

        /* Set the dtd field */
        dtd->nextDtdPointer         = USB_DEVICE_ECHI_DTD_TERMINATE_MASK;
        dtd->dtdTokenUnion.dtdToken = 0U;
        dtd->bufferPointerPage[0]   = (uint32_t)(buffer + currentIndex);
        dtd->bufferPointerPage[1] =
            (dtd->bufferPointerPage[0] + USB_DEVICE_ECHI_DTD_PAGE_BLOCK) & USB_DEVICE_ECHI_DTD_PAGE_MASK;
        dtd->bufferPointerPage[2] = dtd->bufferPointerPage[1] + USB_DEVICE_ECHI_DTD_PAGE_BLOCK;
        dtd->bufferPointerPage[3] = dtd->bufferPointerPage[2] + USB_DEVICE_ECHI_DTD_PAGE_BLOCK;
        dtd->bufferPointerPage[4] = dtd->bufferPointerPage[3] + USB_DEVICE_ECHI_DTD_PAGE_BLOCK;

        dtd->dtdTokenUnion.dtdTokenBitmap.totalBytes = sendLength;

        /* Save the data length needed to be transferred. */
        dtd->reservedUnion.originalBufferInfo.originalBufferLength = sendLength;
        /* Save the original buffer address */
        dtd->reservedUnion.originalBufferInfo.originalBufferOffest =
            dtd->bufferPointerPage[0] & USB_DEVICE_ECHI_DTD_PAGE_OFFSET_MASK;
        dtd->reservedUnion.originalBufferInfo.dtdInvalid = 0U;

        /* Set the IOC field in last dtd. */
        // if (0U == length)
        // {
        dtd->dtdTokenUnion.dtdTokenBitmap.ioc = 1U;
        // }

        /* Set dtd active */
        dtd->dtdTokenUnion.dtdTokenBitmap.status = USB_DEVICE_ECHI_DTD_STATUS_ACTIVE;

        /* Move the buffer offset index */
        currentIndex += sendLength;

        /* Add dtd to the in-used dtd queue */
        if (NULL != (ehciState->dtdTail[index]))
        {
            ehciState->dtdTail[index]->nextDtdPointer = (uint32_t)dtd;
            ehciState->dtdTail[index]                 = dtd;
        }
        else
        {
            ehciState->dtdHead[index] = dtd;
            ehciState->dtdTail[index] = dtd;
            qhIdle                    = 1U;
        }
    } while (0U != length);

    /* If the QH is not empty */
    if (0U == qhIdle)
    {
        /* If the prime bit is set, nothing need to do. */
        if (0U != (ehciState->registerBase->EPPRIME & primeBit))
        {
            OSA_EXIT_CRITICAL();
            return kStatus_USB_Success;
        }

        /* To safely a dtd */
        while (0U != waitingSafelyAccess)
        {
            /* set the ATDTW flag to USBHS_USBCMD_REG. */
            ehciState->registerBase->USBCMD |= USBHS_USBCMD_ATDTW_MASK;
            /* Read EPSR */
            epStatus = ehciState->registerBase->EPSR;
            /* Wait the ATDTW bit set */
            if (0U != (ehciState->registerBase->USBCMD & USBHS_USBCMD_ATDTW_MASK))
            {
                waitingSafelyAccess = 0U;
            }
        }
        /* Clear the ATDTW bit */
        ehciState->registerBase->USBCMD &= ~USBHS_USBCMD_ATDTW_MASK;
    }

    /* If QH is empty or the endpoint is not primed, need to link current dtd head to the QH. */
    /* When the endpoint is not primed if qhIdle is zero, it means the QH is empty. */
    if ((0U != qhIdle) || (0U == (epStatus & primeBit)))
    {
        ehciState->qh[index].nextDtdPointer         = (uint32_t)dtdHead;
        ehciState->qh[index].dtdTokenUnion.dtdToken = 0U;
        /*make sure dtd is linked to dqh*/
        __DSB();
        ehciState->registerBase->EPPRIME = primeBit;
        while (0U == (ehciState->registerBase->EPSR & primeBit))
        {
            primeTimesCount++;
            if (primeTimesCount == USB_DEVICE_MAX_TRANSFER_PRIME_TIMES)
            {
                OSA_EXIT_CRITICAL();
                return kStatus_USB_Error;
            }
            if (0U != (ehciState->registerBase->EPCOMPLETE & primeBit))
            {
                break;
            }
            else
            {
                ehciState->registerBase->EPPRIME = primeBit;
            }
        }
    }

    OSA_EXIT_CRITICAL();
    return kStatus_USB_Success;
}

/*!
 * @brief Get setup packet data.
 *
 * The function is used to get setup packet data and copy to a backup buffer.
 *
 * @param ehciState       Pointer of the device EHCI state structure.
 * @param ep               The endpoint number.
 *
 */
static void USB_DeviceEhciGetSetupData(usb_device_ehci_state_struct_t *ehciState, uint8_t index, uint8_t *data)
{
    uint8_t waitingSafelyAccess = 1U;
    uint8_t *temp;

    /* Write 1U to clear corresponding bit in EPSETUPSR. */
    ehciState->registerBase->ENDPTSETUPSTAT = 1UL << (index >> 1);
    /* Get last setup packet */
    temp = (uint8_t *)&ehciState->qh[index].setupBuffer;

    while (0U != waitingSafelyAccess)
    {
        /* Set the setup tripwire bit. */
        ehciState->registerBase->USBCMD |= USBHS_USBCMD_SUTW_MASK;

        /* Copy setup packet data to data buffer */
        for(uint8_t i = 0; i < 8; i++)
        {
            data[i] = temp[i];
        }

        /* Read the USBCMD[SUTW] bit. If set, jump out from the while loop; if cleared continue */
        if (0U != (ehciState->registerBase->USBCMD & USBHS_USBCMD_SUTW_MASK))
        {
            waitingSafelyAccess = 0U;
        }
    }
    /* Clear the setup tripwire bit */
    ehciState->registerBase->USBCMD &= ~USBHS_USBCMD_SUTW_MASK;
}

/*!
 * @brief Cancel the transfer of the control pipe.
 *
 * The function is used to cancel the transfer of the control pipe.
 *
 * @param ehciState       Pointer of the device EHCI state structure.
 * @param index           The dQH index.
 * @param direction        The direction of the endpoint.
 *
 */
static void USB_DeviceEhciCancelControlPipe(usb_device_ehci_state_struct_t *ehciState,
                                            uint8_t index)
{
    usb_device_ehci_dtd_struct_t *currentDtd;

    /* Get the dtd of the control pipe */
    currentDtd =
        (usb_device_ehci_dtd_struct_t *)((uint32_t)ehciState->dtdHead[index] & USB_DEVICE_ECHI_DTD_POINTER_MASK);
    while (NULL != currentDtd)
    {
        /* Move the dtd head pointer to next. */
        /* If the pointer of the head equals to the tail, set the dtd queue to null. */
        if (ehciState->dtdHead[index] == ehciState->dtdTail[index])
        {
            ehciState->dtdHead[index]                   = NULL;
            ehciState->dtdTail[index]                   = NULL;
            ehciState->qh[index].nextDtdPointer         = USB_DEVICE_ECHI_DTD_TERMINATE_MASK;
            ehciState->qh[index].dtdTokenUnion.dtdToken = 0U;
        }
        else
        {
            ehciState->dtdHead[index] = (usb_device_ehci_dtd_struct_t *)ehciState->dtdHead[index]->nextDtdPointer;
        }

        /* Clear the token field of the dtd. */
        currentDtd->dtdTokenUnion.dtdToken = 0U;
        /* Add the dtd to the free dtd queue. */
        currentDtd->nextDtdPointer = (uint32_t)ehciState->dtdFree;
        ehciState->dtdFree         = currentDtd;
        ehciState->dtdCount++;

        /* Get the next in-used dtd. */
        currentDtd =
            (usb_device_ehci_dtd_struct_t *)((uint32_t)ehciState->dtdHead[index] & USB_DEVICE_ECHI_DTD_POINTER_MASK);
    }
}

/*!
 * @brief Cancel the pending transfer in a specified endpoint.
 *
 * The function is used to cancel the pending transfer in a specified endpoint.
 *
 * @param ehciHandle      Pointer of the device EHCI handle.
 * @param ep               Endpoint address, bit7 is the direction of endpoint, 1U - IN, 0U - OUT.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
static usb_status_t USB_DeviceEhciCancel(usb_device_ehci_state_struct_t *ehciState, uint8_t ep)
{
    usb_device_ehci_dtd_struct_t *currentDtd;
    uint32_t primeBit = USB_GET_PRIMEBIT(ep);
    uint8_t index = USB_GET_INDEX(ep);

    OSA_SR_ALLOC();

    OSA_ENTER_CRITICAL();

    /* Get the first dtd */
    currentDtd =
        (usb_device_ehci_dtd_struct_t *)((uint32_t)ehciState->dtdHead[index] & USB_DEVICE_ECHI_DTD_POINTER_MASK);

    /* In the next loop, USB_DeviceNotificationTrigger function may trigger a new transfer and the context always
     * keep in the critical section, so the Dtd sequence would still keep non-empty and the loop would be endless.
     * We set the Dtd's dtdInvalid in this while and add an if statement in the next loop so that this issue could
     * be fixed.
     */
    while (NULL != currentDtd)
    {
        currentDtd->reservedUnion.originalBufferInfo.dtdInvalid = 1U;
        currentDtd = (usb_device_ehci_dtd_struct_t *)(currentDtd->nextDtdPointer & USB_DEVICE_ECHI_DTD_POINTER_MASK);
    }

    /* Get the first dtd */
    currentDtd =
        (usb_device_ehci_dtd_struct_t *)((uint32_t)ehciState->dtdHead[index] & USB_DEVICE_ECHI_DTD_POINTER_MASK);
    while (NULL != currentDtd)
    {
        /* this if statement is used with  the previous while loop to avoid the endless loop */
        if (0U == currentDtd->reservedUnion.originalBufferInfo.dtdInvalid)
        {
            break;
        }
        else
        {
            if (0U != (currentDtd->dtdTokenUnion.dtdTokenBitmap.status & USB_DEVICE_ECHI_DTD_STATUS_ACTIVE))
            {
                /* Flush the endpoint to stop a transfer. */
                do
                {
                    /* Set the corresponding bit(s) in the EPFLUSH register */
                    ehciState->registerBase->EPFLUSH |= primeBit;

                    /* Wait until all bits in the EPFLUSH register are cleared. */
                    while (0U != (ehciState->registerBase->EPFLUSH & primeBit))
                    {
                    }
                    /*
                     * Read the EPSR register to ensure that for all endpoints
                     * commanded to be flushed, that the corresponding bits
                     * are now cleared.
                     */
                } while (0U != (ehciState->registerBase->EPSR & primeBit));
            }

            /* Remove the dtd from the dtd in-used queue. */
            if (ehciState->dtdHead[index] == ehciState->dtdTail[index])
            {
                ehciState->dtdHead[index] = NULL;
                ehciState->dtdTail[index] = NULL;
            }
            else
            {
                ehciState->dtdHead[index] = (usb_device_ehci_dtd_struct_t *)ehciState->dtdHead[index]->nextDtdPointer;
            }

            /* When the ioc is set or the dtd queue is empty, the up layer will be notified. */

            /* Clear the token field. */
            currentDtd->dtdTokenUnion.dtdToken = 0U;
            /* Save the dtd to the free queue. */
            currentDtd->nextDtdPointer = (uint32_t)ehciState->dtdFree;
            ehciState->dtdFree         = currentDtd;
            ehciState->dtdCount++;
        }
        /* Get the next dtd. */
        currentDtd =
            (usb_device_ehci_dtd_struct_t *)((uint32_t)ehciState->dtdHead[index] & USB_DEVICE_ECHI_DTD_POINTER_MASK);
    }
    if (NULL == currentDtd)
    {
        /* Set the QH to empty. */
        ehciState->qh[index].nextDtdPointer         = USB_DEVICE_ECHI_DTD_TERMINATE_MASK;
        ehciState->qh[index].dtdTokenUnion.dtdToken = 0U;
    }
    OSA_EXIT_CRITICAL();

    return kStatus_USB_Success;
}

/*!
 * @brief Initialize a specified endpoint.
 *
 * The function is used to initialize a specified endpoint.
 *
 * @param ehciState       Pointer of the device EHCI state structure.
 * @param epInit          The endpoint initialization structure pointer.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
static usb_status_t USB_DeviceEhciEndpointInit(usb_device_ehci_state_struct_t *ehciState,
                                               usb_device_endpoint_init_struct_t *epInit)
{
    uint32_t primeBit      = USB_GET_PRIMEBIT(epInit->endpointAddress);
    uint16_t maxPacketSize = epInit->maxPacketSize & USB_MAXPACKETSIZE_MASK;
    uint8_t endpoint       = USB_EP_GET_IDX(epInit->endpointAddress);
    uint8_t direction      = USB_EP_DIR_IS_IN(epInit->endpointAddress);
    uint8_t index        = ((uint8_t)((uint32_t)endpoint << 1U)) | direction;
    uint8_t transferType = epInit->transferType & USB_ENDPOINT_TYPE_MASK;

    /* Cancel pending transfer of the endpoint */
    (void)USB_DeviceEhciCancel(ehciState, epInit->endpointAddress);

    if ((0U != (ehciState->registerBase->EPPRIME & primeBit)) || (0U != (ehciState->registerBase->EPSR & primeBit)))
    {
        return kStatus_USB_Busy;
    }

    /* Make the endpoint max packet size align with USB Specification 2.0. */
    if (USB_ENDPOINT_TYPE_ISOCHRONOUS == transferType)
    {
        if (maxPacketSize > USB_DEVICE_MAX_HS_ISO_MAX_PACKET_SIZE)
        {
            maxPacketSize = USB_DEVICE_MAX_HS_ISO_MAX_PACKET_SIZE;
        }
        ehciState->qh[index].capabilttiesCharacteristicsUnion.capabilttiesCharacteristicsBitmap.mult =
            1UL + ((((uint32_t)epInit->maxPacketSize) & USB_DESCRIPTOR_ENDPOINT_MAXPACKETSIZE_MULT_TRANSACTIONS_MASK) >>
                   USB_DESCRIPTOR_ENDPOINT_MAXPACKETSIZE_MULT_TRANSACTIONS_SHFIT);
    }
    else
    {
        ehciState->qh[index].capabilttiesCharacteristicsUnion.capabilttiesCharacteristicsBitmap.mult = 0U;
    }

    /* Save the max packet size of the endpoint */
    ehciState->qh[index].capabilttiesCharacteristicsUnion.capabilttiesCharacteristicsBitmap.maxPacketSize =
        maxPacketSize;
    ehciState->qh[index].endpointStatusUnion.endpointStatusBitmap.zlt = epInit->zlt;
    if ((USB_CONTROL_OUT_EP0 == endpoint))
    {
        /* Set ZLT bit. disable control endpoint automatic zlt by default,only send zlt when it is needed*/
        ehciState->qh[index].capabilttiesCharacteristicsUnion.capabilttiesCharacteristicsBitmap.zlt = 1U;
    }
    else
    {
        /* Set ZLT bit. */
        ehciState->qh[index].capabilttiesCharacteristicsUnion.capabilttiesCharacteristicsBitmap.zlt =
            ((0U == epInit->zlt) ? 1U : 0U);
    }

    /* Enable the endpoint. */
    if ((USB_CONTROL_OUT_EP0 == endpoint))
    {
        ehciState->qh[index].capabilttiesCharacteristicsUnion.capabilttiesCharacteristicsBitmap.ios = 1U;
        ehciState->registerBase->EPCR0 |=
            ((0U != direction) ?
                 (USBHS_EPCR_TXE_MASK | USBHS_EPCR_TXR_MASK | ((uint32_t)transferType << USBHS_EPCR_TXT_SHIFT)) :
                 (USBHS_EPCR_RXE_MASK | USBHS_EPCR_RXR_MASK | ((uint32_t)transferType << USBHS_EPCR_RXT_SHIFT)));
    }
    else
    {
        ehciState->qh[index].capabilttiesCharacteristicsUnion.capabilttiesCharacteristicsBitmap.ios = 0U;
        ehciState->registerBase->EPCR[endpoint - 1U] |=
            ((0U != direction) ?
                 (USBHS_EPCR_TXE_MASK | USBHS_EPCR_TXR_MASK | ((uint32_t)transferType << USBHS_EPCR_TXT_SHIFT)) :
                 (USBHS_EPCR_RXE_MASK | USBHS_EPCR_RXR_MASK | ((uint32_t)transferType << USBHS_EPCR_RXT_SHIFT)));
    }

    ehciState->qh[index].endpointStatusUnion.endpointStatusBitmap.isOpened = 1U;
    return kStatus_USB_Success;
}

/*!
 * @brief De-initialize a specified endpoint.
 *
 * The function is used to de-initialize a specified endpoint.
 * Current transfer of the endpoint will be cancelled and the specified endpoint will be disabled.
 *
 * @param ehciState       Pointer of the device EHCI state structure.
 * @param ep               The endpoint address, Bit7, 0U - USB_OUT, 1U - USB_IN.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
static usb_status_t USB_DeviceEhciEndpointDeinit(usb_device_ehci_state_struct_t *ehciState, uint8_t ep)
{
    uint32_t primeBit = USB_GET_PRIMEBIT(ep);
    uint8_t endpoint = USB_EP_GET_IDX(ep);
    uint8_t direction = USB_EP_DIR_IS_IN(ep);
    uint8_t index = USB_GET_INDEX(ep);

    ehciState->qh[index].endpointStatusUnion.endpointStatusBitmap.isOpened = 0U;

    /* Cancel the transfer of the endpoint */
    (void)USB_DeviceEhciCancel(ehciState, ep);
    if ((0U != (ehciState->registerBase->EPPRIME & primeBit)) || (0U != (ehciState->registerBase->EPSR & primeBit)))
    {
        return kStatus_USB_Busy;
    }

    /* Clear endpoint state */
    ehciState->qh[index].capabilttiesCharacteristicsUnion.capabilttiesCharacteristics = 0U;
    /* Disable the endpoint */
    if (0U == endpoint)
    {
        ehciState->registerBase->EPCR0 &=
            ~((0U != direction) ? (USBHS_EPCR_TXE_MASK | USBHS_EPCR_TXT_MASK | USBHS_EPCR_TXS_MASK) :
                                  (USBHS_EPCR_RXE_MASK | USBHS_EPCR_RXT_MASK | USBHS_EPCR_RXS_MASK));
    }
    else
    {
        ehciState->registerBase->EPCR[endpoint - 1U] &=
            ~((0U != direction) ? (USBHS_EPCR_TXE_MASK | USBHS_EPCR_TXT_MASK | USBHS_EPCR_TXS_MASK) :
                                  (USBHS_EPCR_RXE_MASK | USBHS_EPCR_RXT_MASK | USBHS_EPCR_RXS_MASK));
    }

    return kStatus_USB_Success;
}

/*!
 * @brief Stall a specified endpoint.
 *
 * The function is used to stall a specified endpoint.
 * Current transfer of the endpoint will be cancelled and the specified endpoint will be stalled.
 *
 * @param ehciState       Pointer of the device EHCI state structure.
 * @param ep               The endpoint address, Bit7, 0U - USB_OUT, 1U - USB_IN.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
static usb_status_t USB_DeviceEhciEndpointStall(usb_device_ehci_state_struct_t *ehciState, uint8_t ep)
{
    uint8_t endpoint = USB_EP_GET_IDX(ep);
    uint8_t direction = USB_EP_DIR_IS_IN(ep);

    if (0U == endpoint)
    {
        /* Cancel the transfer of the endpoint */
        (void)USB_DeviceEhciCancel(ehciState, 0x00);
        (void)USB_DeviceEhciCancel(ehciState, 0x80);
        ehciState->registerBase->EPCR0 |= (USBHS_EPCR_TXS_MASK | USBHS_EPCR_RXS_MASK);
    }
    else
    {
        /* Cancel the transfer of the endpoint */
        (void)USB_DeviceEhciCancel(ehciState, ep);

        ehciState->registerBase->EPCR[endpoint - 1U] |= ((0U != direction) ? USBHS_EPCR_TXS_MASK : USBHS_EPCR_RXS_MASK);
    }

    return kStatus_USB_Success;
}

/*!
 * @brief Un-stall a specified endpoint.
 *
 * The function is used to un-stall a specified endpoint.
 * Current transfer of the endpoint will be cancelled and the specified endpoint will be un-stalled.
 *
 * @param ehciState       Pointer of the device EHCI state structure.
 * @param ep               The endpoint address, Bit7, 0U - USB_OUT, 1U - USB_IN.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
static usb_status_t USB_DeviceEhciEndpointUnstall(usb_device_ehci_state_struct_t *ehciState, uint8_t ep)
{
    uint8_t endpoint = USB_EP_GET_IDX(ep);
    uint8_t direction = USB_EP_DIR_IS_IN(ep);

    /* Clear the endpoint stall state */
    if (0U == endpoint)
    {
        ehciState->registerBase->EPCR0 &= ~((0U != direction) ? USBHS_EPCR_TXS_MASK : USBHS_EPCR_RXS_MASK);
    }
    else
    {
        ehciState->registerBase->EPCR[endpoint - 1U] &=
            ~((0U != direction) ? USBHS_EPCR_TXS_MASK : USBHS_EPCR_RXS_MASK);
        ehciState->registerBase->EPCR[endpoint - 1U] |= ((0U != direction) ? USBHS_EPCR_TXR_MASK : USBHS_EPCR_RXR_MASK);
    }
    /* Cancel the transfer of the endpoint */
    (void)USB_DeviceEhciCancel(ehciState, ep);

    return kStatus_USB_Success;
}

/*!
 * @brief Set device controller state to default state.
 *
 * The function is used to set device controller state to default state.
 * The function will be called when USB_DeviceEhciInit called or the control type kUSB_DeviceControlGetEndpointStatus
 * received in USB_DeviceEhciControl.
 *
 * @param ehciState       Pointer of the device EHCI state structure.
 *
 */
static void USB_DeviceEhciSetDefaultState(usb_device_ehci_state_struct_t *ehciState)
{
    usb_device_ehci_dtd_struct_t *p;

    for (uint8_t count = 0U; count < USB_DEVICE_ENDPOINTS; count++)
    {
        (void)USB_DeviceEhciEndpointDeinit(ehciState, (count | USB_EP_DIR_IN));
        (void)USB_DeviceEhciEndpointDeinit(ehciState, (count | USB_EP_DIR_OUT));
    }

    /* Initialize the dtd free queue */
    ehciState->dtdFree = ehciState->dtd;
    p                  = ehciState->dtdFree;
    for (uint32_t i = 1U; i < USB_DEVICE_MAX_DTD; i++)
    {
        p->nextDtdPointer = (uint32_t)&ehciState->dtd[i];
        p                 = (usb_device_ehci_dtd_struct_t *)p->nextDtdPointer;
    }
    p->nextDtdPointer   = 0U;
    ehciState->dtdCount = USB_DEVICE_MAX_DTD;

    /* Not use interrupt threshold. */
    ehciState->registerBase->USBCMD &= ~USBHS_USBCMD_ITC_MASK;
    ehciState->registerBase->USBCMD |= USBHS_USBCMD_ITC(0U);

    /* Disable setup lockout, please refer to "Control Endpoint Operation" section in RM. */
    ehciState->registerBase->USBMODE |= USBHS_USBMODE_SLOM_MASK;

/* Set the endian by using CPU's endian */
#if (ENDIANNESS == USB_BIG_ENDIAN)
    ehciState->registerBase->USBMODE |= USBHS_USBMODE_ES_MASK;
#else
    ehciState->registerBase->USBMODE &= ~USBHS_USBMODE_ES_MASK;
#endif
    /* Initialize the QHs of endpoint. */
    for (uint32_t i = 0U; i < (USB_DEVICE_ENDPOINTS * 2U); i++)
    {
        ehciState->qh[i].nextDtdPointer = USB_DEVICE_ECHI_DTD_TERMINATE_MASK;
        ehciState->qh[i].capabilttiesCharacteristicsUnion.capabilttiesCharacteristicsBitmap.maxPacketSize =
            USB_CTRL_EP_MPS;
        ehciState->dtdHead[i]                                              = NULL;
        ehciState->dtdTail[i]                                              = NULL;
        ehciState->qh[i].endpointStatusUnion.endpointStatusBitmap.isOpened = 0U;
    }

    /* Add QH buffer address to USBHS_EPLISTADDR_REG */
    ehciState->registerBase->EPLISTADDR = (uint32_t)ehciState->qh;

    /* Clear device address */
    ehciState->registerBase->DEVICEADDR = 0U;

#if defined(USB_DEVICE_CONFIG_DETACH_ENABLE) && (USB_DEVICE_CONFIG_DETACH_ENABLE > 0U)
    ehciState->registerBase->OTGSC = ehciState->registerBase->OTGSC & 0x0000FFFFU;
    ehciState->registerBase->OTGSC |= USBHS_OTGSC_BSVIE_MASK;
#endif /* USB_DEVICE_CONFIG_DETACH_ENABLE */

    /* Enable USB Interrupt, USB Error Interrupt, Port Change detect Interrupt, USB-Reset Interrupt*/
    ehciState->registerBase->USBINTR =
        (USBHS_USBINTR_UE_MASK | USBHS_USBINTR_UEE_MASK | USBHS_USBINTR_PCE_MASK | USBHS_USBINTR_URE_MASK
#if (defined(USB_DEVICE_CONFIG_LOW_POWER_MODE) && (USB_DEVICE_CONFIG_LOW_POWER_MODE > 0U))
         | USBHS_USBINTR_SLE_MASK
#endif /* USB_DEVICE_CONFIG_LOW_POWER_MODE */
        );

    /* Clear reset flag */
    ehciState->isResetting = 0U;
}

static void usb_device_set_prime(usb_device_ehci_state_struct_t *ehciState, uint8_t ep)
{
    uint32_t index = USB_GET_INDEX(ep);
    uint32_t primeBit;
    usb_device_ehci_dtd_struct_t *currentDtd;
    /* Get the next in-used dtd */
    currentDtd = (usb_device_ehci_dtd_struct_t *)((uint32_t)ehciState->dtdHead[index] &
                                                    USB_DEVICE_ECHI_DTD_POINTER_MASK);

    if ((NULL != currentDtd) && (0U != (currentDtd->dtdTokenUnion.dtdTokenBitmap.status &
                                        USB_DEVICE_ECHI_DTD_STATUS_ACTIVE)))
    {   
        primeBit = USB_GET_PRIMEBIT(ep);

        /* Try to prime the next dtd. */
        ehciState->registerBase->EPPRIME = primeBit;

        /* Whether the endpoint transmit/receive buffer is ready or not. If not, wait for prime bit
            * cleared and prime the next dtd. */
        if (0U == (ehciState->registerBase->EPSR & primeBit))
        {
            /* Wait for the endpoint prime bit cleared by HW */
            while (0U != (ehciState->registerBase->EPPRIME & primeBit))
            {
            }

            /* If the endpoint transmit/receive buffer is not ready */
            if (0U == (ehciState->registerBase->EPSR & primeBit))
            {
                /* Prime next dtd and prime the transfer */
                ehciState->qh[index].nextDtdPointer         = (uint32_t)currentDtd;
                ehciState->qh[index].dtdTokenUnion.dtdToken = 0U;
                ehciState->registerBase->EPPRIME            = primeBit;
            }
        }
    }
}

/***********************************usb dc func**********************************************/
#if CONFIG_USB_HS
/* pointer to high speed mode descriptor */
uint8_t *USB_Descriptor_HS = 0;

void usbd_desc_hs_register(uint8_t *descriptor)
{
    if(descriptor)
    {
        USB_Descriptor_HS = descriptor;
    }
}
#endif

int usb_dtd_buf_init(uint8_t instance)
{
    usb_dtd_buffer_t *ptr = g_UsbDtdBufList[instance];
    for(int i = 0; i < USB_DEVICE_MAX_DTD; i++)
    {
        ptr[i].sta = 0;
        ptr[i].pbuf = 0;
        ptr[i].mps = 0;
    }

    return 0;
}

uint8_t usb_dtd_buf_set_manual(uint8_t ep)
{
    usb_dtd_buffer_t *ptr = g_UsbDtdBufList[0];
    uint32_t index = USB_GET_INDEX(ep);

    ptr[index].type = 2;

    return 0;
}

int usb_transfer_data(uint8_t ep, uint8_t *data, uint16_t len)
{
    usb_dtd_buffer_t *ptr = g_UsbDtdBufList[0];
    usb_device_ehci_state_struct_t *ehciState = &g_UsbDeviceEhciState[0];
    uint32_t index = USB_GET_INDEX(ep);
    uint8_t *pdata = data;

    switch (ptr[index].type)
    {
    case 1:
        if((ptr[index].sta != 1) || (len > ptr[index].mps))
        {
            return -1;
        }

        pdata = ptr[index].pbuf;
        for(int i = 0; i < len; i++)
        {
            pdata[i] = data[i];
        }
    case 2:
        if(USB_DeviceEhciTransfer(ehciState, ep, pdata, len) != kStatus_USB_Success)
        {
            return -1;
        }
        break;
    default:
        return -1;
    }

    
    return 0;
}

void usb_connect(uint8_t instance, uint8_t ctrl)
{
    IRQn_Type irqNum;
    USB_Type *pdevice = g_UsbDeviceEhciState[instance].registerBase;

    if(instance == 0)
    {
        irqNum = USB_OTG1_IRQn;
    }
    else
    {
        irqNum = USB_OTG2_IRQn;
    }

    if(ctrl)
    {
        /* Install isr, set priority, and enable IRQ. */
        NVIC_SetPriority((IRQn_Type)irqNum, 3);
        EnableIRQ((IRQn_Type)irqNum);

        pdevice->USBCMD |= USBHS_USBCMD_RS_MASK;
    }
    else
    {
        pdevice->USBCMD &= ~USBHS_USBCMD_RS_MASK;
        DisableIRQ((IRQn_Type)irqNum);
    }
}

static uint8_t usb_device_init(uint8_t instance)
{
    USB_Type *pdevice;
    usb_device_ehci_state_struct_t *ehciState = NULL;

    if(instance == 0)
    {
        ehciState = &g_UsbDeviceEhciState[instance];
        ehciState->controllerId = 1;
        ehciState->qh = (usb_device_ehci_qh_struct_t *)&qh_buffer[instance * 2048];
        ehciState->dtd = s_UsbDeviceEhciDtd[instance];
        ehciState->registerBase = USB1;
    #if (defined(USB_DEVICE_CONFIG_LOW_POWER_MODE) && (USB_DEVICE_CONFIG_LOW_POWER_MODE > 0U))
        ehciState->registerPhyBase = USBPHY1;
    #if (defined(FSL_FEATURE_SOC_USBNC_COUNT) && (FSL_FEATURE_SOC_USBNC_COUNT > 0U))
        ehciState->registerNcBase = USBNC1;
    #endif
    #endif
    }
    else
    {
        return 1;
    }

    pdevice = ehciState->registerBase;

    /* Reset the controller. */
    pdevice->USBCMD |= USBHS_USBCMD_RST_MASK;
    while (0U != (pdevice->USBCMD & USBHS_USBCMD_RST_MASK))
    {
    }
    /* Get the HW's endpoint count */
    ehciState->endpointCount =
        (uint8_t)((pdevice->DCCPARAMS & USB_DCCPARAMS_DEN_MASK) >> USB_DCCPARAMS_DEN_SHIFT);
    if(ehciState->endpointCount != USB_DEVICE_ENDPOINTS)
    {
        return 1;
    }
    /* Clear the controller mode field and set to device mode. */
    pdevice->USBMODE &= ~USBHS_USBMODE_CM_MASK;
    pdevice->USBMODE |= USBHS_USBMODE_CM(0x02U) | USB_USBMODE_SLOM(0x01);

#ifndef CONFIG_USB_HS
    pdevice->PORTSC1 |= (0x01u << 24);  //fixed full-speed
#endif // !CONFIG_USB_HS

    /* Set the EHCI to default status. */
    USB_DeviceEhciSetDefaultState(ehciState);

    return 0;
}

static uint8_t usb_device_deinit(uint8_t instance)
{
    USB_Type *pdevice;
    usb_device_ehci_state_struct_t *ehciState = NULL;
    if(instance == 1)
    {
        ehciState = &g_UsbDeviceEhciState[instance];
        pdevice = ehciState->registerBase;
    }
    else
    {
        return 1;
    }

    /* Disable all interrupt. */
    pdevice->USBINTR = 0U;
    /* Stop the device functionality. */
    pdevice->USBCMD &= ~USBHS_USBCMD_RS_MASK;
    /* Reset the controller. */
    pdevice->USBCMD |= USBHS_USBCMD_RST_MASK;

    usb_connect(instance, 0);

    return 0;
}

int usb_dc_init(void)
{
    usb_device_ehci_state_struct_t *ehciState = &g_UsbDeviceEhciState[0];

    memset(ehciState, 0, sizeof(usb_device_ehci_state_struct_t));

    usb_clock_init(0);
    usb_device_deinit(0);
    usb_device_init(0);
    usb_dtd_buf_init(0);

    usb_connect(0, 1);
    return 0;
}

int usbd_set_address(const uint8_t addr)
{
    usb_device_ehci_state_struct_t *ehciState = &g_UsbDeviceEhciState[0];
    if(addr)
    {
        /* set address after IN transaction */
        ehciState->registerBase->DEVICEADDR = (((uint32_t)addr << USB_DEVICEADDR_USBADR_SHIFT) | USB_DEVICEADDR_USBADRA_MASK);
    }
    else
    {
        /* immediately set address */
        ehciState->registerBase->DEVICEADDR = (uint32_t)addr << USB_DEVICEADDR_USBADR_SHIFT;
    }

#if CONFIG_USB_HS
    if((ehciState->speed == USB_SPEED_HIGH) && USB_Descriptor_HS)
    {
        /* set descriptor to high speed */
        usbd_desc_register(USB_Descriptor_HS);
    }
#endif
    
    return 0;
}

int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
    usb_device_ehci_state_struct_t *ehciState = &g_UsbDeviceEhciState[0];
    usb_device_endpoint_init_struct_t epConfig;
    usb_dtd_buffer_t *ptr = g_UsbDtdBufList[0];
    uint32_t index = USB_GET_INDEX(ep_cfg->ep_addr);

    epConfig.endpointAddress = ep_cfg->ep_addr;
    epConfig.maxPacketSize = ep_cfg->ep_mps;
    epConfig.transferType = ep_cfg->ep_type;
    epConfig.zlt = 0;
    epConfig.interval = 0;

    if(kStatus_USB_Success != USB_DeviceEhciEndpointDeinit(ehciState, ep_cfg->ep_addr))
    {
        return -1;
    }

    if(USB_DeviceEhciEndpointInit(ehciState, &epConfig) != kStatus_USB_Success)
    {
        return -1;
    }

    if(ptr[index].type == 0)
    {
        if(ptr[index].pbuf == 0)
        {
            ptr[index].pbuf = malloc(ep_cfg->ep_mps);
            if(ptr[index].pbuf == 0)
            {
                return -1;
            }
            ptr[index].mps = ep_cfg->ep_mps;
            ptr[index].type = 1; /* set to auto mode */
            ptr[index].sta = 1; /* ready */
        }
    }

    if((ptr[index].type == 1) && (ep_cfg->ep_addr != USB_CONTROL_OUT_EP0))
    {
        /* is auto */
        if(USB_EP_DIR_IS_OUT(ep_cfg->ep_addr))
        {
            if(USB_DeviceEhciTransfer(ehciState, ep_cfg->ep_addr, ptr[index].pbuf, ptr[index].mps) != kStatus_USB_Success)
            {
                return -1;
            }
            ptr[index].sta = 2; /* using */
        }
    }

    return 0;
}

int usbd_ep_close(const uint8_t ep)
{
    usb_device_ehci_state_struct_t *ehciState = &g_UsbDeviceEhciState[0];

    if(USB_DeviceEhciEndpointDeinit(ehciState, ep) != kStatus_USB_Success)
    {
        return -1;
    }
    return 0;
}

int usbd_ep_set_stall(const uint8_t ep)
{
    usb_device_ehci_state_struct_t *ehciState = &g_UsbDeviceEhciState[0];

    if(USB_DeviceEhciEndpointStall(ehciState, ep) != kStatus_USB_Success)
    {
        return -1;
    }
    return 0;
}

int usbd_ep_clear_stall(const uint8_t ep)
{
    usb_device_ehci_state_struct_t *ehciState = &g_UsbDeviceEhciState[0];

    if(USB_DeviceEhciEndpointUnstall(ehciState, ep) != kStatus_USB_Success)
    {
        return -1;
    }
    return 0;
}

int usbd_ep_is_stalled(const uint8_t ep, uint8_t *stalled)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t ep_dir = USB_EP_GET_DIR(ep);
    usb_device_ehci_state_struct_t *ehciState = &g_UsbDeviceEhciState[0];

    if(ep_idx == 0)
    {
        if(ep_dir)
        {
            *stalled = (ehciState->registerBase->ENDPTCTRL0 & 0x00000001) ? 1 : 0;
        }
        else
        {
            *stalled = (ehciState->registerBase->ENDPTCTRL0 & 0x00010000) ? 1 : 0;
        }
    }
    else
    {
        if(ep_dir)
        {
            *stalled = (ehciState->registerBase->ENDPTCTRL[ep_idx - 1] & 0x00000001) ? 1 : 0;
        }
        else
        {
            *stalled = (ehciState->registerBase->ENDPTCTRL[ep_idx - 1] & 0x00010000) ? 1 : 0;
        }
    }
    return 0;
}

int usbd_ep_write(const uint8_t ep, const uint8_t *data, uint32_t data_len, uint32_t *ret_bytes)
{
    uint8_t *txbuf = NULL;
    uint8_t index = USB_GET_INDEX(ep);
    usb_dtd_buffer_t *ptr = g_UsbDtdBufList[0];
    usb_device_ehci_state_struct_t *ehciState = &g_UsbDeviceEhciState[0];

    *ret_bytes = 0;

    switch(ptr[index].type)
    {
        case 1:
            txbuf = ptr[index].pbuf;
            data_len = (data_len > ptr[index].mps) ? ptr[index].mps : data_len;
            for(int i = 0; i < data_len; i++)
            {
                txbuf[i] = data[i];
            }
            break;
        case 2:
            data_len = (data_len > ptr[index].mps) ? ptr[index].mps : data_len;
            txbuf = (uint8_t *)data;
            break;
        default:
            return -1;
    }

    if(USB_DeviceEhciTransfer(ehciState, ep, txbuf, data_len) != kStatus_USB_Success)
    {
        return -1;
    }

    *ret_bytes = data_len;
    return 0;
}

int usbd_ep_read(const uint8_t ep, uint8_t *data, uint32_t max_data_len, uint32_t *read_bytes)
{
    uint8_t index = USB_GET_INDEX(ep);
    usb_dtd_buffer_t *ptr = g_UsbDtdBufList[0];
    uint8_t *temp;
    uint32_t count;
    usb_device_ehci_dtd_struct_t *currentDtd;
    usb_device_ehci_state_struct_t *ehciState = &g_UsbDeviceEhciState[0];

    if (USB_EP_DIR_IS_IN(ep))
    {
        return -1;
    }

    if (!data && max_data_len) {
        return -1;
    }

    if (!max_data_len) {
        return 0;
    }

    if((ep == USB_CONTROL_OUT_EP0) && (max_data_len == 8) && !read_bytes) /* is read setup packet */
    {
        /* Cancel the data phase transfer */
        USB_DeviceEhciCancelControlPipe(ehciState, index);
        /* Cancel the status phase transfer */
        USB_DeviceEhciCancelControlPipe(ehciState, index);
        USB_DeviceEhciGetSetupData(ehciState, index, data);

        struct usb_setup_packet *setup = (struct usb_setup_packet *)data;

        if(setup->bmRequestType & USB_REQUEST_DIR_IN)
        {
            /* is in */
            if(USB_DeviceEhciTransfer(ehciState, USB_CONTROL_OUT_EP0, ptr[index].pbuf, 0) != kStatus_USB_Success)
            {
                return -2;
            }
        }
        else
        {
            /* is out */
            if(setup->wLength)
            {
                if(setup->wLength > ptr[index].mps)
                {
                    if(ptr[index].pbuf != NULL)
                    {
                        usb_free(ptr[index].pbuf);
                    }
                    ptr[index].mps = setup->wLength;
                    ptr[index].pbuf = usb_malloc(setup->wLength);
                }
                if(!ptr[index].pbuf)
                {
                    return -3;
                }
                if(USB_DeviceEhciTransfer(ehciState, USB_CONTROL_OUT_EP0, ptr[index].pbuf, setup->wLength) != kStatus_USB_Success)
                {
                    return -4;
                }
                ptr[index].sta = 2; /* using */
            }
        }
    }
    else
    {
        currentDtd = ehciState->dtdHead[index];
        if (NULL != currentDtd)
        {
            /* Don't handle the active dtd. */
            if ((0U == (currentDtd->dtdTokenUnion.dtdTokenBitmap.status & USB_DEVICE_ECHI_DTD_STATUS_ACTIVE)) &&
                (0U != currentDtd->dtdTokenUnion.dtdTokenBitmap.ioc))
            {
                count = (currentDtd->reservedUnion.originalBufferInfo.originalBufferLength -
                                            currentDtd->dtdTokenUnion.dtdTokenBitmap.totalBytes);
                count = (count <= max_data_len) ? count : max_data_len;
                temp = (uint8_t *)((currentDtd->bufferPointerPage[0] & USB_DEVICE_ECHI_DTD_PAGE_MASK) |
                                    (currentDtd->reservedUnion.originalBufferInfo.originalBufferOffest));
                /* Save the transfer buffer address */
                if(data != NULL)
                {
                    if(ptr[index].type == 1)
                    {
                        for(int i = 0; i < count; i++)
                        {
                            *data = temp[i];
                            data++;
                        }
                    }
                    else if(ptr[index].type == 2)
                    {
                        data = temp;
                    }
                }
                /* Save the transferred data length */
                if (read_bytes)
                {
                    *read_bytes = count;
                }

                /* Move the dtd queue head pointer to next */
                if (ehciState->dtdHead[index] == ehciState->dtdTail[index])
                {
                    ehciState->dtdHead[index]                   = NULL;
                    ehciState->dtdTail[index]                   = NULL;
                    ehciState->qh[index].nextDtdPointer         = USB_DEVICE_ECHI_DTD_TERMINATE_MASK;
                    ehciState->qh[index].dtdTokenUnion.dtdToken = 0U;
                }
                else
                {
                    ehciState->dtdHead[index] =
                        (usb_device_ehci_dtd_struct_t *)ehciState->dtdHead[index]->nextDtdPointer;
                }

                /* Clear the token field of the dtd */
                currentDtd->dtdTokenUnion.dtdToken = 0U;
                currentDtd->nextDtdPointer         = (uint32_t)ehciState->dtdFree;
                ehciState->dtdFree                 = currentDtd;
                ehciState->dtdCount++;

                if(USB_EP_GET_IDX(ep) && (ptr[index].type == 1))  /* if not ep0 */
                {
                    if(USB_DeviceEhciTransfer(ehciState, ep, ptr[index].pbuf, ptr[index].mps) != kStatus_USB_Success)
                    {
                        return -5;
                    }
                    ptr[index].sta = 2; /* using */
                }
            }
            else
            {
                *read_bytes = 0;
                return 0;
            }
        }
        else
        {
            return -6;
        }
    }

    return 0;
}

void usb_device_isr_func(usb_device_ehci_state_struct_t *ehciState)
{
    USB_Type *device = ehciState->registerBase;
    usb_device_ehci_dtd_struct_t *currentDtd;
    uint8_t index = 0;
    uint32_t int_stat;
    uint32_t ep_setup_stat;
    uint32_t ep_int_stat;
    uint8_t endpoint;

#if ((defined(USB_DEVICE_CONFIG_LOW_POWER_MODE)) && (USB_DEVICE_CONFIG_LOW_POWER_MODE > 0U))
    USBNC_Type *usbnc = p->registerNcBase;
    if (0U != (usbnc->USB_OTGn_CTRL & USBNC_USB_OTGn_CTRL_WIE_MASK))
    {
        if (0U != (usbnc->USB_OTGn_CTRL & USBNC_USB_OTGn_CTRL_WIR_MASK))
        {
            usbnc->PORTSC1 &= ~USBHS_PORTSC1_PHCD_MASK;
            usbnc->USB_OTGn_CTRL &= ~USBNC_USB_OTGn_CTRL_WIE_MASK;
        }
    }
    else
    {
    }
#endif

    int_stat = device->USBSTS;
    int_stat &= device->USBINTR;

    device->USBSTS = int_stat;

#if defined(USB_DEVICE_CONFIG_ERROR_HANDLING) && (USB_DEVICE_CONFIG_ERROR_HANDLING > 0U)
    if (0U != (status & USBHS_USBSTS_UEI_MASK))
    {
        /* Error interrupt */
        // USB_DeviceEhciInterruptError(ehciState);
    }
#endif /* USB_DEVICE_CONFIG_ERROR_HANDLING */

    if (0U != (int_stat & USBHS_USBSTS_URI_MASK))
    {
        /* Reset interrupt */
        uint32_t status = 0U;

        /* Clear the setup flag */
        status                             = ehciState->registerBase->EPSETUPSR;
        ehciState->registerBase->EPSETUPSR = status;
        /* Clear the endpoint complete flag */
        status                              = ehciState->registerBase->EPCOMPLETE;
        ehciState->registerBase->EPCOMPLETE = status;

        do
        {
            /* Flush the pending transfers */
            ehciState->registerBase->EPFLUSH = USBHS_EPFLUSH_FERB_MASK | USBHS_EPFLUSH_FETB_MASK;
        } while (0U != (ehciState->registerBase->EPPRIME & (USBHS_EPPRIME_PERB_MASK | USBHS_EPPRIME_PETB_MASK)));

        /* Whether is the port reset. If yes, set the isResetting flag. Or, notify the up layer. */
        if (0U != (ehciState->registerBase->PORTSC1 & USBHS_PORTSC1_PR_MASK))
        {
            ehciState->isResetting = 1U;
        }
        else
        {
            USB_DeviceEhciSetDefaultState(ehciState);
            usbd_event_notify_handler(USBD_EVENT_RESET, NULL);
        }
    }

    if (0U != (int_stat & USBHS_USBSTS_UI_MASK))
    {
        /* Token done interrupt */
        /* Get the EPSETUPSR to check the setup packect received in which one endpoint. */
        ep_setup_stat = device->ENDPTSETUPSTAT;

        if(ep_setup_stat != 0)
        {
            /* read setup data */
            for (endpoint = 0U; endpoint < USB_DEVICE_ENDPOINTS; endpoint++)
            {
                if (0U != (ep_setup_stat & (1UL << endpoint)))
                {
                    usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);
                }
            }
        }
        /* Read the USBHS_EPCOMPLETE_REG to get the endpoint transfer done status */
        ep_int_stat = device->ENDPTCOMPLETE;
        /* Clear the endpoint transfer done status */
        device->ENDPTCOMPLETE = ep_int_stat;
        if (0U != ep_int_stat)
        {
            for (uint8_t endpoint = 0U; endpoint < 8; endpoint++)
            {
                volatile uint32_t tmp = (0x00010001UL << endpoint);
                /* Check the transfer is done or not in the specified endpoint. */
                if (ep_int_stat & tmp)
                {
                    if(ep_int_stat & 0x0000FFFFu)
                    {
                        /* out int */
                        index = endpoint << 1;
                        
                        if(endpoint == 0) /* is ep0 */
                        {
                            usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
                        }
                        else
                        {
                            usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)USB_ENDPOINT_OUT(endpoint));
                        }
                    }

                    if(ep_int_stat & 0xFFFF0000u)
                    {
                        /* in int */
                        index = (endpoint << 1) + 1;

                        /* Get the in-used dtd of the specified endpoint. */
                        currentDtd = (usb_device_ehci_dtd_struct_t *)((uint32_t)ehciState->dtdHead[index] &
                                                                                USB_DEVICE_ECHI_DTD_POINTER_MASK);
                        
                        /* Don't handle the active dtd. */
                        if ((0U == (currentDtd->dtdTokenUnion.dtdTokenBitmap.status & USB_DEVICE_ECHI_DTD_STATUS_ACTIVE)) &&
                            (0U != currentDtd->dtdTokenUnion.dtdTokenBitmap.ioc))
                        {
                            /* Move the dtd queue head pointer to next */
                            if (ehciState->dtdHead[index] == ehciState->dtdTail[index])
                            {
                                ehciState->dtdHead[index]                   = NULL;
                                ehciState->dtdTail[index]                   = NULL;
                                ehciState->qh[index].nextDtdPointer         = USB_DEVICE_ECHI_DTD_TERMINATE_MASK;
                                ehciState->qh[index].dtdTokenUnion.dtdToken = 0U;
                            }
                            else
                            {
                                ehciState->dtdHead[index] =
                                    (usb_device_ehci_dtd_struct_t *)ehciState->dtdHead[index]->nextDtdPointer;
                            }

                            /* Clear the token field of the dtd */
                            currentDtd->dtdTokenUnion.dtdToken = 0U;
                            currentDtd->nextDtdPointer         = (uint32_t)ehciState->dtdFree;
                            ehciState->dtdFree                 = currentDtd;
                            ehciState->dtdCount++;
                        }
                        
                        if(endpoint == 0)  /* is ep0 */
                        {
                            usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
                        }
                        else
                        {
                            usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)USB_ENDPOINT_IN(endpoint));
                        }
                    }
                }
            }
        }
    }

    if (0U != (int_stat & USBHS_USBSTS_PCI_MASK))
    {
        /* Port status change interrupt */
        /* Whether the port is doing reset. */
        if (0U == (device->PORTSC1 & USBHS_PORTSC1_PR_MASK))
        {
            /* If not, update the USB speed. */
            if (0U != (device->PORTSC1 & USBHS_PORTSC1_HSP_MASK))
            {
                ehciState->speed = USB_SPEED_HIGH;
            }
            else
            {
                ehciState->speed = USB_SPEED_FULL;
            }

            /* If the device reset flag is non-zero, notify the up layer the device reset finished. */
            if (0U != ehciState->isResetting)
            {
                USB_DeviceEhciSetDefaultState(ehciState);
                usbd_event_notify_handler(USBD_EVENT_RESET, NULL);
                ehciState->isResetting = 0U;
            }
        }

#if (defined(USB_DEVICE_CONFIG_LOW_POWER_MODE) && (USB_DEVICE_CONFIG_LOW_POWER_MODE > 0U))
        if ((0U != ehciState->isSuspending) && (0U == (device->PORTSC1 & USB_PORTSC1_SUSP_MASK)))
        {
            /* Set the resume flag */
            ehciState->isSuspending = 0U;
            usbd_event_notify_handler(USBD_EVENT_RESUME, NULL);
        }
#endif /* USB_DEVICE_CONFIG_LOW_POWER_MODE */
        }
#if (defined(USB_DEVICE_CONFIG_LOW_POWER_MODE) && (USB_DEVICE_CONFIG_LOW_POWER_MODE > 0U))
    if (0U != (int_stat & USBHS_USBSTS_SLI_MASK))
    {
        /* Suspend interrupt */
        // USB_DeviceEhciInterruptSuspend(ehciState);
        usbd_event_notify_handler(USBD_EVENT_SUSPEND, NULL);
    }
#endif /* USB_DEVICE_CONFIG_LOW_POWER_MODE */

    if (0U != (int_stat & USBHS_USBSTS_SRI_MASK))
    {
        /* Sof interrupt */
        // USB_DeviceEhciInterruptSof(int_stat);
        usbd_event_notify_handler(USBD_EVENT_SOF, NULL);
    }
}

void USB_OTG1_IRQHandler(void)
{
    usb_device_isr_func(&g_UsbDeviceEhciState[0]);
    /* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
    exception return operation might vector to incorrect interrupt */
    __DSB();
}

void USB_OTG2_IRQHandler(void)
{
    usb_device_isr_func(&g_UsbDeviceEhciState[1]);
    /* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
    exception return operation might vector to incorrect interrupt */
    __DSB();
}

/*
 * usbd_composite.c
 *
 *  Created on: Jul 3, 2026
 *      Author: haodu
 *
 * Composite USB class: HID Keyboard (Interface 0) + CDC ACM (Interfaces 1-2).
 *
 * This file replaces usbd_hid.c as the single registered class.
 * It owns the full Configuration Descriptor, the HID Report Descriptor,
 * and all class-specific control requests for both HID and CDC sub-classes.
 *
 * Endpoint map:
 *   EP1 IN  0x81  HID Keyboard Interrupt IN  8 bytes  10 ms
 *   EP2 IN  0x82  CDC Notification  IN        8 bytes  16 ms
 *   EP3 OUT 0x03  CDC Data OUT               64 bytes
 *   EP3 IN  0x83  CDC Data IN                64 bytes
 */

/* Includes ------------------------------------------------------------------*/
#include "usbd_composite.h"
#include "usbd_ctlreq.h"
#include "hid_keyboard_app.h"
#include "vendor_cmd.h"
#include <stddef.h>
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define HID_DESCRIPTOR_TYPE            0x21U
#define HID_REPORT_DESC_TYPE           0x22U
#define HID_KEYBOARD_REPORT_DESC_SIZE  45U

#define HID_REQ_SET_PROTOCOL           0x0BU
#define HID_REQ_GET_PROTOCOL           0x03U
#define HID_REQ_SET_IDLE               0x0AU
#define HID_REQ_GET_IDLE               0x02U

/* Interface numbers */
#define COMP_IF_HID                    0U
#define COMP_IF_CDC_COMM               1U
#define COMP_IF_CDC_DATA               2U

/* Private variables ---------------------------------------------------------*/

/*
 * HID Boot Keyboard Report Descriptor — identical to Project 05.
 * 45 bytes; 8-byte report: modifier(1) + reserved(1) + keycodes(6).
 */
__ALIGN_BEGIN static const uint8_t gHidReportDesc[HID_KEYBOARD_REPORT_DESC_SIZE] __ALIGN_END =
{
  0x05, 0x01,        /* Usage Page (Generic Desktop) */
  0x09, 0x06,        /* Usage (Keyboard) */
  0xA1, 0x01,        /* Collection (Application) */

  0x05, 0x07,        /* Usage Page (Key Codes) */
  0x19, 0xE0,        /* Usage Minimum (224) */
  0x29, 0xE7,        /* Usage Maximum (231) */
  0x15, 0x00,        /* Logical Minimum (0) */
  0x25, 0x01,        /* Logical Maximum (1) */
  0x75, 0x01,        /* Report Size (1) */
  0x95, 0x08,        /* Report Count (8) */
  0x81, 0x02,        /* Input (Data, Variable, Absolute) */

  0x95, 0x01,        /* Report Count (1) */
  0x75, 0x08,        /* Report Size (8) */
  0x81, 0x03,        /* Input (Constant) */

  0x95, 0x06,        /* Report Count (6) */
  0x75, 0x08,        /* Report Size (8) */
  0x15, 0x00,        /* Logical Minimum (0) */
  0x25, 0x65,        /* Logical Maximum (101) */
  0x05, 0x07,        /* Usage Page (Key Codes) */
  0x19, 0x00,        /* Usage Minimum (0) */
  0x29, 0x65,        /* Usage Maximum (101) */
  0x81, 0x00,        /* Input (Data, Array) */

  0xC0               /* End Collection */
};

/*
 * Composite Configuration Descriptor — 110 bytes.
 *
 * Layout:
 *   [0]    Configuration descriptor         9 bytes
 *   [9]    Interface 0: HID Keyboard        9 bytes
 *   [18]   HID class descriptor             9 bytes
 *   [27]   EP1 IN  (HID IN)                 7 bytes
 *   [34]   IAD for CDC (IF 1+2)             8 bytes
 *   [42]   Interface 1: CDC Comm            9 bytes
 *   [51]   CDC Header functional            5 bytes
 *   [56]   CDC Call Management functional   5 bytes
 *   [61]   CDC ACM functional               4 bytes
 *   [65]   CDC Union functional             5 bytes
 *   [70]   EP2 IN  (CDC Notification)       7 bytes
 *   [77]   Interface 2: CDC Data            9 bytes
 *   [86]   EP3 OUT (CDC Data OUT)           7 bytes
 *   [93]   EP3 IN  (CDC Data IN)            7 bytes
 *   Total: 100 bytes
 *
 * Note: Actual total is 100 bytes. USBD_COMPOSITE_CFG_DESC_SIZE must match.
 */
#define COMP_CFG_DESC_SIZE  100U

__ALIGN_BEGIN static uint8_t gCompositeCfgDesc[COMP_CFG_DESC_SIZE] __ALIGN_END =
{
  /* ---------- Configuration Descriptor ---------- */
  0x09,                          /* bLength */
  USB_DESC_TYPE_CONFIGURATION,   /* bDescriptorType */
  COMP_CFG_DESC_SIZE, 0x00,      /* wTotalLength */
  0x03,                          /* bNumInterfaces: HID(0) + CDC Comm(1) + CDC Data(2) */
  0x01,                          /* bConfigurationValue */
  0x00,                          /* iConfiguration */
#if (USBD_SELF_POWERED == 1U)
  0xC0,                          /* bmAttributes: Self-powered */
#else
  0x80,                          /* bmAttributes: Bus-powered */
#endif
  0x32,                          /* MaxPower: 100 mA */

  /* ---------- Interface 0: HID Keyboard ---------- */
  0x09,                          /* bLength */
  USB_DESC_TYPE_INTERFACE,       /* bDescriptorType */
  COMP_IF_HID,                   /* bInterfaceNumber */
  0x00,                          /* bAlternateSetting */
  0x01,                          /* bNumEndpoints */
  0x03,                          /* bInterfaceClass: HID */
  0x01,                          /* bInterfaceSubClass: Boot */
  0x01,                          /* bInterfaceProtocol: Keyboard */
  0x00,                          /* iInterface */

  /* HID Class Descriptor */
  0x09,                          /* bLength */
  HID_DESCRIPTOR_TYPE,           /* bDescriptorType: HID */
  0x11, 0x01,                    /* bcdHID: 1.11 */
  0x00,                          /* bCountryCode: Not localized */
  0x01,                          /* bNumDescriptors */
  HID_REPORT_DESC_TYPE,          /* bDescriptorType: Report */
  HID_KEYBOARD_REPORT_DESC_SIZE, /* wItemLength low */
  0x00,                          /* wItemLength high */

  /* EP1 IN: HID Keyboard Interrupt IN */
  0x07,                          /* bLength */
  USB_DESC_TYPE_ENDPOINT,        /* bDescriptorType */
  COMP_HID_EPIN_ADDR,            /* bEndpointAddress: IN EP1 */
  0x03,                          /* bmAttributes: Interrupt */
  COMP_HID_EPIN_SIZE, 0x00,      /* wMaxPacketSize: 8 bytes */
  COMP_HID_FS_BINTERVAL,         /* bInterval: 10 ms */

  /* ---------- IAD: CDC ACM (Interfaces 1 and 2) ---------- */
  0x08,                          /* bLength */
  0x0B,                          /* bDescriptorType: IAD */
  COMP_IF_CDC_COMM,              /* bFirstInterface */
  0x02,                          /* bInterfaceCount */
  0x02,                          /* bFunctionClass: CDC */
  0x02,                          /* bFunctionSubClass: ACM */
  0x01,                          /* bFunctionProtocol: AT commands */
  0x00,                          /* iFunction */

  /* ---------- Interface 1: CDC Communication ---------- */
  0x09,                          /* bLength */
  USB_DESC_TYPE_INTERFACE,       /* bDescriptorType */
  COMP_IF_CDC_COMM,              /* bInterfaceNumber */
  0x00,                          /* bAlternateSetting */
  0x01,                          /* bNumEndpoints */
  0x02,                          /* bInterfaceClass: CDC */
  0x02,                          /* bInterfaceSubClass: ACM */
  0x01,                          /* bInterfaceProtocol: AT commands */
  0x00,                          /* iInterface */

  /* CDC Header Functional Descriptor */
  0x05,                          /* bLength */
  0x24,                          /* bDescriptorType: CS_INTERFACE */
  0x00,                          /* bDescriptorSubtype: Header */
  0x10, 0x01,                    /* bcdCDC: 1.10 */

  /* CDC Call Management Functional Descriptor */
  0x05,                          /* bLength */
  0x24,                          /* bDescriptorType: CS_INTERFACE */
  0x01,                          /* bDescriptorSubtype: Call Management */
  0x00,                          /* bmCapabilities: no call management */
  COMP_IF_CDC_DATA,              /* bDataInterface */

  /* CDC ACM Functional Descriptor */
  0x04,                          /* bLength */
  0x24,                          /* bDescriptorType: CS_INTERFACE */
  0x02,                          /* bDescriptorSubtype: ACM */
  0x02,                          /* bmCapabilities: supports SET/GET_LINE_CODING */

  /* CDC Union Functional Descriptor */
  0x05,                          /* bLength */
  0x24,                          /* bDescriptorType: CS_INTERFACE */
  0x06,                          /* bDescriptorSubtype: Union */
  COMP_IF_CDC_COMM,              /* bControlInterface */
  COMP_IF_CDC_DATA,              /* bSubordinateInterface0 */

  /* EP2 IN: CDC Notification Interrupt IN */
  0x07,                          /* bLength */
  USB_DESC_TYPE_ENDPOINT,        /* bDescriptorType */
  COMP_CDC_CMD_EPIN_ADDR,        /* bEndpointAddress: IN EP2 */
  0x03,                          /* bmAttributes: Interrupt */
  COMP_CDC_CMD_EPIN_SIZE, 0x00,  /* wMaxPacketSize: 8 bytes */
  COMP_CDC_CMD_FS_BINTERVAL,     /* bInterval: 16 ms */

  /* ---------- Interface 2: CDC Data ---------- */
  0x09,                          /* bLength */
  USB_DESC_TYPE_INTERFACE,       /* bDescriptorType */
  COMP_IF_CDC_DATA,              /* bInterfaceNumber */
  0x00,                          /* bAlternateSetting */
  0x02,                          /* bNumEndpoints */
  0x0A,                          /* bInterfaceClass: CDC Data */
  0x00,                          /* bInterfaceSubClass */
  0x00,                          /* bInterfaceProtocol */
  0x00,                          /* iInterface */

  /* EP3 OUT: CDC Data OUT */
  0x07,                          /* bLength */
  USB_DESC_TYPE_ENDPOINT,        /* bDescriptorType */
  COMP_CDC_DATA_OUT_EP_ADDR,     /* bEndpointAddress: OUT EP3 */
  0x02,                          /* bmAttributes: Bulk */
  COMP_CDC_DATA_EP_SIZE, 0x00,   /* wMaxPacketSize: 64 bytes */
  0x00,                          /* bInterval: ignored for Bulk */

  /* EP3 IN: CDC Data IN */
  0x07,                          /* bLength */
  USB_DESC_TYPE_ENDPOINT,        /* bDescriptorType */
  COMP_CDC_DATA_IN_EP_ADDR,      /* bEndpointAddress: IN EP3 */
  0x02,                          /* bmAttributes: Bulk */
  COMP_CDC_DATA_EP_SIZE, 0x00,   /* wMaxPacketSize: 64 bytes */
  0x00,                          /* bInterval: ignored for Bulk */
};

/* Default CDC line coding: 115200 baud, 8N1 */
static const USBD_CDC_LineCodingTypeDef gDefaultLineCoding =
{
  .baudRate = 115200U,
  .stopBits = 0U,  /* 1 stop bit */
  .parity   = 0U,  /* No parity */
  .dataBits = 8U,
};

/* EP0 scratch buffer for CDC control requests */
static uint8_t gEp0Buf[CDC_LINE_CODING_SIZE];

/* Private function prototypes -----------------------------------------------*/
static uint8_t Composite_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t Composite_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t Composite_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t Composite_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t Composite_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t Composite_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t Composite_SOF(USBD_HandleTypeDef *pdev);
static uint8_t *Composite_GetCfgDesc(uint16_t *length);
static uint8_t *Composite_GetDeviceQualifierDesc(uint16_t *length);

/* Exported class handle -----------------------------------------------------*/
USBD_ClassTypeDef USBD_COMPOSITE =
{
  Composite_Init,
  Composite_DeInit,
  Composite_Setup,
  NULL,                          /* EP0_TxSent */
  Composite_EP0_RxReady,
  Composite_DataIn,
  Composite_DataOut,
  Composite_SOF,                 /* SOF */
  NULL,                          /* IsoINIncomplete */
  NULL,                          /* IsoOUTIncomplete */
  Composite_GetCfgDesc,
  Composite_GetCfgDesc,          /* HS config = FS config */
  Composite_GetCfgDesc,          /* Other speed */
  Composite_GetDeviceQualifierDesc,
};

/* Private functions ---------------------------------------------------------*/

static uint8_t Composite_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  USBD_Composite_HandleTypeDef *hcomp;
  UNUSED(cfgidx);

  hcomp = (USBD_Composite_HandleTypeDef *)USBD_malloc(sizeof(USBD_Composite_HandleTypeDef));

  if (hcomp == NULL)
  {
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    return (uint8_t)USBD_EMEM;
  }

  (void)memset(hcomp, 0, sizeof(USBD_Composite_HandleTypeDef));
  pdev->pClassDataCmsit[pdev->classId] = hcomp;

  hcomp->lineCoding    = gDefaultLineCoding;
  hcomp->hidTxBusy     = false;
  hcomp->cdcTxBusy     = false;
  hcomp->cdcHostConnected = false;

  /* Open HID IN endpoint */
  (void)USBD_LL_OpenEP(pdev, COMP_HID_EPIN_ADDR, USBD_EP_TYPE_INTR, COMP_HID_EPIN_SIZE);
  pdev->ep_in[COMP_HID_EPIN_ADDR & 0x0FU].is_used = 1U;

  /* Open CDC Notification IN endpoint */
  (void)USBD_LL_OpenEP(pdev, COMP_CDC_CMD_EPIN_ADDR, USBD_EP_TYPE_INTR, COMP_CDC_CMD_EPIN_SIZE);
  pdev->ep_in[COMP_CDC_CMD_EPIN_ADDR & 0x0FU].is_used = 1U;

  /* Open CDC Data OUT endpoint and prime receiver */
  (void)USBD_LL_OpenEP(pdev, COMP_CDC_DATA_OUT_EP_ADDR, USBD_EP_TYPE_BULK, COMP_CDC_DATA_EP_SIZE);
  pdev->ep_out[COMP_CDC_DATA_OUT_EP_ADDR & 0x0FU].is_used = 1U;
  (void)USBD_LL_PrepareReceive(pdev, COMP_CDC_DATA_OUT_EP_ADDR, hcomp->cdcRxBuf, COMP_CDC_DATA_EP_SIZE);

  /* Open CDC Data IN endpoint */
  (void)USBD_LL_OpenEP(pdev, COMP_CDC_DATA_IN_EP_ADDR, USBD_EP_TYPE_BULK, COMP_CDC_DATA_EP_SIZE);
  pdev->ep_in[COMP_CDC_DATA_IN_EP_ADDR & 0x0FU].is_used = 1U;

  return (uint8_t)USBD_OK;
}

static uint8_t Composite_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);

  (void)USBD_LL_CloseEP(pdev, COMP_HID_EPIN_ADDR);
  pdev->ep_in[COMP_HID_EPIN_ADDR & 0x0FU].is_used = 0U;

  (void)USBD_LL_CloseEP(pdev, COMP_CDC_CMD_EPIN_ADDR);
  pdev->ep_in[COMP_CDC_CMD_EPIN_ADDR & 0x0FU].is_used = 0U;

  (void)USBD_LL_CloseEP(pdev, COMP_CDC_DATA_OUT_EP_ADDR);
  pdev->ep_out[COMP_CDC_DATA_OUT_EP_ADDR & 0x0FU].is_used = 0U;

  (void)USBD_LL_CloseEP(pdev, COMP_CDC_DATA_IN_EP_ADDR);
  pdev->ep_in[COMP_CDC_DATA_IN_EP_ADDR & 0x0FU].is_used = 0U;

  if (pdev->pClassDataCmsit[pdev->classId] != NULL)
  {
    USBD_free(pdev->pClassDataCmsit[pdev->classId]);
    pdev->pClassDataCmsit[pdev->classId] = NULL;
  }

  return (uint8_t)USBD_OK;
}

static uint8_t Composite_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  uint8_t  ifNum = (uint8_t)(req->wIndex & 0xFFU);
  uint16_t len   = 0U;
  const uint8_t *pbuf = NULL;

  if (hcomp == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  /* ---- Vendor device-level requests (bmRequestType = 0x40 or 0xC0) ---- */
  if ((req->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_VENDOR)
  {
    return VendorCmd_HandleSetup(pdev, req);
  }

  /* ---- HID class requests (Interface 0) ---- */
  if (ifNum == COMP_IF_HID)
  {
    switch (req->bmRequest & USB_REQ_TYPE_MASK)
    {
    case USB_REQ_TYPE_CLASS:
      switch (req->bRequest)
      {
      case HID_REQ_SET_PROTOCOL:
        hcomp->hidProtocol = (uint32_t)(req->wValue);
        break;

      case HID_REQ_GET_PROTOCOL:
        (void)USBD_CtlSendData(pdev, (uint8_t *)&hcomp->hidProtocol, 1U);
        break;

      case HID_REQ_SET_IDLE:
        hcomp->hidIdleState = (uint32_t)(req->wValue >> 8U);
        break;

      case HID_REQ_GET_IDLE:
        (void)USBD_CtlSendData(pdev, (uint8_t *)&hcomp->hidIdleState, 1U);
        break;

      default:
        USBD_CtlError(pdev, req);
        return (uint8_t)USBD_FAIL;
      }
      break;

    case USB_REQ_TYPE_STANDARD:
      switch (req->bRequest)
      {
      case USB_REQ_GET_DESCRIPTOR:
        if ((req->wValue >> 8U) == HID_REPORT_DESC_TYPE)
        {
          pbuf = gHidReportDesc;
          len  = (uint16_t)MIN(HID_KEYBOARD_REPORT_DESC_SIZE, req->wLength);
        }
        else if ((req->wValue >> 8U) == HID_DESCRIPTOR_TYPE)
        {
          /* HID descriptor sits at byte offset 18 in the config descriptor */
          pbuf = &gCompositeCfgDesc[18U];
          len  = (uint16_t)MIN(9U, req->wLength);
        }
        else
        {
          USBD_CtlError(pdev, req);
          return (uint8_t)USBD_FAIL;
        }

        (void)USBD_CtlSendData(pdev, (uint8_t *)pbuf, len);
        break;

      case USB_REQ_GET_INTERFACE:
        (void)USBD_CtlSendData(pdev, (uint8_t *)&hcomp->hidProtocol, 1U);
        break;

      case USB_REQ_SET_INTERFACE:
        break;

      default:
        USBD_CtlError(pdev, req);
        return (uint8_t)USBD_FAIL;
      }
      break;

    default:
      USBD_CtlError(pdev, req);
      return (uint8_t)USBD_FAIL;
    }
  }
  /* ---- CDC class requests (Interfaces 1 and 2) ---- */
  else if ((ifNum == COMP_IF_CDC_COMM) || (ifNum == COMP_IF_CDC_DATA))
  {
    switch (req->bmRequest & USB_REQ_TYPE_MASK)
    {
    case USB_REQ_TYPE_CLASS:
      switch (req->bRequest)
      {
      case CDC_SET_LINE_CODING:
        /* Data arrives via EP0 DataOut → handled in EP0_RxReady */
        (void)USBD_CtlPrepareRx(pdev, gEp0Buf, CDC_LINE_CODING_SIZE);
        break;

      case CDC_GET_LINE_CODING:
        (void)USBD_CtlSendData(pdev, (uint8_t *)&hcomp->lineCoding, CDC_LINE_CODING_SIZE);
        break;

      case CDC_SET_CONTROL_LINE_STATE:
        hcomp->controlLineState  = (uint8_t)(req->wValue & 0xFFU);
        hcomp->cdcHostConnected  = ((hcomp->controlLineState & 0x01U) != 0U);
        break;

      case CDC_SEND_BREAK:
        /* Not implemented */
        break;

      default:
        USBD_CtlError(pdev, req);
        return (uint8_t)USBD_FAIL;
      }
      break;

    default:
      USBD_CtlError(pdev, req);
      return (uint8_t)USBD_FAIL;
    }
  }
  else
  {
    USBD_CtlError(pdev, req);
    return (uint8_t)USBD_FAIL;
  }

  return (uint8_t)USBD_OK;
}

static uint8_t Composite_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  if (hcomp == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  if (epnum == (COMP_HID_EPIN_ADDR & 0x0FU))
  {
    hcomp->hidTxBusy = false;
    /* Forward to app callback so transport layer can update its own state */
    UsbHidTransport_TxCpltCallback();
  }
  else if (epnum == (COMP_CDC_DATA_IN_EP_ADDR & 0x0FU))
  {
    hcomp->cdcTxBusy = false;
  }
  /* CDC notification EP (ep 2) — nothing to do */

  return (uint8_t)USBD_OK;
}

static uint8_t Composite_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  if (hcomp == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  if (epnum == COMP_CDC_DATA_OUT_EP_ADDR)
  {
    /* Re-arm the OUT endpoint for the next host packet */
    (void)USBD_LL_PrepareReceive(pdev, COMP_CDC_DATA_OUT_EP_ADDR, hcomp->cdcRxBuf, COMP_CDC_DATA_EP_SIZE);
  }

  return (uint8_t)USBD_OK;
}

static uint8_t Composite_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  if (hcomp == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  /* Only CDC SET_LINE_CODING sends data through EP0 */
  (void)memcpy(&hcomp->lineCoding, gEp0Buf, CDC_LINE_CODING_SIZE);

  return (uint8_t)USBD_OK;
}

/*
 * SOF fires every 1 ms from the host. Used here to clear hidTxBusy when the
 * HID IN endpoint has become idle after a USB reset or re-enumeration, so the
 * transport layer does not get permanently stuck in BUSY state.
 */
static uint8_t Composite_SOF(USBD_HandleTypeDef *pdev)
{
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  if (hcomp == NULL)
  {
    return (uint8_t)USBD_OK;
  }

  /* If EP1 is no longer marked as used (e.g. after reset), release the busy flag */
  if ((hcomp->hidTxBusy) && (pdev->ep_in[COMP_HID_EPIN_ADDR & 0x0FU].is_used == 0U))
  {
    hcomp->hidTxBusy = false;
    UsbHidTransport_TxCpltCallback();
  }

  return (uint8_t)USBD_OK;
}

static uint8_t *Composite_GetCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(gCompositeCfgDesc);
  return gCompositeCfgDesc;
}

/* Device qualifier — not used in FS-only devices but required by the class interface */
__ALIGN_BEGIN static uint8_t gDeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,    /* bLength */
  USB_DESC_TYPE_DEVICE_QUALIFIER,/* bDescriptorType */
  0x00, 0x02,                    /* bcdUSB */
  0x00,                          /* bDeviceClass */
  0x00,                          /* bDeviceSubClass */
  0x00,                          /* bDeviceProtocol */
  0x40,                          /* bMaxPacketSize0 */
  0x01,                          /* bNumConfigurations */
  0x00,                          /* bReserved */
};

static uint8_t *Composite_GetDeviceQualifierDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(gDeviceQualifierDesc);
  return gDeviceQualifierDesc;
}

/* Exported functions --------------------------------------------------------*/

uint8_t USBD_COMPOSITE_HID_SendReport(USBD_HandleTypeDef *pdev,
                                       uint8_t *report,
                                       uint16_t len)
{
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  if (hcomp == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  if (pdev->dev_state != USBD_STATE_CONFIGURED)
  {
    return (uint8_t)USBD_FAIL;
  }

  if (hcomp->hidTxBusy)
  {
    return (uint8_t)USBD_BUSY;
  }

  hcomp->hidTxBusy = true;

  (void)USBD_LL_Transmit(pdev, COMP_HID_EPIN_ADDR, report, len);

  return (uint8_t)USBD_OK;
}

uint8_t USBD_COMPOSITE_CDC_Transmit(USBD_HandleTypeDef *pdev,
                                     const uint8_t *buf,
                                     uint16_t len)
{
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  if (hcomp == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  if (pdev->dev_state != USBD_STATE_CONFIGURED)
  {
    return (uint8_t)USBD_FAIL;
  }

  if (hcomp->cdcTxBusy)
  {
    return (uint8_t)USBD_BUSY;
  }

  if (len > COMP_CDC_DATA_EP_SIZE)
  {
    len = COMP_CDC_DATA_EP_SIZE;
  }

  (void)memcpy(hcomp->cdcTxBuf, buf, len);
  hcomp->cdcTxLen  = len;
  hcomp->cdcTxBusy = true;

  (void)USBD_LL_Transmit(pdev, COMP_CDC_DATA_IN_EP_ADDR, hcomp->cdcTxBuf, len);

  return (uint8_t)USBD_OK;
}

bool USBD_COMPOSITE_CDC_IsHostConnected(USBD_HandleTypeDef *pdev)
{
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  if (hcomp == NULL)
  {
    return false;
  }

  return hcomp->cdcHostConnected;
}

bool USBD_COMPOSITE_CDC_IsTxIdle(USBD_HandleTypeDef *pdev)
{
  USBD_Composite_HandleTypeDef *hcomp = (USBD_Composite_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  if (hcomp == NULL)
  {
    return false;
  }

  return !hcomp->cdcTxBusy;
}

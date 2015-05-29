#include "usbd_cdc_hid.h"
#include "usbd_desc.h"
#include "usbd_ctlreq.h"

#include "jshardware.h"
#include "jsinteractive.h"

#define NOHID


#define CDC_IN_EP                                   0x83  /* EP1 for data IN */
#define CDC_OUT_EP                                  0x03  /* EP1 for data OUT */
#define CDC_CMD_EP                                  0x82  /* EP2 for CDC commands */

#define HID_IN_EP                     0x81
#define HID_INTERFACE_NUMBER          0
#define HID_MOUSE_REPORT_DESC_SIZE   74

extern USBD_HandleTypeDef hUsbDeviceFS;
// CDC Buffers -----------------------------------
#define CDC_RX_DATA_SIZE  CDC_DATA_FS_OUT_PACKET_SIZE
#define CDC_TX_DATA_SIZE  CDC_DATA_FS_IN_PACKET_SIZE
uint8_t CDCRxBufferFS[CDC_RX_DATA_SIZE];
uint8_t CDCTxBufferFS[CDC_TX_DATA_SIZE];
// ..
static int8_t CDC_Control_FS  (uint8_t cmd, uint8_t* pbuf, uint16_t length);
static void CDC_TxReady(void);
//-------------------------------------------------

static uint8_t  USBD_CDC_HID_Init (USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t  USBD_CDC_HID_DeInit (USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t  USBD_CDC_HID_Setup (USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t  USBD_CDC_HID_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  USBD_CDC_HID_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  USBD_CDC_HID_EP0_RxReady (USBD_HandleTypeDef *pdev);
static uint8_t  *USBD_CDC_HID_GetCfgDesc (uint16_t *length);
uint8_t  *USBD_CDC_HID_GetDeviceQualifierDescriptor (uint16_t *length);

/* CDC interface class callbacks structure */
const USBD_ClassTypeDef  USBD_CDC_HID =
{
  USBD_CDC_HID_Init,
  USBD_CDC_HID_DeInit,
  USBD_CDC_HID_Setup,
  NULL,                 /* EP0_TxSent, */
  USBD_CDC_HID_EP0_RxReady,
  USBD_CDC_HID_DataIn,
  USBD_CDC_HID_DataOut,
  NULL,
  NULL,
  NULL,     
  USBD_CDC_HID_GetCfgDesc,
  USBD_CDC_HID_GetCfgDesc,
  USBD_CDC_HID_GetCfgDesc,
  USBD_CDC_HID_GetDeviceQualifierDescriptor,
};

/* USB Standard Device Descriptor */
__ALIGN_BEGIN static const uint8_t USBD_CDC_HID_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};


/* USB CDC device Configuration Descriptor
 * ============================================================================
 *
 * No HID
 * CDC on Interfaces 0 and 1
 */
#define USBD_CDC_CFGDESC_SIZE              67
const __ALIGN_BEGIN uint8_t USBD_CDC_CfgDesc[USBD_CDC_CFGDESC_SIZE] __ALIGN_END =
{
  /*Configuration Descriptor*/
  0x09,   /* bLength: Configuration Descriptor size */
  USB_DESC_TYPE_CONFIGURATION,      /* bDescriptorType: Configuration */
  USBD_CDC_CFGDESC_SIZE, 0x00,               /* wTotalLength:no of returned bytes */
  0x02,   /* bNumInterfaces: 2 interface */
  0x01,   /* bConfigurationValue: Configuration value */
  0x00,   /* iConfiguration: Index of string descriptor describing the configuration */
  0xC0,   /* bmAttributes: self powered */
  0x32,   /* MaxPower 0 mA */
  
  // -----------------------------------------------------------------------
  /*CDC Interface Descriptor */
  0x09,   /* bLength: Interface Descriptor size */
  USB_DESC_TYPE_INTERFACE,  /* bDescriptorType: Interface */
  /* Interface descriptor type */
  0,   /* bInterfaceNumber: Number of Interface */
  0x00,   /* bAlternateSetting: Alternate setting */
  0x01,   /* bNumEndpoints: One endpoints used */
  0x02,   /* bInterfaceClass: Communication Interface Class */
  0x02,   /* bInterfaceSubClass: Abstract Control Model */
  0x01,   /* bInterfaceProtocol: Common AT commands */
  0x00,   /* iInterface: */
  
  /*Header Functional Descriptor*/
  0x05,   /* bLength: Endpoint Descriptor size */
  0x24,   /* bDescriptorType: CS_INTERFACE */
  0x00,   /* bDescriptorSubtype: Header Func Desc */
  0x10,   /* bcdCDC: spec release number */
  0x01,
  
  /*Call Management Functional Descriptor*/
  0x05,   /* bFunctionLength */
  0x24,   /* bDescriptorType: CS_INTERFACE */
  0x01,   /* bDescriptorSubtype: Call Management Func Desc */
  0x00,   /* bmCapabilities: D0+D1 */
  1,   /* bDataInterface */
  
  /*ACM Functional Descriptor*/
  0x04,   /* bFunctionLength */
  0x24,   /* bDescriptorType: CS_INTERFACE */
  0x02,   /* bDescriptorSubtype: Abstract Control Management desc */
  0x02,   /* bmCapabilities */
  
  /*Union Functional Descriptor*/
  0x05,   /* bFunctionLength */
  0x24,   /* bDescriptorType: CS_INTERFACE */
  0x06,   /* bDescriptorSubtype: Union func desc */
  0,   /* bMasterInterface: Communication class interface */
  1,   /* bSlaveInterface0: Data Class Interface */
  
  /*Endpoint 2 Descriptor*/
  0x07,                           /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,   /* bDescriptorType: Endpoint */
  CDC_CMD_EP,                     /* bEndpointAddress */
  0x03,                           /* bmAttributes: Interrupt */
  LOBYTE(CDC_CMD_PACKET_SIZE),     /* wMaxPacketSize: */
  HIBYTE(CDC_CMD_PACKET_SIZE),
  0x10,                           /* bInterval: */ 
  /*---------------------------------------------------------------------------*/
  
  /*Data class interface descriptor*/
  0x09,   /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_INTERFACE,  /* bDescriptorType: */
  1,   /* bInterfaceNumber: Number of Interface */
  0x00,   /* bAlternateSetting: Alternate setting */
  0x02,   /* bNumEndpoints: Two endpoints used */
  0x0A,   /* bInterfaceClass: CDC */
  0x00,   /* bInterfaceSubClass: */
  0x00,   /* bInterfaceProtocol: */
  0x00,   /* iInterface: */
  
  /*Endpoint OUT Descriptor*/
  0x07,   /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,      /* bDescriptorType: Endpoint */
  CDC_OUT_EP,                        /* bEndpointAddress */
  0x02,                              /* bmAttributes: Bulk */
  LOBYTE(CDC_DATA_FS_OUT_PACKET_SIZE),  /* wMaxPacketSize: */
  HIBYTE(CDC_DATA_FS_OUT_PACKET_SIZE),
  0x00,                              /* bInterval: ignore for Bulk transfer */
  
  /*Endpoint IN Descriptor*/
  0x07,   /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,      /* bDescriptorType: Endpoint */
  CDC_IN_EP,                         /* bEndpointAddress */
  0x02,                              /* bmAttributes: Bulk */
  LOBYTE(CDC_DATA_FS_IN_PACKET_SIZE),  /* wMaxPacketSize: */
  HIBYTE(CDC_DATA_FS_IN_PACKET_SIZE),
  0x00,                               /* bInterval: ignore for Bulk transfer */
} ;

/* USB HID + CDC device Configuration Descriptor
 * ============================================================================
 *
 * HID on Interface 0
 * CDC on Interfaces 1 and 2
 */
#define USBD_CDC_HID_CFGDESC_SIZE              (67+25)
#define USBD_CDC_HID_CFGDESC_REPORT_SIZE_IDX   25
// NOT CONST - descriptor size needs updating as this is sent out
__ALIGN_BEGIN uint8_t USBD_CDC_HID_CfgDesc[USBD_CDC_HID_CFGDESC_SIZE] __ALIGN_END =
{
  /*Configuration Descriptor*/
  0x09,   /* bLength: Configuration Descriptor size */
  USB_DESC_TYPE_CONFIGURATION,      /* bDescriptorType: Configuration */
  USBD_CDC_HID_CFGDESC_SIZE, 0x00,               /* wTotalLength:no of returned bytes */
  0x03,   /* bNumInterfaces: 3 interface */
  0x01,   /* bConfigurationValue: Configuration value */
  0x00,   /* iConfiguration: Index of string descriptor describing the configuration */
  0xC0,   /* bmAttributes: self powered */
  0x32,   /* MaxPower 0 mA */

  /************** Descriptor of Joystick Mouse interface ****************/
  /* 9 */
  0x09,         /*bLength: Interface Descriptor size*/
  USB_DESC_TYPE_INTERFACE,/*bDescriptorType: Interface descriptor type*/
  HID_INTERFACE_NUMBER,   /*bInterfaceNumber: Number of Interface*/
  0x00,         /*bAlternateSetting: Alternate setting*/
  0x01,         /*bNumEndpoints*/
  0x03,         /*bInterfaceClass: HID*/
  0x01,         /*bInterfaceSubClass : 1=BOOT, 0=no boot*/
  0x02,         /*nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse*/
  0,            /*iInterface: Index of string descriptor*/
  /******************** Descriptor of Joystick Mouse HID ********************/
  /* 18 */
  0x09,         /*bLength: HID Descriptor size*/
  HID_DESCRIPTOR_TYPE, /*bDescriptorType: HID*/
  0x11,         /*bcdHID: HID Class Spec release number*/
  0x01,
  0x00,         /*bCountryCode: Hardware target country*/
  0x01,         /*bNumDescriptors: Number of HID class descriptors to follow*/
  0x22,         /*bDescriptorType*/
  0/*HID_REPORT_DESC_SIZE*/, 0, /*wItemLength: Total length of Report descriptor*/
  /******************** Descriptor of Mouse endpoint ********************/
  /* 27 */
  0x07,          /*bLength: Endpoint Descriptor size*/
  USB_DESC_TYPE_ENDPOINT, /*bDescriptorType:*/
  HID_IN_EP,     /*bEndpointAddress: Endpoint Address (IN)*/
  0x03,          /*bmAttributes: Interrupt endpoint*/
  HID_DATA_IN_PACKET_SIZE,0x00, /*wMaxPacketSize: 4 Byte max */
  HID_FS_BINTERVAL,          /*bInterval: Polling Interval (10 ms)*/

  // -----------------------------------------------------------------------
  /*CDC Interface Descriptor */
  0x09,   /* bLength: Interface Descriptor size */
  USB_DESC_TYPE_INTERFACE,  /* bDescriptorType: Interface */
  /* Interface descriptor type */
  1,   /* bInterfaceNumber: Number of Interface */
  0x00,   /* bAlternateSetting: Alternate setting */
  0x01,   /* bNumEndpoints: One endpoints used */
  0x02,   /* bInterfaceClass: Communication Interface Class */
  0x02,   /* bInterfaceSubClass: Abstract Control Model */
  0x01,   /* bInterfaceProtocol: Common AT commands */
  0x00,   /* iInterface: */

  /*Header Functional Descriptor*/
  0x05,   /* bLength: Endpoint Descriptor size */
  0x24,   /* bDescriptorType: CS_INTERFACE */
  0x00,   /* bDescriptorSubtype: Header Func Desc */
  0x10,   /* bcdCDC: spec release number */
  0x01,

  /*Call Management Functional Descriptor*/
  0x05,   /* bFunctionLength */
  0x24,   /* bDescriptorType: CS_INTERFACE */
  0x01,   /* bDescriptorSubtype: Call Management Func Desc */
  0x00,   /* bmCapabilities: D0+D1 */
  2,   /* bDataInterface */

  /*ACM Functional Descriptor*/
  0x04,   /* bFunctionLength */
  0x24,   /* bDescriptorType: CS_INTERFACE */
  0x02,   /* bDescriptorSubtype: Abstract Control Management desc */
  0x02,   /* bmCapabilities */

  /*Union Functional Descriptor*/
  0x05,   /* bFunctionLength */
  0x24,   /* bDescriptorType: CS_INTERFACE */
  0x06,   /* bDescriptorSubtype: Union func desc */
  1,   /* bMasterInterface: Communication class interface */
  2,   /* bSlaveInterface0: Data Class Interface */

  /*Endpoint 2 Descriptor*/
  0x07,                           /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,   /* bDescriptorType: Endpoint */
  CDC_CMD_EP,                     /* bEndpointAddress */
  0x03,                           /* bmAttributes: Interrupt */
  LOBYTE(CDC_CMD_PACKET_SIZE),     /* wMaxPacketSize: */
  HIBYTE(CDC_CMD_PACKET_SIZE),
  0x10,                           /* bInterval: */
  /*---------------------------------------------------------------------------*/

  /*Data class interface descriptor*/
  0x09,   /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_INTERFACE,  /* bDescriptorType: */
  2,   /* bInterfaceNumber: Number of Interface */
  0x00,   /* bAlternateSetting: Alternate setting */
  0x02,   /* bNumEndpoints: Two endpoints used */
  0x0A,   /* bInterfaceClass: CDC */
  0x00,   /* bInterfaceSubClass: */
  0x00,   /* bInterfaceProtocol: */
  0x00,   /* iInterface: */

  /*Endpoint OUT Descriptor*/
  0x07,   /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,      /* bDescriptorType: Endpoint */
  CDC_OUT_EP,                        /* bEndpointAddress */
  0x02,                              /* bmAttributes: Bulk */
  LOBYTE(CDC_DATA_FS_OUT_PACKET_SIZE),  /* wMaxPacketSize: */
  HIBYTE(CDC_DATA_FS_OUT_PACKET_SIZE),
  0x00,                              /* bInterval: ignore for Bulk transfer */

  /*Endpoint IN Descriptor*/
  0x07,   /* bLength: Endpoint Descriptor size */
  USB_DESC_TYPE_ENDPOINT,      /* bDescriptorType: Endpoint */
  CDC_IN_EP,                         /* bEndpointAddress */
  0x02,                              /* bmAttributes: Bulk */
  LOBYTE(CDC_DATA_FS_IN_PACKET_SIZE),  /* wMaxPacketSize: */
  HIBYTE(CDC_DATA_FS_IN_PACKET_SIZE),
  0x00,                               /* bInterval: ignore for Bulk transfer */
} ;

#define USB_HID_DESC_SIZ              9
#define USBD_HID_DESC_REPORT_SIZE_IDX 7
/* USB HID device Configuration Descriptor */
__ALIGN_BEGIN static uint8_t USBD_HID_Desc[USB_HID_DESC_SIZ]  __ALIGN_END  =
{
  0x09,         /*bLength: HID Descriptor size*/
  HID_DESCRIPTOR_TYPE, /*bDescriptorType: HID*/
  0x11,         /*bcdHID: HID Class Spec release number*/
  0x01,
  0x00,         /*bCountryCode: Hardware target country*/
  0x01,         /*bNumDescriptors: Number of HID class descriptors to follow*/
  0x22,         /*bDescriptorType*/
  0/*HID_REPORT_DESC_SIZE*/, 0x00, /*wItemLength: Total length of Report descriptor*/
};

#ifndef  NOHID
__ALIGN_BEGIN static const uint8_t HID_ReportDesc[HID_MOUSE_REPORT_DESC_SIZE]  __ALIGN_END =
{
  0x05,   0x01,
  0x09,   0x02,
  0xA1,   0x01,
  0x09,   0x01,

  0xA1,   0x00,
  0x05,   0x09,
  0x19,   0x01,
  0x29,   0x03,

  0x15,   0x00,
  0x25,   0x01,
  0x95,   0x03,
  0x75,   0x01,

  0x81,   0x02,
  0x95,   0x01,
  0x75,   0x05,
  0x81,   0x01,

  0x05,   0x01,
  0x09,   0x30,
  0x09,   0x31,
  0x09,   0x38,

  0x15,   0x81,
  0x25,   0x7F,
  0x75,   0x08,
  0x95,   0x03,

  0x81,   0x06,
  0xC0,   0x09,
  0x3c,   0x05,
  0xff,   0x09,

  0x01,   0x15,
  0x00,   0x25,
  0x01,   0x75,
  0x01,   0x95,

  0x02,   0xb1,
  0x22,   0x75,
  0x06,   0x95,
  0x01,   0xb1,

  0x01,   0xc0
};
#endif


// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------


/**
  * @brief  USBD_CDC_HID_Init
  *         Initialize the CDC interface
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t  USBD_CDC_HID_Init (USBD_HandleTypeDef *pdev,
                               uint8_t cfgidx)
{

  static USBD_CDC_HID_HandleTypeDef no_malloc_thx;
  pdev->pClassData = &no_malloc_thx;
  USBD_CDC_HID_HandleTypeDef   *handle = (USBD_CDC_HID_HandleTypeDef*) pdev->pClassData;

  NOT_USED(cfgidx);
  uint8_t ret = 0;
  
  /* Open EP IN */
  USBD_LL_OpenEP(pdev,
                 CDC_IN_EP,
                 USBD_EP_TYPE_BULK,
                 CDC_DATA_FS_IN_PACKET_SIZE);

  /* Open EP OUT */
  USBD_LL_OpenEP(pdev,
                 CDC_OUT_EP,
                 USBD_EP_TYPE_BULK,
                 CDC_DATA_FS_OUT_PACKET_SIZE);
  /* Open Command IN EP */
  USBD_LL_OpenEP(pdev,
                 CDC_CMD_EP,
                 USBD_EP_TYPE_INTR,
                 CDC_CMD_PACKET_SIZE);
  
  /* Init Xfer states */
  handle->cdcState = CDC_IDLE;

  /* Prepare Out endpoint to receive next packet */
  USBD_LL_PrepareReceive(pdev,
                         CDC_OUT_EP,
                         CDCRxBufferFS,
                         CDC_RX_DATA_SIZE);

  unsigned int l = 0;
  handle->hidReportDesc = USB_GetHIDReportDesc(&l); // do we have a HID report?
  handle->hidReportDescSize = (uint16_t)l;
  if (handle->hidReportDescSize) {
    /* Open EP IN */
    USBD_LL_OpenEP(pdev,
                   HID_IN_EP,
                   USBD_EP_TYPE_INTR,
                   HID_DATA_IN_PACKET_SIZE);
    handle->hidState = HID_IDLE;
  }
    
  return ret;
}

/**
  * @brief  USBD_CDC_HID_Init
  *         DeInitialize the CDC layer
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
static uint8_t  USBD_CDC_HID_DeInit (USBD_HandleTypeDef *pdev,
                                 uint8_t cfgidx)
{
  USBD_CDC_HID_HandleTypeDef   *handle = (USBD_CDC_HID_HandleTypeDef*) pdev->pClassData;
  NOT_USED(cfgidx);
  uint8_t ret = 0;
  
  /* Open EP IN */
  USBD_LL_CloseEP(pdev,
              CDC_IN_EP);
  
  /* Open EP OUT */
  USBD_LL_CloseEP(pdev,
              CDC_OUT_EP);
  
  /* Open Command IN EP */
  USBD_LL_CloseEP(pdev,
               CDC_CMD_EP);

  if (handle->hidReportDescSize) {
    USBD_LL_CloseEP(pdev,
                  HID_IN_EP);
  }
  
  /* DeInit  physical Interface components */
  if(pdev->pClassData != NULL)
  {
    //USBD_free(pdev->pClassData);
    pdev->pClassData = NULL;
  }
  

  return ret;
}


static uint8_t  USBD_CDC_Setup (USBD_HandleTypeDef *pdev,
                                USBD_SetupReqTypedef *req)
{
  USBD_CDC_HID_HandleTypeDef   *handle = (USBD_CDC_HID_HandleTypeDef*) pdev->pClassData;
  static uint8_t ifalt = 0;
    
  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
  case USB_REQ_TYPE_CLASS :
    if (req->wLength)
    {
      if (req->bmRequest & 0x80)
      {
        CDC_Control_FS(req->bRequest, (uint8_t *)handle->data, req->wLength);
        USBD_CtlSendData (pdev,
                            (uint8_t *)handle->data,
                            req->wLength);
      }
      else
      {
        handle->CmdOpCode = req->bRequest;
        handle->CmdLength = (uint8_t)req->wLength;
        
        USBD_CtlPrepareRx (pdev, 
                           (uint8_t *)handle->data,
                           req->wLength);
      }
      
    }
    else
    {
      CDC_Control_FS(req->bRequest, (uint8_t*)req, 0);
    }
    break;

  case USB_REQ_TYPE_STANDARD:
    switch (req->bRequest)
    {      
    case USB_REQ_GET_INTERFACE :
      USBD_CtlSendData (pdev,
                        &ifalt,
                        1);
      break;
      
    case USB_REQ_SET_INTERFACE :
      break;
    }
 
  default: 
    break;
  }
  return USBD_OK;
}

static uint8_t  USBD_HID_Setup (USBD_HandleTypeDef *pdev,
                                USBD_SetupReqTypedef *req)
{
  int len = 0;
  uint8_t  *pbuf = NULL;
  USBD_CDC_HID_HandleTypeDef *handle = (USBD_CDC_HID_HandleTypeDef*)pdev->pClassData;

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
  case USB_REQ_TYPE_CLASS :
    switch (req->bRequest)
    {


    case HID_REQ_SET_PROTOCOL:
      handle->hidProtocol = (uint8_t)(req->wValue);
      break;

    case HID_REQ_GET_PROTOCOL:
      USBD_CtlSendData (pdev,
                        (uint8_t *)&handle->hidProtocol,
                        1);
      break;

    case HID_REQ_SET_IDLE:
      handle->hidIdleState = (uint8_t)(req->wValue >> 8);
      break;

    case HID_REQ_GET_IDLE:
      USBD_CtlSendData (pdev,
                        (uint8_t *)&handle->hidIdleState,
                        1);
      break;

    default:
      USBD_CtlError (pdev, req);
      return USBD_FAIL;
    }
    break;

  case USB_REQ_TYPE_STANDARD:
    switch (req->bRequest)
    {
    case USB_REQ_GET_DESCRIPTOR:
      if( req->wValue >> 8 == HID_REPORT_DESC)
      {
        pbuf = handle->hidReportDesc;
        len = MIN(handle->hidReportDescSize, req->wLength);
      }
      else if( req->wValue >> 8 == HID_DESCRIPTOR_TYPE)
      {
        USBD_HID_Desc[USBD_HID_DESC_REPORT_SIZE_IDX] = (uint8_t)handle->hidReportDescSize;
        USBD_HID_Desc[USBD_HID_DESC_REPORT_SIZE_IDX+1] = (uint8_t)(handle->hidReportDescSize>>8);
        pbuf = USBD_HID_Desc;
        len = MIN(USB_HID_DESC_SIZ , req->wLength);
      }

      USBD_CtlSendData (pdev,
                        pbuf,
                        (uint16_t)len);

      break;

    case USB_REQ_GET_INTERFACE :
      USBD_CtlSendData (pdev,
                        (uint8_t *)&handle->hidAltSetting,
                        1);
      break;

    case USB_REQ_SET_INTERFACE :
      handle->hidAltSetting = (uint8_t)(req->wValue);
      break;
    }
  }
  return USBD_OK;
}

static uint8_t  USBD_CDC_HID_Setup (USBD_HandleTypeDef *pdev,
                                    USBD_SetupReqTypedef *req) {
  USBD_CDC_HID_HandleTypeDef *handle = (USBD_CDC_HID_HandleTypeDef*)pdev->pClassData;

  if (handle->hidReportDescSize && req->wIndex == HID_INTERFACE_NUMBER)
    return USBD_HID_Setup(pdev, req);
  else
    return USBD_CDC_Setup(pdev, req);
}


/**
  * @brief  usbd_audio_DataIn
  *         Data sent on non-control IN endpoint
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t  USBD_CDC_HID_DataIn (USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  USBD_CDC_HID_HandleTypeDef   *handle = (USBD_CDC_HID_HandleTypeDef*) pdev->pClassData;
  
  if (epnum == (HID_IN_EP&0x7F)) {
    /* Ensure that the FIFO is empty before a new transfer, this condition could
    be caused by  a new transfer before the end of the previous transfer */
    ((USBD_CDC_HID_HandleTypeDef *)pdev->pClassData)->hidState = HID_IDLE;
  } else {
    // USB CDC
    handle->cdcState &= ~CDC_WRITE_TX_WAIT;
    CDC_TxReady();
  }

  return USBD_OK;
}

/**
  * @brief  USBD_CDC_HID_DataOut
  *         Data received on non-control Out endpoint
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t  USBD_CDC_HID_DataOut (USBD_HandleTypeDef *pdev, uint8_t epnum)
{      
  /* Get the received data length */
  unsigned int RxLength = USBD_LL_GetRxDataSize (pdev, epnum);
  
  /* USB data will be immediately processed, this allow next USB traffic being 
  NAKed till the end of the application Xfer */
  if(pdev->pClassData != NULL)
  {
    jshPushIOCharEvents(EV_USBSERIAL, (char*)CDCRxBufferFS, RxLength);

    USBD_LL_PrepareReceive(pdev,
                           CDC_OUT_EP,
                           CDCRxBufferFS,
                           CDC_DATA_FS_OUT_PACKET_SIZE);

    return USBD_OK;
  }
  else
  {
    return USBD_FAIL;
  }
}


/**
  * @brief  USBD_CDC_HID_DataOut
  *         Data received on non-control Out endpoint
  * @param  pdev: device instance
  * @param  epnum: endpoint number
  * @retval status
  */
static uint8_t  USBD_CDC_HID_EP0_RxReady (USBD_HandleTypeDef *pdev)
{ 
  USBD_CDC_HID_HandleTypeDef   *handle = (USBD_CDC_HID_HandleTypeDef*) pdev->pClassData;
  
  if(handle->CmdOpCode != 0xFF)
  {
    CDC_Control_FS(handle->CmdOpCode, (uint8_t *)handle->data, handle->CmdLength);
    handle->CmdOpCode = 0xFF;
  }
  return USBD_OK;
}

/**
  * @brief  USBD_CDC_HID_GetFSCfgDesc
  *         Return configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
static uint8_t  *USBD_CDC_HID_GetCfgDesc (uint16_t *length)
{
  USBD_CDC_HID_HandleTypeDef     *handle = (USBD_CDC_HID_HandleTypeDef*)hUsbDeviceFS.pClassData;

  if (handle->hidReportDescSize) {
    USBD_CDC_HID_CfgDesc[USBD_CDC_HID_CFGDESC_REPORT_SIZE_IDX] = (uint8_t)handle->hidReportDescSize;
    USBD_CDC_HID_CfgDesc[USBD_CDC_HID_CFGDESC_REPORT_SIZE_IDX+1] = (uint8_t)(handle->hidReportDescSize>>8);
    *length = USBD_CDC_HID_CFGDESC_SIZE;
    return USBD_CDC_HID_CfgDesc;
  } else {
    *length = USBD_CDC_CFGDESC_SIZE;
    return USBD_CDC_CfgDesc;
  }
}

/**
* @brief  DeviceQualifierDescriptor 
*         return Device Qualifier descriptor
* @param  length : pointer data length
* @retval pointer to descriptor buffer
*/
uint8_t  *USBD_CDC_HID_GetDeviceQualifierDescriptor (uint16_t *length)
{
  *length = sizeof (USBD_CDC_HID_DeviceQualifierDesc);
  return USBD_CDC_HID_DeviceQualifierDesc;
}

//----------------------------------------------------------------------------

static int8_t CDC_Control_FS  (uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  NOT_USED(pbuf);
  NOT_USED(length);
  switch (cmd)
  {
  case CDC_SEND_ENCAPSULATED_COMMAND:

    break;

  case CDC_GET_ENCAPSULATED_RESPONSE:

    break;

  case CDC_SET_COMM_FEATURE:

    break;

  case CDC_GET_COMM_FEATURE:

    break;

  case CDC_CLEAR_COMM_FEATURE:

    break;

  /*******************************************************************************/
  /* Line Coding Structure                                                       */
  /*-----------------------------------------------------------------------------*/
  /* Offset | Field       | Size | Value  | Description                          */
  /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits per second*/
  /* 4      | bCharFormat |   1  | Number | Stop bits                            */
  /*                                        0 - 1 Stop bit                       */
  /*                                        1 - 1.5 Stop bits                    */
  /*                                        2 - 2 Stop bits                      */
  /* 5      | bParityType |  1   | Number | Parity                               */
  /*                                        0 - None                             */
  /*                                        1 - Odd                              */
  /*                                        2 - Even                             */
  /*                                        3 - Mark                             */
  /*                                        4 - Space                            */
  /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or 16).          */
  /*******************************************************************************/
  case CDC_SET_LINE_CODING:
    // called when plugged in and app connects
    break;

  case CDC_GET_LINE_CODING:

    break;

  case CDC_SET_CONTROL_LINE_STATE:
    // called on connect/disconnect by app
    break;

  case CDC_SEND_BREAK:

    break;

  default:
    break;
  }

  return (USBD_OK);
}

// USB transmit is ready for more data
static void CDC_TxReady(void)
{
  if (!USB_IsConnected() || // not connected
      (((USBD_CDC_HID_HandleTypeDef*)hUsbDeviceFS.pClassData)->cdcState & CDC_WRITE_TX_WAIT)) // already waiting for send
    return;

  ((USBD_CDC_HID_HandleTypeDef*)hUsbDeviceFS.pClassData)->cdcState &= ~CDC_WRITE_DELAY;

  unsigned int len = 0;

  // try and fill the buffer
  int c;
  while (len<CDC_TX_DATA_SIZE-1 && // TODO: send max packet size -1 to ensure data is pushed through
         ((c = jshGetCharToTransmit(EV_USBSERIAL)) >= 0) ) { // get byte to transmit
    CDCTxBufferFS[len++] = (uint8_t)c;
  }

  // send data if we have any...
  if (len) {
    ((USBD_CDC_HID_HandleTypeDef*)hUsbDeviceFS.pClassData)->cdcState |= CDC_WRITE_TX_WAIT;

    /* Transmit next packet */
    USBD_LL_Transmit(&hUsbDeviceFS,
                     CDC_IN_EP,
                     CDCTxBufferFS,
                     (uint16_t)len);
  }
}


// ----------------------------------------------------------------------------
// --------------------------------------------------------- PUBLIC Functions
// ----------------------------------------------------------------------------

uint8_t USBD_HID_SendReport     (uint8_t *report,  unsigned int len)
{
  USBD_CDC_HID_HandleTypeDef     *handle = (USBD_CDC_HID_HandleTypeDef*)hUsbDeviceFS.pClassData;

  if (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED ) {
    if(handle->hidState == HID_IDLE) {
      handle->hidState = HID_BUSY;
      memcpy(handle->hidData, report, len);
      USBD_LL_Transmit (&hUsbDeviceFS,
                        HID_IN_EP,
                        (uint8_t*)handle->hidData,
                        (uint16_t)len);
      return USBD_OK;
    }
  }
  return USBD_FAIL;
}


void USB_StartTransmission() {
  ((USBD_CDC_HID_HandleTypeDef*)hUsbDeviceFS.pClassData)->cdcState |= CDC_WRITE_DELAY;
}

int USB_IsConnected() {
  return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED;
}

// To be called on SysTick timer
void USB_SysTick() {
  if (!USB_IsConnected()) return;
  if (((USBD_CDC_HID_HandleTypeDef*)hUsbDeviceFS.pClassData)->cdcState & CDC_WRITE_DELAY) {
    ((USBD_CDC_HID_HandleTypeDef*)hUsbDeviceFS.pClassData)->cdcState &= ~CDC_WRITE_DELAY;
    CDC_TxReady();
  }
}

unsigned char *USB_GetHIDReportDesc(unsigned int *len) {
  if (len) *len = HID_MOUSE_REPORT_DESC_SIZE;
#ifdef NOHID
  return 0;
#else
  return HID_ReportDesc;
#endif
}
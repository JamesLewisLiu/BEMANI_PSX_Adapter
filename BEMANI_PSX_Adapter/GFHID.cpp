#include "GFHID.h"
#include <string.h>
#include <avr/pgmspace.h>

/* HID string and device descriptor */
const DeviceDescriptor PROGMEM GF_USB_DeviceDescriptor =
  D_DEVICE(USB_DEVICE_CLASS_HUMAN_INTERFACE, 0, 0, 64, 0x1ccf, 0x1002, 0x100, IMANUFACTURER, IPRODUCT, ISERIAL, 1);

static bool SendControl(uint8_t d) {
  return USB_SendControl(0, &d, 1) == 1;
}

static bool USB_SendStringDescriptor(const char* string_P, uint8_t string_len, uint8_t flags) {
  SendControl(2 + string_len * 2);
  SendControl(3);
  bool pgm = flags & TRANSFER_PGM;
  for (uint8_t i = 0; i < string_len; i++) {
    bool r = SendControl(pgm ? pgm_read_byte(&string_P[i]) : string_P[i]);
    r &= SendControl(0);
    if (!r) {
      return false;
    }
  }
  return true;
}

static bool USB_SendStringDescriptorUTF16(const uint16_t* string_P, uint8_t string_len) {
  SendControl(2 + string_len * 2);
  SendControl(3);
  for (uint8_t i = 0; i < string_len; i++) {
    uint16_t w = pgm_read_word(&string_P[i]);
    if (!SendControl(w & 0xFF)) {
      return false;
    }
    if (!SendControl((w >> 8) & 0xFF)) {
      return false;
    }
  }
  return true;
}

static const uint8_t PROGMEM _hidReportGF[] = {
  0x05, 0x01,                    /* USAGE_PAGE (Generic Desktop) */
  0x09, 0x05,                    /* USAGE (Game Pad) */
  0xa1, 0x01,                    /* COLLECTION (Application) */

  0x85, GF_REPORT_ID,            /*   REPORT_ID (6) */
  0x05, 0x09,                    /*   USAGE_PAGE (Button) */
  0x19, 0x01,                    /*   USAGE_MINIMUM (Button 1) */
  0x29, GF_NUMBER_OF_BUTTONS,    /*   USAGE_MAXIMUM (Button 7) */
  0x15, 0x00,                    /*   LOGICAL_MINIMUM (0) */
  0x25, 0x01,                    /*   LOGICAL_MAXIMUM (1) */
  0x75, 0x01,                    /*   REPORT_SIZE (1) */
  0x95, GF_NUMBER_OF_BUTTONS,    /*   REPORT_COUNT (7) */
  0x81, 0x02,                    /*   INPUT (Data,Var,Abs) */

  /* Padding to next byte */
  0x75, 0x01,                    /*   REPORT_SIZE (1) */
  0x95, 0x01,                    /*   REPORT_COUNT (1) */
  0x81, 0x03,                    /*   INPUT (Cnst,Var,Abs) */

  0xc0                           /* END_COLLECTION */
};

static const char* const PROGMEM GF_String_Manufacturer = "Konami Digital Entertainment Co., Ltd.";
static const uint16_t PROGMEM GF_String_Product[] = {
  0x30ae, 0x30bf, 0x30fc, 0x30d5, 0x30ea, 0x30fc, 0x30af, 0x30b9, 
  0x0020, 0x30a2, 0x30fc, 0x30b1, 0x30fc, 0x30c9, 0x30b9, 0x30bf, 
  0x30a4, 0x30eb, 0x30b3, 0x30f3, 0x30c8, 0x30ed, 0x30fc, 0x30e9
};
static const char* const PROGMEM GF_String_Serial = "GFHID";
static constexpr uint8_t GF_STRING_PRODUCT_LEN = (sizeof(GF_String_Product) / sizeof(uint16_t)) - 1;

GFHID_::GFHID_() : PluggableUSBModule(1, 1, epType) {
  epType[0] = EP_TYPE_INTERRUPT_IN;
}

int GFHID_::getInterface(uint8_t* interfaceCount) {
  *interfaceCount += 1; // uses 1
  HIDDescriptor hidInterface = {
    D_INTERFACE(pluggedInterface, 1, USB_DEVICE_CLASS_HUMAN_INTERFACE, HID_SUBCLASS_NONE, HID_PROTOCOL_NONE),
    D_HIDREPORT(sizeof(_hidReportGF)),
    D_ENDPOINT(USB_ENDPOINT_IN(pluggedEndpoint), USB_ENDPOINT_TYPE_INTERRUPT, USB_EP_SIZE, USB_EP_BINTERVAL)
  };
  return USB_SendControl(0, &hidInterface, sizeof(hidInterface));
}

int GFHID_::getDescriptor(USBSetup& setup) {
  if (setup.wValueH == USB_DEVICE_DESCRIPTOR_TYPE) {
    return USB_SendControl(TRANSFER_PGM, (const uint8_t*)&GF_USB_DeviceDescriptor, sizeof(GF_USB_DeviceDescriptor));
  }
  if (setup.wValueH == USB_STRING_DESCRIPTOR_TYPE) {
    if (setup.wValueL == IPRODUCT) {
      return USB_SendStringDescriptorUTF16(GF_String_Product, GF_STRING_PRODUCT_LEN);
    }
    else if (setup.wValueL == IMANUFACTURER) {
      return USB_SendStringDescriptor(GF_String_Manufacturer, strlen(GF_String_Manufacturer), 0);
    }
    else if (setup.wValueL == ISERIAL) {
      return USB_SendStringDescriptor(GF_String_Serial, strlen(GF_String_Serial), 0);
    }
  }

  if (setup.bmRequestType != REQUEST_DEVICETOHOST_STANDARD_INTERFACE) {
    return 0;
  }

  if (setup.wValueH != HID_REPORT_DESCRIPTOR_TYPE) {
    return 0;
  }

  if (setup.wIndex != pluggedInterface) {
    return 0;
  }

  return USB_SendControl(TRANSFER_PGM, _hidReportGF, sizeof(_hidReportGF));
}

bool GFHID_::setup(USBSetup& setup) {
  if (pluggedInterface != setup.wIndex) {
    return false;
  }

  uint8_t request = setup.bRequest;
  uint8_t requestType = setup.bmRequestType;

  if (requestType == REQUEST_DEVICETOHOST_CLASS_INTERFACE) {
    // No feature reports to return
    return true;
  }

  if (requestType == REQUEST_HOSTTODEVICE_CLASS_INTERFACE) {
    if (request == HID_SET_PROTOCOL) {
      protocol = setup.wValueL;
      return true;
    }
    if (request == HID_SET_IDLE) {
      idle = setup.wValueL;
      return true;
    }
  }

  return false;
}

int GFHID_::sendState(uint32_t buttonsState) {
  uint8_t data[2];
  data[0] = GF_REPORT_ID;
  data[1] = (uint8_t)(buttonsState & 0x7F);
  return USB_Send(pluggedEndpoint | TRANSFER_RELEASE, data, 2);
}
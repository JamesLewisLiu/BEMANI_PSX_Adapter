#include "HID.h"

#define USB_EP_BINTERVAL 1
#define EPTYPE_DESCRIPTOR_SIZE uint8_t

#define GF_NUMBER_OF_BUTTONS 7
#define GF_REPORT_ID 6

class GFHID_ : public PluggableUSBModule {
  public:
    GFHID_();
    int sendState(uint32_t buttonsState);

  protected:
    EPTYPE_DESCRIPTOR_SIZE epType[1];
    uint8_t protocol;
    uint8_t idle;

    int getInterface(uint8_t* interfaceCount);
    int getDescriptor(USBSetup& setup);
    bool setup(USBSetup& setup);
};

extern GFHID_ GFHID;

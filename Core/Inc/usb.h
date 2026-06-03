#ifndef USB_H_
#define USB_H_

#include "stm32f1xx.h"

void USB_Hardware_Init(void);
void USB_Core_Init(void);

typedef struct{
    uint8_t  bmRequestType; // Direction of data, type of request, and recipient
    uint8_t  bRequest;      // The specific command
    uint16_t wValue;        // Parameters varying based on the request
    uint16_t wIndex;        // Index or offset
    uint16_t wLength;       // Number of bytes to transfer if there is a Data Stage
}USB_SetupPacket;

// The Global Standard Boot Keyboard Report
typedef struct{
    uint8_t modifiers; // Bit 0=LCtrl, 1=LShift, 2=LAlt, 3=LGUI, 4=RCtrl, 5=RShift, 6=RAlt, 7=RGUI
    uint8_t reserved;  // Always 0x00
    uint8_t keys[6];   // Array of up to 6 simultaneous HID keycodes
}HID_KeyboardReport;

void USB_Send_Keystroke(HID_KeyboardReport *report);

#endif /* USB_H_ */
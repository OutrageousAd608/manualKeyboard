#include "usb.h"

static uint8_t pending_device_address = 0;

/**
 * @brief  Configures physical pins (PA11/PA12), enables peripheral clocks,
 * and registers the low-priority USB interrupt in the NVIC.
 */
void USB_Hardware_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USBEN;
    NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
}

/**
 * @brief  Brings the USB macrocell out of power-down mode and
 * unmasks core protocol interrupts (Reset, Correct Transfer).
 */
void USB_Core_Init(void) {
    USB->CNTR = USB_CNTR_FRES; 
    USB->CNTR = 0; 
    USB->ISTR = 0; 
    USB->CNTR = USB_CNTR_RESETM | USB_CNTR_CTRM;
}

#define PMA_BASE ((volatile uint32_t*)(USB_BASE + 0x400))

void USB_LP_CAN1_RX0_IRQHandler(void) {
    uint16_t istr = USB->ISTR;

    // -------------------------------------------------------------
    // EVENT 1: USB BUS RESET
    // -------------------------------------------------------------
    if (istr & USB_ISTR_RESET) {
        USB->ISTR = (uint16_t)(~USB_ISTR_RESET);
        USB->DADDR = USB_DADDR_EF | 0;
        USB->BTABLE = 0x0000;

        PMA_BASE[0] = 0x0020;  // EP0_TX Offset (byte 64)
        PMA_BASE[1] = 0x0000;  // EP0_TX Count initially 0
        PMA_BASE[2] = 0x0060;  // EP0_RX Offset (byte 192)
        PMA_BASE[3] = 0x8400;  // EP0_RX 64-byte block allocation limit

        // Configure Endpoint 0 Register: STAT_RX=VALID, STAT_TX=NAK, TYPE=CONTROL
        USB->EP0R = USB_EP_CTR_RX | USB_EP_CTR_TX | USB_EP_CONTROL | (3 << 12) | (2 << 4);

    }
    
    // -------------------------------------------------------------
    // EVENT 2: CORRECT TRANSFER (Packet Received/Sent)
    // -------------------------------------------------------------
    if (istr & USB_ISTR_CTR) {
        uint8_t ep_num = istr & USB_ISTR_EP_ID;

        // RX on endpoint 0
        if (ep_num == 0) {
            uint16_t epr = USB->EP0R;
            if (epr & USB_EP_SETUP) {
                uint16_t w0 = PMA_BASE[0x0030];
                uint16_t w1 = PMA_BASE[0x0031];
                uint16_t w2 = PMA_BASE[0x0032];
                uint16_t w3 = PMA_BASE[0x0033];

                USB_SetupPacket req;
                req.bmRequestType = w0 & 0xFF;
                req.bRequest      = (w0 >> 8) & 0xFF;
                req.wValue        = w1;
                req.wIndex        = w2;
                req.wLength       = w3;

                USB->EP0R = (epr & 0x070F) | USB_EP_CTR_TX;

                uint8_t request_handled = 0; 

                if (req.bRequest == 0x06) {
                    uint8_t descriptor_type = (req.wValue >> 8) & 0xFF;

                    if (descriptor_type == 1) { // Device descriptor
                        const uint8_t dev_desc[18] = {
                            0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40,
                            0x83, 0x04, 0x22, 0x22, 0x00, 0x02, 0x01, 0x02,
                            0x00, 0x01
                        };

                        uint32_t pma_word_idx = 0x0010;
                        for (int i = 0; i < 18; i += 2) {
                            PMA_BASE[pma_word_idx++] = dev_desc[i] | (dev_desc[i + 1] << 8);
                        }
                        PMA_BASE[1] = 18; 

                        // Set TX=VALID and RX=VALID
                        uint16_t safe_epr = (USB->EP0R & 0x070F) | 0x8080;
                        uint16_t stat_tx = (USB->EP0R >> 4) & 0x03;
                        uint16_t stat_rx = (USB->EP0R >> 12) & 0x03;
                        USB->EP0R = safe_epr | ((stat_tx ^ 0x03) << 4) | ((stat_rx ^ 0x03) << 12);
                        
                        request_handled = 1;
                    }
                    else if (descriptor_type == 2) { // Configuration Descriptor
                        const uint8_t config_desc[34] = {
                            0x09, 0x02, 0x22, 0x00, 0x01, 0x01, 0x00, 0xE0, 0x32,
                            0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x01, 0x01, 0x00,
                            0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x3F, 0x00,
                            0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0x0A
                        };

                        uint16_t bytes_to_send = (req.wLength < 34) ? req.wLength : 34;
                        uint32_t pma_word_idx = 0x0010; 
                        for (int i = 0; i < bytes_to_send; i += 2) {
                            uint16_t val = config_desc[i];
                            if ((i + 1) < bytes_to_send) val |= (config_desc[i + 1] << 8);
                            PMA_BASE[pma_word_idx++] = val;
                        }

                        PMA_BASE[1] = bytes_to_send; 

                        uint16_t safe_epr = (USB->EP0R & 0x070F) | 0x8080;
                        uint16_t stat_tx = (USB->EP0R >> 4) & 0x03;
                        uint16_t stat_rx = (USB->EP0R >> 12) & 0x03;
                        USB->EP0R = safe_epr | ((stat_tx ^ 0x03) << 4) | ((stat_rx ^ 0x03) << 12);
                        
                        request_handled = 1;
                    }
                    else if (descriptor_type == 0x22) { // HID Report Descriptor
                        const uint8_t report_desc[63] = {
                            0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x05, 0x07, 0x19, 0xe0,
                            0x29, 0xe7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08,
                            0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x03, 0x95, 0x05,
                            0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
                            0x95, 0x01, 0x75, 0x03, 0x91, 0x03, 0x95, 0x06, 0x75, 0x08,
                            0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65,
                            0x81, 0x00, 0xc0
                        };

                        uint16_t bytes_to_send = (req.wLength < 63) ? req.wLength : 63;
                        uint32_t pma_word_idx = 0x0010; 
                        for (int i = 0; i < bytes_to_send; i += 2) {
                            uint16_t val = report_desc[i];
                            if ((i + 1) < bytes_to_send) val |= (report_desc[i + 1] << 8);
                            PMA_BASE[pma_word_idx++] = val;
                        }
                        
                        PMA_BASE[1] = bytes_to_send;

                        uint16_t safe_epr = (USB->EP0R & 0x070F) | 0x8080;
                        uint16_t stat_tx = (USB->EP0R >> 4) & 0x03;
                        uint16_t stat_rx = (USB->EP0R >> 12) & 0x03;
                        USB->EP0R = safe_epr | ((stat_tx ^ 0x03) << 4) | ((stat_rx ^ 0x03) << 12);
                        
                        request_handled = 1;
                    }
                    else if (descriptor_type == 3) { // String Descriptors
                        uint8_t string_index = req.wValue & 0xFF;
                        const uint8_t *str_ptr = 0;
                        uint16_t str_len = 0;

                        // English US
                        const uint8_t lang_id[4] = { 0x04, 0x03, 0x09, 0x04 };
                        
                        // Manufacture length is 2 + (2 * bytes)
                        // richad
                        const uint8_t mfg_str[14] = {
                            14, 0x03,
                            'r',0, 'i',0, 'c',0, 'h',0, 'a',0, 'd',0
                        };

                        // name of device
                        // richad's Keyboard
                        const uint8_t prod_str[36] = {
                            36, 0x03,
                            'r',0, 'i',0, 'c',0, 'h',0, 'a',0, 'd',0, '\'',0, 's',0, ' ',0,
                            'K',0, 'e',0, 'y',0, 'b',0, 'o',0, 'a',0, 'r',0, 'd',0
                        };

                        if (string_index == 0) {
                            str_ptr = lang_id;
                            str_len = sizeof(lang_id);
                        } else if (string_index == 1) {
                            str_ptr = mfg_str;
                            str_len = sizeof(mfg_str);
                        } else if (string_index == 2) {
                            str_ptr = prod_str;
                            str_len = sizeof(prod_str);
                        }

                        if (str_ptr != 0) {
                            uint16_t bytes_to_send = (req.wLength < str_len) ? req.wLength : str_len;
                            uint32_t pma_word_idx = 0x0010; 
                            
                            for (int i = 0; i < bytes_to_send; i += 2) {
                                uint16_t val = str_ptr[i];
                                if ((i + 1) < bytes_to_send) val |= (str_ptr[i + 1] << 8);
                                PMA_BASE[pma_word_idx++] = val;
                            }
                            
                            PMA_BASE[1] = bytes_to_send;

                            uint16_t safe_epr = (USB->EP0R & 0x070F) | 0x8080;
                            uint16_t stat_tx = (USB->EP0R >> 4) & 0x03;
                            uint16_t stat_rx = (USB->EP0R >> 12) & 0x03;
                            USB->EP0R = safe_epr | ((stat_tx ^ 0x03) << 4) | ((stat_rx ^ 0x03) << 12);
                            
                            request_handled = 1;
                        }
                    }
                }
                else if (req.bRequest == 0x05) {
                    pending_device_address = req.wValue & 0x7F;
                    PMA_BASE[1] = 0; 

                    uint16_t safe_epr = (USB->EP0R & 0x070F) | 0x8080;
                    uint16_t stat_tx = (USB->EP0R >> 4) & 0x03;
                    USB->EP0R = safe_epr | ((stat_tx ^ 0x03) << 4);
                    
                    request_handled = 1;
                }
                else if (req.bRequest == 0x09) {
                    // Acknowledge the configuration request with a 0-byte Status IN packet
                    PMA_BASE[1] = 0;

                    uint16_t safe_epr = (USB->EP0R & 0x070F) | 0x8080;
                    uint16_t stat_tx = (USB->EP0R >> 4) & 0x03;
                    USB->EP0R = safe_epr | ((stat_tx ^ 0x03) << 4);

                    PMA_BASE[4] = 0x0080; 
                    PMA_BASE[5] = 0x0000; // Transmit count starts at 0

                    uint16_t ep1r_safe = (USB->EP1R & 0x070F) | 0x8080;
                    uint16_t ep1_stat_tx = (USB->EP1R >> 4) & 0x03;
                    USB->EP1R = (ep1r_safe & 0xFFF0) | 1 | ((ep1_stat_tx ^ 0x02) << 4); 
                    
                    request_handled = 1;
                }
                else if (req.bmRequestType == 0x21 && req.bRequest == 0x0A) {
                    PMA_BASE[1] = 0; 

                    uint16_t safe_epr = (USB->EP0R & 0x070F) | 0x8080;
                    uint16_t stat_tx = (USB->EP0R >> 4) & 0x03;
                    USB->EP0R = safe_epr | ((stat_tx ^ 0x03) << 4);
                    
                    request_handled = 1;
                }

                // ====================================================
                // THE SAFETY NET: STALL UNKNOWN REQUESTS
                // ====================================================
                if (request_handled == 0) {
                    // Reply with STALL (0x01) on both RX and TX to reject it instantly.
                    uint16_t safe_epr = (USB->EP0R & 0x070F) | 0x8080;
                    uint16_t stat_tx = (USB->EP0R >> 4) & 0x03;
                    uint16_t stat_rx = (USB->EP0R >> 12) & 0x03;
                    
                    USB->EP0R = safe_epr | ((stat_tx ^ 0x01) << 4) | ((stat_rx ^ 0x01) << 12);
                }
            }
            // DATA IN COMPLETE
            else if (epr & USB_EP_CTR_TX) {
                USB->EP0R = (epr & 0x070F) | USB_EP_CTR_RX;

                if (pending_device_address != 0) {
                    USB->DADDR = USB_DADDR_EF | pending_device_address;
                    pending_device_address = 0;
                }
            }
            // STATUS OUT COMPLETE
            else if (epr & USB_EP_CTR_RX) {
                USB->EP0R = (epr & 0x070F) | USB_EP_CTR_TX;
            }
        }
        
        // TX on endpoint 1
        if (ep_num == 1) {
            uint16_t ep1r = USB->EP1R;
            if (ep1r & USB_EP_CTR_TX) {
                USB->EP1R = (ep1r & 0x070F) | USB_EP_CTR_RX; // need this to clear flag ro we stay here forever
            }
        }

        // Clear the global interrupt flag
        USB->ISTR = (uint16_t)(~USB_ISTR_CTR);
    }
}

/**
 * @brief  Packs an 8-byte input report structure into the Endpoint 1 PMA buffer
 * and sets the status to VALID to signal transmission to the host.
 */
void USB_Send_Keystroke(HID_KeyboardReport *report) {
    uint8_t *byte_ptr = (uint8_t*)report;
    
    uint32_t pma_idx = 0x0040; 
    
    for (int i = 0; i < 8; i += 2) {
        PMA_BASE[pma_idx++] = byte_ptr[i] | (byte_ptr[i + 1] << 8);
    }

    PMA_BASE[5] = 8;

    uint16_t epr = USB->EP1R;
    uint16_t safe_epr = (epr & 0x070F) | 0x8080;
    uint16_t stat_tx = (epr >> 4) & 0x03;
    uint16_t toggle_mask = stat_tx ^ 0x03;

    USB->EP1R = safe_epr | (toggle_mask << 4);
}
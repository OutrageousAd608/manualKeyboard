#include "stm32f1xx.h"
#include "usb.h"

void SystemClock_Config(void);
void Buttons_Init(void);

int main(void) {

    SystemClock_Config();
    
    Buttons_Init();
    
    USB_Hardware_Init();
    USB_Core_Init();

    HID_KeyboardReport my_report = {0};
    
    uint8_t last_b0_state = 0;
    uint8_t last_b1_state = 0;

    while (1) {
        uint8_t current_b0 = (GPIOB->IDR & (1 << 0)) ? 1 : 0;
        uint8_t current_b1 = (GPIOB->IDR & (1 << 1)) ? 1 : 0;

        if (current_b0 != last_b0_state || current_b1 != last_b1_state) {
            
            for(int i = 0; i < 6; i++) {
                my_report.keys[i] = 0;
            }
            
            int key_index = 0;

            if (current_b0) {
                my_report.keys[key_index++] = 0x15; // 'A' key
            }

            if (current_b1) {
                my_report.keys[key_index++] = 0x07; // 'B' key
            }

            USB_Send_Keystroke(&my_report);

            // Debounce delay
            for (volatile int i = 0; i < 150000; i++); 
        }

        last_b0_state = current_b0;
        last_b1_state = current_b1;
    }
}

void SystemClock_Config(void) {
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    FLASH->ACR |= FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

    RCC->CFGR |= RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;
    RCC->CFGR &= ~RCC_CFGR_USBPRE; 

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

void Buttons_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    
    GPIOB->CRL &= ~((0xF << 0) | (0xF << 4)); 
    
    GPIOB->CRL |= ((0x8 << 0) | (0x8 << 4));
    
    GPIOB->ODR &= ~((1 << 0) | (1 << 1)); 
}

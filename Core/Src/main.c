#include "stm32f1xx.h"

void SystemClock_Config(void);
void USB_Hardware_Init(void);
void LED_Init(void);

int main(void) {

    SystemClock_Config();

    USB_Hardware_Init();

    LED_Init();

    GPIOC->BSRR = GPIO_BSRR_BR13; // Reset bit turns ON the active-low LED

    while (1) {

    }
}

void SystemClock_Config(void) {

    RCC->CR |= RCC_CR_HSEON;

    while (!(RCC->CR & RCC_CR_HSERDY));

    // Configure Flash prefetch and 2 wait states (required for 72MHz operation)
    FLASH->ACR |= FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

    // Configure the PLL:
    // Source = HSE, Multiplier = 9 (8MHz * 9 = 72MHz)
    // USB Prescaler = 0 (Divide 72MHz by 1.5 = 48MHz)
    RCC->CFGR |= RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;
    RCC->CFGR &= ~RCC_CFGR_USBPRE;

    // Enable the PLL
    RCC->CR |= RCC_CR_PLLON;
    // Wait for PLL to lock
    while (!(RCC->CR & RCC_CR_PLLRDY));

    // Switch System Clock source to the PLL
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    // Wait until the hardware confirms the switch
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

void USB_Hardware_Init(void) {
    // 1. Enable clock for GPIOA (USB D- and D+ are PA11 and PA12)
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // 2. Enable clock for the USB Peripheral itself
    RCC->APB1ENR |= RCC_APB1ENR_USBEN;

    // 3. Enable the USB Low Priority Interrupt in the NVIC
    NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
}

void LED_Init(void) {
    // The Blue Pill LED is on PC13
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN; // Enable GPIOC clock

    // Configure PC13 as General Purpose Output Push-Pull, max speed 50MHz
    GPIOC->CRH &= ~GPIO_CRH_MODE13;  // Clear mode
    GPIOC->CRH &= ~GPIO_CRH_CNF13;   // Clear configuration
    GPIOC->CRH |= GPIO_CRH_MODE13;   // Set mode to Output 50MHz
}

void USB_LP_CAN1_RX0_IRQHandler(void) {
   
}

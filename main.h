#ifndef main_h
#define main_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define wdt_reset() NRF_WDT->RR[0] = WDT_RR_RR_Reload

#define wdt_enable(timeout) \
  NRF_WDT->CONFIG = NRF_WDT->CONFIG = (WDT_CONFIG_HALT_Run << WDT_CONFIG_HALT_Pos) | ( WDT_CONFIG_SLEEP_Run << WDT_CONFIG_SLEEP_Pos); \
  NRF_WDT->CRV = (32768*timeout)/1000; \
  NRF_WDT->RREN |= WDT_RREN_RR0_Msk;  \
  NRF_WDT->TASKS_START = 1 

void flash_led (uint8_t pin, uint32_t delay, uint32_t count);
void nrf_bootloader_app_start(uint32_t start_addr);
bool start_Firmware(uint32_t sizeFw);
uint32_t flash_read(uint32_t address);
uint8_t flash_read_byte(uint32_t address);
static void flash_word_write(uint32_t address, uint32_t value);
static void flash_page_erase(uint32_t page_address);
uint16_t FLASH_CRC(uint32_t FW_size);
uint16_t flash_CRC(uint32_t FW_size);





#ifdef __cplusplus
}
#endif

#endif // main_h
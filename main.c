/* Copyright (c) 2019 Dab0G
    MySensors nrf52 bootloader for OTA
 */


//#include <stdint.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "main.h"

#ifdef MY_DEBUG
    #include "SEGGER_RTT.h"
#endif // MY_DEBUG

#include "SPIFlash.h"   

#define APPLICATION_START_ADDRESS ((uint32_t)0x02000)   // Начало основной программы
#define OFFSET_ADDRESS 12           // Смещение прошивки (размер заголовка прошивки)
#define LED_PIN 8                   // Набортный светодиод
#define MY_JEDEC 0x0000 //0x4018             // JEDEC Id используемой SPI flash, при 0x0000 - любая
#define PAGE_SIZE 4096              // Размер flash страницы в nrf5

#define WATCHDOG_s 10000

extern void nrf_bootloader_app_start_impl(uint32_t start_addr);
extern uint32_t CheckFlashImage(uint32_t);
uint8_t record_counter = 0;

int main(void)
{
    #ifdef WATCHDOG_s
        wdt_enable(WATCHDOG_s);
    #endif // WATCHDOG_s

    #ifdef MY_DEBUG
        //SEGGER_RTT_WriteString(0, "\n\n***********\n");
    #endif // MY_DEBUG
    
    #ifdef LED_PIN
        //leds_init
        nrf_gpio_cfg_output(LED_PIN);
    #endif // LED_PIN

    //Flash memory
    FLASH_Init();
    FLASH_WakeUP();

    // Проверка наличия флешки
    if (JEDEC_id(MY_JEDEC)) {
        #ifdef MY_DEBUG
            SEGGER_RTT_printf(0, "Need SPI flash found.\r\n");
        #endif // MY_DEBUG
        // Проверка наличия заголовка
        uint32_t FW_size=CheckFlashImage(0x00000);
        if (FW_size) { // Если не ноль, то обновляемся
            #ifdef LED_PIN                
                // Заголовок найден, мигаем 3 раз и обновляем
                flash_led(LED_PIN, 500, 3);
            #endif // LED_PIN
            #ifdef MY_DEBUG
                SEGGER_RTT_printf(0, "New fw found, start update process ...\r\n");
            #endif // MY_DEBUG
            // Обновляем программу 
            start_Firmware(FW_size);
            // Очистка заголовка
            #ifdef MY_DEBUG
                SEGGER_RTT_printf(0, "Start main application ...\r\n");
                nrf_delay_ms(50);
            #endif // MY_DEBUG
            //FLASH_Sleep();  // переводими SPI flash в сон
            // Проверить регистры флеша, после очистки его секторов
            
                #ifdef WATCHDOG_s
                    wdt_reset();
                #endif // WATCHDOG_s
                nrf_bootloader_app_start(APPLICATION_START_ADDRESS);             // Прыгаем в программу
          
        } else {
            #ifdef LED_PIN
                // Заголовок не найден, мигаем 2 раза и прыгаем в программу
                flash_led(LED_PIN, 500, 2);
            #endif // LED_PIN
            #ifdef MY_DEBUG
                SEGGER_RTT_printf(0, "New fw not found, start main application ...\r\n");
            #endif // MY_DEBUG

            //FLASH_Sleep();  // переводими SPI flash в сон            
            #ifdef WATCHDOG_s
                wdt_reset();
            #endif // WATCHDOG_s           
            // Прыгаем в программу
            nrf_bootloader_app_start(APPLICATION_START_ADDRESS);
        }

    } else {
        #ifdef LED_PIN
            // Флешка не найдена - мигаем светодиодом 1 раз
            flash_led(LED_PIN, 500, 1);
        #endif // LED_PIN
        #ifdef MY_DEBUG
            SEGGER_RTT_printf(0, "SPI flash not found, start main application ...\r\n");
        #endif // MY_DEBUG

        //FLASH_Sleep();  // переводими SPI flash в сон
        #ifdef WATCHDOG_s
            wdt_reset();
        #endif // WATCHDOG_s        
        nrf_bootloader_app_start(APPLICATION_START_ADDRESS);         // Прыгаем в программу
    }
}

#ifdef LED_PIN     
void flash_led (uint8_t pin, uint32_t delay, uint32_t count) {
	// Toggle LED. 
    for (uint8_t i=0; i<count; i++)
    {
		nrf_gpio_pin_set(pin);
        nrf_delay_ms(delay/2);
        nrf_gpio_pin_clear(pin);
		nrf_delay_ms(delay/2);
    }
}
#endif // LED_PIN

void nrf_bootloader_app_start(uint32_t start_addr) {    // Подготовка к прыжку в основную программу
    // Disable and clear interrupts
    // Notice that this disables only 'external' interrupts (positive IRQn).

    NVIC->ICER[0]=0xFFFFFFFF;
    NVIC->ICPR[0]=0xFFFFFFFF;
#if defined(__NRF_NVIC_ISER_COUNT) && __NRF_NVIC_ISER_COUNT == 2
    NVIC->ICER[1]=0xFFFFFFFF;
    NVIC->ICPR[1]=0xFFFFFFFF;
#endif

    SCB->VTOR = APPLICATION_START_ADDRESS;  // Меняем адрес таблицы векторов прерываний
    // Run application
    nrf_bootloader_app_start_impl(start_addr);
}

bool start_Firmware(uint32_t sizeFw) {
    // Надо ли делать Erase страницы?
    for (uint8_t page=APPLICATION_START_ADDRESS/PAGE_SIZE; page<(APPLICATION_START_ADDRESS+sizeFw)/PAGE_SIZE;page++) {
        for (uint16_t i=0;i<PAGE_SIZE;i=i+4) {
            if (flash_read((page*PAGE_SIZE)+i) != 0xFF) {
                flash_page_erase(page*PAGE_SIZE);   // очищаем страницу
                #ifdef WATCHDOG_s
                    wdt_reset();
                #endif // WATCHDOG_s
                #ifdef MY_DEBUG        
                // Чистим страницу
                    SEGGER_RTT_printf(0, "Erase page 0x%x: \r\n", page);
                    nrf_delay_ms(10);
                #endif // MY_DEBUG  
                break;
            }
        }
    }

        for (uint32_t i=0;i<sizeFw;i=i+4){
            // Ждём доступности флэша МК
            while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
                }
            // Ждём доступности SPI флэша
            while ( FLASH_Is_Busy() ) {
            }
            flash_word_write(APPLICATION_START_ADDRESS+i, FLASH_Read_Word(OFFSET_ADDRESS+i));
        }
        FLASH_Write_Byte(0, 0); // Портим заголовок, чтобы лишний раз не чистить
    return true;
}

uint32_t flash_read(uint32_t address)
{
    return (*(__IO uint32_t*)address);
}

uint8_t flash_read_byte(uint32_t address)
{
    return (uint8_t)(*(__IO uint32_t*)address);
}

/** @brief Function for filling a page in flash with a value.
 *
 * @param[in] address Address of the first word in the page to be filled.
 * @param[in] value Value to be written to flash.
 */
static void flash_word_write(uint32_t address, uint32_t value)
{
    // Turn on flash write enable and wait until the NVMC is ready:
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    *(__IO uint32_t*)address = value;

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    // Turn off flash write enable and wait until the NVMC is ready:
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }
}

static void flash_page_erase(uint32_t page_address) //TODO: Убрал * - надо проверить
{
    // Turn on flash erase enable and wait until the NVMC is ready:
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    // Erase page:
    NRF_NVMC->ERASEPAGE = (uint32_t)page_address;

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    // Turn off flash erase enable and wait until the NVMC is ready:
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }
}

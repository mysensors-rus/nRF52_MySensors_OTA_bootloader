#ifndef SPIFlash_h
#define SPIFlash_h

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_drv_spi.h"
  
void FLASH_Init( void );
void FLASH_WakeUP( void );
void FLASH_Sleep( void );
bool FLASH_Is_Busy( void ); 
bool FLASH_Set_Write_Enable( void );
uint32_t FLASH_Read_Word(uint32_t address);
uint8_t  FLASH_Read_Byte(uint32_t address);
void FLASH_Write_Byte( uint32_t address, uint8_t data );
bool JEDEC_id( uint16_t need_jedec );

#ifdef __cplusplus
}
#endif

#endif /* SPIFlash_h */


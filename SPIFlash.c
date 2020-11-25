#include "string.h"
#include "SPIFlash.h"
#include "nrf_delay.h"
#ifdef MY_DEBUG
  #include "SEGGER_RTT.h"
#endif // MY_DEBUG

//#define NRF_LOG_MODULE_NAME "FLASH"

#define CMD_PAGE_PROGRAM        ((uint8_t)0x02)
#define CMD_WRITE_ENABLE        ((uint8_t)0x06)
#define CMD_READ_DATA           ((uint8_t)0x03)
#define CMD_READ_ID             ((uint8_t)0x9F)
#define RELEASE                 ((uint8_t)0xAB)
#define SLEEP                   ((uint8_t)0xB9)
#define CMD_READ_STATUS_REG     ((uint8_t)0x05)
#define SECTOR_ERASE            ((uint8_t)0x20)

#define SPI_INSTANCE_ID   1
// #define QUEUE_LENGTH     10

// //will return either a 1 or 0 if the bit is enabled
#define CHECK_BIT(var,pos) (((var)>>(pos)) & 1)

static const nrf_drv_spi_t m_spi_master_1 = NRF_DRV_SPI_INSTANCE(1);

static uint8_t cmd[1] = { 0 };

static uint8_t tx4[4];
static uint8_t tx5[5];

static uint8_t rx2[2];
static uint8_t rx4[4];

static uint8_t rx20[16+4];

#define APP_IRQ_PRIORITY_LOW 3  //overrides definition elsewhere

void FLASH_Init( void )
{
    nrf_drv_spi_config_t const spi_config =
    {
        .sck_pin        = 14, 
        .mosi_pin       = 13,    
        .miso_pin       = 16,
        .ss_pin         = 15,
        .irq_priority   = APP_IRQ_PRIORITY_LOW,
        .orc            = 0xFF,
        .frequency      = NRF_DRV_SPI_FREQ_8M,
        .mode           = NRF_DRV_SPI_MODE_0,
        .bit_order      = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST, 
    };
        
    nrf_drv_spi_init(&m_spi_master_1, &spi_config, NULL, NULL);
}

void FLASH_WakeUP( void )
{
    cmd[0] = RELEASE; //WakeUP
    nrf_drv_spi_transfer(&m_spi_master_1, cmd, sizeof(cmd), NULL, 0); 
}

void FLASH_Sleep( void )
{
    cmd[0] = SLEEP; //WakeUP
    nrf_drv_spi_transfer(&m_spi_master_1, cmd, sizeof(cmd), NULL, 0);  
}


uint8_t FLASH_Read_Status( void )
{
    memset(rx2, 0, sizeof(rx2));
    cmd[0] = CMD_READ_STATUS_REG;
    nrf_drv_spi_transfer(&m_spi_master_1, cmd, sizeof(cmd), rx2, sizeof(rx2));
    
    /* S0; 1 = busy
       S1; Write Enable Latch (WEL) is a read only bit in the status register 
       that is set to 1 after executing a Write Enable Instruction. 
       The WEL status bit is cleared to 0 when the device is write disabled
    */

    if ( CHECK_BIT(rx2[1],0) ) {  
        //NRF_LOG_DEBUG("SR: Busy\r\n")
  //  } else {
        //NRF_LOG_DEBUG("SR: Ready\r\n")
    };
   
    if ( CHECK_BIT(rx2[1],1) ) {
        //NRF_LOG_DEBUG("SR: WEL\r\n")
    // } else {
        //NRF_LOG_DEBUG("SR: NoWEL\r\n")
    };
    
    return rx2[1];
}

bool FLASH_Is_Write_Enabled( void ) 
{
    memset(rx2, 0, sizeof(rx2));
    cmd[0] = CMD_READ_STATUS_REG;
    nrf_drv_spi_transfer(&m_spi_master_1, cmd, sizeof(cmd), rx2, sizeof(rx2));
    
    //S1 == 1 Write Enable Latch (WEL) is a read only bit in the status register 
    if ( CHECK_BIT(rx2[1],1) != 1) return false;
    return true;
}

bool FLASH_Is_Busy( void ) 
{
    memset(rx2, 0, sizeof(rx2));
    cmd[0] = CMD_READ_STATUS_REG;
    nrf_drv_spi_transfer(&m_spi_master_1, cmd, sizeof(cmd), rx2, sizeof(rx2));
    
    if ( CHECK_BIT(rx2[1],0) == 1 ) return true;
    return false;
}

void sector_erase (uint32_t addr) {
  while ( FLASH_Is_Busy() );
  memset(tx4, 0, sizeof(tx4));
  
  tx4[0] = SECTOR_ERASE;
  tx4[1] = (addr >> 16) & 0xFF;
  tx4[2] = (addr >>  8) & 0xFF;
  tx4[3] =  addr        & 0xFF;

  nrf_drv_spi_transfer(&m_spi_master_1, tx4, sizeof(tx4), NULL, 0);
  //wait for some
  while ( FLASH_Is_Busy() ) {
  }
}


bool JEDEC_id( uint16_t need_jedec )
{
    cmd[0] = CMD_READ_ID;
    memset(rx4, 0, sizeof(rx4));
    nrf_drv_spi_transfer(&m_spi_master_1, cmd, sizeof(cmd), rx4, sizeof(rx4));
    
    volatile uint16_t find_jedec = (rx4[2] << 8) | rx4[3];
    if (find_jedec == 0xffff ) {
        return false;
    } else if (find_jedec == need_jedec ) {
        return true;
    } else if (need_jedec == 0x0000) {
        return true;
    } else {
        return false;
    }
}

void FLASH_Write_Byte( uint32_t address, uint8_t data )
{
  tx5[0] = CMD_PAGE_PROGRAM;
  tx5[1] = (address >> 16) & 0xFF;
  tx5[2] = (address >> 8)  & 0xFF;
  tx5[3] =  address        & 0xFF;
  tx5[4] =  data;

  nrf_drv_spi_transfer(&m_spi_master_1, tx5, sizeof(tx5), NULL, 0);
};


uint32_t CheckFlashImage(uint32_t addr) {

  memset(rx20, 0, sizeof(rx20));
  memset(tx4, 0, sizeof(tx4));
  
  tx4[0] = CMD_READ_DATA;
  tx4[1] = (addr >> 16) & 0xFF;
  tx4[2] = (addr >>  8) & 0xFF;
  tx4[3] = addr         & 0xFF;

  FLASH_Set_Write_Enable();

  nrf_drv_spi_transfer(&m_spi_master_1, tx4, sizeof(4), rx20, sizeof(rx20));

    if (rx20[4]==0x4d && rx20[5]==0x59 && rx20[6]==0x53 && rx20[7]==0x49 && rx20[8]==0x4d && rx20[9]==0x47 && rx20[10]==0x3a)
      {
      // Сигнатура прошивки найдена
      #ifdef MY_DEBUG
        //SEGGER_RTT_printf(0, "Header fw found, FW size: 0x%x%x%x%x\r\n", rx20[11],rx20[12],rx20[13],rx20[14]);
      #endif // MY_DEBUG
      return (rx20[11] << 24) | (rx20[12] << 16) | (rx20[13] << 8) | rx20[14];
    
      } else {
      // Сигнатура прошивки не найдена
      #ifdef MY_DEBUG
      //SEGGER_RTT_printf(0, "Header fw not found! 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x,0x%x, 0x%x, 0x%x, 0x%x\r\n", rx20[4],rx20[5],rx20[6],rx20[7],rx20[8],rx20[9],rx20[10],rx20[11],rx20[12],rx20[13]);
      #endif // MY_DEBUG

    return 0;
    }
 } 

uint32_t FLASH_Read_Word(uint32_t address)
{
   
  tx4[0] = CMD_READ_DATA;
  tx4[1] = (address >> 16) & 0xFF;
  tx4[2] = (address >>  8) & 0xFF;
  tx4[3] =  address        & 0xFF;

  memset( rx20, 0, sizeof( rx20));
  nrf_drv_spi_transfer(&m_spi_master_1, tx4, sizeof(tx4), rx20, 8);
  return rx20[7]<<24|rx20[6]<<16|rx20[5]<<8|rx20[4]; // TODO: Может менять местами в другом месте, а тут оставить правильный порядок?
  
}

uint8_t FLASH_Read_Byte(uint32_t address)
{
   
  tx4[0] = CMD_READ_DATA;
  tx4[1] = (address >> 16) & 0xFF;
  tx4[2] = (address >>  8) & 0xFF;
  tx4[3] =  address        & 0xFF;

  memset( rx20, 0, sizeof( rx20));
  nrf_drv_spi_transfer(&m_spi_master_1, tx4, sizeof(tx4), rx20, 5);
  return rx20[4]; 
  
}

//=====================================
void FLASH_Reset( void )
{
    cmd[0] = 0xF0;
    nrf_drv_spi_transfer(&m_spi_master_1, cmd, sizeof(cmd), NULL, 0); 
}

//=====================================
bool FLASH_Set_Write_Enable( void )
{

  //wait for some
  while ( FLASH_Is_Busy() ) {
      nrf_delay_ms(100);
  }

  bool enabled = false;
  cmd[0] = CMD_WRITE_ENABLE;
  nrf_drv_spi_transfer(&m_spi_master_1, cmd, sizeof(cmd), NULL, 0);
  
  if( FLASH_Is_Write_Enabled() ) {
    enabled = true;
  } else {
    enabled = false;
  };
    
  return enabled;
}


//EEPROM emulation library for HAL
//V1.0


//define to prevent recursive inclusion
#ifndef __EEPROM_H
#define __EEPROM_H

//includes
#include "stm32f1xx_hal.h"

//-------------------------------------------library configuration-------------------------------------------

//number of variables (maximum variable name is EEPROM_VARIABLE_COUNT - 1)
//max 255 on 1KByte page size, max 511 on 2KByte page size
//keep in mind that high VARIABLE_COUNT/PAGE_SIZE rations reduce the lifetime of the flash significantly
#define EEPROM_VARIABLE_COUNT	(uint16_t) 3

//flash size of used STM32F1XX device in KByte
#define EEPROM_FLASH_SIZE		(uint16_t) 64

//-------------------------------------------------constants-------------------------------------------------

//EEPROM emulation start address in flash: use last two pages of flash memory
#define EEPROM_START_ADDRESS	(uint32_t) (0x08000000 + 0x400 * EEPROM_FLASH_SIZE - 0x2 * FLASH_PAGE_SIZE)

//used flash pages for EEPROM emulation
typedef enum
{
	EEPROM_PAGE0			= EEPROM_START_ADDRESS,						//Page0
	EEPROM_PAGE1			= EEPROM_START_ADDRESS + FLASH_PAGE_SIZE,	//Page1
	EEPROM_PAGE_NONE		= 0x00000000								//no page
} EEPROM_Page;

//page status
typedef enum
{
	EEPROM_ERASED			= 0xFFFF,									//Page is empty
	EEPROM_RECEIVING		= 0xEEEE,									//Page is marked to receive data
	EEPROM_VALID			= 0x0000									//Page containing valid data
} EEPROM_PageStatus;

//results
typedef enum
{
	EEPROM_SUCCESS			= 0x00,										//Method successful / HAL_OK
	EEPROM_ERROR			= 0x01,										//HAL_ERROR occurred
	EEPROM_BUSY				= 0x02,										//HAL_BUSY occurred
	EEPROM_TIMEOUT			= 0x03,										//HAL_TIMEOUT occurred
	EEPROM_NO_VALID_PAGE	= 0x04,										//Error no valid page found
	EEPROM_PAGE_FULL		= 0x05,										//Error page full
	EEPROM_NOT_ASSIGNED		= 0x06,										//Error variable was never assigned
	EEPROM_INVALID_NAME		= 0x07,										//Error variable name to high for variable count
	EEPROM_UNKNOWN_ERROR	= 0x08										//unknown error occurred
} EEPROM_Result;

//operations
typedef enum
{
	EEPROM_READ				= 0x00,										//read operation
	EEPROM_WRITE			= 0x01										//write operation
} EEPROM_Operation;

//----------------------------------------------public functions---------------------------------------------

EEPROM_Result EEPROM_Init();
EEPROM_Result EEPROM_ReadVariable(uint16_t VariableName, uint16_t* Value);
EEPROM_Result EEPROM_WriteVariable(uint16_t VariableName, uint16_t Value);
EEPROM_Result EEPROM_Format();

#endif

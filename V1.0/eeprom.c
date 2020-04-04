//EEPROM emulation library for HAL
//V1.0


//includes
#include "eeprom.h"


//declare extern HAL function FLASH_PageErase
extern void FLASH_PageErase(uint32_t PageAddress);


//private function prototypes;
static EEPROM_Result EEPROM_TryWriteVariable(uint16_t VariableName, uint16_t Value);
static EEPROM_Result EEPROM_PageTransfer(EEPROM_Page SourcePage, EEPROM_Page TargetPage);
static EEPROM_Page EEPROM_FindValidPage(EEPROM_Operation Operation);
static EEPROM_Result EEPROM_SetValidPage(EEPROM_Page Page);
static EEPROM_Result EEPROM_PageErase(EEPROM_Page Page);
static void EEPROM_PageToIndex(EEPROM_Page Page);


//global variables
static uint16_t EEPROM_Index[EEPROM_VARIABLE_COUNT];													//EEPROM_Index[i]: actual address of variable i (as offset)
																										//physical address = EEPROM_START_ADDRESS + address
																										//if EEPROM_Index[i] = 0 variable i not initialized


//restore the pages to a known good state in case of page's status corruption after a power loss.
//return value is EEPROM_SUCCESS, EEPROM_NO_VALID_PAGE, EEPROM_PAGE_FULL, EEPROM_ERROR, EEPROM_BUSY or EEPROM_TIMEOUT
EEPROM_Result EEPROM_Init()
{
	EEPROM_Result result = EEPROM_SUCCESS;																//preset the result

	HAL_FLASH_Unlock();																					//unlock the flash program erase controller

	EEPROM_PageStatus PageStatus0 = *((__IO uint16_t*) EEPROM_PAGE0);									//get page0 status
	EEPROM_PageStatus PageStatus1 = *((__IO uint16_t*) EEPROM_PAGE1);									//get page1 status

	//build index
	if (PageStatus0 == EEPROM_VALID)																	//if page0 is valid
	{
		EEPROM_PageToIndex(EEPROM_PAGE0);																//index page0
		if (PageStatus1 == EEPROM_RECEIVING) EEPROM_PageToIndex(EEPROM_PAGE1);							//if page1 receives data overwrite index with page1
	}
	else if (PageStatus1 == EEPROM_VALID)																//if page1 is valid
	{
		EEPROM_PageToIndex(EEPROM_PAGE1);																//index page1
		if (PageStatus0 == EEPROM_RECEIVING) EEPROM_PageToIndex(EEPROM_PAGE0);							//if page0 receives data overwrite index with page0
	}

	//check for invalid header states and repair if necessary
	switch (PageStatus0)
	{
		case EEPROM_ERASED:
			switch (PageStatus1)
			{
				case EEPROM_VALID: break;																//do nothing (ST says EEPROM_PageErase(EEPROM_PAGE0);)
				case EEPROM_RECEIVING: result = EEPROM_SetValidPage(EEPROM_PAGE1); break;				//erase page0 & mark page1 as valid
				default: result = EEPROM_Format(); break;												//first EEPROM access or invalid state -> format EEPROM
			}
		break;
	
		case EEPROM_RECEIVING:
			switch (PageStatus1)
			{
				case EEPROM_VALID: result = EEPROM_PageTransfer(EEPROM_PAGE1, EEPROM_PAGE0); break;		//repair pages
				case EEPROM_ERASED: result = EEPROM_SetValidPage(EEPROM_PAGE0); break;					//erase page1 & mark page0 as valid
				default: result = EEPROM_Format(); break;												//invalid state -> format EEPROM
			}
		break;
	
		case EEPROM_VALID:
			switch (PageStatus1)
			{
				case EEPROM_RECEIVING: result = EEPROM_PageTransfer(EEPROM_PAGE0, EEPROM_PAGE1); break;	//repair pages
				case EEPROM_ERASED: break;																//do nothing (ST says EEPROM_PageErase(EEPROM_PAGE1);)
				default: result = EEPROM_Format(); break;												//invalid state -> format EEPROM
			}
		break;
	
		default:
			result = EEPROM_Format();																	//format EEPROM
		break;
	}

	return result;
}


//returns the last stored variable value which correspond to the passed variable name
//return value is EEPROM_SUCCESS, EEPROM_INVALID_NAME, EEPROM_NOT_ASSIGNED
EEPROM_Result EEPROM_ReadVariable(uint16_t VariableName, uint16_t* Value)
{
	if (VariableName >= EEPROM_VARIABLE_COUNT) return EEPROM_INVALID_NAME;								//if variable name is to high for variable count throw error

	uint32_t Address = EEPROM_START_ADDRESS + EEPROM_Index[VariableName];
	if (Address == EEPROM_PAGE0) return EEPROM_NOT_ASSIGNED;											//if variable was never assigned (index = 0) throw error
	if (Address == EEPROM_PAGE1) return EEPROM_INVALID_NAME;											//if index points to page header throw error

	*Value = *((__IO uint16_t*) Address);																//read value from physical address
	return EEPROM_SUCCESS;
}


//writes/updates variable in EEPROM
//return value is EEPROM_SUCCESS, EEPROM_NO_VALID_PAGE, EEPROM_PAGE_FULL, EEPROM_ERROR, EEPROM_BUSY or EEPROM_TIMEOUT
EEPROM_Result EEPROM_WriteVariable(uint16_t VariableName, uint16_t Value)
{
	EEPROM_Result result = EEPROM_UNKNOWN_ERROR;

	result = EEPROM_TryWriteVariable(VariableName, Value);												//write the variable name and value in the EEPROM (valid page)

	if (result == EEPROM_PAGE_FULL) 																	//if page is full perform page transfer
	{
		EEPROM_Page SourcePage = EEPROM_FindValidPage(EEPROM_READ); 									//get source page for read operation
		EEPROM_Page TargetPage = EEPROM_PAGE_NONE;														//get target page for write operation
		switch (SourcePage)
		{
			case EEPROM_PAGE1: TargetPage = EEPROM_PAGE0; break;
			case EEPROM_PAGE0: TargetPage = EEPROM_PAGE1; break;
			case EEPROM_PAGE_NONE: return EEPROM_NO_VALID_PAGE;											//if error return EEPROM_NO_VALID_PAGE
		}

		result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, TargetPage, EEPROM_RECEIVING);			//set the target page status to EEPROM_RECEIVING
		if (result != EEPROM_SUCCESS) return result;

		result = EEPROM_TryWriteVariable(VariableName, Value);											//write the new variable passed as parameter in the target page
		if (result != EEPROM_SUCCESS) return result;													//if write operation failed return the error code

		result = EEPROM_PageTransfer(SourcePage, TargetPage);											//transfer data from source page to target page
	}

	return result;																						//return last operation result
}


//erases Page0 and Page1 and writes EEPROM_VALID header to Page0
//return value is EEPROM_SUCCESS, EEPROM_ERROR, EEPROM_BUSY or EEPROM_TIMEOUT
EEPROM_Result EEPROM_Format()
{
	EEPROM_Result result = EEPROM_UNKNOWN_ERROR;

	result = EEPROM_PageErase(EEPROM_PAGE0);															//erase page0
	if (result != EEPROM_SUCCESS) return result;

	result = EEPROM_SetValidPage(EEPROM_PAGE0);															//set page0 as valid page (also erases page1)
	return result;
}


//writes variable in EEPROM if page not full
//return value is EEPROM_SUCCESS, EEPROM_PAGE_FULL, EEPROM_NO_VALID_PAGE, EEPROM_ERROR, EEPROM_BUSY or EEPROM_TIMEOUT
static EEPROM_Result EEPROM_TryWriteVariable(uint16_t VariableName, uint16_t Value)
{
	EEPROM_Result result = EEPROM_UNKNOWN_ERROR;

	EEPROM_Page ValidPage = EEPROM_FindValidPage(EEPROM_WRITE);											//get valid page for write operation
	if (ValidPage == EEPROM_PAGE_NONE) return EEPROM_NO_VALID_PAGE;										//if no valid page return error

	uint32_t Address = ValidPage + 4;																	//get the valid page start address (without page header)
	uint32_t PageEndAddress = ValidPage + FLASH_PAGE_SIZE - 2;											//get the valid page end address

	while (Address < PageEndAddress)																	//check each variable address of the page starting from beginning
	{
		if (*((__IO uint32_t*) Address) == 0xFFFFFFFF)													//if variable address is empty
		{
			result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, Address, Value);						//write variable value
			if (result != EEPROM_SUCCESS) return result;
			result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, Address + 2, VariableName);			//write variable name
			if (result != EEPROM_SUCCESS) return result;

			EEPROM_Index[VariableName] = Address - EEPROM_START_ADDRESS;								//update index
			return EEPROM_SUCCESS;																		//return EEPROM_SUCCESS
		}
		Address = Address + 4;																			//next address location
	}

	return EEPROM_PAGE_FULL;																			//return EEPROM_PAGE_FULL if loop runs out
}


//transfers latest variable values from one page to another.
//also called, when reset occurred during page transfer. continues the transfer of variables
//return value is EEPROM_SUCCESS, EEPROM_NO_VALID_PAGE, EEPROM_PAGE_FULL, EEPROM_ERROR, EEPROM_BUSY or EEPROM_TIMEOUT
static EEPROM_Result EEPROM_PageTransfer(EEPROM_Page SourcePage, EEPROM_Page TargetPage)
{
	EEPROM_Result result = EEPROM_UNKNOWN_ERROR;
	uint16_t Value = 0xFFFF;

	uint16_t StartAddress = SourcePage - EEPROM_START_ADDRESS;											//get the page start (field-)address
	uint16_t EndAddress = SourcePage - EEPROM_START_ADDRESS + FLASH_PAGE_SIZE;							//get the page end (field-)address

	//Transfer data
	for (uint16_t i = 0; i < EEPROM_VARIABLE_COUNT; i++)												//for each variable
	{
		if (StartAddress < EEPROM_Index[i] && EEPROM_Index[i] < EndAddress)								//if index on source page
		{
			if (EEPROM_ReadVariable(i, &Value) == EEPROM_SUCCESS)										//read latest variable value if possible
			{
				result = EEPROM_TryWriteVariable(i, Value);												//write variable to target page
				if (result != EEPROM_SUCCESS) return result;											//on error return with result code
			}
		}
	}

	result = EEPROM_SetValidPage(TargetPage);															//set target page status to EEPROM_VALID
	return result;																						//if loop runs through and this was successful return EEPROM_SUCCESS
}


//Find valid Page.
//argument "Operation" can be EEPROM_WRITE or EEPROM_READ
//return value is EEPROM_PAGE0, EEPROM_PAGE1 or EEPROM_PAGE_NONE
static EEPROM_Page EEPROM_FindValidPage(EEPROM_Operation Operation)
{
	EEPROM_Page ValidPage = EEPROM_PAGE_NONE;
	EEPROM_PageStatus PageStatus0 = *((__IO uint16_t*) EEPROM_PAGE0);									//get Page0 status
	EEPROM_PageStatus PageStatus1 = *((__IO uint16_t*) EEPROM_PAGE1);									//get Page1 status

	if (PageStatus0 == EEPROM_VALID) ValidPage = EEPROM_PAGE0;											//valid page is the page marked as valid
	else if (PageStatus1 == EEPROM_VALID) ValidPage = EEPROM_PAGE1;
	else return ValidPage;																				//if no page marked as valid return no page

	if(Operation == EEPROM_WRITE)																		//if write operation (e.g. during page transfer)
	{
		if (PageStatus0 == EEPROM_RECEIVING) ValidPage = EEPROM_PAGE0;									//valid page is the page that receives data
		else if (PageStatus1 == EEPROM_RECEIVING) ValidPage = EEPROM_PAGE1;
	}

	return ValidPage;																					//return valid page
}


//set page "Page" as valid and erase the other page
//if page is EEPROM_PAGE_NONE erase both and return
//return value is EEPROM_SUCCESS, EEPROM_ERROR, EEPROM_BUSY or EEPROM_TIMEOUT
static EEPROM_Result EEPROM_SetValidPage(EEPROM_Page Page)
{
	EEPROM_Result result = EEPROM_UNKNOWN_ERROR;

	switch (Page)																						//erase other page and set status to EEPROM_ERASED
	{
		case EEPROM_PAGE1:
			result = EEPROM_PageErase(EEPROM_PAGE0);
			if (result != EEPROM_SUCCESS) return result;
		break;

		case EEPROM_PAGE0:
			result = EEPROM_PageErase(EEPROM_PAGE1);
			if (result != EEPROM_SUCCESS) return result;
		break;

		case EEPROM_PAGE_NONE:																			//erase both pages and return
			result = EEPROM_PageErase(EEPROM_PAGE0);
			if (result != EEPROM_SUCCESS) return result;
			result = EEPROM_PageErase(EEPROM_PAGE1);
			return result;
		break;
	}

	result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, Page, EEPROM_VALID);							//mark page as valid
	return result;
}


//erases a page after removing references to it from index
static EEPROM_Result EEPROM_PageErase(EEPROM_Page Page)
{
	uint16_t StartAddress = Page - EEPROM_START_ADDRESS;												//get the page start (field-)address
	uint16_t EndAddress = Page - EEPROM_START_ADDRESS + FLASH_PAGE_SIZE;								//get the page end (field-)address

	for (uint16_t i = 0; i < EEPROM_VARIABLE_COUNT; i++)												//for each variable
	{
		if (StartAddress < EEPROM_Index[i] && EEPROM_Index[i] < EndAddress) EEPROM_Index[i] = 0;		//if variable is stored on erased page set index reference = 0
	}

	FLASH_EraseInitTypeDef EraseDefinitions; 															//setup erase definitions
	EraseDefinitions.TypeErase = FLASH_TYPEERASE_PAGES;													//page erase mode
	EraseDefinitions.Banks = FLASH_BANK_1;																//use bank 1 (bank 2 not always there)
	EraseDefinitions.PageAddress = Page;																//erase page start address
	EraseDefinitions.NbPages = 1;																		//number of pages to erase
	uint32_t PageError;																					//pointer to further information in case of error

	return HAL_FLASHEx_Erase(&EraseDefinitions, &PageError);											//erase page in flash
}


//builds the index for page "Page"
static void EEPROM_PageToIndex(EEPROM_Page Page)
{
	uint32_t Address = Page + 4;																		//get the valid page start address (physical)
	uint32_t PageEndAddress = Page + FLASH_PAGE_SIZE - 2;												//get the valid page end address (physical)
	uint16_t VariableName = 0xFFFF;
	uint16_t Value = 0xFFFF;

	while (Address < PageEndAddress)																	//check each variable address of the page starting from beginning
	{
		VariableName = *((__IO uint16_t*) (Address + 2));												//read variable name
		Value = *((__IO uint16_t*) Address);															//read variable value
		if (VariableName == 0xFFFF && Value == 0xFFFF) return;											//if variable name and value are empty, last variable of page was reached
		if (VariableName < EEPROM_VARIABLE_COUNT)														//if variable name is valid (and not 0xFFFF in case of reset while writing)
		{
			EEPROM_Index[VariableName] = (uint16_t) (Address - EEPROM_START_ADDRESS);					//update the variable address in the cache
		}

		Address = Address + 4;																			//next address location
	}
}

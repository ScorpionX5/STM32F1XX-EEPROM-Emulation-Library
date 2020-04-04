#include "__project__.h"

void main()
{
	//initialize EEPROM
	EEPROM_Result result;
	result = EEPROM_Init();

	//set default value if variabel not assigned
	EEPROM_Value value;
	for (uint16_t i = 0; i < EEPROM_VARIABLE_COUNT; i++)
	{
		if (EEPROM_ReadVariable(i, &value) == EEPROM_NOT_ASSIGNED)
		{
			switch (i)
			{
			case 0: result = EEPROM_WriteVariable(i, (EEPROM_Value) (uint16_t) 0x0000, EEPROM_SIZE16); break;
			case 1: result = EEPROM_WriteVariable(i, (EEPROM_Value) (uint32_t) 0xFFFFFFFF, EEPROM_SIZE32); break;
			case 2: result = EEPROM_WriteVariable(i, (EEPROM_Value) (double) 3.14159265358979, EEPROM_SIZE64); break;
			case 3: break;
			}
		}
	}

	//read and write variable values
	EEPROM_Value var0;
	EEPROM_Value var1;
	for (uint8_t i = 0; i < 5; i++)
	{
		result = EEPROM_ReadVariable(0, &var0);
		result = EEPROM_ReadVariable(1, &var1);

		var0.uInt16++;
		var1.uInt32--;

		result = EEPROM_WriteVariable(0, var0, EEPROM_SIZE16);
		result = EEPROM_WriteVariable(1, var1, EEPROM_SIZE32);
	}

	EEPROM_Value var2;
	result = EEPROM_ReadVariable(2, &var2);

	//delete variables
	EEPROM_Value var3;
	result = EEPROM_ReadVariable(3, &var3); //EEPROM_NOT_ASSIGNED
	result = EEPROM_WriteVariable(3, (EEPROM_Value) (float) 1.2345, EEPROM_SIZE32);
	result = EEPROM_ReadVariable(3, &var3); //EEPROM_SUCCESS
	result = EEPROM_DeleteVariable(3);
	result = EEPROM_ReadVariable(3, &var3); //EEPROM_NOT_ASSIGNED
}


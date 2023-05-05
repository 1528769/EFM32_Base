#include <stdio.h>
#include <stdbool.h>
#include "em_i2c.h"
#include "em_gpio.h"
#include "em_cmu.h"
#include "FreeRTOS.h"
#include "semphr.h"
static uint8_t device_addr;
SemaphoreHandle_t xSemaphore;

//adress is 0x77
void BSP_I2C_Init(uint8_t addr) {
	xSemaphore = xSemaphoreCreateBinary();
	I2C_Init_TypeDef i2cInit = I2C_INIT_DEFAULT;
	CMU_ClockEnable(cmuClock_I2C1, true);
	GPIO_PinModeSet(gpioPortC, 4, gpioModeWiredAnd, 1);
	GPIO_PinModeSet(gpioPortC, 5, gpioModeWiredAnd, 1);
	I2C1->ROUTE = I2C_ROUTE_SDAPEN |
	I2C_ROUTE_SCLPEN | I2C_ROUTE_LOCATION_LOC0;
	I2C_Init(I2C1, &i2cInit);

	device_addr = addr;

}

/**
 * @brief Write register using default I2C bus
 * @param reg register to write
 * @param data data to write
 * @return true on success
 */
bool I2C_WriteRegister(uint8_t reg, uint8_t data) {
	if(xSemaphore!=NULL)
	{
		if(xSemaphoreTake(xSemaphore,  ( TickType_t ) 0)== pdFALSE)
		{
			I2C_TransferReturn_TypeDef I2C_Status;
			bool ret_value = false;

			I2C_TransferSeq_TypeDef seq;
			uint8_t dataW[2];

			seq.addr = device_addr;
			seq.flags = I2C_FLAG_WRITE;

			/* Register to write: 0x67 ( INT_FLAT )*/
			dataW[0] = reg;
			dataW[1] = data;

			seq.buf[0].data = dataW;
			seq.buf[0].len = 2;
			I2C_Status = I2C_TransferInit(I2C1, &seq);

			while (I2C_Status == i2cTransferInProgress) {
				I2C_Status = I2C_Transfer(I2C1);
			}

			if (I2C_Status != i2cTransferDone) {
				ret_value = false;
			} else {
				ret_value = true;
			}
			return ret_value;
			xSemaphoreGive(xSemaphore);
		}
		else{};
	}
}

/**
 * @brief Read register from I2C device
 * @param reg Register to read
 * @param val Value read
 * @return true on success
 */
bool I2C_ReadRegister(uint8_t reg, uint8_t *val) {
	if(xSemaphore!=NULL)
		{
			if(xSemaphoreTake(xSemaphore,  ( TickType_t ) 0)== pdFALSE)
			{
				I2C_TransferReturn_TypeDef I2C_Status;
				I2C_TransferSeq_TypeDef seq;
				uint8_t data[2];

				seq.addr = device_addr;
				seq.flags = I2C_FLAG_WRITE_READ;

				seq.buf[0].data = &reg;
				seq.buf[0].len = 1;
				seq.buf[1].data = data;
				seq.buf[1].len = 1;

				I2C_Status = I2C_TransferInit(I2C1, &seq);

				while (I2C_Status == i2cTransferInProgress) {
					I2C_Status = I2C_Transfer(I2C1);
				}

				if (I2C_Status != i2cTransferDone) {
					return false;
				}

				*val = data[0];

				return true;
				xSemaphoreGive(xSemaphore);
			}
			else{};
		}


}

bool I2C_Test() {
	uint8_t data;

	I2C_ReadRegister(0xD0, &data);

	printf("I2C: %02X\n", data);

	if (data == 0x60) {
		return true;
	} else {
		return false;
	}

}

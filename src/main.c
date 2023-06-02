/***************************************************************************//**
 * @file
 * @brief FreeRTOS Blink Demo for Energy Micro EFM32GG_STK3700 Starter Kit
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "croutine.h"

#include "em_chip.h"
#include "bsp.h"
#include "bsp_trace.h"

#include "sleep.h"
//#include "bsp_print.c"
#include <time.h>

#define STACK_SIZE_FOR_TASK    (configMINIMAL_STACK_SIZE + 10)
#define TASK_PRIORITY          (tskIDLE_PRIORITY + 1)

void BSP_I2C_Init(uint8_t addr);
bool I2C_ReadRegister(uint8_t reg, uint8_t *val);
bool I2C_Test();

/* Structure with parameters for LedBlink */
typedef struct {
  /* Delay between blink of led */
  portTickType delay;
  /* Number of led */
  int          ledNo;
} TaskParams_t;


//versi� final

typedef struct {	//creem una estructura per obtenir els tres digits necessaris per la compensation formula per calcular la temperatura
	uint32_t temp;
	uint16_t dig_T1;
	uint16_t dig_T2;
	uint16_t dig_T3;
}Temperatura;

QueueHandle_t rData; //creem una variable que llegir� el valor de temperatura del sensor i ser� enviat per les cues
QueueHandle_t rTemp; //aquesta variable obt� el valor de temperatura de rData(fa el c�lcul).

/***************************************************************************//**
 * @brief Simple task which is blinking led
 * @param *pParameters pointer to parameters passed to the function
 ******************************************************************************/
static void LedBlink(void *pParameters)
{
  TaskParams_t     * pData = (TaskParams_t*) pParameters;
  const portTickType delay = pData->delay;

  for (;; ) {
    BSP_LedToggle(pData->ledNo);
    vTaskDelay(delay);
  }
}

static void vATaskRead( void *pParameters ) //generar dades del sensor
{
	//Assignem data i registre de la humitat
	uint8_t ctrl_hum = 0x1;
	uint8_t regH = 0xF2;
	I2C_WriteRegister(regH, ctrl_hum);

	//Assignem data i registre de la humitat
	uint8_t ctrl_meas = 0x47; //donar un valor incial a la temperatura
	uint8_t regT = 0xF4;
	I2C_WriteRegister(regT, ctrl_meas);
	while(1){
		if(true) //aqu� anir�a una condici� per comprovar que s'han escrit correctament els valors
		{
			//comprovar contingut registres
			uint8_t data;
			I2C_ReadRegister(regH, &data);

			I2C_ReadRegister(regT, &data);

			uint8_t temp_msb;
			uint8_t reg = 0xFA;

			I2C_ReadRegister(reg, &temp_msb);

			uint8_t temp_lsb;
			reg = 0xFB;

			I2C_ReadRegister(reg, &temp_lsb);


			uint8_t temp_xlsb;
			reg = 0xFC;

			I2C_ReadRegister(reg, &temp_xlsb);

			//concatenem la temperatura
			uint32_t temp=0;
			temp |= temp_msb << 12;
			temp |= temp_msb << 4;
			temp |= temp_msb << 0;

			uint16_t dig_T1;
			uint16_t dig_T2;
			uint16_t dig_T3;

			//Trimming parameter readout
			reg = 0x89;
			I2C_ReadRegister(reg, &data);
			dig_T1 |= data << 8;
			reg = 0x88;
			I2C_ReadRegister(reg, &data);
			dig_T1 |= data << 0;

			reg = 0x8B;
			I2C_ReadRegister(reg, &data);
			dig_T2 |= data << 8;
			reg = 0x8A;
			I2C_ReadRegister(reg, &data);
			dig_T2 |= data << 0;

			reg = 0x8D;
			I2C_ReadRegister(reg, &data);
			dig_T3 |= data << 8;
			reg = 0x8C;
			I2C_ReadRegister(reg, &data);
			dig_T3 |= data << 0;


			//CREEM MISSATGE i assignem els valor obtinguts a la estructura temperatura.
			Temperatura x;
			x.temp = temp;
			x.dig_T1 = dig_T1;
			x.dig_T2 = dig_T2;
			x.dig_T3 = dig_T3;

			xQueueSend(rData, &x, 0);
		}


	}



}
//Compensation formulasin double precision floating point
static void vATaskProcess( void *pvParameters ) //crear cua per procesar les dades
{
	Temperatura xTemp;
	double var1, var2, temp;
	double tempMin = -10;
	double tempMax = 70;

	// user initialization
    while(1)
    {
    	if(xQueueReceive(rData, &xTemp, 0))
    	{
    		// Returns temperature in DegC, double precision. Output value of �51.23� equals 51.23 DegC.
    		var1  = (((double)xTemp.temp)/16384.0 - ((double)xTemp.dig_T1)/1024.0) * ((double)xTemp.dig_T2);
    		var2  = ((((double)xTemp.temp)/131072.0) - ((double)xTemp.dig_T1)/8192.0) *(((double)xTemp.temp)/131072.0 -((double)xTemp.dig_T1)/8192.0)*((double)xTemp.dig_T3);
    		//t_fine = (BME280_S32_t)(var1 + var2);
    		temp  = (var1 + var2) / 5120.0;
    		if(temp < tempMin)   //comprovem si la temperatura es menor o major als limits de temperatura assignats
    		{
    			temp = tempMin;
    		}
    		if(temp > tempMax)
    		{
    		    temp = tempMax;
    		}
    		//int itemp = (int)temp;
    		int itemp = rand()%(35)+10; //generem un valor de temperatura aleatori amb la funci� rand, ja que el sensor no llegeix cap valor d'entrada
    		xQueueSend(rTemp, &itemp, 0);

    	}

    }
}

static void vATaskPrint( void *pvParameters ) //funci� per mostrar els valors de temperatura per la pantalla de la consola i modificar els leds
{
	int temp;
	while(1){
		if(xQueueReceive(rTemp, &temp, 0))
		{
			if(temp<25)
			{
				BSP_LedClear(1);
				BSP_LedSet(0);
			}
			else{
				BSP_LedClear(0);
				BSP_LedSet(1);
			}
			printf("Temperatura: %d C\n", temp);
		}
	}
        // user initialization


}
/*xQueue = xQueueCreate( 1, sizeof( uint16_t));
if(xQueue != NULL)
{
	xTaskCreate( vaTaskRead, ( signed char * ) "Rx", configMINIMAL_STACK_SIZE, NULL, 1, NULL );
	xTaskCreate( prvQueueSendTask, ( signed char * ) "TX", configMINIMAL_STACK_SIZE, NULL, mainQUEUE_SEND_TASK_PRIORITY, NULL );
	vTaskStartScheduler();
}*/




/***************************************************************************//**
 * @brief  Main function
 ******************************************************************************/

//void setupSWOForPrint(void);

void SWO_SetupForPrint(void); //funci� per poder visualitzar a la consola els printf
int main(void)
{

  /* Chip errata */
  CHIP_Init();
  BSP_LedsInit();
  SWO_SetupForPrint();
  BSP_I2C_Init(0x77 << 1);


  uint8_t data = 0;

  //I2C_WriteRegister(0xD0, data);
  //I2C_ReadRegister(0xD0, &data);
  I2C_Test();
  SLEEP_Init(NULL, NULL);
#if (configSLEEP_MODE < 3)
   do not let to sleep deeper than define
  SLEEP_SleepBlockBegin((SLEEP_EnergyMode_t)(configSLEEP_MODE + 1));
#endif

  rData = xQueueCreate(1, sizeof(Temperatura));
  rTemp = xQueueCreate(1, sizeof(double));
  xTaskCreate(vATaskRead, (signed char *) "vATaskRead", STACK_SIZE_FOR_TASK,(void *) 1, TASK_PRIORITY, NULL ) ;
  xTaskCreate(vATaskProcess, (signed char *) "vATaskProcess", STACK_SIZE_FOR_TASK,(void *) 1, TASK_PRIORITY, NULL ) ;
  xTaskCreate(vATaskPrint, (signed char *) "vATaskPrint", STACK_SIZE_FOR_TASK,(void *) 1, TASK_PRIORITY, NULL ) ;
  /*xQueue = xQueueCreate( 1, sizeof( uint16_t));
  if(xQueue != NULL)
  {
  	xTaskCreate( vaTaskRead, ( signed char * ) "Rx", configMINIMAL_STACK_SIZE, NULL, 1, NULL );
  	xTaskCreate( prvQueueSendTask, ( signed char * ) "TX", configMINIMAL_STACK_SIZE, NULL, mainQUEUE_SEND_TASK_PRIORITY, NULL );
  	vTaskStartScheduler();
  }*/
  /* If first word of user data page is non-zero, enable Energy Profiler trace */
  /*BSP_TraceProfilerSetup();

   Initialize LED driver
  BSP_LedsInit();
   Setting state of leds
  BSP_LedSet(0);
  BSP_LedSet(1);

   Initialize SLEEP driver, no calbacks are used
  SLEEP_Init(NULL, NULL);
#if (configSLEEP_MODE < 3)
   do not let to sleep deeper than define
  SLEEP_SleepBlockBegin((SLEEP_EnergyMode_t)(configSLEEP_MODE + 1));
#endif

   Parameters value for taks
  static TaskParams_t parametersToTask1 = { pdMS_TO_TICKS(1000), 0 };
  static TaskParams_t parametersToTask2 = { pdMS_TO_TICKS(500), 1 };

  Create two task for blinking leds
  xTaskCreate(LedBlink, (const char *) "LedBlink1", STACK_SIZE_FOR_TASK, &parametersToTask1, TASK_PRIORITY, NULL);
  xTaskCreate(LedBlink, (const char *) "LedBlink2", STACK_SIZE_FOR_TASK, &parametersToTask2, TASK_PRIORITY, NULL);

  Start FreeRTOS Scheduler
  vTaskStartScheduler();*/
  vTaskStartScheduler();
  return 0;

}

//=======================================================================================================
//
//     ██╗██████╗     ███████╗██╗     ███████╗ ██████╗████████╗██████╗  ██████╗ ███╗   ██╗██╗ ██████╗
//     ██║██╔══██╗    ██╔════╝██║     ██╔════╝██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗████╗  ██║██║██╔════╝
//     ██║██████╔╝    █████╗  ██║     █████╗  ██║        ██║   ██████╔╝██║   ██║██╔██╗ ██║██║██║     
//██   ██║██╔══██╗    ██╔══╝  ██║     ██╔══╝  ██║        ██║   ██╔══██╗██║   ██║██║╚██╗██║██║██║     
//╚█████╔╝██║  ██║    ███████╗███████╗███████╗╚██████╗   ██║   ██║  ██║╚██████╔╝██║ ╚████║██║╚██████╗
// ╚════╝ ╚═╝  ╚═╝    ╚══════╝╚══════╝╚══════╝ ╚═════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═══╝╚═╝ ╚═════╝
//                                                                                                 
//=======================================================================================================

//=======================================================================================================
//
//   Title: Display LCD 16x2 control, mode 4 bits.
//   Author: Joao Ricardo Chaves.
//   Date: August,2024.
//  
//   Compiler: Espressif sdk - Vscode.
//   Module: ESP32 - DevKit v1.
//=======================================================================================================

#ifndef LCD_JR_h
#define LCD_JR_h

//=======================================================================================================
//--- Libraries ---
#include <inttypes.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_mac.h"
#include "lora.h"
#include "esp_log.h"

//=======================================================================================================
//--- Hardware Mapping ---

#define I2C0_SDA GPIO_NUM_4
#define I2C0_SCL GPIO_NUM_15

//=======================================================================================================
//--- Macros and Constants ---

#define __Delay(t)   vTaskDelay(t/portTICK_PERIOD_MS)
#define __DelayUs(t) esp_rom_delay_us(t)

#define LCD_BACKLIGHT   0x08
#define LCD_NOBACKLIGHT 0x00
#define LCD_CLEAR       0x01
#define LCD_HOME        0x02
#define LCD_1POS        0x80
#define LCD_2POS        0xC0
#define LCD_NOCURSOR    0x0C

//=======================================================================================================
//--- Functions Prototypes ---

void disp_Init(void);                                 // Inicializa o LCD
void disp_Clear(void);                                // Limpa o LCD
void disp_Putc(unsigned char chr);                    // Escreve um caracter no LCD
void disp_Puts(char *buffer);                         // Escreve uma string no LCD
void disp_WriteCmd(unsigned char cmd);                // Envia um comando para o LCD
void send_number(long int num);                       // Exibe um num inteiro de ate 5 digitos
void disp_Putrs(const char *buffer);                  // Escreve uma string no LCD

#endif
//=======================================================================================================
//--- End of Program ---
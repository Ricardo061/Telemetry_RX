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
//=======================================================================================================

//=======================================================================================================
//--- Bibliotecas ---
#include "lcd_jr.h"

//=======================================================================================================
//--- Const and Macro ---
#define LCD_ADDR 0x27
static const char *TAG1 = "I2C";

//=======================================================================================================
//--- Functions prototypes ---
void send_nibble(uint8_t nib, uint8_t rsel);          // Envia cada nibble separadamente
void send_PulseEnable(uint8_t data);                  // Envia pulso de enable para o display
esp_err_t I2C0_Init(void);                                 // Inicializa o modo I2C

//=======================================================================================================
//--- Functions ---

//=======================================================================================================
//--- I2C0_Init ---
esp_err_t I2C0_Init(void)
{
  i2c_config_t i2cConfig = 
  {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = I2C0_SDA,
    .scl_io_num = I2C0_SCL,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 60000,
  };
  i2c_param_config(I2C_NUM_0,&i2cConfig);
  return i2c_driver_install(I2C_NUM_0,I2C_MODE_MASTER,0,0,0); 
  
}//end I2C0_Init

//=======================================================================================================
//--- disp_Init ---
void disp_Init()
{
  ESP_ERROR_CHECK(I2C0_Init());   // Inicializa o modo I2C
  __Delay(100);                   // Delay de 50ms para garantir a inicialização do LCD

  send_nibble(0x30, 0);           // Envia os 4 primeiros nibbles 0011 0000
  __Delay(10);                    // Delay de 5 milisegundos
  send_nibble(0x30, 0);           // Envia novamente os 4 primeiros nibbles
  __DelayUs(200);                 // Delay de 150us
  send_nibble(0x30, 0);           // Envia novamente os 4 primeiros nibbles
  __Delay(10);
  send_nibble(0x20, 0);           // Envia o nibble 0010 para o modo 4-bits
  __DelayUs(80);

  disp_WriteCmd(0x28);            // 5x8 pontos por caractere, duas linhas
  __DelayUs(80);
  disp_WriteCmd(LCD_BACKLIGHT);  
  __Delay(2);          
  disp_WriteCmd(0x01);
  __Delay(2);
  disp_WriteCmd(0x06);
  __Delay(1);
  disp_WriteCmd(0x0C);
  __Delay(1);
}//end disp_Init
   
//=======================================================================================================
//--- disp_Clear ---
void disp_Clear()
{
  disp_WriteCmd(0x02);          // Retorna o cursor
  __Delay(2);
  disp_WriteCmd(0x01);          // Limpa o display
  __Delay(2);
}//end disp_Clear 

//=======================================================================================================
//--- disp_Putc ---
void disp_Putc(unsigned char chr)
{
  send_nibble(chr & 0xF0,1);           // Envia o char completo, enviando primeiro os 4 bits mais significativos
  send_nibble((chr << 4) & 0xF0,1);    // Como e um envio de dados o Register select eh 1, caso fosse um comando
}// end disp_Putc                        

//=======================================================================================================
//--- disp_Puts ---
void disp_Puts(char *buffer)
{
  while(*buffer)
  {
    disp_Putc(*buffer);             // Envia cada caractere para o lcd
    buffer++;
  }//end while
  return;
}// end disp_Puts

//=======================================================================================================
//--- disp_WriteCmd ---
void disp_WriteCmd(unsigned char cmd)
{
  send_nibble(cmd & 0xF0,0);        // envia os 4 bits mais significativos limpando os menos 
  send_nibble((cmd<<4) & 0xF0,0);   // envia os 4 bits menos significativos realizando um deslocamento
                                    // e limpando os bits menos.
}//end disp_WriteCmd 

//=======================================================================================================
//--- send_nibble ---
void send_nibble(uint8_t nib, uint8_t rsel)
{
    uint8_t data = (nib & 0xF0) | LCD_BACKLIGHT | rsel;

    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    ESP_ERROR_CHECK(i2c_master_start(cmd_handle));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd_handle, (LCD_ADDR << 1) | I2C_MASTER_WRITE, true));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd_handle, data, true));
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_NUM_0, cmd_handle, 1000 / portTICK_PERIOD_MS));
    i2c_cmd_link_delete(cmd_handle);

    send_PulseEnable(data);  // Envio do pulso de enable para o display
}//end send_nibble
  
//=======================================================================================================
//--- send_PulseEnable ---
void send_PulseEnable(uint8_t data)
{
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    ESP_ERROR_CHECK(i2c_master_start(cmd_handle));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd_handle, (LCD_ADDR << 1) | I2C_MASTER_WRITE, true));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd_handle, data | 0x04, true));  // EN = 1
    ESP_ERROR_CHECK(i2c_master_stop(cmd_handle));
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_NUM_0, cmd_handle, 1000 / portTICK_PERIOD_MS));
    i2c_cmd_link_delete(cmd_handle);
    __DelayUs(1);  // EN HIGH

    cmd_handle = i2c_cmd_link_create();
    ESP_ERROR_CHECK(i2c_master_start(cmd_handle));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd_handle, (LCD_ADDR << 1) | I2C_MASTER_WRITE, true));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd_handle, data & ~0x04, true));  // EN = 0
    ESP_ERROR_CHECK(i2c_master_stop(cmd_handle));
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_NUM_0, cmd_handle, 1000 / portTICK_PERIOD_MS));
    i2c_cmd_link_delete(cmd_handle);
    __DelayUs(500);  // EN LOW
}// end send_PulseEnable

//=======================================================================================================
//--- Send_number ---
void send_number(long int num)
{
  char dem,mil,cen,dez,uni;
  short no_zero = 0;

  dem = (char) (num/10000);
  mil = (char) ((num%10000)/1000);
  cen = (char) ((num%1000)/100);
  dez = (char) ((num%100)/10);
  uni = (char) (num%10);

  if(!dem && !no_zero)
    disp_Putc(' ');
  else
  {
    disp_Putc(0x30+dem);
    no_zero = 1;
  }// end else

  if(!mil && !no_zero)
    disp_Putc(' ');
  else
  {
    disp_Putc(0x30+mil);
    no_zero = 1;
  }// end else

  if(!cen && !no_zero)
    disp_Putc(' ');
  else
  {
    disp_Putc(0x30+cen);
    no_zero = 1;
  }// end else

  if(!dez && !no_zero)
    disp_Putc(' ');
  else
  {
    disp_Putc(0x30+dez);
    no_zero = 1;
  }// end else

  disp_Putc(0x30+uni);

}//end send_number   

//=======================================================================================================
//--- disp_Putrs ---
void disp_Putrs(const char *buffer)
{
  while(*buffer)
  {
    disp_Putc(*buffer);
    buffer++;
  }//end while
  return;
}// end dip_Putrs

//=======================================================================================================
//--- End of Program ---
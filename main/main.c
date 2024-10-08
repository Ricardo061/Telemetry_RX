/*==================================================================================================================================================================

Telemetry System - v.01 main.c - ESP IDE

Author: Joao Ricardo Chaves and Marcelo Haziel
Date: 2024, March.

4x push buttons
1x LCD 16x2 - communication i2c

====================================================================================================================================================================*/

//==================================================================================================================================================================
//--- Bibliotecas ---
#include <stdio.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "lcd_jr.h"
#include <string.h>
#include "mqtt/mqtt.h"
#include "wifi/wifi.h"

//==================================================================================================================================================================
//--- Variaveis Globais ---
volatile bool EnterPressed = false;
volatile bool ExitPressed  = false;
volatile bool UpPressed    = false;
volatile bool DownPressed  = false;
volatile int cont= 0;

const char *Menu[]          = {"LoRa","Temperatura","MPU6050","Altitude","Velocidade","Pressao"};
const char *MenuMPU6050[]   = {"Roll", "Pitch"};
const char *MPUValues[2];

#define tamMenu 6

//==================================================================================================================================================================
//--- Handles para gerenciamento ---
QueueHandle_t Queueintr;	// Cria a fila como variavel global
SemaphoreHandle_t MutexMenu;
SemaphoreHandle_t MutexLora;
xSemaphoreHandle wifiConnection;
xSemaphoreHandle mqttConnection;

//==================================================================================================================================================================
//--- Variaveis Controle Push Button ---
#define ButtonEnter 23
#define ButtonUP    13
#define ButtonDown  17
#define ButtonExit  2

//==================================================================================================================================================================
//--- Variaveis LoRa ---
#define BUFFER 2024
#define FREQUENCY 915e6
#define BW 250e3
static const char *TAG2 = "LoRa";

//==================================================================================================================================================================
static const char *TAG4 = "MQTT";

//==================================================================================================================================================================
//--- Structs ---
typedef struct{
    uint32_t pressure_bmp;
    float temp;
    float anglePitchDeg;
    float angleRollDeg;
    float lat;
    char lat_dir[1];
    float lon;
    char lon_dir[1];
    float altitude;
    float speed;
    char buf[BUFFER];
    uint8_t packetLoRa[255];
    uint8_t SNR;
    int RSSI;
}variable;

variable vars;

//==================================================================================================================================================================
//--- Tasks prototipos ---
void MenuDisp(void *p);				 // Configura o menu do LCD mostrando suas opções
void ReadButton(void *p);			 // Realiza a leitura dos botões
void ReceiveLoraData(void *p); // Recebe parametros LoRa.
void wifi_treat(void);
void mqtt_treat(void *p);
//==================================================================================================================================================================
//--- Functions prototipos ---
esp_err_t setupLoRa(void);

//==================================================================================================================================================================
//--- interrupcoes prototipos ---
static void DataButton(void *args);  // interrupção para verificar qual botão foi acionado

//==================================================================================================================================================================
//--- Main Function ---
void app_main(void)
{
	// Config dos GPIO e seus modos
	gpio_set_direction(ButtonEnter,GPIO_MODE_INPUT);					// Define ButtonEnter como Entrada
	gpio_set_direction(ButtonExit,GPIO_MODE_INPUT);						// Define ButtonExit  como Entrada
	gpio_set_direction(ButtonUP,GPIO_MODE_INPUT);						  // Define ButtonUP    como Entrada
	gpio_set_direction(ButtonDown,GPIO_MODE_INPUT);						// Define ButtonDown  como Entrada

	gpio_set_pull_mode(ButtonEnter,GPIO_PULLUP_ONLY);					// Habilita o Resistor de Pull-UP do ButtonEnter
	gpio_set_pull_mode(ButtonExit,GPIO_PULLUP_ONLY);					// Habilita o Resistor de Pull-UP do ButtonExit
	gpio_set_pull_mode(ButtonUP,GPIO_PULLUP_ONLY);						// Habilita o Resistor de Pull-UP do ButtonUp
	gpio_set_pull_mode(ButtonDown,GPIO_PULLUP_ONLY);					// Habilita o Resistor de Pull-UP do ButtonDown


	gpio_set_intr_type(ButtonEnter,GPIO_INTR_NEGEDGE);				// Conigura o tipo de interrupção p/ FALLING, ou seja
	gpio_set_intr_type(ButtonExit,GPIO_INTR_NEGEDGE);					// Detecta a mudança para de HIGH P/LOW.
	gpio_set_intr_type(ButtonUP,GPIO_INTR_NEGEDGE);
	gpio_set_intr_type(ButtonDown,GPIO_INTR_NEGEDGE);

  ESP_ERROR_CHECK(setupLoRa());                             // Inicializa LoRa.

  // Initialize NETIF
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == 
                                                ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK( ret );

  wifiConnection = xSemaphoreCreateBinary();
  mqttConnection = xSemaphoreCreateBinary();

	Queueintr = xQueueCreate(10,sizeof(int));							    // Cria a fila e passa seu tamanho junto com o tipo de dados
  MutexMenu = xSemaphoreCreateMutex();
  MutexLora = xSemaphoreCreateMutex();

  wifi_connect();

	xTaskCreatePinnedToCore(ReadButton,"ReadButton",configMINIMAL_STACK_SIZE + 1000,NULL,3,NULL,0);		            // Cria uma task para Ler o botão com prioridade alta
	xTaskCreatePinnedToCore(MenuDisp,"menuDisp",configMINIMAL_STACK_SIZE + 2000,(void*)&vars,3,NULL,0);				    // Cria uma task para Manipular o menu e mostrar as informacoes no LCD
  xTaskCreatePinnedToCore(ReceiveLoraData,"ReceiveLoraData",
                                                  configMINIMAL_STACK_SIZE+2500,
                                                         (void*)&vars,4,NULL,1);
  xTaskCreatePinnedToCore((void *)wifi_treat, "Tratamento Wifi", 
                                                configMINIMAL_STACK_SIZE + 2000, 
                                                              NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(mqtt_treat, "Tratamento Mqtt", 
                                                configMINIMAL_STACK_SIZE + 2000, 
                                                      (void*)&vars, 2, NULL, 0);

	gpio_install_isr_service(0);										          // Config. das interrupcoes p/ adicionar pinos individualmente.
	gpio_isr_handler_add(ButtonEnter, DataButton,(void *)ButtonEnter);	
	gpio_isr_handler_add(ButtonExit, DataButton,(void *)ButtonExit);
	gpio_isr_handler_add(ButtonUP, DataButton,(void *)ButtonUP);
	gpio_isr_handler_add(ButtonDown, DataButton,(void *)ButtonDown);
  disp_Init();
  disp_Clear();
  __Delay(200);
  disp_WriteCmd(LCD_1POS);  // Move o cursor para a primeira posição
  disp_Puts("Telemetry System");
  disp_WriteCmd(LCD_2POS);
  disp_Puts("Abutres - v.01");
  __Delay(2000);
  disp_Clear();
  disp_WriteCmd(LCD_1POS);
  disp_Putrs(">");
  disp_Putrs(Menu[cont]);
  disp_WriteCmd(LCD_2POS);
  disp_Putrs(" ");
  disp_Putrs(Menu[( cont+1 )< 6 ? (cont+1) : 0 ]);

  while(true)
  {
    vTaskDelay(100/portTICK_PERIOD_MS);
  }//end while
}//end main function

//==================================================================================================================================================================
//--- Desenvolvimento de Funcoes/tasks/Interrupcoes ---

//==================================================================================================================================================================
//--- DataButton ---
static void IRAM_ATTR DataButton(void *args)
{
	int pin = (int)args;
	xQueueSendFromISR(Queueintr,&pin,NULL);
}//end dataButton

//==================================================================================================================================================================
//--- readButton ---
void ReadButton(void *p)
{
	int pin;
	while(true)
	{
		if(xQueueReceive(Queueintr,&pin, 1000/portTICK_PERIOD_MS)) 	// recebe a fila e o pino que está sendo lido.
		{
			if(gpio_get_level(pin) == 0)
			{
        __Delay(50);
        if(gpio_get_level(pin)==0)
				{
          switch(pin)
          {
            case ButtonEnter:
              EnterPressed = true;				// Habilita a flag do botão foi pressionado
              break;
            case ButtonExit:
              ExitPressed = true;
              break;
            case ButtonUP:
              UpPressed = true;
              break;
            case ButtonDown:
              DownPressed = true;
              break;
          }//end switch
        }//end if aninhado
			}//end if aninhado
		}//end if
    __Delay(15);
	}//end while

}//end readButton

//==================================================================================================================================================================
//--- MenuDisp ---
void MenuDisp(void *p)
{
  variable *PacketMenu=(variable*)p;
	while(true)
	{
    if(xSemaphoreTake(MutexMenu,portMAX_DELAY))
		{
      __Delay(250);
      disp_Clear();
      disp_WriteCmd(LCD_1POS);
      disp_Putrs(">");
      disp_Putrs(Menu[cont]);
      disp_WriteCmd(LCD_2POS);
      disp_Putrs(" ");
      disp_Putrs(Menu[( cont+1 )< 6 ? (cont+1) : 0 ]);
      while(!EnterPressed && !ExitPressed && !UpPressed && !DownPressed)
		  {
			  vTaskDelay(350/portTICK_PERIOD_MS);
		  }//end while aninhado
      if(UpPressed)
      {
        cont = (cont-1) >= 0 ? (cont-1) : (tamMenu - 1);
        //printf("Botao up pressionado\n");
        UpPressed = false;
        __Delay(10);
      }//end else if
      else if(DownPressed)
      {
        cont = (cont+1) < tamMenu ? (cont+1) :0;
       // printf("Botao down pressionado\n");
        DownPressed = false;
        __Delay(10);
      }//end else if
      else if(ExitPressed)
      {
        //printf("Botao Exit pressionado\n");
        ExitPressed = false;
        if(cont >= 0 && cont <= 5)
        {
          __Delay(2);
          disp_Clear();
          disp_WriteCmd(LCD_1POS);
          disp_Putrs(">");
          disp_Putrs(Menu[cont]);
          disp_WriteCmd(LCD_2POS);
          disp_Putrs(" ");
          disp_Putrs(Menu[( cont+1 )< 6 ? (cont+1) : 0 ]);
        }//end if
        __Delay(10);
      }//end else if
      else if(EnterPressed)
      {
        __Delay(20);
        //printf("Botao Enter pressionado\n");
        switch(cont)
        {
          case 0:
            while(!ExitPressed)
            {
              char SnrStr[5];
              char RssiStr[5];
              sprintf(SnrStr,"%d",PacketMenu->SNR);
              sprintf(RssiStr,"%d",PacketMenu->RSSI);
              __Delay(2);
              disp_Clear();
              disp_WriteCmd(LCD_1POS);
              disp_Puts("SNR:");
              disp_Puts(SnrStr);
              disp_WriteCmd(LCD_2POS);
              disp_Puts("RSSI:");
              disp_Puts(RssiStr);
              __Delay(250);
            }//end while
            break;
          case 1:
            while(!ExitPressed)
            {
              __Delay(2);
              disp_Clear();
              disp_WriteCmd(LCD_1POS);
              disp_Puts("Temperatura:");
              disp_WriteCmd(LCD_2POS);
              char TempStr[10];
              sprintf(TempStr,"%.2f",PacketMenu->temp);
              disp_Puts(TempStr);
              __Delay(250);
            }//end While
            break;
          case 2:
            int contMenuMP = 0;
            while(!ExitPressed)
            {
              while(!EnterPressed && !ExitPressed && !UpPressed && !DownPressed)
              {
                vTaskDelay(350/portTICK_PERIOD_MS);
              }//end while aninhado

              char RollStr[10];
              char PitchStr[10];
              sprintf(RollStr,"%.2f",PacketMenu->angleRollDeg);
              sprintf(PitchStr,"%.2f",PacketMenu->anglePitchDeg);
              MPUValues[0] = RollStr;
              MPUValues[1] = PitchStr;
              disp_Clear();
              disp_WriteCmd(LCD_1POS);
              disp_Putrs(">");
              disp_Putrs(MenuMPU6050[contMenuMP]);
              disp_Puts(":");
              disp_Putrs(MPUValues[contMenuMP]);
              disp_WriteCmd(LCD_2POS);
              disp_Putrs(" ");
              disp_Putrs(MenuMPU6050[(contMenuMP+1) < 2 ? (contMenuMP+1) : 0]);
              disp_Puts(":");
              disp_Putrs(MPUValues[(contMenuMP+1) < 2 ? (contMenuMP+1) : 0]);

              if(DownPressed)
              {
                contMenuMP = (contMenuMP+1)<2 ? (contMenuMP+1) : 0;
                DownPressed = false;
              }//end downPressed
              else if(UpPressed)
              {
                contMenuMP = (contMenuMP - 1) >= 0 ? (contMenuMP - 1) : 1;
                UpPressed = false;
              }//end UpPressed
              __Delay(200);
            }//end while
            break;
          case 3:
            while(!ExitPressed)
            {
              char AltiStr[10];
              sprintf(AltiStr,"%.2f",PacketMenu->altitude);
              __Delay(2);
              disp_Clear();
              disp_WriteCmd(LCD_1POS);
              disp_Puts("Altitude:");
              disp_WriteCmd(LCD_2POS);
              disp_Puts(AltiStr);
              __Delay(250);
            }//end While
            break;
          case 4:
            while(!ExitPressed)
            {
              char StrVel[10];
              sprintf(StrVel,"%.3f",PacketMenu->speed);
              __Delay(2);
              disp_Clear();
              disp_WriteCmd(LCD_1POS);
              disp_Puts("Velocidade:");
              disp_WriteCmd(LCD_2POS);
              disp_Puts(StrVel);
              disp_Putrs(" Km/h");
              __Delay(250);
            }//end While
            break;
          case 5:
            while(!ExitPressed)
            {
              char StrPressure[15];
              sprintf(StrPressure,"%lu",PacketMenu->pressure_bmp);
              __Delay(2);
              disp_Clear();
              disp_WriteCmd(LCD_1POS);
              disp_Puts("Pressao:");
              disp_WriteCmd(LCD_2POS);
              disp_Puts(StrPressure);
              __Delay(250);
              //printf("case 5 pressionado\n");
            }//end While
            break;
          default:
            __Delay(2);
            disp_Clear();
            disp_WriteCmd(LCD_1POS);
            disp_Puts("Comando invalido");
            vTaskDelay(250/portTICK_PERIOD_MS);
            break;
        }//end switch
        __Delay(250);
        EnterPressed = false;
      }//end else if
      xSemaphoreGive(MutexMenu);
      __Delay(10);
    }//end if
	}//end while
}//end MenuDisp

//==================================================================================================================================================================
//--- setupLoRa ---
esp_err_t setupLoRa(void)
{   
    if(lora_init() == 0){
        ESP_LOGE(TAG2, "Error!");
        return ESP_FAIL;
    }//end if

    lora_set_frequency(FREQUENCY);
    lora_set_bandwidth(BW);
    lora_set_spreading_factor(10);
    lora_set_tx_power(20); //lora__init() seta tx power em 17 dbm 
    lora_enable_crc(); // CRC (verificação de redundancia ciclica) método de detecção de erros, que verifica a integridade dos dados transmitidos com os dados recebidos
    lora_set_coding_rate(5);
    lora_set_sync_word(0x12);
    lora_set_preamble_length(6);

    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGW(TAG2, "LoRa OK!");

    return ESP_OK;
}//end SetupLoRa

//==================================================================================================================================================================
//--- ReceiveLoraData ---
void ReceiveLoraData(void *p)
{
  variable *vPacket=(variable*)p;
  while(true)
  {
    lora_receive();
    while(lora_received())
    {
      vPacket->SNR = lora_packet_snr();
      vPacket->RSSI = lora_packet_rssi();
      strcpy((char *)vPacket->packetLoRa, "");
      lora_receive_packet(vPacket->packetLoRa,sizeof(vPacket->packetLoRa));
      //printf("%s\n",(char *)vPacket->packetLoRa);
      char *index1  = strchr((char *)vPacket->packetLoRa,'!');
      //if(index1) printf("Index1 encontrado\n");
      char *index2  = strchr((char *)vPacket->packetLoRa,'@');
      //if(index2) printf("Index2 encontrado\n");
      char *index3  = strchr((char *)vPacket->packetLoRa,'#');
      //if(index3) printf("Index3 encontrado\n");
      char *index4  = strchr((char *)vPacket->packetLoRa,'C');
      //if(index4) printf("Index4 encontrado\n");
      char *index5  = strchr((char *)vPacket->packetLoRa,'A');
      //if(index5) printf("Index5 encontrado\n");
      char *index6  = strchr((char *)vPacket->packetLoRa,'&');
      //if(index6) printf("Index6 encontrado\n");
      char *index7  = strchr((char *)vPacket->packetLoRa,'*');
      //if(index7) printf("Index7 encontrado\n");
      char *index8  = strchr((char *)vPacket->packetLoRa,'(');
      //if(index8) printf("Index8 encontrado\n");
      char *index9  = strchr((char *)vPacket->packetLoRa,')');
      //if(index9) printf("Index9 encontrado\n");
      char *index10 = strchr((char *)vPacket->packetLoRa,'B');
      //if(index10) printf("Index10 encontrado\n");
        //if(index11) printf("Index11 encontrado\n");
      
      if (index1!=NULL && index2!=NULL && index3!=NULL && index4!=NULL && index5!=NULL && index6!=NULL
           && index7!=NULL && index8!=NULL && index9!=NULL && index10!=NULL) 
      {
        //printf("Os index foram encontrados!\n");
        char tempBuffer[50];

        // Ângulo Pitch
        strncpy(tempBuffer, (char *)vPacket->packetLoRa, index1 - (char *)vPacket->packetLoRa);
        tempBuffer[index1 - (char *)vPacket->packetLoRa] = '\0';
        vPacket->anglePitchDeg = atof(tempBuffer);  // Converte para float
        
        // Ângulo Roll
        strncpy(tempBuffer, index1 + 1, index2 - index1 - 1);
        tempBuffer[index2 - index1 - 1] = '\0';
        vPacket->angleRollDeg = atof(tempBuffer);  // Converte para float

        // Temperatura
        strncpy(tempBuffer, index2 + 1, index3 - index2 - 1);
        tempBuffer[index3 - index2 - 1] = '\0';
        vPacket->temp = atof(tempBuffer);  // Converte para float

        strncpy(tempBuffer, index3 + 1, index4 - index3 - 1);
        tempBuffer[index4 - index3 - 1] = '\0';
        vPacket->pressure_bmp = strtoul(tempBuffer, NULL, 10);

        strncpy(tempBuffer, index4 + 1, index5 - index4 - 1);
        tempBuffer[index5 - index4 - 1] = '\0';
        vPacket->lat = atof(tempBuffer);

        strncpy(vPacket->lat_dir, index5 + 1, index6 - index5 - 1);
        vPacket->lat_dir[index6 - index5 - 1] = '\0';

        strncpy(tempBuffer, index6 + 1, index7 - index6 - 1);
        tempBuffer[index7 - index6 - 1] = '\0';
        vPacket->lon = atof(tempBuffer);

        strncpy(vPacket->lon_dir, index7 + 1, index8 - index7 - 1);
        vPacket->lon_dir[index8 - index7 - 1] = '\0';

        // Altitude
        strncpy(tempBuffer, index8 + 1, index9 - index8 - 1);
        tempBuffer[index9 - index8 - 1] = '\0';
        vPacket->altitude = atof(tempBuffer);  // Converte para float

        // Velocidade
        strncpy(tempBuffer, index9 + 1, index10 - index9 - 1);
        tempBuffer[index10 - index9 - 1] = '\0';
        vPacket->speed = atof(tempBuffer);  // Converte para float
        
      }//end if 
      lora_receive();
      __Delay(2000);
    }//end while aninhado
  }//end while
}//end ReceiveLoraData

//==================================================================================================================================================================
//--- Treat_Wifi_Mqtt ---
void wifi_treat(void)
{
    while(true)
    {
        if(xSemaphoreTake(wifiConnection, portMAX_DELAY))
        {
          mqtt_start();
        }
        __Delay(50);
    }
}

void mqtt_treat(void *pvParameters)
{
    variable *variables = (variable*)pvParameters;
    char msg[200];
    if(xSemaphoreTake(mqttConnection, portMAX_DELAY))
    {
        while(true)
        {
            sprintf(msg, "{\n  \"data\":\n  {\n    \"Temperatura\": %f,\n    \"gpstrack\": \"%.6f,%.6f\",\n    \"Pitch\": %.1f,\n    \"Roll\": %.1f,\n    \"Altitude\": %.2f,\n    \"Speed\": %.3f,\n    \"SNR\": %d,\n    \"RSSI\": %d\n    }\n}", variables->temp, variables->lat, variables->lon, variables->anglePitchDeg, variables->angleRollDeg, variables->altitude, variables->speed, variables->SNR, variables->RSSI);
            mqtt_publish_msg("wnology/67005b8d6357fd1387ea3dc2/state", msg);
            //printf(msg);

            UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
            ESP_LOGI(TAG4,"Espaço mínimo livre na stack: %u\n",uxHighWaterMark);

            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}//end Treat_Wifi_Mqtt

//==================================================================================================================================================================
//--- End of Program --

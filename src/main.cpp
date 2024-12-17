#include <Arduino.h>
/* CAN Libraries */
#include <CAN.h>
/* Libraries of SD and Conectivity state Machine */
#include <SD_state_machine.h>
#include <CON_state_machine.h>
#include <BLE_state_machine.h>

// String ok = "OK";
// String erro = "ERRO";

TaskHandle_t SDlogging = NULL, ConectivityState = NULL, BLE_RESQUEST_State = NULL;
bool _sd = false; // flag to check if SD module compile
uint8_t _sot = DISCONNECTED;

bluetooth bluetooth_packet;

/* States Machines */
void SdStateMachine(void *pvParameters);
void ConnStateMachine(void *pvParameters);
void BLE_RESQUEST_StateMachine(void *pvParameters);

void setup()
{
  Serial.begin(115200);
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

  pinConfig(); // Hardware and Interrupt Config

  /* CAN-BUS Initialize */
  if (!CAN_start_device())
    esp_restart();

  /* Tasks */
  // This state machine is responsible for the Basic CAN logging
  xTaskCreatePinnedToCore(SdStateMachine, "SDStateMachine", 4096, NULL, 5, &SDlogging, 0);
  xTaskCreatePinnedToCore(BLE_RESQUEST_StateMachine, "BLE_RESQUEST_StateMachine", 4096, NULL, 3, &BLE_RESQUEST_State, 1);
  // This state machine is responsible for the GPRS connection
  xTaskCreatePinnedToCore(ConnStateMachine, "ConnectivityStateMachine", 4096, NULL, 5, &ConectivityState, 1);
}

void loop() { /**/ }

/* SD State Machine */
void SdStateMachine(void *pvParameters)
{
  _sd = start_SD_device();

  save_SD_data(_sd, &bluetooth_packet);

  /* For synchronization between ECU and panel */
  Send_SOT_msg(_sot);

  while (1)
  {
    Check_SD_for_storage(&bluetooth_packet);

    vTaskDelay((_sd ? 1 : 100));
  }

  vTaskDelay(1);
}

void BLE_RESQUEST_StateMachine(void *pvParameters)
{
  while (1)
  {
    if (MPU_request_Debug_data())
      Send_SCU_FLAGS(bluetooth_packet);

    vTaskDelay(750 / portTICK_PERIOD_MS);
  }
}

/* Connectivity State Machine */
void ConnStateMachine(void *pvParameters)
{
  Serial.println("Into the conn_function");
  _sot = Initialize_GSM();
  Serial.println("After Initialize_GSM");

  if ((_sot & 0x04) == ERROR_CONECTION)
  { // enable the error bit
    Send_SOT_msg(_sot);
    vTaskDelay(DELAY_ERROR(_sot));
  }

  save_SOT_data((_sot & 0x04), &bluetooth_packet);
  Send_SOT_msg(_sot);

  while (1)
  {
    Serial.print("Into the internet while --> ");
    bool mqtt_client_conection = Check_mqtt_client_conection();
    if (!mqtt_client_conection)
    {
      _sot == CONNECTED ? _sot = DISCONNECTED : 0; // disable online flag
      Send_SOT_msg(_sot);
      gsmReconnect(_sot);
      Send_SOT_msg(_sot);
    }

    Send_msg_MQTT();

    save_mqtt_client_connection_data(mqtt_client_conection, &bluetooth_packet);

    vTaskDelay(1);
  }
}

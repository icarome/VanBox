#include <Arduino.h>
#include <VwRaiseCanbox.h>
#include <esp32_arduino_rmt_van_rx.h>
#include <esp32_rmt_van_rx.h>
#include <RCSwitch.h>
#include "main.h"
#include "psa_msgs.h"

TaskHandle_t VANReadDataTask;
VwRaiseCanboxRemote* remote;
DoorStatus door_status;
CarStatus car_status;
RCSwitch mySwitch = RCSwitch();
ESP32_RMT_VAN_RX VAN_RX;

const uint8_t VAN_DATA_RX_RMT_CHANNEL = 0;

uint8_t vanMessageLength;
uint8_t vanMessage[34];

void send_car_info() {
  remote->SendCarInfo(&car_status, &door_status);
}

void DumpData(uint16_t iden, uint8_t* data, uint8_t data_len){
  printf("\nIden: %03X; Data: ", iden);
  for (size_t i = 0; i < data_len; i++){
    if (i != vanMessageLength - 1)
    {
        printf("%02X ", data[i]);
    }
    else
    {
        printf("%02X", data[i]);
    }
  }
  printf("\n");
}

void ParseData(uint16_t iden, uint8_t* data, uint8_t dataLen) {
    if (dataLen < 0 || dataLen > 28) return;

    switch (iden) {
        case LIGHTS_STATUS_IDEN:
        {
            if (dataLen != 11 && dataLen != 14)
            {
                return;
            }

            if(data[5] & 0x40){
                mySwitch.send(GARAGE_DOOR_CODE);
            }

            car_status.current_fuel = (data[7]*50) / 100;

            uint16_t remainingKmToService = ((uint16_t)data[2] << 8 | data[3]) * 20;

            if (remainingKmToService < 100) Serial.printf("Próxima manutenção em %i Km\n", remainingKmToService);
        }
        break;

        case CAR_STATUS2_IDEN:
        {
            if (dataLen != 14 && dataLen != 16)
            {
                return;
            }

            door_status.status.hand_brake = data[5] & 0x01 ? 0 : 1;
            door_status.status.seat_belts_sensor = data[5] & 0x02 ? 1 : 0;
            car_status.current_voltage = data[4] & 0x10 ? 11 : 13;
            door_status.status.clean_fluid_error = data[5] & 0x08 ? 1 : 0;


            uint8_t currentMsg = data[9];

            if (currentMsg <= 0x47 && strlen_P(psa_msgs[currentMsg]) > 0) {
                Serial.printf_P(PSTR("%S"), psa_msgs[currentMsg]);
            }
        }
        break;

        case DASHBOARD_IDEN:
        {
            if (dataLen != 7)
            {
                return;
            }

            uint16_t engineRpm = (uint16_t)data[0] << 8 | data[1];
            uint16_t vehicleSpeed = (uint16_t)data[2] << 8 | data[3];

            car_status.current_rpm = engineRpm / 8.0;
            car_status.current_speed = vehicleSpeed / 100.0;
        }
        break;

        case ENGINE_IDEN:
        {
            if (dataLen != 7)
            {
                return;
            }

            car_status.current_temperature = (((data[6] - 80) / 2.0) - 32) * 5/9;
            car_status.current_mileage =  ((uint32_t)data[3] << 16 | (uint32_t)data[4] << 8 | data[5]) / 10.0;
        }
        break;

        case CAR_STATUS1_IDEN:
        {
            if (dataLen != 27)
            {
                return;
            }

            door_status.status.front_left  =    data[7] & 0x40 ? 1 : 0;
            door_status.status.front_right =    data[7] & 0x80 ? 1 : 0;
            door_status.status.rear_right  =    data[7] & 0x20 ? 1 : 0;
            door_status.status.rear_left   =    data[7] & 0x10 ? 1 : 0;
            door_status.status.trunk       =    data[7] & 0x08 ? 1 : 0;
        }
        break;

        default:
        {
            return;
        }
        break;
    }
}

void VANReadDataTaskFunction(void * parameter) {
  VAN_RX.Init(VAN_DATA_RX_RMT_CHANNEL, VAN_RX_PIN, VAN_LINE_LEVEL_HIGH, VAN_NETWORK_TYPE_COMFORT);

  for (;;) {
    VAN_RX.Receive(&vanMessageLength, vanMessage);
    if (vanMessageLength > 0) {
      if(VAN_RX.IsCrcOk(vanMessage, vanMessageLength)) {
        uint16_t vanIden = (vanMessage[1] << 8 | vanMessage[2]) >> 4;
        uint8_t dataLenght = vanMessageLength - 5;

        memmove(vanMessage, vanMessage + 3, dataLenght);
        ParseData(vanIden, vanMessage, dataLenght);
      }
    }
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void setup()
{
    Serial.begin(115200);
    Serial1.begin(38400, SERIAL_8N1, RXD2, TXD2);
    remote = new VwRaiseCanboxRemote(Serial1);
    uint8_t ver[] = SW_VERSION;
    remote->SendVersion(ver, sizeof(ver));
    Serial.println(SW_VERSION);
    mySwitch.enableTransmit(RF_TX_PIN);
    mySwitch.setProtocol(6);
    mySwitch.setRepeatTransmit(15);

    xTaskCreatePinnedToCore(
        VANReadDataTaskFunction,        // Function to implement the task
        "VANReadDataTask",              // Name of the task
        20000,                          // Stack size in words
        NULL,                           // Task input parameter
        1,                              // Priority of the task (higher the number, higher the priority)
        &VANReadDataTask,               // Task handle.
        0);                             // Core where the task should run
}

void loop() {
    vTaskDelay(50 / portTICK_PERIOD_MS);

    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 200UL)
    {
        lastUpdate = millis();
        send_car_info();
    }
}

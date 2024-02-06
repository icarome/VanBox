#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <VanBusRx.h>
#include <VwRaiseCanbox.h>
#include <SoftwareSerial.h>
#include <RCSwitch.h>
#include "main.h"
#include "psa_msgs.h"

VwRaiseCanboxRemote* remote;
SoftwareSerial usbSerial;
DoorStatus door_status;
CarStatus car_status;
RCSwitch mySwitch = RCSwitch();

char* ToHexStr(uint8_t data) {
    #define MAX_UINT8_HEX_STR_SIZE 5
    static char buffer[MAX_UINT8_HEX_STR_SIZE];
    sprintf_P(buffer, PSTR("0x%02X"), data);

    return buffer;
}

void send_car_info() {
  remote->SendCarInfo(&car_status, &door_status);
}

VanPacketParseResult_t ParseVanPacket(TVanPacketRxDesc* pkt) {
    if (! pkt->CheckCrc()) return VAN_PACKET_PARSE_CRC_ERROR;

    int dataLen = pkt->DataLen();
    if (dataLen < 0 || dataLen > 28) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt->Data();
    uint16_t iden = pkt->Iden();

    switch (iden) {
        case LIGHTS_STATUS_IDEN:
        {
            static uint8_t packetData[VAN_MAX_DATA_BYTES] = "";
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            if (dataLen != 11 && dataLen != 14)
            {
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
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
            static uint8_t packetData[VAN_MAX_DATA_BYTES] = "";
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            if (dataLen != 14 && dataLen != 16)
            {
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
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

        case DASHBOARD_BUTTONS_IDEN:
        {
            static uint8_t packetData[VAN_MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            if (dataLen != 11 && dataLen != 12)
            {
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            }

            car_status.current_fuel = (data[4] / 2.0);
        }
        break;

        case DASHBOARD_IDEN:
        {
            static uint8_t packetData[VAN_MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            if (dataLen != 7)
            {
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            }

            uint16_t engineRpm = (uint16_t)data[0] << 8 | data[1];
            uint16_t vehicleSpeed = (uint16_t)data[2] << 8 | data[3];

            car_status.current_rpm = engineRpm / 8.0;
            car_status.current_speed = vehicleSpeed / 100.0;
        }
        break;

        case ENGINE_IDEN:
        {
            static uint8_t packetData[VAN_MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            if (dataLen != 7)
            {
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            }

            car_status.current_temperature = (((data[6] - 80) / 2.0) - 32) * 5/9;
            car_status.current_mileage =  ((uint32_t)data[3] << 16 | (uint32_t)data[4] << 8 | data[5]) / 10.0;
        }
        break;

        case CAR_STATUS1_IDEN:
        {
            static uint8_t packetData[VAN_MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data + 1, packetData, dataLen - 2) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data + 1, dataLen - 2);

            if (dataLen != 27)
            {
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
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
            return VAN_PACKET_PARSE_UNRECOGNIZED_IDEN;
        }
        break;
    }

    return VAN_PACKET_PARSE_OK;
}

void setup()
{
    delay(1000);
    Serial.begin(115200);
    WiFi.disconnect(true);
    delay(1);
    WiFi.mode(WIFI_OFF);
    delay(1);
    WiFi.forceSleepBegin();
    delay(1);

    VanBusRx.Setup(VAN_RX_PIN);
    usbSerial.begin(38400, SWSERIAL_8N1, RXD2, TXD2);

    remote = new VwRaiseCanboxRemote(usbSerial);
    uint8_t ver[] = SW_VERSION;
    remote->SendVersion(ver, sizeof(ver));
    Serial.println(SW_VERSION);
    mySwitch.enableTransmit(RF_TX_PIN);
    mySwitch.setProtocol(6);
    mySwitch.setRepeatTransmit(15);
}

bool IsPacketSelected(uint16_t iden, VanPacketFilter_t filter) {
    if (filter == VAN_PACKETS_ALL_VAN_PKTS) {
        if (
            true
            && iden != ENGINE_IDEN
            && iden != LIGHTS_STATUS_IDEN
            && iden != CAR_STATUS1_IDEN
            && iden != CAR_STATUS2_IDEN
            && iden != DASHBOARD_IDEN
           )
        {
            return false;
        }

        return true;
    }

    return true;
}

void loop()
{
    int n = 0;
    while (VanBusRx.Available()) {
        TVanPacketRxDesc pkt;
        bool isQueueOverrun = false;
        VanBusRx.Receive(pkt, &isQueueOverrun);

        if (isQueueOverrun) Serial.print(F("QUEUE OVERRUN!\n"));

        pkt.CheckCrcAndRepair();

        uint16_t iden = pkt.Iden();

        if (! IsPacketSelected(iden, VAN_PACKETS_ALL_VAN_PKTS)) continue;

        ParseVanPacket(&pkt);

        //pkt.DumpRaw(Serial);

        if (++n >= 15) break;
    }

    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 200UL)
    {
        lastUpdate = millis();
        send_car_info();
    }
}

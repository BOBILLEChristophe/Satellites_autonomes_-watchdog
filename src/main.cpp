/*

copyright (c) 2022 christophe.bobille - LOCODUINO - www.locoduino.org


*/

#ifndef ARDUINO_ARCH_ESP32
#error "Select an ESP32 board"
#endif

#define VERSION "v 0.7.1"
#define PROJECT "Satellites Watchdog"

#include <Arduino.h>
#include <ACAN_ESP32.h>
static const uint32_t CAN_BITRATE = 250UL * 1000UL; // 250 Kb/s

const uint16_t thisNodeId = 252; // N° de nœud CAN du watchdog sur le bus
const uint16_t tabIdSize = 250;
volatile int64_t lastHeartbeatTime[tabIdSize] = {0}; // Tableau pour stocker le temps du dernier battement de cœur

const int64_t watchdogTimeout = 500;     // en millisecondes
const int64_t stillLivingInterval = 250; // en millisecondes
const int64_t recMsgInterval = 1;        // en millisecondes

const byte pinLed = 14;

void IRAM_ATTR stillLiving(void *parameter)
{
  (void)parameter;
  pinMode(pinLed, OUTPUT);

  for (;;)
  {
    // Vérifier le temps écoulé depuis le dernier battement de cœur
    for (uint8_t i = 1; i < tabIdSize; i++)
    {
      // if ((lastHeartbeatTime[i] > 0) && ((millis() - lastHeartbeatTime[i]) > watchdogTimeout))
      if (lastHeartbeatTime[i] > 0 && millis() > watchdogTimeout + lastHeartbeatTime[i])
      {
        // Déclencher la procédure d'erreur pour i+1
        CANMessage frame;
        /************************ ACAN : paramètres du messages *******************/
        const byte prio = 0x00;     // Priorité
        const byte commande = 0x00; // Commande
        const byte resp = 0x00;     // Ceci n'est pas une réponse

        frame.id |= prio << 25;     // Priorite 0, 1 ou 2
        frame.id |= commande << 17; // commande appelée
        frame.id |= resp << 16;     // Response
        frame.id |= thisNodeId;     // ID expediteur
        frame.ext = true;
        frame.len = 5;
        frame.data[4] = 0x02;       // Emergency stop
        const bool ok = ACAN_ESP32::can.tryToSend(frame);
        Serial.print("Aucun signe de vie pour le satellite ");
        digitalWrite(pinLed, LOW);
        Serial.println(i);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(stillLivingInterval)); // Attendre avant de vérifier à nouveau les battements de cœur
    digitalWrite(pinLed, HIGH);
  }
}

void IRAM_ATTR recMsg(void *parameter)
{
  (void)parameter;
  byte idSatExpediteur = 0;
  for (;;)
  {
    CANMessage frame;
    if (ACAN_ESP32::can.receive(frame))
    {
      idSatExpediteur = frame.id & 0xFFFF;           // ID du satellite qui envoie
      lastHeartbeatTime[idSatExpediteur] = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(recMsgInterval));
  }
}

void setup()
{
  // Start serial
  Serial.begin(115200);
  while (!Serial)
    ;

  Serial.printf("\n\nProject :    %s", PROJECT);
  Serial.printf("\nVersion :      %s", VERSION);
  Serial.printf("\nFichier :      %s", __FILE__);
  Serial.printf("\nCompiled :     %s", __DATE__);
  Serial.printf(" - %s\n\n", __TIME__);

  //--- Configure ESP32 CAN
  Serial.print("Configure ESP32 CAN ");
  ACAN_ESP32_Settings settings(CAN_BITRATE);
  settings.mDriverReceiveBufferSize = 100;
  settings.mRxPin = GPIO_NUM_22;
  settings.mTxPin = GPIO_NUM_23;
  const ACAN_ESP32_Filter filter = ACAN_ESP32_Filter::singleExtendedFilter(
      ACAN_ESP32_Filter::data, 0xE0 << 17, 0x1E01FFFF);
  uint32_t errorCode = ACAN_ESP32::can.begin(settings, filter);
  if (errorCode == 0)
    Serial.print("ok !\n");
  else
    Serial.printf("error 0x%x\n", errorCode);
  delay(1000);
  // Serial.end();

  // Créer la tâche d'enregistrement
  xTaskCreatePinnedToCore(recMsg, "recMsg", 4096, NULL, 8, NULL, 0);

  // Créer la tâche de surveillance
  xTaskCreatePinnedToCore(stillLiving, "stillLiving", 4096, NULL, 10, NULL, 1);
}

void loop() {}

#ifndef ARDUINO_ARCH_ESP32
#error "Select an ESP32 board"
#endif

#define VERSION "v 0.2"
#define PROJECT "Satellites Watchdog"

#include <ACAN_ESP32.h>
static const uint32_t CAN_BITRATE = 1000UL * 1000UL;  // 1 Mb/s

const uint8_t tabIdSize = 253;
volatile int64_t lastHeartbeatTime[tabIdSize] = {0};  // Tableau pour stocker le temps du dernier battement de cœur

const int64_t watchdogTimeout = 500;      // en millisecondes
const int64_t stillLivingInterval = 100;  // en millisecondes
const int64_t recMsgInterval = 10;        // en millisecondes



void IRAM_ATTR stillLiving(void *parameter) {
  (void)parameter;

  for (;;) {
    // Vérifier le temps écoulé depuis le dernier battement de cœur
    for ( uint8_t i = 1; i < tabIdSize; i++) {
      if (lastHeartbeatTime[i] > 0 && millis() - lastHeartbeatTime[i] > watchdogTimeout) {
        // Déclencher la procédure d'erreur pour i+1
        Serial.print("Aucun signe de vie pour le satellite ");
        Serial.println(i + 1);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(stillLivingInterval));  // Attendre avant de vérifier à nouveau les battements de cœur
  }
}

void IRAM_ATTR recMsg(void *parameter) {
  (void)parameter;

  for (;;) {
  CANMessage frame ;
    if (ACAN_ESP32::can.receive(frame)) {
      const byte idSatExpediteur = (frame.id & 0x7F80000) >> 19;  // ID du satellite qui envoie
      Serial.println(idSatExpediteur);
      lastHeartbeatTime[idSatExpediteur] = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(recMsgInterval));
  }
}


void setup() {

  //--- Start serial
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
  //settings.mDriverReceiveBufferSize = 100;
  settings.mRxPin = GPIO_NUM_22;
  settings.mTxPin = GPIO_NUM_23;
  const ACAN_ESP32_Filter filter = ACAN_ESP32_Filter::singleExtendedFilter(
  ACAN_ESP32_Filter::data, 0xE0 << 3, 0x1FFFF807);
  uint32_t errorCode = ACAN_ESP32::can.begin(settings, filter);
  if (errorCode == 0)
    Serial.print("ok !\n");
  else {
    Serial.printf("error 0x%x\n", errorCode);
  }

  // Créer la tâche d'enregistrement
  xTaskCreatePinnedToCore(recMsg, "recMsg", 4096, NULL, 1, NULL, 0);

  // Créer la tâche de surveillance
  xTaskCreatePinnedToCore(stillLiving, "stillLiving", 4096, NULL, 1, NULL, 0);
}

void loop() {}

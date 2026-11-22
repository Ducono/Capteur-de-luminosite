// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.



#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include <Arduino.h>
#include "Zigbee.h"
#include <Wire.h>
#include <BH1750.h>

#define LUX_SENSOR_ENDPOINT_NUMBER       1
#define LUX_REPORT_THRESHOLD       10.0f   // Seuil de changement (lux) pour forcer un rapport
#define READ_INTERVAL_MS           10000   // Intervalle de lecture (ms)


uint8_t led = LED_BUILTIN;
uint8_t button = BOOT_PIN;
BH1750 lightMeter;

ZigbeeAnalog zbLuxSensor = ZigbeeAnalog(LUX_SENSOR_ENDPOINT_NUMBER);

float lastReportedLux = -1.0f;  // Dernière valeur rapportée
unsigned long lastReadTime = 0;

/********************* Arduino functions **************************/
void setup() {
  Wire.begin(6,7);
  Serial.begin(115200);
  while (!Serial);             
  Serial.println("\nI2C Scanner");


  // Init 
  pinMode(button, INPUT_PULLUP);
  pinMode(led, OUTPUT); // Initialisation : GPIO en haute impédance 
  digitalWrite(led, HIGH);

  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  if (lightMeter.begin()) 
  {
    Serial.println(F("BH1750 initialised"));
  }
  else 
  {
    Serial.println(F("Error initialising BH1750"));
  }

  // Configuration des endpoints avec modèles UNIQUES
  const char* manufacturer = "Fifi 2026";
  const char* model = "Lux_Sensor-ESP32";

  zbLuxSensor.setManufacturerAndModel(manufacturer, model);
  zbLuxSensor.addAnalogInput();
  zbLuxSensor.setAnalogInputDescription("Lux");


  // Optional: configure analog input
  // analogSetAttenuation(ADC_11db);  // set analog to digital converter (ADC) attenuation to 11 dB (up to ~3.3V input)
  // analogReadResolution(12);        // set analog read resolution to 12 bits (value range from 0 to 4095), 12 is default


  Zigbee.addEndpoint(&zbLuxSensor);



  
  Serial.println("Starting Zigbee...");
  // When all EPs are registered, start Zigbee in End Device mode
  
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  } else {
    Serial.println("Zigbee started successfully!");
  }
 
  
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Reporting automatique Zigbee : min 30s, max 300s, seuil 10 lux
  zbLuxSensor.setAnalogInputReporting(10, 100, LUX_REPORT_THRESHOLD);


   digitalWrite(led, LOW);
  // Set reporting schedule for illuminance value measurement in seconds, must be called after Zigbee.begin()
   //zbLuxSensor.setAnalogInputReporting(10, 60, 1.0);

}

/********************* Main loop **************************/
void loop() {
  unsigned long now = millis();


  // Checking button for factory reset
  if (digitalRead(button) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3 secs, factory reset Zigbee and reboot
        Serial.println("Resetting Zigbee to factory and rebooting in 1s");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
  }


// Lecture capteur toutes les READ_INTERVAL_MS
  if (now - lastReadTime >= READ_INTERVAL_MS) {
    lastReadTime = now;

    float lux = lightMeter.readLightLevel();
    zbLuxSensor.setAnalogInput(lux);  // Met à jour la valeur locale

    Serial.print("Light: ");
    Serial.print(lux);
    Serial.println(" lx");

    // ✅ Rapport forcé UNIQUEMENT si changement significatif
    if (abs(lux - lastReportedLux) >= LUX_REPORT_THRESHOLD || lastReportedLux < 0) {
      zbLuxSensor.reportAnalogInput();
      lastReportedLux = lux;
      Serial.println("  → Rapport Zigbee envoyé");
    }
  }

  delay(10);
}

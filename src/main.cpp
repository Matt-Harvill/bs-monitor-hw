#include <Arduino.h>

#define MIC_PIN 34

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ; // <-- Wait until serial monitor is ready
  Serial.println("Mic test started");
  analogReadResolution(12); // 0–4095
}

void loop()
{
  int micValue = analogRead(MIC_PIN);
  Serial.println(micValue);
  delay(100);
}
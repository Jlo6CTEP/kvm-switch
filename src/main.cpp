#include <ESP8266WiFi.h>

void setup()
{
    Serial.begin(9600);
    Serial.println();

    WiFi.begin("KoT_MyP4aJlKa", "kotikiii");

    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
    while (1) {
        delay(2000);
        Serial.print(".");
    }
}

void loop() {}
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <SoftwareSerial.h>
// DHT11 Sensor
#define DHTPIN D5
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
// Gas Sensor & Panic Button
#define GAS_SENSOR A0
#define BUZZER D6
#define PANIC_BUTTON D0

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// WiFi Credentials
const char* ssid = "YOUR_WIFI_NAME";
const char* pass = "YOUR_WIFI_PASSWORD";

// ThingSpeak
const char* host = "api.thingspeak.com";
String ApiKey = "YOUR_THINGSPEAK_API_KEY";
String path = "/update?key=" + ApiKey + "&field1=";

// Telegram Bot
String botToken = "YOUR_TELEGRAM_BOT_TOKEN";
String chatID = "YOUR_CHAT_ID";

// GPS Module (NEO-6M) on SoftwareSerial
SoftwareSerial gpsSerial(D7, D4);  // RX, TX
#include <TinyGPS.h>
TinyGPS gps;
float flat=0, flon=0;
void read_gps()
{
    bool newData = false;
  unsigned long chars;
  unsigned short sentences, failed;
  for (unsigned long start = millis(); millis() - start < 1000;)
  {
    while (Serial.available())
    {
      char c = Serial.read();
      if (gps.encode(c)) 
        newData = true;
    }
  }

  if (newData)
  {

    unsigned long age;
    gps.f_get_position(&flat,&flon,&age);

  }
}
// Function to send alert to Telegram using HTTPS
void sendTelegramAlert(String message) {
    WiFiClientSecure client;
    client.setInsecure();  // Bypass SSL certificate validation

    HTTPClient http;
    String url = "https://api.telegram.org/bot" + botToken + "/sendMessage?chat_id=" + chatID + "&text=" + message;
    
    Serial.print("Sending alert to Telegram: ");
    Serial.println(message);

    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpCode);
        String response = http.getString();
        Serial.println("Telegram Response: " + response);
    } else {
        Serial.print("Error on HTTP request: ");
        Serial.println(http.errorToString(httpCode));
    }

    http.end();
}

// Function to read GPS data
void setup() {
    Serial.begin(9600);
    dht.begin();
    pinMode(BUZZER, OUTPUT);
    pinMode(GAS_SENSOR, INPUT);
    pinMode(PANIC_BUTTON, INPUT_PULLUP);

    // Initialize LCD
    lcd.begin();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("System Starting...");
    delay(2000);

    // Connect to WiFi
    WiFi.begin(ssid, pass);
    Serial.print("Connecting to WiFi");
    lcd.setCursor(0, 0);
    lcd.print("Connecting WiFi...");
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 50) {
        delay(100);
        Serial.print(".");
        attempt++;
    }
    lcd.clear();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        lcd.setCursor(0, 0);
        lcd.print("WiFi Connected!");
    } else {
        Serial.println("\nWiFi Failed!");
        lcd.setCursor(0, 0);
        lcd.print("WiFi Failed!");
    }
    delay(2000);
    lcd.clear();
    
    // Start GPS
    gpsSerial.begin(9600);

    // Send bot started message
    sendTelegramAlert("🚀 System Started Successfully!");
}

void loop() {
  read_gps();
    int temperature = dht.readTemperature();
    int humidity = dht.readHumidity();
    int gasValue = analogRead(GAS_SENSOR);
//    readGPS(); // Read GPS coordinates

    Serial.print("Temp: "); Serial.println(temperature);
    Serial.print("Humidity: "); Serial.println(humidity);
    Serial.print("Gas: "); Serial.println(gasValue);
    Serial.print("Latitude: "); Serial.println(flat,6);
    Serial.print("Longitude: "); Serial.println(flon,6);
    // Display values on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T:"); lcd.print(temperature);
    lcd.print(" H:"); lcd.print(humidity);   
    lcd.setCursor(0, 1);
    lcd.print("G:"); lcd.print(gasValue);
    lcd.setCursor(8, 1);
    lcd.print("P:"); lcd.print(digitalRead(PANIC_BUTTON));

    // Alert conditions
    if (temperature > 40 || gasValue > 400) {
        digitalWrite(BUZZER, HIGH);
        String alertMsg = "🚨 ALERT! High Temp: " + String(temperature) + "C or Gas Level: " + String(gasValue);
        sendTelegramAlert(alertMsg);
    } else {
        digitalWrite(BUZZER, LOW);
    }

    // Panic button alert
    if (digitalRead(PANIC_BUTTON) == LOW) {
      digitalWrite(BUZZER, HIGH);
        sendTelegramAlert("⚠️ PANIC ALERT! Emergency Button Pressed! Location: " +String (flat,6) + ", " +String (flon,6));
        digitalWrite(BUZZER,LOW);
        delay(3000); // Avoid multiple alerts
    }

    // Send data to ThingSpeak
    WiFiClient client;
    if (client.connect(host, 80)) {
        Serial.println("Connected to ThingSpeak");
        String url = path + String(temperature) + "&field2=" + String(humidity) + "&field3=" + String(digitalRead(PANIC_BUTTON)) + "&field4=" + String(gasValue) + "&field5=" + String(flat,6) + "&field6=" + String(flon,6);
        client.print(String("GET ") + url + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: keep-alive\r\n\r\n");
    }   
    delay(5000); // Delay between readings
}

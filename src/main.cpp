#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

// Create this file,it should define `STASSID` and `STAPSK`:
// #define STASSID "SSID"
// #define STAPSK "PASSWORD"
// #define CMB_API_KEY = "APIKEY"
// #define PHONE_NR = "PHONE NUMBER"
#include <config.h>

// #define DEBUG

// Power latch pin.
#define POWER_PIN 5

#ifndef STASSID
#error "Define configuration in config.h"
#endif

const char *ssid = STASSID;
const char *password = STAPSK;

// TODO: Can be done much more efficiently.
const auto url = String("https://api.callmebot.com/whatsapp.php?phone=") +
                 String(PHONE_NR) + String("&apikey=") + String(CMB_API_KEY) +
                 String("&text=");

// Mapping from Li-Ion voltage to charge percentage.
const float liIonVoltageChargeMap[][2] = {
    {4.2, 100}, {4.15, 95}, {4.11, 90}, {4.08, 85}, {4.02, 80}, {3.98, 75},
    {3.95, 70}, {3.91, 65}, {3.87, 60}, {3.85, 55}, {3.84, 50}, {3.82, 45},
    {3.80, 40}, {3.79, 35}, {3.77, 30}, {3.75, 25}, {3.73, 20}, {3.71, 15},
    {3.69, 10}, {3.61, 5},  {3.27, 0},  {0, 0}};

// Converts Li-Ion voltage to charge percentage.
int GetLiIonChargePercentage(float voltage) {
  const auto arraySize =
      sizeof(liIonVoltageChargeMap) / sizeof(liIonVoltageChargeMap[0]);
  for (int i = 0; i < arraySize; i++) {
    if (liIonVoltageChargeMap[i][0] < voltage) {
      return liIonVoltageChargeMap[i][1];
    }
  }

  return 0;
}

void Shutdown() {
  WiFi.disconnect();

#ifdef DEBUG
  Serial.println("Will shut down");
#endif
  delay(500);

  digitalWrite(POWER_PIN, LOW); // Shutdown.

  // Leave some time to ensure power is completely gone.
  delay(1000);

  // Don't continue.
  while (true) {
    yield();
  }
}

void setup() {
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH); // Ensure power stays on.

#ifdef DEBUG
  Serial.begin(115200);

  Serial.println("");
  Serial.println("");

  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
#endif

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int looped = 0;
  while (WiFi.status() != WL_CONNECTED && looped++ < 50) {
    delay(500);
  }

  // We were not able to connect :( Just shut down.
  if (WiFi.status() != WL_CONNECTED) {
#ifdef DEBUG
    Serial.println("Not able to connect, will show found networks.");
    Serial.println("Scanning...");

    WiFi.disconnect();
    int nbNetworks = WiFi.scanNetworks();

    for (int i = 0; i < nbNetworks; i++) {
      Serial.println(WiFi.SSID(i));
    }

    Serial.println("Shutting down and giving up.");
#endif

    Shutdown();
  }

#ifdef DEBUG
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
#endif

  // Read battery voltage.
  int rawValue = analogRead(A0);
  float voltage = (float)rawValue / 217;

  long rssi = WiFi.RSSI();

  std::unique_ptr<BearSSL::WiFiClientSecure> client(
      new BearSSL::WiFiClientSecure);

  client->setInsecure();

  HTTPClient https;

  const auto fullUrl = url + "Mailbox%20opened!%20Battery%20voltage:%20" +
                       String(voltage) + "%20(" +
                       String(GetLiIonChargePercentage(voltage)) +
                       "%).%20Wi-Fi%20strength:%20" + String(rssi) + "dBm.";

  int tryCount = 0;
  int httpResponse = 0;

  do {
#ifdef DEBUG
    Serial.println("Will call URL: " + fullUrl);
#endif

    https.begin(*client, fullUrl);
    httpResponse = https.GET();

#ifdef DEBUG
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponse);
    String payload = https.getString();
    Serial.println("Response: " + payload);
#endif
    // Free resources
    https.end();

  } while (httpResponse != 200 && tryCount++ < 20);

#ifdef DEBUG
  if (httpResponse != 200) {
    Serial.print("Unable to reach server, giving up.");
  }
#endif

  Shutdown();
}

void loop() {
  // If we ever end up here, just shut down.
  Shutdown();
}

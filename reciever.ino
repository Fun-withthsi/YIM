#include <WiFi.h>
#include <esp_now.h>

// Relay pin definitions
#define RELAY_LIGHT1 25
#define RELAY_LIGHT2 26
#define RELAY_TV     27
#define RELAY_FAN    14

// Structure to receive 2-byte data
typedef struct struct_message {
  uint8_t bytes[2];
} struct_message;

struct_message incomingData;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Initialize relay pins
  pinMode(RELAY_LIGHT1, OUTPUT);
  pinMode(RELAY_LIGHT2, OUTPUT);
  pinMode(RELAY_TV, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);

  // Set all relays to OFF (HIGH for low-trigger relays)
  digitalWrite(RELAY_LIGHT1, HIGH);
  digitalWrite(RELAY_LIGHT2, HIGH);
  digitalWrite(RELAY_TV, HIGH);
  digitalWrite(RELAY_FAN, HIGH);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("📡 Receiver Ready — Waiting for ESP-NOW Commands...");
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len == sizeof(incomingData)) {
    memcpy(&incomingData, data, len);

    // Reconstruct HEX from high and low byte
    uint8_t lowByte  = incomingData.bytes[1];
    uint8_t highByte = incomingData.bytes[0];
    uint16_t receivedHex = (highByte << 8) | lowByte;

    Serial.printf("📥 Received HEX: 0x%04X → ", receivedHex);

    switch (receivedHex) {
      case 0xAA23:
        digitalWrite(RELAY_LIGHT1, LOW);
        Serial.println("💡 Light 1 ON");
        break;
      case 0xAA24:
        digitalWrite(RELAY_LIGHT2, LOW);
        Serial.println("💡 Light 2 ON");
        break;
      case 0xAA25:
        digitalWrite(RELAY_TV, LOW);
        Serial.println("📺 TV ON");
        break;
      case 0xAA26:
        digitalWrite(RELAY_FAN, LOW);
        Serial.println("🌀 Fan ON");
        break;
      case 0xAA27:
        digitalWrite(RELAY_LIGHT1, HIGH);
        Serial.println("💡 Light 1 OFF");
        break;
      case 0xAA28:
        digitalWrite(RELAY_LIGHT2, HIGH);
        Serial.println("💡 Light 2 OFF");
        break;
      case 0xAA29:
        digitalWrite(RELAY_TV, HIGH);
        Serial.println("📺 TV OFF");
        break;
      case 0xAA30:
        digitalWrite(RELAY_FAN, HIGH);
        Serial.println("🌀 Fan OFF");
        break;
      default:
        Serial.println("❗ Unknown Command");
        break;
    }
  } else {
    Serial.println("❗ Invalid data length received");
  }
}

void loop() {
  // Nothing in loop — all logic is interrupt-driven by ESP-NOW
}

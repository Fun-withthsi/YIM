#include <WiFi.h>
#include <esp_now.h>
#include <SPI.h>
#include <MFRC522.h>

// RFID Configuration (CHANGE THESE TO YOUR PINS!)
#define RST_PIN 22     // ESP32 RST pin (e.g., GPIO 22)
#define SS_PIN 21      // ESP32 SDA pin (e.g., GPIO 21)
MFRC522 rfid(SS_PIN, RST_PIN);

// Receiver MAC Address (Update this!)
uint8_t receiverMAC[] = {0xF4, 0x65, 0x0B, 0xE7, 0xD8, 0xFC};

// Data Structures
typedef struct struct_message {
  uint8_t bytes[2];  // VC-02 command bytes
} struct_message;
struct_message outgoingData;

// Serial Pins
#define ARDUINO_TX 4  // ESP32 TX pin for Arduino Uno

void setup() {
  // Initialize serial ports
  Serial.begin(115200);
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);  // VC-02
  Serial1.begin(9600, SERIAL_8N1, -1,  ARDUINO_TX);  // Arduino Uno

  // Initialize RFID
  SPI.begin();
  rfid.PCD_Init();
  
  // Initialize WiFi/ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }

  // Register peer
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Failed to add peer");
    return;
  }

  Serial.println("🚀 System Ready: VC-02 + RFID");
}

void loop() {
  // Handle VC-02 Commands
  if (Serial2.available() >= 2) {
    uint8_t byte1 = Serial2.read();
    uint8_t byte2 = Serial2.read();

    // Send via ESP-NOW
    outgoingData.bytes[0] = byte1;
    outgoingData.bytes[1] = byte2;
    esp_now_send(receiverMAC, (uint8_t *)&outgoingData, sizeof(outgoingData));

    // Send to Arduino with 'V' prefix
    Serial1.write('V'); 
    Serial1.write(byte1);
    Serial1.write(byte2);
    
    Serial.printf("VC-02 Sent: %02X %02X\n", byte1, byte2);
  }

  // Handle RFID Tags
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    
    // Send to Arduino with 'R' prefix
    Serial1.print("R" + uid); 
    
    Serial.print("RFID Detected: ");
    Serial.println(uid);

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}

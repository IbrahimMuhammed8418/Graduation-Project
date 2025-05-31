#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <SoftwareSerial.h>

// Initialize PN532 module (I2C interface)
PN532_I2C pn532i2c(Wire);
PN532 nfc(pn532i2c);

// SoftwareSerial for ESP32 communication
SoftwareSerial espSerial(2, 3); // RX = Pin 2, TX = Pin 3

// Define known NFC tags (UIDs)
uint8_t allowedUID1[] = {0x04, 0x6F, 0x16, 0x8E, 0x10, 0x03, 0x89};
uint8_t allowedUID2[] = {0xE3, 0xBF, 0x6C, 0x0};
size_t allowedUID1Length = sizeof(allowedUID1);
size_t allowedUID2Length = sizeof(allowedUID2);

void setup() {
  // Initialize Serial Monitor and SoftwareSerial
  Serial.begin(9600);         // For Serial Monitor
  espSerial.begin(9600);        // For communication with ESP32

  Serial.println("*** PN532 NFC RFID with Serial Command ***");

  // Initialize I2C communication
  Wire.begin();  // Default I2C pins for Arduino (SDA = A4, SCL = A5 for Uno)

  // Start PN532 module
  nfc.begin();

  // Check if the PN532 module is connected
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Didn't find PN532 module!");
    while (1);  // Halt the program
  }

  // Print chip information
  Serial.println("PN532 module detected:");
  Serial.print("Firmware Version: ");
  Serial.print((versiondata >> 24) & 0xFF, HEX);  // Chip type
  Serial.print(".");
  Serial.print((versiondata >> 16) & 0xFF, HEX);  // Firmware version
  Serial.println();

  // Configure the module to read passive RFID cards
  nfc.SAMConfig();
  Serial.println("Waiting for an NFC card...");
}

void loop() {
  uint8_t uid[7];   // Buffer to store UID
  uint8_t uidLength;  // Length of UID

  // Try to read an NFC card
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    Serial.println("Card detected!");

    // Print UID length and data
    Serial.print("UID Length: ");
    Serial.print(uidLength);
    Serial.println(" bytes");
    Serial.print("UID: ");
    for (uint8_t i = 0; i < uidLength; i++) {
      Serial.print(" 0x");
      Serial.print(uid[i], HEX);  // Print UID in hexadecimal format
    }
    Serial.println();

    // Check if the detected UID matches any of the allowed UIDs
    if (uidLength == allowedUID1Length && compareUID(uid, allowedUID1, uidLength)) {
      Serial.println("Authorized tag detected: UID1");
      espSerial.println("OPEN");  // Send "OPEN" command to ESP32 via 
            Serial.println("OPEN");  // Send "OPEN" command to ESP32 via SoftwareSerial

    } else if (uidLength == allowedUID2Length && compareUID(uid, allowedUID2, uidLength)) {
      Serial.println("Authorized tag detected: UID2");
      espSerial.println("OPEN");  // Send "OPEN" command to ESP32 via SoftwareSerial
    } else {
      Serial.println("Unauthorized tag detected.");
    }

    // Wait for the card to be removed before scanning again
    while (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
      delay(50);
    }
  }

  delay(500);  // Poll every 500 ms
}

// Function to compare two UIDs
bool compareUID(uint8_t* uid1, uint8_t* uid2, uint8_t length) {
  for (uint8_t i = 0; i < length; i++) {
    if (uid1[i] != uid2[i]) {
      return false;  // UIDs do not match
    }
  }
  return true;  // UIDs match
}

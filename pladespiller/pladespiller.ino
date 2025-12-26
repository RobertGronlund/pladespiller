/*
 * NFC Record Player for Kids - Firmware v2
 * Board: DFRobot Beetle ESP32-C6
 * * MODES:
 * 1. NORMAL MODE (Default): Wakes up, plays music based on tag, goes back to sleep.
 * 2. MAINTENANCE MODE: Hold BOOT BUTTON (GPIO 9) while turning on.
 * - Prevents Deep Sleep.
 * - Dumps NFC Tag IDs to Serial Monitor (for setup).
 */

#include <SPI.h>
#include <MFRC522.h>
#include <DFRobotDFPlayerMini.h>
#include <HardwareSerial.h>

// ================= PIN DEFINITIONS =================
#define NFC_PWR_PIN   7
#define NFC_SCK_PIN   23 
#define NFC_MISO_PIN  21 
#define NFC_MOSI_PIN  22 
#define NFC_CS_PIN    19 
#define NFC_RST_PIN   20 

#define DF_RX_PIN     17 
#define DF_TX_PIN     16 

#define WAKE_BUTTON_PIN  6   // External "Play" Button
#define ONBOARD_LED      15  
#define BOOT_BUTTON_PIN  9   // Internal Beetle C6 Button

// ================= OBJECTS =================
MFRC522 mfrc522(NFC_CS_PIN, NFC_RST_PIN);
HardwareSerial dfSerial(1); 
DFRobotDFPlayerMini myDFPlayer;

// ================= GLOBAL VARIABLES =================
bool maintenanceMode = false; // Flag to track if we are debugging

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  dfSerial.begin(9600, SERIAL_8N1, DF_RX_PIN, DF_TX_PIN);
  
  // 1. Init Controls
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(NFC_PWR_PIN, OUTPUT);
  digitalWrite(NFC_PWR_PIN, HIGH); // Power on NFC module
  pinMode(WAKE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP); // Internal button is Active LOW
  
  // 2. CHECK FOR MAINTENANCE MODE
  // If the button is pressed (LOW) during startup, stay awake.
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    maintenanceMode = true;
    digitalWrite(ONBOARD_LED, HIGH); // Keep LED on solid to indicate Debug Mode
    Serial.println("\n!!! MAINTENANCE MODE ACTIVE !!!");
    Serial.println("Deep Sleep DISABLED.");
    Serial.println("Scan a tag to see its UID...");
    delay(1000); // Small delay to let you release the button
  } else {
    // Normal startup blink
    digitalWrite(ONBOARD_LED, HIGH);
    delay(100);
    digitalWrite(ONBOARD_LED, LOW);
  }
  
  // 3. Init Hardware
  SPI.begin(NFC_SCK_PIN, NFC_MISO_PIN, NFC_MOSI_PIN);
  mfrc522.PCD_Init();
  
  if (!myDFPlayer.begin(dfSerial, false, true)) {
    Serial.println("Error: DFPlayer not detected.");
  } else {
    Serial.println("DFPlayer Mini online.");
  }
  myDFPlayer.volume(5);

  // 4. NORMAL MODE LOGIC (Only runs if NOT in maintenance mode)
  if (!maintenanceMode) {
    
    Serial.println("Normal Mode: Waiting for Record...");
    int songID = -1;
    unsigned long startTime = millis();

    // Try to read a card for 5 seconds
    while (millis() - startTime < 5000) {
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        Serial.print("Normal Mode UID: ");
        printHex(mfrc522.uid.uidByte, mfrc522.uid.size);
        Serial.println();
        songID = getSongFromUID(mfrc522.uid.uidByte, mfrc522.uid.size);
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        break; 
      }
      delay(50);
    }

    if (songID != -1) {
      Serial.printf("Playing Song #%d\n", songID);
      myDFPlayer.play(songID); 
      delay(500); 
    } else {
      Serial.println("No card found.");
    }

    // ENTER DEEP SLEEP
    Serial.println("Going to Sleep.");
    Serial.flush(); 
    
    // Configure wake up source
    esp_deep_sleep_enable_gpio_wakeup(1ULL << WAKE_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
  }
  
  // If we are in maintenance mode, setup() finishes and we fall into loop()
}

// ================= LOOP =================
void loop() {
  // This ONLY runs if maintenanceMode is TRUE
  
  if (maintenanceMode) {
    // Continuous NFC Scanning for Debugging
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      
      Serial.print("UID Found: ");
      printHex(mfrc522.uid.uidByte, mfrc522.uid.size);
      Serial.println();
      
      // Visual feedback
      digitalWrite(ONBOARD_LED, LOW);
      delay(100);
      digitalWrite(ONBOARD_LED, HIGH);
      
      // Stop reading this card so we don't spam serial
      mfrc522.PICC_HaltA(); 
      mfrc522.PCD_StopCrypto1();
    }
  }
}

// ================= HELPERS =================

int getSongFromUID(byte *uid, byte size) {
  // Fail fast if UID length is unexpected.
  if (size < 4) {
    Serial.println("UID too short, ignoring");
    return -1;
  }

  // Example mappings (replace with your own UIDs)
  const byte tag1[] = {0x04, 0x1B, 0x7E, 0xF2, 0x2F, 0x4D, 0x81};
  const byte tag2[] = {0x04, 0x23, 0x7E, 0xF2, 0x2F, 0x4D, 0x81};

  if (size == sizeof(tag1) && memcmp(uid, tag1, sizeof(tag1)) == 0) {
    return 2;
  }
  if (size == sizeof(tag2) && memcmp(uid, tag2, sizeof(tag2)) == 0) {
    return 3;
  }

  Serial.print("Unknown UID: ");
  printHex(uid, size);
  Serial.println();
  return -1;
}

// Helper to print UIDs clearly in Maintenance Mode
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/*
 * NFC Record Player for Kids - Firmware v3 (MOSFET Power Gate Edition)
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

// ================= PIN DEFINITIONS =================
#define POWER_GATE_PIN 5   // MOSFET Gate to control GND of modules
#define NFC_SCK_PIN    23 
#define NFC_MISO_PIN   21 
#define NFC_MOSI_PIN   22 
#define NFC_CS_PIN     19 
#define NFC_RST_PIN    20 

#define DF_RX_PIN      17 
#define DF_TX_PIN      16 

#define WAKE_BUTTON_PIN 6  // External "Play" Button
#define ONBOARD_LED     15  
#define BOOT_BUTTON_PIN 9  // Maintenance Mode Button

// ================= OBJECTS =================
MFRC522 mfrc522(NFC_CS_PIN, NFC_RST_PIN);
#define dfSerial Serial1
DFRobotDFPlayerMini myDFPlayer;

bool maintenanceMode = false;

void setup() {
  Serial.begin(115200);
  
  // 1. POWER UP THE ISLAND
  pinMode(POWER_GATE_PIN, OUTPUT);
  digitalWrite(POWER_GATE_PIN, HIGH); // Turn on MOSFET
  
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(WAKE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  // Give modules time to boot from a cold start
  delay(1500); 

  // 2. CHECK MAINTENANCE MODE
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    maintenanceMode = true;
    digitalWrite(ONBOARD_LED, HIGH);
    Serial.println("\n!!! MAINTENANCE MODE ACTIVE !!!");
  }

  // 3. INITIALIZE HARDWARE
  SPI.begin(NFC_SCK_PIN, NFC_MISO_PIN, NFC_MOSI_PIN);
  mfrc522.PCD_Init();
  
  // Enable antenna power
  if (mfrc522.PCD_GetAntennaGain() == 0x00) {
    mfrc522.PCD_SetAntennaGain(0x07 << 4);
    Serial.println("Antenna gain set to max.");
  }
  mfrc522.PCD_AntennaOn();
  Serial.println("NFC Antenna powered ON.");
  
  dfSerial.begin(9600, SERIAL_8N1, DF_RX_PIN, DF_TX_PIN);

  // 4. INIT DFPLAYER
  if (!myDFPlayer.begin(dfSerial, true, true)) {
    Serial.println("DFPlayer Error: Check SD card or wiring.");
  } else {
    Serial.println("DFPlayer Online.");
    myDFPlayer.volume(20);
  }

  // 5. NORMAL MODE LOGIC
  if (!maintenanceMode) {
    int songID = -1;
    unsigned long scanStart = millis();

    Serial.println("Scanning for NFC Tag...");
    while (millis() - scanStart < 5000) {
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
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
      
      // Wait for song to finish (using your 30s max duration)
      // ESP32 stays awake to keep the MOSFET ON
      Serial.println("Song playing... staying awake for 30s.");
      delay(30000); 
    } else {
      Serial.println("No card found.");
    }

    // 6. SHUTDOWN SEQUENCE
    Serial.println("Shutting down Power Island...");
    mfrc522.PCD_SoftPowerDown();
    digitalWrite(POWER_GATE_PIN, LOW); // MOSFET OFF - Kills GND to DFPlayer/NFC
    
    // Configure ESP32-C6 Wakeup
    // 0 = Wake on LOW (button press)
    esp_deep_sleep_enable_gpio_wakeup(1ULL << WAKE_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
    
    Serial.println("Deep Sleep. Zzz...");
    Serial.flush();
    delay(10);
    esp_deep_sleep_start();
  }
}

void loop() {
  if (maintenanceMode) {
    // Ensure antenna stays powered
    mfrc522.PCD_AntennaOn();
    
    // Maintenance mode keeps the MOSFET ON so you can scan tags
    if (mfrc522.PICC_IsNewCardPresent()) {
      Serial.println("Card detected!");
      if (mfrc522.PICC_ReadCardSerial()) {
        Serial.print("Tag UID:");
        for (byte i = 0; i < mfrc522.uid.size; i++) {
          Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
          Serial.print(mfrc522.uid.uidByte[i], HEX);
        }
        Serial.println();
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
      }
    } else {
      Serial.println("Waiting for card...");
    }
  }
  delay(500);
}


int getSongFromUID(byte *uid, byte size) {
  // Add your tag UIDs here
  const byte tag1[] = {0x04, 0x1B, 0x7E, 0xF2, 0x2F, 0x4D, 0x81};
  const byte tag2[] = {0x04, 0x23, 0x7E, 0xF2, 0x2F, 0x4D, 0x81};

  if (memcmp(uid, tag1, 7) == 0) return 2;
  if (memcmp(uid, tag2, 7) == 0) return 3;

  return -1;
}
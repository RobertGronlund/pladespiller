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
int volumeLevel = 20; // Default volume (0-30)

bool maintenanceMode = false;
volatile bool buttonPressed = false;
unsigned long lastPlayStart = 0;
bool isPlaying = false;
byte currentCardUID[10];
byte currentCardUIDSize = 0;
int cardMissCount = 0;
int cardMissThreshold = 2; // Number of consecutive misses before considering card removed
unsigned long lastCardCheckTime = 0;


// Interrupt handler for WAKE_BUTTON_PIN
void IRAM_ATTR handleButtonPress() {
  buttonPressed = true;
}

void setup() {
  Serial.begin(115200);
  
  // 1. POWER UP THE ISLAND
  pinMode(POWER_GATE_PIN, OUTPUT);
  digitalWrite(POWER_GATE_PIN, HIGH); // Turn on MOSFET
  
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(WAKE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  // Give modules time to boot from a cold start
  delay(100); 

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
    myDFPlayer.volume(volumeLevel);
  }

  // 5. ATTACH INTERRUPT FOR WAKE BUTTON
  attachInterrupt(digitalPinToInterrupt(WAKE_BUTTON_PIN), handleButtonPress, FALLING);
  
  if (!maintenanceMode) {
    Serial.println("Normal mode: Press button to scan and play.");
    buttonPressed = true; // Trigger initial scan on startup
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
    delay(500);
  } else {
    // NORMAL MODE: Scan and play on button press
    if (buttonPressed) {
      buttonPressed = false;
      lastPlayStart = millis();
      isPlaying = false;
      
      int songID = -1;
      unsigned long scanStart = millis();

      Serial.println("Scanning for NFC Tag (5s timeout)...");
      while (millis() - scanStart < 5000) {
        // Allow button to interrupt scan
        if (buttonPressed) {
          buttonPressed = false;
          lastPlayStart = millis();
          scanStart = millis(); // Restart the 5-second scan window
          Serial.println("Button pressed - restarting scan...");
          continue;
        }
        
        if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
          songID = getSongFromUID(mfrc522.uid.uidByte, mfrc522.uid.size);
          break; 
        }
        delay(50);
      }

      if (songID != -1) {
        Serial.printf("Playing Song #%d\n", songID);
        myDFPlayer.play(songID);
        isPlaying = true;
        lastPlayStart = millis(); // Reset the 30s timer
        lastCardCheckTime = millis(); // Initialize card check timer
        cardMissCount = 0; // Reset miss counter
        // Store the current card UID for removal detection
        memcpy(currentCardUID, mfrc522.uid.uidByte, mfrc522.uid.size);
        currentCardUIDSize = mfrc522.uid.size;
        Serial.println("Song playing. Remove tag to stop and shutdown.");
      } else {
        Serial.println("No card found. Press button to try again.");
      }
    }

    // Check for card removal during playback (every 500ms)
    if (isPlaying && (millis() - lastCardCheckTime >= 500)) {
      lastCardCheckTime = millis();
      if (isCurrentCardPresent()) {
        cardMissCount = 0; // Card still there, reset counter
      } else {
        cardMissCount++;
        if (cardMissCount >= cardMissThreshold) {
          Serial.println("Card removed. Stopping playback and shutting down...");
          shutdownDevice();
        }
      }
    }

    // Check if 30 seconds have elapsed since play started (backup timeout)
    if (isPlaying && (millis() - lastPlayStart >= 30000)) {
      Serial.println("30s timeout reached. Shutting down...");
      shutdownDevice();
    }

    delay(50);
  }
}

void shutdownDevice() {
  isPlaying = false;
  myDFPlayer.stop();
  
  // Clean up the NFC card - halt and stop crypto
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  
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


bool isCurrentCardPresent() {
  // Detect Tag without looking for collisions using PICC_RequestA
  byte bufferATQA[2];
  byte bufferSize = sizeof(bufferATQA);

  // Reset baud rates
  mfrc522.PCD_WriteRegister(mfrc522.TxModeReg, 0x00);
  mfrc522.PCD_WriteRegister(mfrc522.RxModeReg, 0x00);
  // Reset ModWidthReg
  mfrc522.PCD_WriteRegister(mfrc522.ModWidthReg, 0x26);

  MFRC522::StatusCode result = mfrc522.PICC_RequestA(bufferATQA, &bufferSize);

  if (result == mfrc522.STATUS_OK) {
    if (!mfrc522.PICC_ReadCardSerial()) {
      return false;
    }
    
    // Check if it matches the card that triggered playback
    if (mfrc522.uid.size == currentCardUIDSize && 
        memcmp(mfrc522.uid.uidByte, currentCardUID, currentCardUIDSize) == 0) {
      // Don't halt yet - keep card active for next check
      return true;
    }
    
    // Card detected but doesn't match - halt this one
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return false;
  }
  
  return false; // No card detected
}

int getSongFromUID(byte *uid, byte size) {
  // Add your tag UIDs here
  const byte tag1[] = {0x04, 0x6F, 0x7F, 0xF2, 0x2F, 0x4D, 0x81};
  const byte tag2[] = {0x04, 0x1B, 0x7E, 0xF2, 0x2F, 0x4D, 0x81};
  const byte tag3[] = {0x04, 0x23, 0x7E, 0xF2, 0x2F, 0x4D, 0x81};
  const byte tag4[] = {0x04, 0x77, 0x7F, 0xF2, 0x2F, 0x4D, 0x81};
  const byte tag5[] = {0x04, 0x7F, 0x7F, 0xF2, 0x2F, 0x4D, 0x81};
  const byte tag6[] = {0x04, 0x86, 0x7E, 0xF2, 0x2F, 0x4D, 0x81};
  const byte tag7[] = {0x04, 0x8E, 0x7E, 0xF2, 0x2F, 0x4D, 0x81};
  const byte tag8[] = {0x04, 0x45, 0x7F, 0xF2, 0x2F, 0x4D, 0x81};

  if (memcmp(uid, tag1, 7) == 0) return 1;
  if (memcmp(uid, tag2, 7) == 0) return 2;
  if (memcmp(uid, tag3, 7) == 0) return 3;
  if (memcmp(uid, tag4, 7) == 0) return 4;
  if (memcmp(uid, tag5, 7) == 0) return 5;
  if (memcmp(uid, tag6, 7) == 0) return 6;
  if (memcmp(uid, tag7, 7) == 0) return 7;
  if (memcmp(uid, tag8, 7) == 0) return 8;

  return -1;
}
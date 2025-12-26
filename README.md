# 🎵 NFC Kids Record Player

A robust, battery-powered, open-source toy that mimics a classic record player. Designed for toddlers and young children, it uses 3D-printed "records" with embedded NFC tags to trigger music playback.

The project prioritizes **low power consumption**, **ease of assembly** (no custom PCBs), and **simple physical interaction**.

## ✨ Features

  * **Physical Interaction:** Place a record to play, remove to stop (or change songs).
  * **Long Battery Life:** Utilizes **Deep Sleep** modes on the ESP32-C6; the system wakes up only to read a tag or process a button press.
  * **Rechargeable:** Powered by a single 18650 Li-Ion cell with USB-C charging (via DFRobot Beetle C6).
  * **High-Quality Audio:** Uses the DFPlayer Mini with a dedicated speaker.
  * **No Custom PCBs:** Designed for point-to-point wiring on perfboard.

## 🛠️ Bill of Materials (BOM)

| Component | Description | Quantity | Notes |
| :--- | :--- | :--- | :--- |
| **MCU** | **DFRobot Beetle ESP32-C6** | 1 | Selected for ultra-low power & integrated TP4057 battery charging. |
| **Audio Module** | **DFPlayer Mini** | 1 | MP3/WAV decoder with built-in 3W mono amp. |
| **NFC Reader** | **RC522 Module** (13.56MHz) | 1 | Standard SPI interface. |
| **Battery** | **18650 Li-Ion Cell** | 1 | 2000mAh - 3500mAh recommended. **Must include protection circuit if board does not.** |
| **Storage** | **MicroSD Card** | 1 | Max 32GB, formatted FAT32. |
| **Speaker** | **3W 4Ω Full Range Driver** | 1 | Or a salvaged laptop speaker (approx 4Ω-8Ω). |
 uhb | **Tags** | **NTAG215 Stickers** | 10+ | One sticker per 3D printed record. |
| **Switch** | **Slide/Toggle Switch** | 1 | Main system power cut-off. |
| **Button** | **Momentary Push Button** | 1 | "Play/Wake" button. |
| **Misc** | Perfboard, Wires, Resistors | - | For assembly. |

## 🔌 Wiring & Connections

### 1\. Power Distribution

  * **Battery:** Connected to Beetle C6 Battery Pads.
  * **Main Switch:** Breaks the positive line between Battery and Beetle (or use the Beetle's power pads if applicable).
  * **Peripherals:** The DFPlayer and RC522 are powered via the Beetle's VCC/GND pins.

### 2\. Pinout Table (Suggested)

| ESP32-C6 Pin | Component Pin | Function |
| :--- | :--- | :--- |
| **TX (GPIO 16)** | DFPlayer **RX** | Serial Audio Control (UART) |
| **RX (GPIO 17)** | DFPlayer **TX** | Serial Audio Feedback (UART) |
| **SCK** | RC522 **SCK** | SPI Clock |
| **MISO** | RC522 **MISO** | SPI Data In |
| **MOSI** | RC522 **MOSI** | SPI Data Out |
| **SDA (GPIO X)** | RC522 **SDA/SS** | Chip Select |
| **GPIO 0 (or similar)**| Button | **Deep Sleep Wake Trigger** |

*\> **Note:** The DFPlayer RX pin may require a 1kΩ resistor in series if the ESP32 logic level causes noise, though usually direct connection works at 3.3V logic.*

## 💾 Firmware Logic

To maximize battery life, the firmware operates on an event-driven "Deep Sleep" cycle:

1.  **Idle State:** System is in Deep Sleep (uA current draw).
2.  **Wake Event:** User presses "Play" button.
3.  **Action:**
      * MCU wakes up.
      * Powers up RC522 reader.
      * Checks for NFC Tag.
      * **If Tag Found:** Maps UID to a specific folder/file on the SD card -\> Sends UART command to DFPlayer -\> ESP32 goes back to Sleep (Music continues playing via DFPlayer).
      * **If No Tag:** Go back to Sleep.

## 📂 SD Card Structure

The DFPlayer Mini requires a specific folder structure to index songs correctly.

```text
SD Card Root
│
├── mp3
│   ├── 0001songtitle.mp3
│   ├── 0002songtitle.mp3
│   └── ...
```

## 🖨️ Mechanical Design

  * **Enclosure:** Box housing the electronics, speaker, and battery.
  * **Top Surface:** Features a recess for the NFC reader (underneath the plastic) and a center spindle for the record.
  * **Records:** 3D printed discs with a bottom recess to hide the NTAG215 sticker.

## 🚀 Getting Started

1.  **Format SD Card:** Format to FAT32. Copy MP3 files into numbered folders (e.g., `01/001.mp3`).
2.  **Flash Firmware:** Open the project in VS Code (PlatformIO) or Arduino IDE. Install `MFRC522` and `DFRobotDFPlayerMini` libraries.
3.  **Map Tags:** Run the "Reader" sketch to get UIDs from your stickers. Update the `uid_to_song` mapping in the main code.
4.  **Assemble:** Solder components to perfboard following the wiring diagram.
5.  **Play:** Insert battery, flip the switch, place a record, and press Play\!

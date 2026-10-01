# 🎵 Pladespiller

A robust, battery-powered, open-source toy that mimics a classic record player. Designed for toddlers and young children, it uses 3D-printed "records" with embedded NFC tags to trigger music playback.

The project prioritizes **fun**, **ease of use** , and **great diverse music selection**.


## ✨ Features

* **Physical Interaction:** Place a record to play, remove to stop (or change songs).
* **Long Battery Life:** Deep Sleep between reads on the ESP32-C6, with a high-side load switch (AP22815) cutting power to the NFC reader and DFPlayer entirely while asleep.
* **Rechargeable:** Powered by a single-cell Li-Ion/LiPo battery with USB-C charging (via DFRobot Beetle C6).
* **High-Quality Audio:** Uses the DFPlayer Mini with a dedicated speaker.
* **No Custom PCBs:** Designed for point-to-point wiring on perfboard.

## 🛠️ Bill of Materials (BOM)

| Component | Description | Quantity | Notes |
| :--- | :--- | :--- | :--- |
| **MCU** | **DFRobot Beetle ESP32-C6** | 1 | Ultra-low power, integrated battery charging. |
| **Audio Module** | **DFPlayer Mini** | 1 | MP3/WAV decoder with built-in 3W mono amp. |
| **NFC Reader** | **RC522 Module** (13.56MHz) | 1 | Standard SPI interface. |
| **Power Switch IC** | **AP22815AWT-7** (TSOT25) | 1 | High-side load switch; gates VCC to the NFC reader and DFPlayer during sleep. |
| **Battery** | **Single-cell Li-Ion/LiPo** | 1 | Must include protection circuit if the board does not. |
| **Storage** | **MicroSD Card** | 1 | Formatted FAT32. |
| **Speaker** | **3W 4Ω Full Range Driver** | 1 | Or a salvaged laptop speaker (approx 4Ω-8Ω). |
| **Tags** | **NTAG215 Stickers** | 10+ | One sticker per 3D printed record. |
| **Button** | **Momentary Push Button** | 1 | "Play/Wake" button. |
| **Misc** | Perfboard, Wires, Resistors | - | For assembly. |

## 🔌 Wiring & Connections

### 1. Power Distribution

* **Battery → Beetle:** Connected to the Beetle's battery pads.
* **Main Switch:** Breaks the positive line between battery and Beetle — a manual, full power cut-off independent of the deep-sleep logic below.
* **Battery → AP22815 IN:** The load switch's input is wired to the raw battery pad (unswitched).
* **AP22815 OUT → NFC + DFPlayer VCC:** Both peripherals share this single switched rail — they're always powered together, never independently.
* **AP22815 EN ← GPIO 5:** Active-high. HIGH = peripherals powered, LOW = powered off (asserted before deep sleep).
* **AP22815 FLG:** Tied directly to GND — fault flag is unused, not wired to any GPIO.

### 2. Pinout Table

| ESP32-C6 Pin | Component Pin | Function |
| :--- | :--- | :--- |
| **GPIO 5** | AP22815 **EN** | Power gate (active-high) |
| **GPIO 23** | RC522 **SCK** | SPI Clock |
| **GPIO 21** | RC522 **MISO** | SPI Data In |
| **GPIO 22** | RC522 **MOSI** | SPI Data Out |
| **GPIO 19** | RC522 **SDA/SS** | Chip Select |
| **GPIO 20** | RC522 **RST** | Reset |
| **GPIO 17** | DFPlayer **TX** | ESP32 RX1 (UART) |
| **GPIO 16** | DFPlayer **RX** | ESP32 TX1 (UART) — via 1kΩ series resistor |
| **GPIO 6** | Wake Button | Active-low; wakes from Deep Sleep |
| **GPIO 9** | Boot Button (onboard) | Active-low; hold at power-on for Maintenance Mode |
| **GPIO 15** | Status LED (onboard) | |

## 💾 Firmware Logic

1. **Idle:** Deep Sleep (µA current draw), GPIO 5 LOW, peripherals unpowered.
2. **Wake:** Button press → GPIO 5 HIGH → ~150ms to let peripherals stabilize → init NFC reader and DFPlayer.
3. **Scan:** Poll for a tag for up to 5 seconds (a button press restarts the window).
4. **Tag found:** Map UID to a song, start playback. The MCU stays awake, checking every 500ms that the tag is still present.
5. **Shutdown:** Triggered by no tag found within the scan window, the tag being removed, or a 35s playback timeout — stop playback, drive GPIO 5 LOW, return to Deep Sleep.
6. **Maintenance Mode:** Hold the Boot Button while powering on to keep peripherals powered and print scanned tag UIDs to the Serial Monitor, for mapping new records. Never sleeps.

## 📂 SD Card Structure

The DFPlayer Mini's folder-based playback expects a specific structure:

```text
SD Card Root
└── mp3/
    ├── 0001robert.mp3
    ├── 0002pil.mp3
    └── ...
```

## 🖨️ Mechanical Design

* **Enclosure:** Box housing the electronics, speaker, and battery.
* **Top Surface:** Center spindle for the record, start button and a needle arm.
* **Front Interface** Button, switch and encoder to play with.
* **Records:** 3D printed discs with a the NTAG215 sticker embedded inside. Pause print, add sticker to pocket, and finish print.

## 🚀 Getting Started

1. **Format SD Card:** FAT32, with MP3 files in a `mp3/` folder named `0001robert.mp3`, `0002pil.mp3`, etc.
2. **Flash Firmware:** Open the project in the Arduino IDE. Install the `MFRC522` and `DFRobotDFPlayerMini` libraries.
3. **Map Tags:** Hold the Boot Button while powering on to enter Maintenance Mode, which prints each scanned tag's UID to the Serial Monitor. Add those UIDs to the tag list in `getSongFromUID()`.
4. **Assemble:** Solder components to perfboard following the wiring table above.
5. **Play:** Charged battery, place a record, and press Play!

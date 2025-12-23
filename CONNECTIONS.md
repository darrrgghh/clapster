# Clapster — Hardware Wiring Guide

Here I'll try to explain how to connect all hardware components for **Clapster 1.0**.
All connections match the pin usage in `Clapster.ino`, but you are free to choose your own pins.

---

## Microcontroller

- **Arduino Uno / Nano** (or compatible)

---

## Components Used

- Analog microphone module (sound sensor)
- 2× Push buttons
- 2× Resistors (for buttons, pull-down)
- Active buzzer
- LCD display (HD44780-compatible, e.g. 16x2)
- Potentiometer (for LCD contrast)
- Jumper wires
- Breadboard
- USB cable

---

## Pin Mapping Overview

| Component            | Arduino Pin |
|----------------------|-------------|
| Microphone OUT       | A0          |
| Main button          | D2          |
| Mode switch button   | D6          |
| Buzzer (+)           | D3          |
| LCD RS               | D12         |
| LCD EN               | D11         |
| LCD D4               | D10         |
| LCD D5               | D9          |
| LCD D6               | D8          |
| LCD D7               | D7          |
| Power (VCC)          | 5V          |
| Ground (GND)         | GND         |

---

## Microphone Sensor

**Purpose:** Detect claps / hits (analog signal)

### Connections
- **OUT** → `A0`
- **VCC** → `5V`
- **GND** → `GND`

---

## Buttons (with External Resistors)

Both buttons use **external pull-down resistors**.

---

### Main Button (Program Button)

**Purpose:**
- Start / stop recording
- Confirm actions

### Connections
- One leg → `D2`
- Same leg → **resistor → GND**
- Other leg → `5V`

**Logic**
- Not pressed → `LOW`
- Pressed → `HIGH`

---

### Mode Switch Button

**Purpose:**  
Switch between **Learning Mode** and **Gaming Mode**

### Connections
- One leg → `D6`
- Same leg → **resistor → GND**
- Other leg → `5V`

**Logic**
- Not pressed → `LOW`
- Pressed → `HIGH`

---

## LCD Display (HD44780, 4-bit Mode)

**Purpose:**  
Display current mode, rhythm patterns, and system status.

---

### LCD Power & Contrast

| LCD Pin | Connection |
|--------|------------|
| VSS    | GND        |
| VDD    | 5V         |
| VO     | Potentiometer middle pin |
| RW     | GND        |

> The potentiometer controls contrast (connect its other two pins to 5V and GND).

---

### LCD Control & Data Pins

| LCD Pin | Arduino Pin |
|--------|-------------|
| RS     | D12         |
| EN     | D11         |
| D4     | D10         |
| D5     | D9          |
| D6     | D8          |
| D7     | D7          |

The LCD is used in **4-bit mode**.

---

### LCD Backlight (if present)

| LCD Pin | Connection |
|--------|------------|
| A (LED+) | 5V (via resistor if needed) |
| K (LED-) | GND |

---

## Buzzer

**Purpose:**  
Audio feedback for clap detection and game events.

### Connections
- **Positive (+)** → `D3`
- **Negative (–)** → `GND`

### Notes
- Controlled with `tone()` / `noTone()`
- Use an **active buzzer**

---

## Power

- Power the board via **3xAAA Power Module** like I did, or **USB**
- All components share a **common GND**
- Ensure correct contrast adjustment on the LCD

---

## Related Code

Pin definitions are declared in `Clapster.ino`, for example:

```cpp
const int micSensor     = A0;
const int programSwitch = 2;
const int modeSwitch    = 6;
const int buzzer        = 3;

```

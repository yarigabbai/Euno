# **ESP32-S3 Autopilot System - Complete Setup Guide**

## **1. Introduction**
This guide provides a step-by-step approach to building, programming, and operating an autopilot system based on **ESP32-S3** for actuator control and **ESP32-2432S028R (CYD - Cheap Yellow Display)** for user interaction. The system allows for automated heading correction using a compass, GPS, and actuator while enabling manual overrides and remote control through a WiFi network.

## **2. System Overview**
The autopilot system consists of two main modules:

- **ESP32-S3 Module (Autopilot Core)**:
  - Controls the actuator (linear drive motor) using **PWM** signals via the **IBT-2 motor driver**.
  - Reads heading data from a **QMC5883L digital compass**.
  - Receives GPS data from a **Beitian BN-880 GPS module**.
  - Implements an algorithm for automatic heading correction.
  - Communicates with the CYD via **UDP and serial interface**.

- **ESP32-2432S028R (CYD - Cheap Yellow Display)**:
  - Functions as a touchscreen **interface**.
  - Creates a **WiFi network** to receive external bearing commands from a phone or PC.
  - Displays real-time heading, error, and autopilot status.
  - Allows modification of parameters such as speed limits and error thresholds.
  - Sends control commands to the ESP32-S3 via UDP packets.

## **3. Hardware Components**
### **Main Components:**
- **ESP32-S3** (Autopilot control)
- **ESP32-2432S028R (CYD)** (2.8-inch TFT Touchscreen Display)
- **QMC5883L Compass Sensor** (Heading data)
- **Beitian BN-880 GPS Module** (Positioning and GPS-based heading)
- **IBT-2 Motor Driver** (Controls actuator motion)
- **12V Linear Actuator** (Physically adjusts boat heading)
- **Power Supply**: 5V for CYD, 12V for the actuator
- **Wiring & Connectors**: Jumpers, power cables, resistors as needed

## **4. Wiring and Assembly**
### **ESP32-S3 Wiring:**
- **Compass (QMC5883L):**
  - **SCL** → GPIO 8
  - **SDA** → GPIO 9
  - **VCC** → 3.3V
  - **GND** → GND
QMC sensori is included in Beitan 880, just wire.

- **GPS (BN-880):** (Beitan 220 is a cheap alternative, but it isn's provided a compass sensor)
  - **TX** → GPIO 16
  - **RX** → GPIO 17
  - **VCC** → 3.3V
  - **GND** → GND

- **Motor Driver (IBT-2):**
  - **RPWM** → GPIO 3
  - **LPWM** → GPIO 46
  - **12V Power** → External power supply

### **ESP32-2432S028R (CYD) Wiring:**
- **Power:** Connect CYD to a 5V power source.
- **Communication:** Connect CYD **via WiFi** to ESP32-S3.
- **Touchscreen Inputs:** Handled by CYD internally.

## **5. Software Setup**
### **Step 1: Install Required Software**
- Install **Arduino IDE**
- Install **ESP32 board support package** (via Boards Manager)
- Install required libraries:
  - **Wire.h** (I2C communication)
  - **EEPROM.h** (Memory storage)
  - **QMC5883LCompass.h** (Compass handling)
  - **TinyGPS++.h** (GPS handling)
  - **WiFi.h & WiFiUDP.h** (Network communication)
  - **TFT_eSPI.h** (Display control)

### **Step 2: Upload Code**
- **Load "ESP32-S3_autopilot.ino" onto ESP32-S3** (Handles actuator and heading corrections)
- **Load "ESP32_YellowDisplay.ino" onto CYD** (Manages touchscreen UI and WiFi communication)

## **6. System Functionalities**
### **Autopilot Algorithm**
- Reads heading from the **compass and GPS**.
- Computes the error between the **current heading** and the **desired heading**.
- Adjusts the actuator's speed proportionally to the error.
- Implements **error tolerance** and **smooth braking** to prevent oscillations.

### **Manual Control**
- **+1° / -1° Buttons**: Moves the actuator manually in small increments.
- **+10° / -10° Buttons**: Makes larger adjustments.
- **Toggle Button**: Turns autopilot **ON/OFF**.

### **Parameter Configuration**
- **Speed settings**: Adjusts minimum and maximum PWM power.
- **Error threshold**: Defines when corrections should start.
- **Tolerance**: Prevents unnecessary micro-adjustments.
- **Calibration Mode**: Corrects compass offsets, stored in EEPROM.

### **WiFi & External Bearing**
- **CYD creates a WiFi AP** ("ESP32_AP", "password").
- External devices (smartphone, tablet) can **send a bearing via UDP**.
- CYD **forwards the external bearing** to ESP32-S3 for correction.

## **7. User Guide**
### **Powering the System**
1. Connect 12V power to **actuator and IBT-2**.
2. Connect 5V power to **CYD and ESP32-S3**.
3. Wait for the **display to initialize**.

### **Connecting via WiFi**
1. Connect your device to the "ESP32_AP" WiFi network.
2. Open a UDP client app (e.g., "UDP Sender").
3. Send commands to **192.168.4.1, port 4210**.

### **Sending Commands**
- **ACTION:-1** → Adjust heading by -1°
- **ACTION:+1** → Adjust heading by +1°
- **ACTION:-10** → Adjust heading by -10°
- **ACTION:+10** → Adjust heading by +10°
- **ACTION:TOGGLE** → Toggle autopilot on/off
- **ACTION:CAL** → Start compass calibration
- **SET:V_max=200** → Change maximum actuator speed
- **SET:E_tol=2** → Adjust error tolerance

### **Calibration Process**
1. Press "CAL" on CYD.
2. Rotate the device in all directions for **30 seconds**.
3. System stores **compass offsets in EEPROM**.

### **Behavior Based on Speed**
- **Higher speeds → Lower actuator correction**.
- **Lower speeds → More aggressive correction**.

## **8. Debugging & Optimization**
### **Common Issues & Fixes**
| Issue | Possible Cause | Solution |
|--------|--------------|----------|
| No actuator movement | Wrong wiring | Check motor driver connections |
| Erratic heading readings | Compass uncalibrated | Perform calibration |
| GPS not working | No satellite fix | Move to an open area |
| WiFi connection lost | Weak signal | Reduce interference |

## **9. Conclusion**
This guide provides everything needed to **build, program, and operate** the ESP32 autopilot system. With the ability to accept external bearings, manually override controls, and fine-tune navigation parameters, this system is versatile and highly customizable for different use cases.


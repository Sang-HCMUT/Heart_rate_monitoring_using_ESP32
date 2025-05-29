# Heart Rate Monitor

A heart rate monitoring system built with ESP32, Pulse Sensor, OLED display, and a web server for real-time monitoring.

## Overview
This project implements a heart rate monitoring system using:
- **ESP32**: Main microcontroller for processing sensor data and hosting a web server.
- **Pulse Sensor**: Measures heart rate via optical method.
- **OLED Display (SSD1306)**: Displays IP address and BPM locally.
- **Web Server**: Provides a web interface to monitor BPM and download data as CSV.

## Features
- Real-time heart rate measurement (60–89 BPM range).
- Local display on OLED (128x64, I2C).
- Web interface for remote monitoring with history table.
- Data export in CSV format.
- WiFi AP mode (SSID: HeartRateMonitor, IP: 192.168.4.1).

## Hardware Requirements
- ESP32 NodeMCU
- Pulse Sensor V1706
- OLED Display (0.96" I2C, SSD1306)
- Breadboard and jumper wires

## Software Requirements
- Arduino IDE
- Libraries: `WiFi.h`, `WebServer.h`, `Wire.h`, `Adafruit_GFX.h`, `Adafruit_SSD1306.h`, `ArduinoJson.h`

## Setup Instructions
1. Connect the hardware as per the block diagram in the documentation.
2. Install required libraries in Arduino IDE.
3. Upload `src/main.c` to the ESP32.
4. Connect to the WiFi AP (SSID: HeartRateMonitor, Password: 12345678).
5. Access the web interface at `http://192.168.4.1`.

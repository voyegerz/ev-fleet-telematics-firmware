# EV Fleet Telematics - Project Context

## Project Overview
**Goal:** Remote tracking and operation of an EV fleet module using ESP32 over 2G GPRS.
**Architecture:** ESP32 (Client) <--> SIM800L (GPRS) <--> Custom Flask Server (Render.com).

## Hardware Configuration
*   **MCU:** ESP32 Dev Module (Board: `esp32dev`)
*   **GSM:** SIM800L (UART2: TX=17, RX=16) - **Requires 4V 2A external power**
*   **GPS:** NEO-6M (UART1: TX=27, RX=26)
*   **Actuators:** 
    *   Relay (GPIO 18)
    *   LED (GPIO 19)

## Firmware (Phase 1)
*   **Framework:** Arduino
*   **Build System:** PlatformIO
*   **Key Libraries:** TinyGSM, TinyGPS++, ArduinoHttpClient, ArduinoJson
*   **Status:** Initial firmware created. Implements 10s polling loop.

## Server (Phase 2)
*   **Stack:** Python Flask
*   **Hosting:** Render.com
*   **Status:** Pending implementation.

## Commands
*   **Build:** `pio run`
*   **Upload:** `pio run --target upload`
*   **Monitor:** `pio device monitor`

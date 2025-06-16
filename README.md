# WisBlock NB-IoT Integration

This project is based on [RAKwireless WisBlock](https://docs.rakwireless.com/) modules, specifically tailored to integrate and enable **NB-IoT connectivity** via the **RAK5860** module.

## Hardware Used

- **RAK4630** – Core module (nRF52840 MCU with LoRa)
- **RAK19007** – WisBlock Base Board
- **RAK5860** – NB-IoT module (based on Quectel BG77)
- **RAK12027** – Seismic/accelerometer sensor
- **Battery** – Rechargeable lithium battery (optional)
- **Antenna** – NB-IoT + GPS (for RAK5860)

## Purpose

This repository contains firmware code and configuration to:

- Read sensor data (e.g., seismic readings from RAK12027)
- Initialize and manage NB-IoT connectivity
- Send sensor data via MQTT (or UDP/TCP, depending on firmware)
- Enable low-power operation and cellular reconnect logic

## RAK Code Base

The core of this project was **adapted from the official RAK example codes**, available on:

- [RAKwireless WisBlock Examples](https://github.com/RAKWireless/WisBlock)

### Key RAK References:
- Sensor interfacing (RAK12027): [docs.rakwireless.com](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK12027/)
- Cellular modem commands (BG77 AT Commands): [Quectel BG77 documentation](https://www.quectel.com/)

---

## My Adjustments

I made several modifications to tailor the codebase to **NB-IoT usage**, specifically:

- Added and configured AT command sequences for **BG77 modem** startup and connectivity.
- Customized **MQTT uplink logic** for NB-IoT network reliability (including retries and signal quality checks).
- Adjusted **power-saving configurations**, optimized for NB-IoT idle state.
- Improved **serial communication stability** between RAK4630 and RAK5860.
- Added optional debug outputs via UART.

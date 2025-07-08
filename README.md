# ssdv_over_aprs
A microcontroller-based implementation of SSDV encoding and packet transmission over amateur radio APRS. Ideal for high-altitude balloon imaging, space payloads, or ham radio experimentation.

*Built specifically for*:
**MCU:** ATmega2560
**Camera:** Adafruit VC0706 (TTL JPEG Camera)
**Transmission:** APRS-compatible modem or module (tested with DRA818V)

This is not a generic SSDV encoder — it is **hardware-specific**, **MCU-aware**, and designed for **space/balloon payloads** where lightweight, reliable image transfer is key.

*Features:*
- Captures JPEG snapshots using VC0706 UART protocol
- Converts images to SSDV format (via Phil Heron’s SSDV spec)
- Optimized for 256-byte SSDV chunking (accounts for JPEG reset markers)
- Supports optional telemetry (call sign, packet index)


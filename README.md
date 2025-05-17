# ESP32 WiFi Manager

A simple and efficient WiFi configuration manager for ESP32 devices that allows for easy WiFi setup through a web interface.

## Features

- **Dual Mode Operation**:
  - Station (STA) mode for normal operation
  - Access Point (AP) mode for configuration
- **Persistent Storage**:
  - Saves WiFi credentials to SPIFFS
  - Stores network configuration (IP, Gateway)
- **Web Interface**:
  - Easy-to-use configuration portal
  - Supports custom IP and gateway settings
- **Auto-Recovery**:
  - Falls back to AP mode if WiFi connection fails
  - Auto-restarts after new configuration

## Hardware Requirements

- ESP32 Development Board
- USB Cable for programming

## Software Dependencies

- ESP32 Arduino Core
- SPIFFS filesystem
- ESPAsyncWebServer library
- WiFi library

## How It Works

1. **Initial Boot**:
   - Device tries to connect to previously configured WiFi
   - If successful, operates in normal mode
   - If failed, switches to configuration mode

2. **Configuration Mode**:
   - Creates an Access Point named "WASA Grw"
   - Default Password: "12345678"
   - Access configuration page at 192.168.4.1

3. **Configuration Options**:
   - WiFi SSID
   - WiFi Password
   - Custom IP Address
   - Gateway Address

## Usage

1. **First Time Setup**:
   - Power on the device
   - Connect to "WASA Grw" WiFi network
   - Navigate to 192.168.4.1 in a web browser
   - Enter your WiFi credentials
   - Device will restart and connect to your network

2. **Normal Operation**:
   - Device connects to configured WiFi
   - Serves status page on local IP address

## File Structure

```
ESP32_WiFiManager/
├── data/
│   └── wifimanager.html    # Configuration portal webpage
├── src/
│   └── main.cpp            # Main application code
└── platformio.ini          # PlatformIO configuration
```

## Development

This project is built using PlatformIO. To modify or upload:

1. Clone the repository
2. Open in PlatformIO
3. Upload filesystem image (for web interface)
4. Upload the code

## Reset Configuration

To reset WiFi settings, you need to clear the SPIFFS storage. This can be done by:
1. Uploading an empty filesystem image
2. The device will then return to AP mode for reconfiguration

## License

This project is open-source and available under the MIT License.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

# ESP32 WiFi Manager

A smart and efficient WiFi configuration manager for ESP32 devices that supports multiple networks and provides an easy-to-use web interface for setup.

## Features

- **Multi-Network Support**:
  - Store up to 5 different WiFi networks
  - Automatic connection to available networks
  - Seamless failover between networks
- **Dual Mode Operation**:
  - Station (STA) mode for normal operation
  - Access Point (AP) mode for configuration
- **Persistent Storage**:
  - Saves multiple WiFi credentials using Preferences
  - Secure storage of network configurations
- **Web Interface**:
  - Easy-to-use configuration portal
  - Add new networks without losing existing ones
  - Supports custom IP and gateway settings
- **Smart Network Management**:
  - Automatic connection to strongest available network
  - Falls back to AP mode if no networks are available
  - Smart reconnection with failover support
- **Auto-Recovery**:
  - Attempts reconnection to all saved networks
  - Falls back to configuration portal as last resort
  - Auto-restarts after new configuration

## Hardware Requirements

- ESP32 Development Board
- USB Cable for programming

## Software Dependencies

- ESP32 Arduino Core
- WiFiManager library
- WiFiMulti library
- ESP32 Preferences library (built-in)

## How It Works

1. **Initial Boot**:
   - Device loads saved network credentials from permanent storage
   - Attempts to connect to any available saved network
   - If successful, operates in normal mode
   - If no networks available, switches to configuration mode

2. **Configuration Mode**:
   - Creates an Access Point named "WASA Grw"
   - Default Password: "12345678"
   - Access configuration page at 192.168.4.1
   - New networks are added to existing saved networks

3. **Smart Network Management**:
   - Continuously monitors WiFi connection status
   - Automatically tries all saved networks when disconnected
   - Maintains up to 5 different network configurations
   - Connects to the strongest available network

4. **Configuration Options**:
   - WiFi SSID and Password
   - Stores multiple network credentials
   - Custom IP Address
   - Gateway Address

## Usage

1. **First Time Setup**:
   - Power on the device
   - Connect to "WASA Grw" WiFi network
   - Navigate to 192.168.4.1 in a web browser
   - Enter your primary WiFi credentials
   - Device will save credentials and connect to your network

2. **Adding Additional Networks**:
   - Press reset button or power cycle the device
   - Wait for connection attempt to existing networks
   - If no connection is possible, the AP mode will start
   - Connect to "WASA Grw" WiFi network again
   - Add another network through the portal
   - Up to 5 networks can be stored

3. **Normal Operation**:
   - Device automatically connects to available known networks
   - Seamlessly switches between networks if one becomes unavailable
   - Shows current connection status via Serial monitor
   - Automatically manages network failover

4. **Troubleshooting**:
   - If no saved networks are available, device enters AP mode
   - Connection attempts timeout after 10 seconds
   - Device will try all saved networks before entering AP mode
   - Serial monitor (115200 baud) shows detailed connection status

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

To reset all WiFi settings and clear saved networks:

1. **Using Code**:
   - Uncomment the line `// wm.resetSettings();` in setup()
   - Upload the code
   - The device will clear all saved networks
   - After restart, it will enter AP mode

2. **Using Preferences**:
   - The WiFi credentials are stored in the "wifi_creds" namespace
   - Use ESP32 Preferences clear() function to reset
   - Or flash a new firmware to completely reset the device

3. **Manual Reset**:
   - Power cycle the device
   - When AP mode starts, any new configuration will be added to existing networks
   - Up to 5 networks can be stored before oldest are overwritten

## License

This project is open-source and available under the MIT License.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

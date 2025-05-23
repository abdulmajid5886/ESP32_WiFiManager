# ESP32 WiFi Manager with Trip Logger

A smart WiFi configuration manager for ESP32 devices with integrated trip logging functionality, featuring RTC-based time tracking, SD card storage, and Firebase synchronization.

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
- **Trip Logging System**:
  - Real-time trip tracking with RTC
  - Local storage on SD card
  - Automatic Firebase synchronization
  - Offline operation capability
  - Break time tracking between trips
- **Data Synchronization**:
  - Trip data logging every 30 seconds to SD card
  - Automatic 5-minute interval Firebase updates
  - Independent logging and sync intervals
  - Automatic retry for failed uploads
  - Status tracking for each trip
  - Persistent storage of unsent data
  - WiFi-dependent sync mechanism
  - Offline operation with SD card backup

## Hardware Requirements

- ESP32 Development Board
- DS3231 RTC Module
- SD Card Module
- Status LEDs (2x)
- USB Cable for programming

## Pin Configuration

### Core Connections
```
RTC_SDA: GPIO21
RTC_SCL: GPIO22
SD_CS:   GPIO5
```

### LED Indicators
```
POWER_LED:      GPIO26  // Power indication
SD_FAULT_LED:   GPIO25  // SD Card fault indicator
RTC_FAULT_LED:  GPIO33  // RTC/Timer fault indicator
WIFI_STATUS_LED: GPIO32 // WiFi connectivity status
FIREBASE_LED:   GPIO35  // Firebase data publish status
```

### LED Status Guide
- **Power LED (GPIO26)**
  - ON: Device is powered and running
  - OFF: No power

- **SD Card Fault LED (GPIO25)**
  - OFF: SD card working normally
  - ON: SD card error or not detected

- **RTC Fault LED (GPIO33)**
  - OFF: RTC working normally
  - ON: RTC error or not detected

- **WiFi Status LED (GPIO32)**
  - ON: Connected to WiFi
  - OFF: Disconnected from WiFi
  - BLINKING: Attempting to connect

- **Firebase LED (GPIO35)**
  - OFF: Idle
  - ON: Attempting to publish data
  - BLINK: Successful data publish
  - RAPID BLINK: Failed to publish

## Software Dependencies

- ESP32 Arduino Core
- WiFiManager library
- WiFiMulti library
- ESP32 Preferences library
- RTClib
- SD library
- Firebase ESP32 Client

## Firebase Setup

1. **Create Firebase Project**:
   - Go to Firebase Console (https://console.firebase.google.com)
   - Create a new project or select an existing one
   - Enable Realtime Database
   - Set up database rules for security
   - Get your database URL and API key

2. **Configure Firebase Credentials**:
   - Open platformio.ini in your project
   - Update the following build flags:
     ```ini
     build_flags = 
         '-DFIREBASE_DATABASE_URL="your-database-url"'
         '-DFIREBASE_API_KEY="your-api-key"'
     ```
   Note: Replace "your-api-key" with your actual Firebase API key. The API key should be kept secure and never shared publicly.

3. **Database Rules Setup**:
   - In Firebase Console, go to Realtime Database > Rules
   - Configure appropriate read/write permissions
   - Example basic rules for development:
     ```json
     {
       "rules": {
         ".read": true,
         ".write": true
       }
     }
     ```
   Note: For production, implement proper authentication and security rules.

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

4. **Connectivity Behavior**:
   - Checks WiFi connection every 5 minutes
   - During connection loss:
     * Continues logging trips to SD card
     * Marks new trips as unsynced (status = 0)
     * Queues data for future synchronization
   - Upon reconnection:
     * Automatically resumes Firebase synchronization
     * Syncs all pending (unsynced) records
     * Updates sync status for successful uploads
   - Portal Activation:
     * Starts config portal if all connection attempts fail
     * Portal timeout after 3 minutes
     * Returns to normal operation after new network added

5. **Trip Logging System**:
   - Initializes RTC and SD card
   - Creates/opens trip log file
   - Records trip start time
   - Calculates and logs break duration
   - Generates unique trip numbers

5. **Data Storage Flow**:
   - Records trip data every 30 seconds consistently
   - Stores data on SD card with sync status (0 = unsynced)
   - Attempts immediate Firebase upload
   - Updates sync status to 1 when upload succeeds
   - Maintains data integrity even during power loss

6. **Firebase Synchronization**:
   - Attempts upload every 5 minutes when WiFi is available
   - Only processes records marked as unsynced (status = 0)
   - Updates sync status to 1 after successful upload
   - Retries failed uploads in next sync cycle
   - Prevents duplicate uploads with sync status tracking

7. **Error Handling and Recovery**:
   - LED indicators for hardware faults:
     * RTC_FAULT_LED (GPIO33) for RTC issues
     * SD_FAULT_LED (GPIO25) for SD card problems
   - Offline operation capability:
     * Continues logging to SD card when offline
     * Maintains sync status for each record
     * No data loss during connection issues
   - Automatic error recovery:
     * Retries failed network connections
     * Attempts to reconnect to all saved networks
     * Falls back to config portal if needed
   - Data integrity protection:
     * Safe file updates using temporary files
     * Atomic operations for sync status updates
     * Verification of written data
     * Persistent storage of sync state

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

5. **RTC Issues**:
   - Check if RTC_FAULT_LED is lit
   - Verify I2C connections
   - Check RTC battery

6. **SD Card Issues**:
   - Check if SD_FAULT_LED is lit
   - Verify card is properly formatted (FAT32)
   - Check SPI connections

7. **Firebase Sync Issues**:
   - Check WiFi connectivity
   - Verify Firebase credentials
   - Monitor serial output for error messages

8. **Data Recovery**:
   - All trip data is stored on SD card
   - Firebase sync will retry automatically
   - Manual data export possible via SD card

## File Structure

```
ESP32_WiFiManager/
├── data/
│   └── wifimanager.html    # Configuration portal webpage
├── src/
│   └── main.cpp            # Main application code
├── platformio.ini          # PlatformIO configuration and Firebase credentials
└── README.md              # Project documentation
```

## Trip Log Format

The trip log file (trip_log.csv) contains the following columns:
```
Trip No., Start DateTime, End DateTime, Duration, Break Time, Synced
```

Example:
```
1, 23-05-18 10:00:00, 23-05-18 10:30:00, 00:30:00, 00:00:00, 1
Trip 1 Duration:, 00:30:00
Break Time:, 00:15:00
2, 23-05-18 10:45:00, 23-05-18 11:15:00, 00:30:00, 00:15:00, 0
```

The 'Synced' column indicates:
- 0: Trip data not yet uploaded to Firebase
- 1: Trip successfully synchronized with Firebase

## Firebase Data Structure

Trips are stored in Firebase with the following structure:
```json
{
  "trips": {
    "1": {
      "tripNumber": 1,
      "startTime": "23-05-18 10:00:00",
      "endTime": "23-05-18 10:30:00",
      "duration": "00:30:00",
      "breakTime": "00:15:00",
      "status": "OK",
      "uploadTimestamp": 1684441200
    }
  }
}
```

## Development

1. **Initial Setup**:
   ```bash
   # Clone repository
   git clone [repository-url]
   cd ESP32_WiFiManager

   # Install dependencies (using PlatformIO)
   pio pkg install
   ```

2. **Configure Firebase**:
   - Update platformio.ini with your Firebase credentials:
     ```ini
     build_flags = 
         '-DFIREBASE_DATABASE_URL="your-database-url"'
         '-DFIREBASE_API_KEY="your-api-key"'
     ```

3. **Hardware Setup**:
   - Connect RTC to SDA (GPIO21) and SCL (GPIO22)
   - Connect SD card module to GPIO5 (CS)
   - Connect status LEDs to GPIO33 and GPIO25

4. **Upload Code**:
   ```bash
   # Upload filesystem
   pio run -t uploadfs
   
   # Upload code
   pio run -t upload
   ```

5. **Monitoring**:
   ```bash
   # Monitor serial output
   pio device monitor
   ```

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

## Debugging

The system provides comprehensive debugging information through:

1. **Serial Monitor Output**:
   - Baud Rate: 115200
   - Prefixed logs for easy filtering:
     - `[RTC]`: RTC-related operations
     - `[SD]`: SD card operations
     - `[WiFi]`: WiFi connection status
     - `[NTP]`: Time synchronization
     - `[Firebase]`: Database operations
     - `[Sync]`: Data synchronization status
     - `[Logger]`: Trip logging information

2. **LED Indicators**:
   - Visual feedback for all major subsystems
   - Quick status assessment without serial monitor
   - Fault indication for hardware issues

## Troubleshooting

### Common Issues

1. **Device Resets**
   - Check power supply stability
   - Ensure proper initialization of all components
   - Monitor serial output for watchdog triggers

2. **WiFi Connection Issues**
   - Check WiFi LED status
   - Verify saved network credentials
   - Monitor connection attempts in serial output
   - Check if the network is in range

3. **Data Logging Issues**
   - Check SD Card LED for hardware faults
   - Verify SD card is properly formatted (FAT32)
   - Check available space on SD card
   - Monitor serial output for write errors

4. **Time Synchronization Issues**
   - Check RTC LED status
   - Verify internet connectivity for NTP
   - Check serial output for sync attempts
   - Verify RTC battery voltage

5. **Firebase Sync Issues**
   - Monitor Firebase LED behavior
   - Check internet connectivity
   - Verify Firebase credentials
   - Check serial output for detailed error messages

### LED Pattern Guide

1. **Normal Operation**
   - Power LED: Solid ON
   - All fault LEDs: OFF
   - WiFi LED: Solid ON
   - Firebase LED: Occasional blinks during sync

2. **Error States**
   - Fault LEDs ON: Check corresponding hardware
   - WiFi LED OFF: Connection issues
   - Firebase LED rapid blink: Sync failures

## Maintenance

1. **Regular Checks**
   - Monitor SD card space
   - Check RTC battery voltage
   - Verify Firebase quota usage
   - Review system logs

2. **Periodic Tasks**
   - Backup SD card data
   - Clean up old Firebase records
   - Update WiFi credentials if changed
   - Check for firmware updates

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of conduct, and the process for submitting pull requests.

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details


## realtime database structure

/{deviceName}/                   # Device name as top level
    ├── status/                 # Device status information
    │   ├── status             # "Online"/"Offline"
    │   ├── lastConnection     # Timestamp
    │   ├── ip                 # Current IP address
    │   ├── ssid               # Connected WiFi network
    │   ├── rssi              # Signal strength
    │   └── uptime            # Device uptime in seconds
    │
    └── trips/                 # All trips for this device
        ├── 1/                 # Trip data by number
        │   ├── deviceName     
        │   ├── tripNumber
        │   ├── startTime
        │   ├── endTime
        │   ├── duration
        │   ├── breakTime
        │   ├── status
        │   └── uploadTimestamp
        ├── 2/
        └── .../
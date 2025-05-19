# ESP32 WiFi Manager with Trip Logger

A smart WiFi configuration manager for ESP32 devices with integrated trip logging functionality, featuring RTC-based time tracking, SD card storage, and Firebase synchronization with enhanced data integrity and error recovery mechanisms. The system ensures zero data loss through multiple backup systems and transaction-based synchronization.

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
  - Independent SD card logging and Firebase sync
  - Trip data logged every 30 seconds to SD card
  - Firebase sync attempts every 5 minutes
  - Break time tracking and synchronization
  - Automatic retry for failed uploads
  - Status tracking for each trip
  - Persistent storage of unsent data
  - WiFi-independent operation with SD card
  - Queued synchronization when offline

## Hardware Requirements

- ESP32 Development Board
- DS3231 RTC Module
- SD Card Module
- Status LEDs (2x)
- USB Cable for programming

## Pin Configuration

```
RTC_SDA: GPIO21
RTC_SCL: GPIO22
SD_CS:   GPIO5
RTC_FAULT_LED: GPIO33
SD_FAULT_LED:  GPIO25
```

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
   - Go to Firebase Console
   - Create a new project
   - Enable Realtime Database
   - Get your database URL and API key

2. **Configure Firebase Credentials**:
   - Open platformio.ini
   - Update FIREBASE_DATABASE_URL
   - Update FIREBASE_API_KEY

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

4. **Trip Logging System**:
   - Initializes RTC and SD card
   - Creates/opens trip log file
   - Records trip start time
   - Calculates and logs break duration
   - Generates unique trip numbers

5. **Data Storage Flow**:
   - Records trip data every 30 seconds on SD card
   - Independent Firebase sync process
   - Prioritizes reliable local storage
   - Queues data for later sync when offline
   - Includes break time calculations
   - Maintains data integrity during power cycles

6. **Firebase Synchronization**:
   - Attempts upload every 5 minutes
   - Marks successful uploads as "OK"
   - Retries failed uploads automatically
   - Maintains sync queue in memory

7. **Error Handling**:
   - LED indicators for hardware faults
   - Offline operation capability
   - Automatic error recovery
   - Data integrity protection

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

## Data Integrity and Error Recovery

1. **SD Card Data Protection**:
   - Write verification after each operation
   - Multiple retry attempts for failed writes
   - Automatic hourly backups
   - Keeps last 3 backup copies
   - Visual LED indication for SD card issues
   - In-memory buffering for write failures

2. **Firebase Data Protection**:
   - Transaction-based data synchronization
   - Pending transactions tracking
   - Automatic retry mechanism
   - Status tracking for each upload
   - Incremental retry counter
   - Transaction cleanup after successful sync

3. **Power Failure Protection**:
   - Immediate local data persistence
   - Multiple backup copies
   - Transaction system for uploads
   - Automatic recovery on power restore
   - No data loss during power cycles

4. **Network Failure Recovery**:
   - Local-first data storage
   - Queued uploads for offline operation
   - Automatic retry when network available
   - Status tracking for failed uploads
   - Multi-network failover support

5. **Error Handling and Monitoring**:
   - Visual LED indicators for hardware status
   - Detailed serial logging
   - Error condition tracking
   - Automatic error recovery
   - Clear error reporting

6. **Data Recovery Options**:
   - Multiple backup files
   - Manual data export via SD card
   - Transaction log recovery
   - Automated consistency checks
   - Recovery from partial writes

## Trip Log Format

The trip log file (trip_log.csv) contains the following columns:
```
Trip No., Start DateTime, End DateTime, Duration, Break Time
```

Example:
```
----------------------------------------
1, 23-05-18 10:00:00, 23-05-18 10:30:00, 00:30:00, 00:00:00
Trip 1 Duration:, 00:30:00
Break Time:, 00:15:00
----------------------------------------
2, 23-05-18 10:45:00, 23-05-18 11:15:00, 00:30:00, 00:15:00
```

## Backup System

1. **Automatic Backups**:
   - Hourly backup creation
   - Rolling backup system
   - Maintains last 3 copies
   - Integrity verification
   - Recovery procedures

2. **Backup Naming Convention**:
   ```
   /backup_[timestamp].csv
   ```

3. **Recovery Process**:
   - Automatic detection of corrupted files
   - Rollback to last valid backup
   - Transaction log replay
   - Data consistency verification
   - Automatic backup after recovery

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
      "breakTime": "00:00:00",
      "status": "completed",
      "uploadTimestamp": 1684404000
    }
  },
  "transactions": {
    "1684404000_1": {
      "tripNumber": 1,
      "status": "pending",
      "retryCount": 0,
      "timestamp": 1684404000
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

## License

This project is open-source and available under the MIT License.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

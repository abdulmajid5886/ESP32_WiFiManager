#include <WiFiManager.h>
#include <WiFiMulti.h>
#include <Preferences.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <time.h>

// Pin definitions
#define RTC_SDA 21
#define RTC_SCL 22
#define SD_CS 5

// LED Pin definitions
#define POWER_LED 26        // Power indication LED
#define SD_FAULT_LED 25     // SD Card fault LED
#define RTC_FAULT_LED 33    // RTC/Timer fault LED
#define WIFI_STATUS_LED 32  // WiFi connectivity LED
#define FIREBASE_LED 35     // Firebase data publish LED

// Device Identification
#define DEFAULT_DEVICE_NAME "WASA-Grw"  // Default device name
const char* DEVICE_NAME_KEY = "WG001";  // Key for storing device name in preferences
String deviceName;

// WiFiManager object
WiFiManager wm;
WiFiMulti wifiMulti;
Preferences preferences;

// RTC and SD objects
RTC_DS3231 rtc;
File logFile;

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Timing intervals and status
unsigned long lastFirebaseSync = 0;
unsigned long lastDataLog = 0;
unsigned long lastStatusUpdate = 0;  // For device status updates
unsigned long deviceStartTime = 0;   // For tracking uptime
const unsigned long FIREBASE_SYNC_INTERVAL = 60000; // 5 minutes for Firebase sync
const unsigned long DATA_LOG_INTERVAL = 30000;  // 30 seconds for data logging
bool firebaseInitialized = false;

// Constants for preferences and logging
const char* PREF_NAMESPACE = "wifi_creds";
const int MAX_NETWORKS = 5;  // Maximum number of networks to store
const char* filename = "/trip_log.csv";
const unsigned long STATUS_UPDATE_INTERVAL = 30000; // Update device status every 30 seconds

// Trip logging variables
DateTime tripStartTime;
DateTime lastEndTime;
unsigned long lastLogMillis = 0;
int tripNumber = 0;
bool rtcOK = false, sdOK = false;
bool firstLog = true;

// Modified trip logging variables to include sync status
struct TripData {
    int number;
    String startTime;
    String endTime;
    String duration;
    String breakTime;  // Added break time field
    bool synced;
};

std::vector<TripData> pendingTrips;

// NTP Server settings
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 18000;  // Replace with your timezone offset in seconds (e.g., IST = UTC+5:00 = 5.0*3600)
const int   daylightOffset_sec = 0;

// Format DateTime as YY-MM-DD HH:MM:SS
String formatDateTime(const DateTime& dt) {
    char buf[20];
    sprintf(buf, "%02d-%02d-%02d %02d:%02d:%02d", dt.year() % 100, dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
    return String(buf);
}

// Format Duration as HH:MM:SS
String formatDuration(const TimeSpan& ts) {
    char buf[10];
    sprintf(buf, "%02d:%02d:%02d", ts.hours(), ts.minutes() % 60, ts.seconds() % 60);
    return String(buf);
}

// Initialize device name from preferences or set default
void initializeDeviceName() {
    preferences.begin(PREF_NAMESPACE, false);
    deviceName = preferences.getString(DEVICE_NAME_KEY, "");
    if (deviceName.length() == 0) {
        // Generate a unique device name using part of MAC address
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char defaultName[20];
        sprintf(defaultName, "%s_%02X%02X", DEFAULT_DEVICE_NAME, mac[4], mac[5]);
        deviceName = String(defaultName);
        preferences.putString(DEVICE_NAME_KEY, deviceName);
    }
    preferences.end();
    Serial.printf("Device Name: %s\n", deviceName.c_str());
}

// Update device status in Firebase
void updateDeviceStatus() {
    if (!Firebase.ready() || WiFi.status() != WL_CONNECTED) {
        return;
    }

    FirebaseJson json;
    json.set("status", "Online");
    json.set("lastConnection", rtc.now().unixtime());
    json.set("ip", WiFi.localIP().toString());
    json.set("ssid", WiFi.SSID());
    json.set("rssi", WiFi.RSSI());
    json.set("uptime", (millis() - deviceStartTime) / 1000);  // Uptime in seconds

    String statusPath = deviceName + "/status";
    if (Firebase.RTDB.setJSON(&fbdo, statusPath.c_str(), &json)) {
        Serial.println("Status updated successfully");
    } else {
        Serial.printf("Status update failed: %s\n", fbdo.errorReason().c_str());
    }
}

// Parse "yy-mm-dd hh:mm:ss" into DateTime
DateTime parseDateTime(String dtStr) {
    int yy = dtStr.substring(0, 2).toInt();
    int mm = dtStr.substring(3, 5).toInt();
    int dd = dtStr.substring(6, 8).toInt();
    int hh = dtStr.substring(9, 11).toInt();
    int mi = dtStr.substring(12, 14).toInt();
    int ss = dtStr.substring(15, 17).toInt();
    return DateTime(2000 + yy, mm, dd, hh, mi, ss);
}

// Get last trip number from file
int getLastTripNumber() {
    int lastTrip = 0;
    File file = SD.open(filename, FILE_READ);
    if (!file) return 0;

    while (file.available()) {
        String line = file.readStringUntil('\n');
        yield(); // Allow watchdog reset
        if (line.length() > 0 && isDigit(line.charAt(0))) {
            int comma = line.indexOf(',');
            if (comma > 0) {
                int trip = line.substring(0, comma).toInt();
                if (trip > lastTrip) lastTrip = trip;
            }
        }
    }
    file.close();
    return lastTrip;
}

// Get last trip's end time
DateTime getLastEndTime() {
    File file = SD.open(filename, FILE_READ);
    if (!file) return DateTime((uint32_t)0);

    String lastLine;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        yield();
        if (line.length() > 0 && isDigit(line.charAt(0))) {
            lastLine = line;
        }
    }
    file.close();

    int secondComma = lastLine.indexOf(',', lastLine.indexOf(',') + 1);
    int thirdComma = lastLine.indexOf(',', secondComma + 1);
    if (secondComma < 0 || thirdComma < 0) return DateTime((uint32_t)0);

    String endTimeStr = lastLine.substring(secondComma + 1, thirdComma);
    return parseDateTime(endTimeStr);
}

// Get last trip's start time
DateTime getLastStartTime() {
    File file = SD.open(filename, FILE_READ);
    if (!file) return DateTime((uint32_t)0);

    String lastLine;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        yield();
        if (line.length() > 0 && isDigit(line.charAt(0))) {
            lastLine = line;
        }
    }
    file.close();

    int firstComma = lastLine.indexOf(',');
    int secondComma = lastLine.indexOf(',', firstComma + 1);
    if (firstComma < 0 || secondComma < 0) return DateTime((uint32_t)0);

    String startTimeStr = lastLine.substring(firstComma + 1, secondComma);
    return parseDateTime(startTimeStr);
}

void saveConfigCallback() {
    Serial.println("Configuration saved");
    
    // Save the new network credentials
    String ssid = WiFi.SSID();
    String password = WiFi.psk();
    
    preferences.begin(PREF_NAMESPACE, false);
    
    // Get the current count of saved networks
    int networkCount = preferences.getInt("networkCount", 0);
    
    // Only save if we haven't reached the maximum
    if (networkCount < MAX_NETWORKS) {
        char ssidKey[15];
        char passwordKey[15];
        sprintf(ssidKey, "ssid%d", networkCount);
        sprintf(passwordKey, "pass%d", networkCount);
        
        preferences.putString(ssidKey, ssid);
        preferences.putString(passwordKey, password);
        preferences.putInt("networkCount", networkCount + 1);
        
        Serial.printf("Saved new network #%d: %s\n", networkCount + 1, ssid.c_str());
    }
    
    preferences.end();
    
    // Add the network to WiFiMulti
    wifiMulti.addAP(ssid.c_str(), password.c_str());
}

void initializeRTCAndSD() {
    pinMode(RTC_FAULT_LED, OUTPUT);
    pinMode(SD_FAULT_LED, OUTPUT);
    digitalWrite(RTC_FAULT_LED, LOW);
    digitalWrite(SD_FAULT_LED, LOW);

    Wire.begin(RTC_SDA, RTC_SCL);

    // RTC Initialization
    Serial.println("\n[RTC] Initializing RTC module...");
    if (!rtc.begin()) {
        Serial.println("[RTC] ERROR: Module not found!");
        Serial.println("[RTC] Please check wiring on pins SDA:" + String(RTC_SDA) + " SCL:" + String(RTC_SCL));
        digitalWrite(RTC_FAULT_LED, HIGH);
    } else {
        rtcOK = true;
        Serial.println("[RTC] Module initialized successfully");
        Serial.printf("[RTC] Current time: %s\n", formatDateTime(rtc.now()).c_str());
    }

    // SD Initialization
    Serial.println("\n[SD] Initializing SD card...");
    if (!SD.begin(SD_CS)) {
        Serial.println("[SD] ERROR: Card Mount Failed!");
        Serial.println("[SD] Please check wiring on pin CS:" + String(SD_CS));
        digitalWrite(SD_FAULT_LED, HIGH);
    } else {
        sdOK = true;
        Serial.println("[SD] Card mounted successfully");
        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        Serial.printf("[SD] Card Size: %lluMB\n", cardSize);
    }

    if (rtcOK && sdOK) {
        tripNumber = getLastTripNumber() + 1;
        lastEndTime = getLastEndTime(); // Get break time reference
        tripStartTime = rtc.now();

        if (lastEndTime.unixtime() > 0) {
            TimeSpan breakDuration = tripStartTime - lastEndTime;
            TimeSpan tripDuration = lastEndTime - getLastStartTime();

            logFile = SD.open(filename, FILE_APPEND);
            if (logFile) {
                logFile.printf("Trip %d Duration:, %s\n", tripNumber - 1, formatDuration(tripDuration).c_str());
                logFile.printf("Break Time:, %s\n", formatDuration(breakDuration).c_str());
                logFile.close();
            }
        }

        // Write CSV header if file is empty
        if (!SD.exists(filename) || SD.open(filename, FILE_READ).size() == 0) {
            logFile = SD.open(filename, FILE_WRITE);
            if (logFile) {
                logFile.println("Trip No.,Start DateTime,End DateTime,Duration,Break Time,Synced");
                logFile.close();
            }
        }

        Serial.printf("Trip %d started at: %s\n", tripNumber, formatDateTime(tripStartTime).c_str());
    }
}

// Function to initialize all LEDs
void initializeLEDs() {
    pinMode(POWER_LED, OUTPUT);
    pinMode(SD_FAULT_LED, OUTPUT);
    pinMode(RTC_FAULT_LED, OUTPUT);
    pinMode(WIFI_STATUS_LED, OUTPUT);
    pinMode(FIREBASE_LED, OUTPUT);

    // Set initial LED states
    digitalWrite(POWER_LED, HIGH);      // Power ON
    digitalWrite(SD_FAULT_LED, LOW);    // No fault initially
    digitalWrite(RTC_FAULT_LED, LOW);   // No fault initially
    digitalWrite(WIFI_STATUS_LED, LOW); // Disconnected initially
    digitalWrite(FIREBASE_LED, LOW);    // No publishing initially
}

void syncTimeWithNTP() {
    Serial.println("[NTP] Configuring time server...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    Serial.println("[NTP] Waiting for time sync...");
    time_t now = time(nullptr);
    int retries = 10;
    while (now < 24 * 3600 && retries-- > 0) {
        Serial.printf("[NTP] Attempt %d/10, timestamp: %ld\n", 10-retries, now);
        delay(1000);
        now = time(nullptr);
    }
    Serial.println();

    if (now > 24 * 3600) {
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        
        // Set RTC time from NTP
        rtc.adjust(DateTime(
            timeinfo.tm_year + 1900,
            timeinfo.tm_mon + 1,
            timeinfo.tm_mday,
            timeinfo.tm_hour,
            timeinfo.tm_min,
            timeinfo.tm_sec
        ));
        
        Serial.println("Time synchronized with NTP");
        Serial.printf("Current time: %s\n", formatDateTime(rtc.now()).c_str());
    } else {
        Serial.println("Failed to get NTP time");
    }
}

// Function to initialize Firebase
void initFirebase() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Cannot initialize Firebase: No WiFi connection");
        return;
    }

    Serial.println("Initializing Firebase...");
    
    config.database_url = FIREBASE_DATABASE_URL;
    config.api_key = FIREBASE_API_KEY;

    // Required for ESP32 because it doesn't have native RTC
    config.timeout.serverResponse = 10 * 1000;

    // Initialize Firebase with anonymous authentication
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    // Sign in anonymously
    if (Firebase.signUp(&config, &auth, "", "")) {
        Serial.println("Anonymous sign-up success");
        firebaseInitialized = true;
    } else {
        Serial.printf("Anonymous sign-up failed: %s\n", config.signer.signupError.message.c_str());
        return;
    }

    // Set database read timeout to 1 minute
    Firebase.RTDB.setReadTimeout(&fbdo, 1000 * 60);
    // Set database write timeout to 30 seconds
    Firebase.RTDB.setwriteSizeLimit(&fbdo, "tiny");

    // Wait for initialization and token generation
    Serial.println("Waiting for Firebase token...");
    unsigned long startTime = millis();
    while (!Firebase.ready() && millis() - startTime < 30000) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println();

    if (Firebase.ready()) {
        firebaseInitialized = true;
        Serial.println("Firebase initialized successfully!");
        
        // Test connection by writing to a test path
        FirebaseJson json;
        json.set("test", "Connection successful");
        json.set("timestamp", rtc.now().unixtime());
        
        if (Firebase.RTDB.setJSON(&fbdo, "test/connection", &json)) {
            Serial.println("Test write successful");
        } else {
            Serial.printf("Test write failed: %s\n", fbdo.errorReason().c_str());
        }
    } else {
        Serial.println("Firebase initialization failed!");
        Serial.println("Please check your credentials and internet connection");
    }
}

// Function to save trip to Firebase
bool publishTripToFirebase(const TripData& trip) {
    digitalWrite(FIREBASE_LED, HIGH); // Turn on LED while attempting to publish
    
    if (!firebaseInitialized) {
        Serial.println("Firebase not initialized, attempting to initialize...");
        initFirebase();
        if (!firebaseInitialized) {
            Serial.println("Firebase initialization failed, cannot publish trip");
            digitalWrite(FIREBASE_LED, LOW); // Turn off LED on failure
            return false;
        }
    }

    if (!Firebase.ready() || WiFi.status() != WL_CONNECTED) {
        Serial.println("Firebase not ready or WiFi not connected");
        return false;
    }

    String path = deviceName + "/trips/" + String(trip.number);
    
    FirebaseJson json;
    json.set("deviceName", deviceName);
    json.set("tripNumber", trip.number);
    json.set("startTime", trip.startTime);
    json.set("endTime", trip.endTime);
    json.set("duration", trip.duration);
    json.set("breakTime", trip.breakTime);
    json.set("status", "OK");
    json.set("uploadTimestamp", rtc.now().unixtime());

    bool success = false;
    int retries = 3;
    
    while (retries > 0 && !success) {
        Serial.printf("[Firebase] Trip #%d | Attempt %d/3 | Path: %s | ", 
                     trip.number, 4 - retries, path.c_str());
                     
        if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json)) {
            Serial.printf("Status: ✓ Success | Time: %s\n", formatDateTime(rtc.now()).c_str());
            success = true;
            // Blink Firebase LED to indicate success
            digitalWrite(FIREBASE_LED, HIGH);
            delay(100);
            digitalWrite(FIREBASE_LED, LOW);
        } else {
            Serial.printf("Status: ✗ Failed | Error: %s\n", fbdo.errorReason().c_str());
            retries--;
            delay(1000);
        }
    }
    digitalWrite(FIREBASE_LED, LOW); // Ensure LED is off after publish attempt
    return success;
}

// Function to read and publish pending trips
void syncPendingTrips() {
    if (!rtcOK || !sdOK || !WiFi.isConnected()) {
        Serial.println("[Sync] Skipped - Prerequisites not met: RTC=" + String(rtcOK) + ", SD=" + String(sdOK) + ", WiFi=" + String(WiFi.isConnected()));
        return;
    }

    Serial.println("\n[Sync] Starting pending trips sync...");
    
    // Open file for reading
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.println("[Sync] ERROR: Could not open source file: " + String(filename));
        return;
    }

    // Create a temporary file for writing updated sync status
    File tempFile = SD.open("/temp.csv", FILE_WRITE);
    if (!tempFile) {
        Serial.println("[Sync] ERROR: Could not create temp file");
        file.close();
        return;
    }

    // Copy header
    String header = file.readStringUntil('\n');
    tempFile.println(header);
    Serial.println("[Sync] CSV Header copied successfully");

    String line;
    while (file.available()) {
        line = file.readStringUntil('\n');
        if (line.length() > 0 && isDigit(line.charAt(0))) {
            // Parse CSV line
            int comma1 = line.indexOf(',');
            int comma2 = line.indexOf(',', comma1 + 1);
            int comma3 = line.indexOf(',', comma2 + 1);
            int comma4 = line.indexOf(',', comma3 + 1);
            int comma5 = line.indexOf(',', comma4 + 1);
            
            if (comma1 > 0 && comma2 > 0 && comma3 > 0 && comma4 > 0 && comma5 > 0) {
                // Check if record is already synced
                int syncStatus = line.substring(comma5 + 1).toInt();
                
                if (syncStatus == 0) {  // Only process unsynced records
                    TripData trip;
                    trip.number = line.substring(0, comma1).toInt();
                    trip.startTime = line.substring(comma1 + 1, comma2);
                    trip.endTime = line.substring(comma2 + 1, comma3);
                    trip.duration = line.substring(comma3 + 1, comma4);
                    trip.breakTime = line.substring(comma4 + 1, comma5);
                    trip.synced = false;

                    Serial.printf("[Sync] Processing Trip #%d | Start: %s | End: %s | Duration: %s | Break: %s\n", 
                        trip.number, trip.startTime.c_str(), trip.endTime.c_str(), trip.duration.c_str(), trip.breakTime.c_str());

                    // Try to publish to Firebase
                    if (publishTripToFirebase(trip)) {
                        tempFile.println(line.substring(0, comma5 + 1) + "1");
                        Serial.printf("[Sync] Trip #%d ✓ Published successfully\n", trip.number);
                    } else {
                        tempFile.println(line);
                        pendingTrips.push_back(trip);
                        Serial.printf("[Sync] Trip #%d ✗ Publish failed - Added to pending queue\n", trip.number);
                    }
                } else {
                    // Already synced, copy line as is
                    tempFile.println(line);
                }
            }
        }
    }

    file.close();
    tempFile.close();

    // Replace original file with updated one
    SD.remove(filename);
    SD.rename("/temp.csv", filename);
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n Starting");
    
    deviceStartTime = millis();
    
    // Initialize all LEDs first
    initializeLEDs();
    
    // Initialize device name
    initializeDeviceName();
    
    // Initialize RTC and SD card first
    initializeRTCAndSD();
    
    // Initialize preferences
    preferences.begin(PREF_NAMESPACE, true);  // true = read-only mode
    int networkCount = preferences.getInt("networkCount", 0);
    
    // Load saved networks into WiFiMulti
    for (int i = 0; i < networkCount; i++) {
        char ssidKey[15], passwordKey[15];
        sprintf(ssidKey, "ssid%d", i);
        sprintf(passwordKey, "pass%d", i);
        
        String ssid = preferences.getString(ssidKey, "");
        String password = preferences.getString(passwordKey, "");
        
        if (ssid.length() > 0) {
            wifiMulti.addAP(ssid.c_str(), password.c_str());
            Serial.printf("Loaded saved network: %s\n", ssid.c_str());
        }
    }
    preferences.end();

    // Initialize device name
    initializeDeviceName();

    // Callbacks
    wm.setSaveConfigCallback(saveConfigCallback);
    
    // Custom parameters
    WiFiManagerParameter custom_text("This is a captive portal for WiFi setup");
    wm.addParameter(&custom_text);
    
    // Configuration portal settings
    wm.setAPStaticIPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
    wm.setConfigPortalTimeout(180); // 3 minutes timeout
    wm.setAPCallback([](WiFiManager*){ Serial.println("Config Portal Started"); });
    wm.setClass("invert"); // Dark theme
    wm.setCountry("US");
    wm.setWiFiAPChannel(11);
    wm.setShowPassword(true);
    wm.setMinimumSignalQuality(30);
    wm.setRemoveDuplicateAPs(true);
    wm.setDebugOutput(true);

    // Try to connect to any of the saved networks
    if (networkCount > 0) {
        Serial.println("Trying to connect to saved networks...");
        if (wifiMulti.run() == WL_CONNECTED) {
            Serial.println("Connected to WiFi!");
            Serial.println(WiFi.SSID());
            Serial.println(WiFi.localIP().toString());
            
            // Sync time and initialize Firebase after WiFi is connected
            syncTimeWithNTP();
            initFirebase();
            
            Serial.println("Connected to WiFi!");
            Serial.println(WiFi.SSID());
            Serial.println(WiFi.localIP().toString());
            return;
        }
    }
    
    // If no saved networks or couldn't connect, start the config portal
    Serial.println("Starting config portal...");
    if (!wm.startConfigPortal("WASA Grw", "12345678")) {
        Serial.println("Failed to connect or hit timeout");
        // Instead of resetting, we'll try again in the loop
        return;
    }
    
    Serial.println("Connected to WiFi!");
    Serial.println(WiFi.SSID());
    Serial.println(WiFi.localIP().toString());
    
    // Sync time and initialize Firebase after successful WiFi connection
    syncTimeWithNTP();
    initFirebase();
}

void loop() {
    // Handle WiFi connection
    static unsigned long lastWiFiCheck = 0;
    static unsigned long lastLEDUpdate = 0;
    static bool portalStarted = false;
    static bool portalActive = false;
    const unsigned long WIFI_CHECK_INTERVAL = 300000; // Check WiFi every 5 minutes
    const unsigned long LED_UPDATE_INTERVAL = 1000;   // Update LEDs every second

    // Update device status in Firebase
    if (WiFi.status() == WL_CONNECTED && millis() - lastStatusUpdate >= STATUS_UPDATE_INTERVAL) {
        lastStatusUpdate = millis();
        updateDeviceStatus();
    }

    // Get current time
    unsigned long currentMillis = millis();
    
    // Update LED states
    if (currentMillis - lastLEDUpdate >= LED_UPDATE_INTERVAL) {
        lastLEDUpdate = currentMillis;
        // Update WiFi LED
        digitalWrite(WIFI_STATUS_LED, WiFi.status() == WL_CONNECTED ? HIGH : LOW);
        // Update fault LEDs
        digitalWrite(RTC_FAULT_LED, !rtcOK);
        digitalWrite(SD_FAULT_LED, !sdOK);
    }

    // Handle WiFi reconnection without resetting
    if (WiFi.status() != WL_CONNECTED) {
        digitalWrite(WIFI_STATUS_LED, LOW); // Turn off WiFi LED when disconnected
        if (!portalActive && currentMillis - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
            lastWiFiCheck = currentMillis;
            Serial.println("\n[WiFi] Status: Disconnected");
            Serial.printf("[WiFi] Time since last check: %lu ms\n", currentMillis - lastWiFiCheck);
            Serial.println("[WiFi] Attempting to reconnect...");
            
            // Try to connect to saved networks
            if (wifiMulti.run(5000) == WL_CONNECTED) {
                digitalWrite(WIFI_STATUS_LED, HIGH); // Turn on WiFi LED when connected
                Serial.println("Reconnected to WiFi!");
                Serial.println(WiFi.SSID());
                Serial.println(WiFi.localIP().toString());
                portalStarted = false;
                portalActive = false;
                
                // Re-initialize Firebase if needed
                if (!firebaseInitialized) {
                    initFirebase();
                }
            } else if (!portalStarted) {
                Serial.println("Couldn't connect to any saved networks");
                Serial.println("Starting config portal...");
                portalStarted = true;
                portalActive = true;
                
                if (wm.startConfigPortal("WASA Grw", "12345678")) {
                    Serial.println("Portal connection successful");
                    portalStarted = false;
                    portalActive = false;
                } else {
                    Serial.println("Portal connection failed or timed out");
                    portalStarted = false;
                    portalActive = false;
                }
            }
        }
    } else {
        // Reset flags when connected
        portalStarted = false;
        portalActive = false;
    }

    // Handle RTC and SD logging
    if (rtcOK && sdOK) {
        unsigned long currentMillis = millis();
        
        // Data logging every 30 seconds
        if (currentMillis - lastDataLog >= DATA_LOG_INTERVAL || firstLog) {
            lastDataLog = currentMillis;
            firstLog = false;

            DateTime now = rtc.now();
            TimeSpan duration = now - tripStartTime;
            TimeSpan breakDuration(0);
            
            if (lastEndTime.unixtime() > 0) {
                breakDuration = tripStartTime - lastEndTime;
            }

            TripData currentTrip;
            currentTrip.number = tripNumber;
            currentTrip.startTime = formatDateTime(tripStartTime);
            currentTrip.endTime = formatDateTime(now);
            currentTrip.duration = formatDuration(duration);
            currentTrip.breakTime = formatDuration(breakDuration);
            currentTrip.synced = false;

            String logLine = String(currentTrip.number) + "," + 
                           currentTrip.startTime + "," + 
                           currentTrip.endTime + "," + 
                           currentTrip.duration + "," + 
                           currentTrip.breakTime + "," + 
                           "0";  // 0 = not synced

            // Always save to SD card regardless of WiFi status
            Serial.println("\n[Logger] Attempting to save trip data to SD card...");
            logFile = SD.open(filename, FILE_APPEND);
            if (logFile) {
                logFile.println(logLine);
                logFile.close();
                Serial.println("[Logger] --- Trip Logged Successfully ---");
                Serial.printf("[Logger] Trip Number: %d\n", currentTrip.number);
                Serial.printf("[Logger] Start Time: %s\n", currentTrip.startTime.c_str());
                Serial.printf("[Logger] End Time: %s\n", currentTrip.endTime.c_str());
                Serial.printf("[Logger] Duration: %s\n", currentTrip.duration.c_str());
                Serial.printf("[Logger] Break Time: %s\n", currentTrip.breakTime.c_str());
                Serial.printf("[Logger] Sync Status: %s\n", currentTrip.synced ? "Synced" : "Pending");
            } else {
                Serial.println("Failed to write to SD card!");
            }

            // Only attempt Firebase upload if connected
            if (WiFi.status() == WL_CONNECTED && firebaseInitialized) {
                if (!publishTripToFirebase(currentTrip)) {
                    pendingTrips.push_back(currentTrip);
                }
            } else {
                pendingTrips.push_back(currentTrip);
            }
        }

        // Handle Firebase sync interval
        if (WiFi.isConnected() && millis() - lastFirebaseSync >= FIREBASE_SYNC_INTERVAL) {
            lastFirebaseSync = millis();
            
            // Try to publish any pending trips
            if (!pendingTrips.empty()) {
                auto it = pendingTrips.begin();
                while (it != pendingTrips.end()) {
                    if (publishTripToFirebase(*it)) {
                        it = pendingTrips.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            // Sync any trips from SD card that might have been missed
            syncPendingTrips();
        }
    }
    
    delay(1000); // Prevent watchdog issues
}

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
#define RTC_FAULT_LED 33
#define SD_FAULT_LED 25

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

// Firebase sync variables
unsigned long lastFirebaseSync = 0;
const unsigned long FIREBASE_SYNC_INTERVAL = 300000; // 5 minutes
bool firebaseInitialized = false;

// Constants for preferences and logging
const char* PREF_NAMESPACE = "wifi_creds";
const int MAX_NETWORKS = 5;  // Maximum number of networks to store
const char* filename = "/trip_log.csv";

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
    bool synced;
};

std::vector<TripData> pendingTrips;

// NTP Server settings
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800;  // Replace with your timezone offset in seconds (e.g., IST = UTC+5:30 = 5.5*3600)
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
    if (!rtc.begin()) {
        Serial.println("RTC not found!");
        digitalWrite(RTC_FAULT_LED, HIGH);
    } else {
        rtcOK = true;
        Serial.println("RTC initialized.");
    }

    // SD Initialization
    if (!SD.begin(SD_CS)) {
        Serial.println("SD Card failed!");
        digitalWrite(SD_FAULT_LED, HIGH);
    } else {
        sdOK = true;
        Serial.println("SD Card initialized.");
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
                logFile.println("Trip No.,Start DateTime,End DateTime,Duration");
                logFile.close();
            }
        }

        Serial.printf("Trip %d started at: %s\n", tripNumber, formatDateTime(tripStartTime).c_str());
    }
}

// Function to sync time with NTP
void syncTimeWithNTP() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    Serial.println("Waiting for NTP time sync...");
    time_t now = time(nullptr);
    int retries = 10;
    while (now < 24 * 3600 && retries-- > 0) {
        Serial.print(".");
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
    if (!firebaseInitialized) {
        Serial.println("Firebase not initialized, attempting to initialize...");
        initFirebase();
        if (!firebaseInitialized) {
            Serial.println("Firebase initialization failed, cannot publish trip");
            return false;
        }
    }

    if (!Firebase.ready() || WiFi.status() != WL_CONNECTED) {
        Serial.println("Firebase not ready or WiFi not connected");
        return false;
    }

    String path = "trips/" + String(trip.number);
    
    FirebaseJson json;
    json.set("tripNumber", trip.number);
    json.set("startTime", trip.startTime);
    json.set("endTime", trip.endTime);
    json.set("duration", trip.duration);
    json.set("status", "OK");
    json.set("uploadTimestamp", rtc.now().unixtime());

    bool success = false;
    int retries = 3;
    
    while (retries > 0 && !success) {
        Serial.printf("Attempting to publish trip %d (attempt %d)...\n", 
                     trip.number, 4 - retries);
                     
        if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json)) {
            Serial.printf("Trip %d published to Firebase successfully\n", trip.number);
            success = true;
        } else {
            Serial.printf("Firebase publish failed: %s\nRetrying... (%d attempts left)\n", 
                        fbdo.errorReason().c_str(), retries - 1);
            retries--;
            delay(1000);
        }
    }
    return success;
}

// Function to read and publish pending trips
void syncPendingTrips() {
    if (!rtcOK || !sdOK || !WiFi.isConnected()) return;

    File file = SD.open(filename, FILE_READ);
    if (!file) return;

    String line;
    while (file.available()) {
        line = file.readStringUntil('\n');
        if (line.length() > 0 && isDigit(line.charAt(0))) {
            // Parse CSV line
            int comma1 = line.indexOf(',');
            int comma2 = line.indexOf(',', comma1 + 1);
            int comma3 = line.indexOf(',', comma2 + 1);
            
            if (comma1 > 0 && comma2 > 0 && comma3 > 0) {
                TripData trip;
                trip.number = line.substring(0, comma1).toInt();
                trip.startTime = line.substring(comma1 + 1, comma2);
                trip.endTime = line.substring(comma2 + 1, comma3);
                trip.duration = line.substring(comma3 + 1);
                trip.synced = false;
                
                if (publishTripToFirebase(trip)) {
                    trip.synced = true;
                } else {
                    pendingTrips.push_back(trip);
                }
            }
        }
    }
    file.close();
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n Starting");
    
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
        delay(3000);
        ESP.restart();
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
    if (wifiMulti.run() != WL_CONNECTED) {
        Serial.println("WiFi disconnected!");
        
        // Try to connect to saved networks for 10 seconds
        unsigned long startAttemptTime = millis();
        while (wifiMulti.run() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
            delay(500);
            Serial.print(".");
        }
        
        // If still not connected, start config portal
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("\nCouldn't connect to any saved networks");
            if (!wm.startConfigPortal("WASA Grw", "12345678")) {
                Serial.println("Failed to connect or hit timeout");
                delay(3000);
                ESP.restart();
            }
        }
    }

    // Handle RTC and SD logging
    if (rtcOK && sdOK) {
        if (millis() - lastLogMillis >= FIREBASE_SYNC_INTERVAL || firstLog) {
            lastLogMillis = millis();
            firstLog = false;

            DateTime now = rtc.now();
            TimeSpan duration = now - tripStartTime;

            TripData currentTrip;
            currentTrip.number = tripNumber;
            currentTrip.startTime = formatDateTime(tripStartTime);
            currentTrip.endTime = formatDateTime(now);
            currentTrip.duration = formatDuration(duration);
            currentTrip.synced = false;

            String logLine = String(currentTrip.number) + "," + 
                           currentTrip.startTime + "," + 
                           currentTrip.endTime + "," + 
                           currentTrip.duration;

            Serial.println(logLine);

            // Save to SD card
            logFile = SD.open(filename, FILE_APPEND);
            if (logFile) {
                logFile.println(logLine);
                logFile.close();
            }

            // Try to publish to Firebase
            if (!publishTripToFirebase(currentTrip)) {
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

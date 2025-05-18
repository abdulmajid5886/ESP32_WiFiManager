#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WiFiMulti.h>
#include <Preferences.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

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
        if (millis() - lastLogMillis >= 30000 || firstLog) {
            lastLogMillis = millis();
            firstLog = false;

            DateTime now = rtc.now();
            TimeSpan duration = now - tripStartTime;

            String logLine = String(tripNumber) + "," + formatDateTime(tripStartTime) + "," + 
                           formatDateTime(now) + "," + formatDuration(duration);

            Serial.println(logLine);

            logFile = SD.open(filename, FILE_APPEND);
            if (logFile) {
                logFile.println(logLine);
                logFile.close();
            }
        }
    }
    
    delay(1000); // Prevent watchdog issues
}

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WiFiMulti.h>
#include <Preferences.h>

// WiFiManager object
WiFiManager wm;
WiFiMulti wifiMulti;
Preferences preferences;

// Constants for preferences
const char* PREF_NAMESPACE = "wifi_creds";
const int MAX_NETWORKS = 5;  // Maximum number of networks to store

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

void setup() {
    Serial.begin(115200);
    Serial.println("\n Starting");
    
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
    // Check if WiFi is connected
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
    
    // Your code to send/receive data goes here
    
    delay(1000); // Prevent watchdog issues
}

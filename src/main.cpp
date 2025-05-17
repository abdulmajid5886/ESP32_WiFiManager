#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

// WiFiManager object
WiFiManager wm;

void saveConfigCallback() {
    Serial.println("Configuration saved");
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n Starting");
    
    // Reset settings - uncomment to test
    // wm.resetSettings();

    // Callbacks
    wm.setSaveConfigCallback(saveConfigCallback);
    
    // Set config save notify callback
    wm.setSaveParamsCallback([]{
        Serial.println("Get Params:");
        Serial.println(WiFi.SSID());
        Serial.println(WiFi.psk());
    });

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
    
    // Configure hosted AccessPoint
    // Try to connect to saved WiFi, if it fails, start config portal
    bool res = wm.autoConnect("WASA Grw", "12345678");

    if(!res) {
        Serial.println("Failed to connect or timeout");
        // ESP.restart() is called automatically in library
    } else {
        Serial.println("Connected to WiFi!");
        Serial.println(WiFi.localIP().toString());
    }
}

void loop() {
    // Check if WiFi is connected
    if(WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected!");
        
        // Start config portal
        if(!wm.startConfigPortal("WASA Grw", "12345678")) {
            Serial.println("Failed to connect or hit timeout");
            delay(3000);
            ESP.restart();
            delay(5000);
        }
    }
    delay(1000); // Prevent watchdog issues
}

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

// Pin definitions
#define BUTTON_1_PIN 13
#define BUTTON_2_PIN 17
#define RELAY_1_PIN 18  // Phase 1 relay
#define RELAY_2_PIN 16   // Phase 2 relay
#define RELAY_3_PIN 23  // Phase 3 relay
#define VOLTAGE_SENSOR_1_PIN 32  // Phase 1
#define VOLTAGE_SENSOR_2_PIN 35  // Phase 2
#define VOLTAGE_SENSOR_3_PIN 34  // Phase 3

// LCD configuration
LiquidCrystal_I2C lcd(0x27, 16, 2);

// WiFi credentials - Leave empty for AP-only mode
const char* ssid = "";  // Your WiFi SSID or leave empty
const char* password = "";  // Your WiFi password or leave empty

// Access Point (Hotspot) credentials
const char* ap_ssid = "BestPhaseDetector";
const char* ap_password = "phase12345";

// Web server on port 80
WebServer server(80);

// Voltage sensor calibration
const float VREF = 3.3;
const int ADC_MAX = 4095;
const int SAMPLES = 300;  // Reduced for better performance (was 500)

// Calibration factor - ADJUST THIS based on your ZMPT101B modules
// Start with 250 and adjust after testing with a multimeter
float CALIBRATION_FACTOR = 250.0;

// Safety thresholds
const float OVERVOLTAGE_THRESHOLD = 260.0;
const float UNDERVOLTAGE_THRESHOLD = 180.0;
const float MIN_VOLTAGE = 150.0;
const unsigned long MIN_SWITCH_INTERVAL = 30000;  // 30 seconds minimum between switches

// Phase data structure
struct PhaseData {
    float voltage;
    float avgVoltage;
    float minVoltage;
    float maxVoltage;
    bool isActive;
    int relayPin;
    String name;
};

PhaseData phases[3] = {
    {0.0, 0.0, 999.0, 0.0, false, RELAY_1_PIN, "Phase 1"},
    {0.0, 0.0, 999.0, 0.0, false, RELAY_2_PIN, "Phase 2"},
    {0.0, 0.0, 999.0, 0.0, false, RELAY_3_PIN, "Phase 3"}
};

// System state
enum SystemMode { MODE_AUTOMATIC, MODE_MANUAL };
enum MenuState { MENU_MAIN, MENU_SELECT_PHASE, MENU_SETTINGS };

SystemMode systemMode = MODE_AUTOMATIC;
MenuState menuState = MENU_MAIN;
int selectedPhase = 0;
int currentMenuIndex = 0;
unsigned long lastSwitchTime = 0;

// Button state
struct ButtonState {
    int pin;
    bool lastState;
    unsigned long pressStartTime;
    bool isPressed;
    bool wasLongPress;
};

ButtonState button1 = {BUTTON_1_PIN, HIGH, 0, false, false};
ButtonState button2 = {BUTTON_2_PIN, HIGH, 0, false, false};

const unsigned long LONG_PRESS_TIME = 1000;
const unsigned long DEBOUNCE_TIME = 50;

// Timing
unsigned long lastVoltageRead = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastTrendUpdate = 0;
const unsigned long VOLTAGE_READ_INTERVAL = 200;
const unsigned long LCD_UPDATE_INTERVAL = 500;
const unsigned long TREND_UPDATE_INTERVAL = 5000;

// Voltage history for trend analysis
const int HISTORY_SIZE = 20;
float voltageHistory[3][HISTORY_SIZE];
int historyIndex = 0;

// Thread safety flag
volatile bool isReadingVoltage = false;

// Function prototypes
void setupWiFi();
void setupWebServer();
void handleRoot();
void handleGetStatus();
void handleSetPhase();
void handleSetMode();
void handleGetNetwork();
void handleNotFound();
void readVoltage(int phaseIndex, int sensorPin);
void updateVoltageTrends();
int findBestPhase();
void switchToPhase(int phaseIndex, bool force = false);
void updateLCD();
void handleButtons();
int checkButton(ButtonState* button);
void processButtonPress(ButtonState* button, bool isLongPress);
void navigateMenu(int direction);
void selectMenuItem();
void resetRelays();
void testRelays();

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== Best Phase Detector Starting ===");
    
    // Initialize pins
    pinMode(BUTTON_1_PIN, INPUT_PULLUP);
    pinMode(BUTTON_2_PIN, INPUT_PULLUP);
    pinMode(RELAY_1_PIN, OUTPUT);
    pinMode(RELAY_2_PIN, OUTPUT);
    pinMode(RELAY_3_PIN, OUTPUT);
    pinMode(VOLTAGE_SENSOR_1_PIN, INPUT);
    pinMode(VOLTAGE_SENSOR_2_PIN, INPUT);
    pinMode(VOLTAGE_SENSOR_3_PIN, INPUT);
    
    // Initialize relays (HIGH = OFF for active-low relays)
    resetRelays();
    Serial.println("Relays initialized (all OFF)");
    
    // Initialize I2C for LCD
    Wire.begin();
    delay(200);
    
    // Scan I2C bus
    Serial.println("Scanning I2C bus...");
    byte foundAddress = 0;
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.print("I2C device found at address 0x");
            if (address < 16) Serial.print("0");
            Serial.println(address, HEX);
            if (address == 0x27 || address == 0x3F || address == 0x20 || address == 0x38) {
                foundAddress = address;
            }
        }
    }
    
    // Initialize LCD
    Serial.println("Initializing LCD...");
    lcd.init();
    lcd.backlight();
    delay(100);
    
    // Test LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Best Phase Det");
    lcd.setCursor(0, 1);
    lcd.print("Initializing...");
    
    if (foundAddress > 0 && foundAddress != 0x27) {
        Serial.print("Note: I2C device at 0x");
        if (foundAddress < 16) Serial.print("0");
        Serial.print(foundAddress, HEX);
        Serial.println(" - Update LCD address in code if display doesn't work");
    }
    
    delay(2000);
    
    // Test relays
    Serial.println("Testing relays (you should hear 3 clicks)...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Testing Relays");
    testRelays();
    
    // Initialize WiFi
    setupWiFi();
    
    // Setup web server
    setupWebServer();
    
    // Initialize voltage history
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < HISTORY_SIZE; j++) {
            voltageHistory[i][j] = 0.0;
        }
    }
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("System Ready");
    lcd.setCursor(0, 1);
    lcd.print("Mode: Auto");
    Serial.println("=== System initialized successfully ===");
    delay(2000);
}

void loop() {
    unsigned long currentMillis = millis();
    
    // Read voltages periodically
    // Read voltages periodically (Round-robin to avoid blocking)
    if (currentMillis - lastVoltageRead >= VOLTAGE_READ_INTERVAL) {
        static int phaseToRead = 0;
        isReadingVoltage = true;
        
        // Read one phase per cycle
        switch (phaseToRead) {
            case 0: readVoltage(0, VOLTAGE_SENSOR_1_PIN); break;
            case 1: readVoltage(1, VOLTAGE_SENSOR_2_PIN); break;
            case 2: readVoltage(2, VOLTAGE_SENSOR_3_PIN); break;
        }
        
        phaseToRead = (phaseToRead + 1) % 3;
        
        isReadingVoltage = false;
        lastVoltageRead = currentMillis;
    }
    
    // Update voltage trends
    if (currentMillis - lastTrendUpdate >= TREND_UPDATE_INTERVAL) {
        updateVoltageTrends();
        
        // Automatic mode: switch to best phase
        if (systemMode == MODE_AUTOMATIC) {
            int bestPhase = findBestPhase();
            if (bestPhase >= 0 && bestPhase < 3 && bestPhase != selectedPhase) {
                Serial.print("Auto mode: Switching from Phase ");
                Serial.print(selectedPhase + 1);
                Serial.print(" to Phase ");
                Serial.println(bestPhase + 1);
                switchToPhase(bestPhase, false);
            }
        }
        lastTrendUpdate = currentMillis;
    }
    
    // Update LCD
    if (currentMillis - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
        if (!isReadingVoltage) {  // Don't update LCD during voltage reading
            updateLCD();
        }
        lastLCDUpdate = currentMillis;
    }
    
    // Handle buttons
    handleButtons();
    
    // Handle web server requests
    server.handleClient();
}

void readVoltage(int phaseIndex, int sensorPin) {
    // Read samples and calculate DC offset
    float sum = 0;
    int readings[SAMPLES];
    
    for (int i = 0; i < SAMPLES; i++) {
        readings[i] = analogRead(sensorPin);
        sum += readings[i];
        delayMicroseconds(200);  // ~100Hz sampling for 50Hz AC
    }
    
    // Calculate DC offset (centered around VCC/2 for ZMPT101B)
    float avgReading = sum / SAMPLES;
    float dcOffset = (avgReading / ADC_MAX) * VREF;
    
    // Calculate RMS of AC component
    float sumSquaredAC = 0;
    for (int i = 0; i < SAMPLES; i++) {
        float voltage = (readings[i] / (float)ADC_MAX) * VREF;
        float acComponent = voltage - dcOffset;
        sumSquaredAC += acComponent * acComponent;
    }
    
    float rmsVoltageAC = sqrt(sumSquaredAC / SAMPLES);
    
    // Convert to actual AC voltage
    // ZMPT101B typically outputs ~1V RMS for 250V AC input
    float acVoltage = rmsVoltageAC * CALIBRATION_FACTOR;
    
    // Apply some filtering to reduce noise
    phases[phaseIndex].voltage = acVoltage;
    
    // Update min/max
    if (acVoltage < phases[phaseIndex].minVoltage && acVoltage > 50.0) {
        phases[phaseIndex].minVoltage = acVoltage;
    }
    if (acVoltage > phases[phaseIndex].maxVoltage) {
        phases[phaseIndex].maxVoltage = acVoltage;
    }
    
    // Update average (exponential moving average)
    if (phases[phaseIndex].avgVoltage == 0.0) {
        phases[phaseIndex].avgVoltage = acVoltage;
    } else {
        phases[phaseIndex].avgVoltage = (phases[phaseIndex].avgVoltage * 0.85) + (acVoltage * 0.15);
    }
}

void updateVoltageTrends() {
    // Store current voltages in history
    for (int i = 0; i < 3; i++) {
        voltageHistory[i][historyIndex] = phases[i].avgVoltage;
    }
    historyIndex = (historyIndex + 1) % HISTORY_SIZE;
    
    // Reset min/max periodically for fresh calculations
    static unsigned long lastReset = 0;
    if (millis() - lastReset > 300000) {  // Reset every 5 minutes
        for (int i = 0; i < 3; i++) {
            phases[i].minVoltage = phases[i].avgVoltage;
            phases[i].maxVoltage = phases[i].avgVoltage;
        }
        lastReset = millis();
    }
}

int findBestPhase() {
    int bestPhase = selectedPhase;
    float bestScore = -1.0;
    
    const float HYSTERESIS_BONUS = 15.0;  // Prefer current phase to avoid excessive switching
    const float TARGET_VOLTAGE = 220.0;
    const float MAX_VARIATION = 30.0;
    
    Serial.println("\n--- Phase Analysis ---");
    
    for (int i = 0; i < 3; i++) {
        if (phases[i].avgVoltage < MIN_VOLTAGE) {
            Serial.print(phases[i].name);
            Serial.println(": REJECTED (voltage too low)");
            continue;
        }
        
        // Calculate stability score (0-100)
        float variation = phases[i].maxVoltage - phases[i].minVoltage;
        float stabilityScore = 100.0 * (1.0 - min(variation / MAX_VARIATION, 1.0f));
        
        // Calculate voltage quality score (0-100)
        float voltageError = abs(phases[i].avgVoltage - TARGET_VOLTAGE);
        float voltageScore = 100.0 * (1.0 - min(voltageError / 50.0f, 1.0f));
        
        // Combined score (weighted average)
        float totalScore = (voltageScore * 0.6) + (stabilityScore * 0.4);
        
        // Add hysteresis bonus to current phase
        if (i == selectedPhase) {
            totalScore += HYSTERESIS_BONUS;
        }
        
        Serial.print(phases[i].name);
        Serial.print(": V=");
        Serial.print(phases[i].avgVoltage, 1);
        Serial.print("V, Var=");
        Serial.print(variation, 1);
        Serial.print("V, Score=");
        Serial.print(totalScore, 1);
        if (i == selectedPhase) Serial.print(" (CURRENT+BONUS)");
        Serial.println();
        
        if (totalScore > bestScore) {
            bestScore = totalScore;
            bestPhase = i;
        }
    }
    
    Serial.print("Best phase: ");
    Serial.println(phases[bestPhase].name);
    
    return bestPhase;
}

void switchToPhase(int phaseIndex, bool force) {
    if (phaseIndex < 0 || phaseIndex >= 3) {
        Serial.println("ERROR: Invalid phase index");
        return;
    }
    
    // Safety check: Don't switch too frequently (unless forced)
    unsigned long timeSinceLastSwitch = millis() - lastSwitchTime;
    if (!force && lastSwitchTime > 0 && timeSinceLastSwitch < MIN_SWITCH_INTERVAL) {
        Serial.print("Switch blocked: Too soon (");
        Serial.print((MIN_SWITCH_INTERVAL - timeSinceLastSwitch) / 1000);
        Serial.println("s remaining)");
        return;
    }
    
    // Safety check: Verify target phase voltage is in safe range
    if (phases[phaseIndex].avgVoltage < UNDERVOLTAGE_THRESHOLD) {
        Serial.println("Switch blocked: Target voltage too low!");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("VOLTAGE TOO LOW!");
        lcd.setCursor(0, 1);
        lcd.print(phases[phaseIndex].name);
        delay(2000);
        return;
    }
    
    if (phases[phaseIndex].avgVoltage > OVERVOLTAGE_THRESHOLD) {
        Serial.println("Switch blocked: Target voltage too high!");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("VOLTAGE TOO HIGH");
        lcd.setCursor(0, 1);
        lcd.print(phases[phaseIndex].name);
        delay(2000);
        return;
    }
    
    // Turn off all relays first (safety)
    resetRelays();
    delay(100);
    
    // Turn on selected phase relay (LOW = ON for active-low relays)
    digitalWrite(phases[phaseIndex].relayPin, LOW);
    phases[phaseIndex].isActive = true;
    
    // Update other phases
    for (int i = 0; i < 3; i++) {
        if (i != phaseIndex) {
            phases[i].isActive = false;
        }
    }
    
    selectedPhase = phaseIndex;
    lastSwitchTime = millis();
    
    Serial.print("Successfully switched to ");
    Serial.println(phases[phaseIndex].name);
}

void resetRelays() {
    // HIGH = OFF for active-low relays
    digitalWrite(RELAY_1_PIN, HIGH);
    digitalWrite(RELAY_2_PIN, HIGH);
    digitalWrite(RELAY_3_PIN, HIGH);
    
    for (int i = 0; i < 3; i++) {
        phases[i].isActive = false;
    }
}

void testRelays() {
    // Test each relay briefly
    for (int i = 0; i < 3; i++) {
        Serial.print("Testing ");
        Serial.println(phases[i].name);
        lcd.setCursor(0, 1);
        lcd.print("                ");  // Clear line
        lcd.setCursor(0, 1);
        lcd.print(phases[i].name);
        
        digitalWrite(phases[i].relayPin, LOW);  // ON
        delay(300);
        digitalWrite(phases[i].relayPin, HIGH); // OFF
        delay(300);
    }
    Serial.println("Relay test complete");
}

void updateLCD() {
    lcd.clear();
    
    if (menuState == MENU_MAIN) {
        // Line 1: Phase voltages
        lcd.setCursor(0, 0);
        lcd.print("P1:");
        lcd.print((int)phases[0].voltage);
        lcd.print(" P2:");
        lcd.print((int)phases[1].voltage);
        
        // Line 2: Phase 3 voltage and mode
        lcd.setCursor(0, 1);
        lcd.print("P3:");
        lcd.print((int)phases[2].voltage);
        lcd.print(" ");
        lcd.print(systemMode == MODE_AUTOMATIC ? "AUTO" : "MAN");
        
        // Show active phase indicator (*)
        for (int i = 0; i < 3; i++) {
            if (phases[i].isActive) {
                if (i == 0) lcd.setCursor(2, 0);
                else if (i == 1) lcd.setCursor(9, 0);
                else lcd.setCursor(2, 1);
                lcd.print("*");
            }
        }
    }
    else if (menuState == MENU_SELECT_PHASE) {
        lcd.setCursor(0, 0);
        lcd.print("Select Phase:");
        lcd.setCursor(0, 1);
        lcd.print(phases[currentMenuIndex].name);
        lcd.print(" ");
        lcd.print((int)phases[currentMenuIndex].voltage);
        lcd.print("V");
        if (currentMenuIndex == selectedPhase) {
            lcd.print("*");
        }
    }
    else if (menuState == MENU_SETTINGS) {
        lcd.setCursor(0, 0);
        lcd.print("Settings:");
        lcd.setCursor(0, 1);
        lcd.print("Mode: ");
        lcd.print(systemMode == MODE_AUTOMATIC ? "Auto" : "Manual");
    }
}

void handleButtons() {
    int btn1Result = checkButton(&button1);
    int btn2Result = checkButton(&button2);
    
    if (btn1Result != -1) {
        processButtonPress(&button1, btn1Result == 1);
    }
    if (btn2Result != -1) {
        processButtonPress(&button2, btn2Result == 1);
    }
}

int checkButton(ButtonState* button) {
    bool currentState = digitalRead(button->pin);
    unsigned long currentTime = millis();
    
    // Button is pressed (LOW because of pull-up)
    if (currentState == LOW && button->lastState == HIGH) {
        button->pressStartTime = currentTime;
        button->isPressed = true;
        button->wasLongPress = false;
    }
    
    // Button is released
    if (currentState == HIGH && button->lastState == LOW && button->isPressed) {
        button->isPressed = false;
        unsigned long pressDuration = currentTime - button->pressStartTime;
        
        if (pressDuration > DEBOUNCE_TIME) {
            button->lastState = currentState;
            if (pressDuration >= LONG_PRESS_TIME) {
                button->wasLongPress = true;
                return 1;  // Long press
            } else {
                button->wasLongPress = false;
                return 0;  // Short press
            }
        }
    }
    
    button->lastState = currentState;
    return -1;  // No action
}

void processButtonPress(ButtonState* button, bool isLongPress) {
    if (button->pin == BUTTON_1_PIN) {
        if (isLongPress) {
            // Long press: Enter/Exit menu
            Serial.println("Button 1: Long press - Toggle menu");
            if (menuState == MENU_MAIN) {
                menuState = MENU_SELECT_PHASE;
                currentMenuIndex = selectedPhase;  // Start at current phase
            } else {
                menuState = MENU_MAIN;
            }
        } else {
            // Short press: Navigate previous
            Serial.println("Button 1: Short press - Previous");
            navigateMenu(-1);
        }
    }
    else if (button->pin == BUTTON_2_PIN) {
        if (isLongPress) {
            // Long press: Select/Confirm
            Serial.println("Button 2: Long press - Select");
            selectMenuItem();
        } else {
            // Short press: Navigate next
            Serial.println("Button 2: Short press - Next");
            navigateMenu(+1);
        }
    }
}

void navigateMenu(int direction) {
    if (menuState == MENU_SELECT_PHASE) {
        currentMenuIndex = (currentMenuIndex + direction + 3) % 3;
        Serial.print("Navigate to: ");
        Serial.println(phases[currentMenuIndex].name);
    }
    else if (menuState == MENU_SETTINGS) {
        currentMenuIndex = (currentMenuIndex + direction + 2) % 2;
        Serial.print("Navigate to setting: ");
        Serial.println(currentMenuIndex);
    }
}

void selectMenuItem() {
    if (menuState == MENU_SELECT_PHASE) {
        Serial.print("Selecting ");
        Serial.println(phases[currentMenuIndex].name);
        systemMode = MODE_MANUAL;
        switchToPhase(currentMenuIndex, true);
        menuState = MENU_MAIN;
    }
    else if (menuState == MENU_SETTINGS) {
        if (currentMenuIndex == 0) {
            // Toggle mode
            systemMode = (systemMode == MODE_AUTOMATIC) ? MODE_MANUAL : MODE_AUTOMATIC;
            Serial.print("Mode changed to: ");
            Serial.println(systemMode == MODE_AUTOMATIC ? "Automatic" : "Manual");
        }
    }
}

void setupWiFi() {
    Serial.println("\n=== WiFi Setup ===");
    
    // Start Access Point
    WiFi.mode(WIFI_AP_STA);
    
    Serial.print("Starting Access Point: ");
    Serial.println(ap_ssid);
    
    bool apStarted = WiFi.softAP(ap_ssid, ap_password);
    
    if (apStarted) {
        IPAddress apIP = WiFi.softAPIP();
        Serial.print("AP IP address: ");
        Serial.println(apIP);
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("AP: ");
        lcd.print(ap_ssid);
        lcd.setCursor(0, 1);
        lcd.print(apIP);
        delay(3000);
    } else {
        Serial.println("AP failed to start!");
    }
    
    // Try to connect to WiFi if credentials provided
    if (strlen(ssid) > 0) {
        Serial.print("Connecting to WiFi: ");
        Serial.println(ssid);
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi Connect...");
        
        WiFi.begin(ssid, password);
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected!");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("WiFi OK");
            lcd.setCursor(0, 1);
            lcd.print(WiFi.localIP());
            delay(3000);
        } else {
            Serial.println("\nWiFi connection failed!");
        }
    } else {
        Serial.println("No WiFi credentials - AP mode only");
    }
}

void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/api/status", HTTP_GET, handleGetStatus);
    server.on("/api/setPhase", HTTP_POST, handleSetPhase);
    server.on("/api/setMode", HTTP_POST, handleSetMode);
    server.on("/api/network", HTTP_GET, handleGetNetwork);
    server.onNotFound(handleNotFound);
    
    server.begin();
    Serial.println("HTTP server started");
    Serial.print("Access at: http://");
    Serial.println(WiFi.softAPIP());
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Best Phase Detector</title>";
    html += "<style>";
    html += "body{font-family:Arial;margin:20px;background:#f0f0f0;}";
    html += ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
    html += "h1{color:#333;text-align:center;}";
    html += ".phase{background:#f9f9f9;margin:10px 0;padding:15px;border-radius:5px;border-left:4px solid #ddd;}";
    html += ".phase.active{border-left-color:#4CAF50;}";
    html += ".voltage{font-size:24px;font-weight:bold;color:#333;}";
    html += ".stats{font-size:12px;color:#666;margin-top:5px;}";
    html += "button{background:#2196F3;color:white;border:none;padding:10px 20px;margin:5px;border-radius:5px;cursor:pointer;font-size:16px;}";
    html += "button:hover{background:#0b7dda;}";
    html += "button.active{background:#4CAF50;}";
    html += ".controls{text-align:center;margin-top:20px;}";
    html += ".mode{text-align:center;margin:20px 0;padding:10px;background:#e3f2fd;border-radius:5px;}";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>Best Phase Detector</h1>";
    html += "<div id='status'>Loading...</div>";
    html += "<div class='mode' id='modeDisplay'></div>";
    html += "<div class='controls'>";
    html += "<button onclick='setMode(\"auto\")' id='autoBtn'>Auto Mode</button>";
    html += "<button onclick='setMode(\"manual\")' id='manBtn'>Manual Mode</button>";
    html += "</div>";
    html += "</div>";
    html += "<script>";
    html += "function updateStatus(){";
    html += "fetch('/api/status').then(r=>r.json()).then(data=>{";
    html += "let html='';";
    html += "data.phases.forEach((p,i)=>{";
    html += "html+='<div class=\"phase'+(p.isActive?' active':'')+'\">';";
    html += "html+='<div style=\"display:flex;justify-content:space-between;align-items:center;\">';";
    html += "html+='<div><strong>'+p.name+'</strong>'+(p.isActive?' <span style=\"background:#4CAF50;color:white;padding:2px 5px;border-radius:3px;font-size:10px;\">ACTIVE</span>':'')+'</div>';";
    html += "html+='<div class=\"voltage\">'+p.voltage.toFixed(1)+'V</div>';";
    html += "html+='</div>';";
    html += "html+='<div class=\"stats\">Avg: '+p.avgVoltage.toFixed(1)+'V | Range: '+p.minVoltage.toFixed(1)+'-'+p.maxVoltage.toFixed(1)+'V</div>';";
    html += "if(data.mode==='manual'){";
    html += "html+='<button onclick=\"setPhase('+i+')\" style=\"margin-top:10px;width:100%;\">Switch to this phase</button>';";
    html += "}";
    html += "html+='</div>';";
    html += "});";
    html += "document.getElementById('status').innerHTML=html;";
    html += "document.getElementById('modeDisplay').innerHTML='<strong>Mode: '+(data.mode==='automatic'?'Automatic':'Manual')+' <br> Active Phase: '+data.phases[data.selectedPhase].name+'</strong>';";
    html += "document.getElementById('autoBtn').className=data.mode==='automatic'?'active':'';";
    html += "document.getElementById('manBtn').className=data.mode==='manual'?'active':'';";
    html += "});";
    html += "}";
    html += "function setPhase(p){";
    html += "fetch('/api/setPhase',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({phase:p})})";
    html += ".then(r=>r.json()).then(d=>{alert(d.message);updateStatus();});";
    html += "}";
    html += "function setMode(m){";
    html += "fetch('/api/setMode',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({mode:m})})";
    html += ".then(r=>r.json()).then(d=>{updateStatus();});";
    html += "}";
    html += "updateStatus();setInterval(updateStatus,2000);";
    html += "</script></body></html>";
    
    server.send(200, "text/html", html);
}

void handleGetStatus() {
    DynamicJsonDocument doc(1024);
    
    doc["mode"] = (systemMode == MODE_AUTOMATIC) ? "automatic" : "manual";
    doc["bestPhase"] = findBestPhase();
    doc["selectedPhase"] = selectedPhase;
    
    JsonArray phasesArray = doc.createNestedArray("phases");
    for (int i = 0; i < 3; i++) {
        JsonObject phaseObj = phasesArray.createNestedObject();
        phaseObj["name"] = phases[i].name;
        phaseObj["voltage"] = phases[i].voltage;
        phaseObj["avgVoltage"] = phases[i].avgVoltage;
        phaseObj["minVoltage"] = phases[i].minVoltage;
        phaseObj["maxVoltage"] = phases[i].maxVoltage;
        phaseObj["isActive"] = phases[i].isActive;
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSetPhase() {
    if (server.hasArg("plain")) {
        DynamicJsonDocument doc(256);
        deserializeJson(doc, server.arg("plain"));
        
        if (doc.containsKey("phase")) {
            int phase = doc["phase"];
            if (phase >= 0 && phase < 3) {
                systemMode = MODE_MANUAL;
                switchToPhase(phase, true);
                
                DynamicJsonDocument response(256);
                response["success"] = true;
                response["message"] = "Switched to " + phases[phase].name;
                
                String responseStr;
                serializeJson(response, responseStr);
                server.send(200, "application/json", responseStr);
                return;
            }
        }
    }
    
    DynamicJsonDocument response(256);
    response["success"] = false;
    response["message"] = "Invalid phase number";
    
    String responseStr;
    serializeJson(response, responseStr);
    server.send(400, "application/json", responseStr);
}

void handleSetMode() {
    if (server.hasArg("plain")) {
        DynamicJsonDocument doc(256);
        deserializeJson(doc, server.arg("plain"));
        
        if (doc.containsKey("mode")) {
            String mode = doc["mode"];
            if (mode == "auto" || mode == "automatic") {
                systemMode = MODE_AUTOMATIC;
            } else if (mode == "manual") {
                systemMode = MODE_MANUAL;
            }
            
            DynamicJsonDocument response(256);
            response["success"] = true;
            response["message"] = "Mode set to " + String((systemMode == MODE_AUTOMATIC) ? "automatic" : "manual");
            
            String responseStr;
            serializeJson(response, responseStr);
            server.send(200, "application/json", responseStr);
            return;
        }
    }
    
    DynamicJsonDocument response(256);
    response["success"] = false;
    response["message"] = "Invalid mode";
    
    String responseStr;
    serializeJson(response, responseStr);
    server.send(400, "application/json", responseStr);
}

void handleGetNetwork() {
    DynamicJsonDocument doc(512);
    
    IPAddress apIP = WiFi.softAPIP();
    doc["ap_ssid"] = ap_ssid;
    doc["ap_ip"] = apIP.toString();
    doc["ap_connected"] = true;
    
    if (WiFi.status() == WL_CONNECTED) {
        doc["sta_connected"] = true;
        doc["sta_ip"] = WiFi.localIP().toString();
        doc["sta_ssid"] = ssid;
    } else {
        doc["sta_connected"] = false;
        doc["sta_ip"] = "";
        doc["sta_ssid"] = "";
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleNotFound() {
    server.send(404, "text/plain", "Not found");
}
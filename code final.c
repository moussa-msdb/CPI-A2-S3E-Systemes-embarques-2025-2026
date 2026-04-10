#include <Arduino.h>
#include <ChainableLED.h>
#include <Wire.h>
#include <rgb_lcd.h>
#include <DHT.h>
#include <SoftwareSerial.h>
#include <MicroNMEA.h> 
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

// ==========================================
// 1. CONFIGURATION MATÉRIELLE
// ==========================================
#define PIN_CLK   7 
#define PIN_DATA  8 
#define PIN_BTN_RED   3  
#define PIN_BTN_GREEN 2 
#define PIN_GPS_RX 5     
#define PIN_GPS_TX 6     
#define PIN_DHT 4
#define PIN_LUM A0
#define PIN_SD_CS 10

ChainableLED leds(PIN_CLK, PIN_DATA, 1);
DHT dht(PIN_DHT, DHT11);
rgb_lcd lcd;
SoftwareSerial gpsSerial(PIN_GPS_RX, PIN_GPS_TX);
RTC_DS1307 rtc; 

char nmeaBuffer[85];
MicroNMEA nmea(nmeaBuffer, sizeof(nmeaBuffer));

// ==========================================
// 2. ÉTATS ET VARIABLES GLOBALES
// ==========================================
typedef enum { MODE_STANDARD, MODE_CONFIG, MODE_ECO, MODE_MAINTENANCE } SystemMode;
typedef enum { LED_OFF, LED_MODE_STANDARD, LED_MODE_CONFIG, LED_MODE_ECO, LED_MODE_MAINTENANCE, LED_ERR_RTC, LED_ERR_SD_FULL, LED_ERR_SD_ACCESS, LED_ERR_SENSOR, LED_ERR_SENSOR_INCOHERENT, LED_ERR_GPS } LedState;

SystemMode currentMode = MODE_STANDARD;
SystemMode previousMode = MODE_STANDARD; 
LedState currentLedState = LED_OFF;

bool event_red_long_press = false;
bool event_green_long_press = false;
bool event_red_double_click = false;
uint32_t lastActivityTime = 0;

bool rtcError = false; 
bool sdAccessError = false; 
bool sdFull = false;        

uint32_t lastLogTime = 0;
char currentLogFile[16]; 

uint32_t log_interval = 100000; 
uint32_t file_max_size = 2048;  

float lastTemp = 0.0;
float lastHum = 0.0;
int lastLum = 0;

char cmdBuffer[22]; // Légèrement agrandi pour les commandes longues comme PRESSURE_MAX=
uint8_t cmdIndex = 0;

uint32_t gpsCharsCount = 0; 

// --- NOUVEAU : STRUCTURE DE CONFIGURATION DES CAPTEURS ---
// Optimisé pour consommer un minimum absolu de RAM (12 octets au total)
struct ConfigParams {
    uint16_t lumin_low;
    uint16_t lumin_high;
    uint16_t pressure_min;
    uint16_t pressure_max;
    int8_t min_temp_air;
    int8_t max_temp_air;
    int8_t hygr_mint;
    int8_t hygr_maxt;
    uint8_t active_lumin : 1;
    uint8_t active_temp_air : 1;
    uint8_t active_hygr : 1;
    uint8_t active_pressure : 1;
};

// Initialisation avec TES valeurs par défaut
ConfigParams cfg = {
    10, 768,       // Luminosité
    850, 1080,      // Pression
    -10, 90,        // Température
    0, 90,          // Hygrométrie
    1, 1, 1, 1      // Activations (1=ON, 0=OFF)
};

// ==========================================
// 3. VARIABLES POUR LES INTERRUPTIONS
// ==========================================
volatile uint32_t redPressTime = 0;
volatile uint32_t redReleaseTime = 0;
volatile uint8_t redClicks = 0;
volatile bool redIsPressed = false;

volatile uint32_t greenPressTime = 0;
volatile uint32_t greenReleaseTime = 0;
volatile bool greenIsPressed = false;

void isr_red() {
    uint32_t now = millis();
    bool currentState = (digitalRead(PIN_BTN_RED) == LOW);
    
    if (currentState && !redIsPressed) { 
        if (now - redReleaseTime > 50) { 
            redPressTime = now;
            redIsPressed = true;
        }
    } else if (!currentState && redIsPressed) { 
        if (now - redPressTime > 50) { 
            redReleaseTime = now;
            redIsPressed = false;
            if (redReleaseTime - redPressTime < 1000) { redClicks++; }
        }
    }
}

void isr_green() {
    uint32_t now = millis();
    bool currentState = (digitalRead(PIN_BTN_GREEN) == LOW);
    
    if (currentState && !greenIsPressed) { 
        if (now - greenReleaseTime > 50) { greenPressTime = now; greenIsPressed = true; }
    } else if (!currentState && greenIsPressed) { 
        if (now - greenPressTime > 50) { greenReleaseTime = now; greenIsPressed = false; }
    }
}

// ==========================================
// 4. GESTION DES LEDS
// ==========================================
void set_color(uint8_t r, uint8_t g, uint8_t b) {
    leds.setColorRGB(0, r, g, b);
}

void led_update() {
    uint32_t cycle = millis() % 1000;
    uint8_t r = 0, g = 0, b = 0;
    
    switch (currentLedState) {
        case LED_ERR_RTC:
            if (cycle < 500) { r=255; } else { b=255; } break;
        case LED_ERR_SD_FULL:
            if (cycle < 500) { r=255; } else { r=255; g=255; b=255; } break;
        case LED_ERR_SD_ACCESS:
            if (cycle < 333) { r=255; } else { r=255; g=255; b=255; } break;
        case LED_ERR_SENSOR:
            if (cycle < 500) { r=255; } else { g=255; } break;
        case LED_ERR_SENSOR_INCOHERENT:
            if (cycle < 333) { r=255; } else { g=255; } break;
        case LED_ERR_GPS:
            if (cycle < 500) { r=255; } else { r=255; g=128; } break;
        case LED_MODE_STANDARD:    g=255; break;
        case LED_MODE_CONFIG:      r=255; g=128; break;
        case LED_MODE_ECO:         b=255; break;
        case LED_MODE_MAINTENANCE: r=255; g=40; break;
        case LED_OFF: default:     break;
    }
    
    static uint8_t lastR = 255, lastG = 255, lastB = 255;
    if (r != lastR || g != lastG || b != lastB) {
        leds.setColorRGB(0, r, g, b);
        lastR = r; lastG = g; lastB = b;
    }
}

// ==========================================
// 5. GESTION CARTE SD 
// ==========================================
void archiveFileIfNeeded() {
    DateTime now = rtc.now();
    char archiveName[16];
    int revision = 1;

    while (revision < 10) {
        sprintf(archiveName, "%02d%02d%02d_%d.LOG", now.year() % 100, now.month(), now.day(), revision);
        if (!SD.exists(archiveName)) break; 
        revision++;
    }
    
    if (revision >= 10) { 
        sdFull = true; 
        return; 
    }

    uint32_t copyPosition = 0;
    uint8_t buffer[32]; 

    while (true) {
        File sourceFile = SD.open(currentLogFile, FILE_READ);
        if (!sourceFile) { sdAccessError = true; return; }
        
        sourceFile.seek(copyPosition); 
        if (!sourceFile.available()) {
            sourceFile.close();
            break; 
        }
        int bytesRead = sourceFile.read(buffer, sizeof(buffer));
        sourceFile.close(); 

        File destFile = SD.open(archiveName, FILE_WRITE);
        if (!destFile) { sdAccessError = true; return; }
        
        destFile.seek(destFile.size()); 
        destFile.write(buffer, bytesRead);
        destFile.close(); 

        copyPosition += bytesRead; 
    }
    
    SD.remove(currentLogFile);
}

void log_data_to_sd() {
    if (rtcError || sdFull) return; 

    DateTime now = rtc.now();
    sprintf(currentLogFile, "%02d%02d%02d_0.LOG", now.year() % 100, now.month(), now.day());
    
    File dataFile = SD.open(currentLogFile, FILE_WRITE);
    
    if (!dataFile) { 
        SD.begin(PIN_SD_CS); 
        dataFile = SD.open(currentLogFile, FILE_WRITE);
        if (!dataFile) { sdAccessError = true; return; }
    }
    
    sdAccessError = false; 
    
    if (dataFile.size() >= file_max_size) {
        dataFile.close();
        archiveFileIfNeeded();
        if (sdAccessError || sdFull) return; 

        dataFile = SD.open(currentLogFile, FILE_WRITE);
        if (!dataFile) { sdAccessError = true; return; }
    }

    dataFile.print(now.year(), DEC); dataFile.print(F("/"));
    dataFile.print(now.month(), DEC); dataFile.print(F("/"));
    dataFile.print(now.day(), DEC); dataFile.print(F(","));
    dataFile.print(now.hour(), DEC); dataFile.print(F(":"));
    dataFile.print(now.minute(), DEC); dataFile.print(F(":"));
    dataFile.print(now.second(), DEC); dataFile.print(F(","));

    if (isnan(lastTemp) || isnan(lastHum) || !cfg.active_temp_air || !cfg.active_hygr) {
        dataFile.print(F("NA,NA,"));
    } else {
        dataFile.print(lastTemp, 1); dataFile.print(F(","));
        dataFile.print(lastHum, 1); dataFile.print(F(","));
    }

    if (cfg.active_lumin) {
        dataFile.print(lastLum); dataFile.print(F(","));
    } else {
        dataFile.print(F("NA,"));
    }

    // LECTURE SÉCURISÉE DES COORDONNÉES MICRONMEA
    if (nmea.isValid()) {
        long latitude_mdeg = nmea.getLatitude();
        long longitude_mdeg = nmea.getLongitude();
        float lat_float = (float)latitude_mdeg / 1000000.0;
        float lng_float = (float)longitude_mdeg / 1000000.0;
        dataFile.print(lat_float, 6); dataFile.print(F(","));
        dataFile.print(lng_float, 6);
    } else {
        dataFile.print(F("NA,NA"));
    }

    dataFile.println();
    
    if (dataFile.getWriteError()) sdAccessError = true; 
    dataFile.close();
    
    if (!sdAccessError) {
        Serial.print(F(">>> LOG ENREGISTRE DANS "));
        Serial.println(currentLogFile);
    }
}

// ==========================================
// 6. SURVEILLANCE DES CAPTEURS
// ==========================================
void check_sensors() {
    Wire.beginTransmission(0x68); 
    rtcError = (Wire.endTransmission() != 0);

    static uint32_t lastSensorCheck = 0;
    static uint8_t dhtErrorCount = 0; 
    static bool sensorIsBroken = false;
    static bool sensorIsIncoherent = false;
    
    if (millis() - lastSensorCheck > 2500) {
        lastTemp = dht.readTemperature();
        lastHum = dht.readHumidity();
        
        // On lit la lumière UNIQUEMENT si le capteur est activé
        if (cfg.active_lumin) {
            lastLum = analogRead(PIN_LUM);
        }
        // --- NOUVEAU : Application des règles de configuration ---
        
        // 1. Simulation de capteurs désactivés
        if (!cfg.active_temp_air) lastTemp = NAN;
        if (!cfg.active_hygr) lastHum = NAN;

        if ((isnan(lastTemp) && cfg.active_temp_air) || (isnan(lastHum) && cfg.active_hygr)) { 
            dhtErrorCount++; 
        } else { 
            dhtErrorCount = 0; 
        }
        
        bool lumBroken = cfg.active_lumin && (lastLum == 0 || lastLum >= 1023);
        
        // 2. Utilisation des nouvelles limites dynamiques pour les incohérences
        bool tempError = cfg.active_temp_air && !isnan(lastTemp) && (lastTemp > cfg.max_temp_air || lastTemp < cfg.min_temp_air);
        bool hygrError = cfg.active_hygr && !isnan(lastHum) && (lastHum > cfg.hygr_maxt || lastHum < cfg.hygr_mint);
        
        bool isIncoherent = tempError || hygrError;
        
        if (dhtErrorCount >= 2 || lumBroken) {
            sensorIsBroken = true; sensorIsIncoherent = false;
        } else if (isIncoherent) {
            sensorIsBroken = false; sensorIsIncoherent = true;
        } else {
            sensorIsBroken = false; sensorIsIncoherent = false;
        }
        
        lastSensorCheck = millis();
    }

    static uint32_t lastGpsCheck = 0;
    static uint32_t lastCharsCount = 0;
    static bool gpsIsBroken = false;
    
    if (currentMode == MODE_STANDARD || currentMode == MODE_ECO) {
        if (millis() > 10000 && millis() - lastGpsCheck > 5000) { 
            gpsIsBroken = (gpsCharsCount == lastCharsCount);
            lastCharsCount = gpsCharsCount;
            lastGpsCheck = millis();
        }
    } else {
        lastGpsCheck = millis();
        lastCharsCount = gpsCharsCount;
        gpsIsBroken = false;
    }

    if (currentMode == MODE_MAINTENANCE) { currentLedState = LED_MODE_MAINTENANCE; return; }
    if (currentMode == MODE_CONFIG)      { currentLedState = LED_MODE_CONFIG; return; } 

    if (rtcError)                        { currentLedState = LED_ERR_RTC; }
    else if (sensorIsBroken)             { currentLedState = LED_ERR_SENSOR; }
    else if (sensorIsIncoherent)         { currentLedState = LED_ERR_SENSOR_INCOHERENT; }
    else if (sdAccessError)              { currentLedState = LED_ERR_SD_ACCESS; }
    else if (sdFull)                     { currentLedState = LED_ERR_SD_FULL; }
    else if (gpsIsBroken)                { currentLedState = LED_ERR_GPS; }
    else {
        if (currentMode == MODE_STANDARD)    currentLedState = LED_MODE_STANDARD;
        else if (currentMode == MODE_ECO)    currentLedState = LED_MODE_ECO;
    }
}

// ==========================================
// 7. LECTURE DES BOUTONS 
// ==========================================
void read_buttons() {
    event_red_long_press = false; 
    event_green_long_press = false; 
    event_red_double_click = false;
    
    uint32_t now = millis();
    
    if (redClicks > 0 && !redIsPressed) {
        if (now - redReleaseTime > 400) { 
            if (redClicks == 2) { event_red_double_click = true; }
            redClicks = 0; 
        }
    }
    
    static bool redLongTriggered = false;
    if (redIsPressed) {
        if (!redLongTriggered && (now - redPressTime >= 5000)) {
            event_red_long_press = true; redLongTriggered = true; redClicks = 0; 
        }
    } else {
        redLongTriggered = false;
    }
    
    static bool greenLongTriggered = false;
    if (greenIsPressed) {
        if (!greenLongTriggered && (now - greenPressTime >= 5000)) {
            event_green_long_press = true; greenLongTriggered = true;
        }
    } else {
        greenLongTriggered = false;
    }
}

// ==========================================
// 8. LOGIQUE DES MODES ET COMMANDES
// ==========================================
void process_command(char* cmd) {
    for (uint8_t i = 0; cmd[i] != '\0'; i++) { cmd[i] = toupper(cmd[i]); }

    // On utilise la mémoire Flash (PSTR) pour les 12 nouvelles commandes !
    if (strcmp_P(cmd, PSTR("VERSION")) == 0) { 
        Serial.println(F("MeteoStation V1.0")); 
    }
    else if (strncmp_P(cmd, PSTR("LOG_INTERVALL="), 14) == 0) { 
        log_interval = atol(cmd + 14); Serial.println(F("OK")); 
    }
    else if (strncmp_P(cmd, PSTR("FILE_MAX_SIZE="), 14) == 0) { 
        file_max_size = atol(cmd + 14); Serial.println(F("OK")); 
    }
    // --- LUMINOSITE ---
    else if (strncmp_P(cmd, PSTR("LUMIN_LOW="), 10) == 0) { cfg.lumin_low = atoi(cmd + 10); Serial.println(F("OK")); }
    else if (strncmp_P(cmd, PSTR("LUMIN_HIGH="), 11) == 0) { cfg.lumin_high = atoi(cmd + 11); Serial.println(F("OK")); }
    else if (strncmp_P(cmd, PSTR("LUMIN="), 6) == 0) { cfg.active_lumin = atoi(cmd + 6); Serial.println(F("OK")); }
    // --- TEMPERATURE ---
    else if (strncmp_P(cmd, PSTR("MIN_TEMP_AIR="), 13) == 0) { cfg.min_temp_air = atoi(cmd + 13); Serial.println(F("OK")); }
    else if (strncmp_P(cmd, PSTR("MAX_TEMP_AIR="), 13) == 0) { cfg.max_temp_air = atoi(cmd + 13); Serial.println(F("OK")); }
    else if (strncmp_P(cmd, PSTR("TEMP_AIR="), 9) == 0) { cfg.active_temp_air = atoi(cmd + 9); Serial.println(F("OK")); }
    // --- HYGROMETRIE ---
    else if (strncmp_P(cmd, PSTR("HYGR_MINT="), 10) == 0) { cfg.hygr_mint = atoi(cmd + 10); Serial.println(F("OK")); }
    else if (strncmp_P(cmd, PSTR("HYGR_MAXT="), 10) == 0) { cfg.hygr_maxt = atoi(cmd + 10); Serial.println(F("OK")); }
    else if (strncmp_P(cmd, PSTR("HYGR="), 5) == 0) { cfg.active_hygr = atoi(cmd + 5); Serial.println(F("OK")); }
    // --- PRESSION ---
    else if (strncmp_P(cmd, PSTR("PRESSURE_MIN="), 13) == 0) { cfg.pressure_min = atoi(cmd + 13); Serial.println(F("OK")); }
    else if (strncmp_P(cmd, PSTR("PRESSURE_MAX="), 13) == 0) { cfg.pressure_max = atoi(cmd + 13); Serial.println(F("OK")); }
    else if (strncmp_P(cmd, PSTR("PRESSURE="), 9) == 0) { cfg.active_pressure = atoi(cmd + 9); Serial.println(F("OK")); }
    
    else if (strlen(cmd) > 0) { 
        Serial.println(F("Erreur")); 
    }
}

void mode_standard_run() { 
    if (event_red_double_click) { 
        currentMode = MODE_CONFIG; 
        lastActivityTime = millis(); 
        Serial.println(F("--- MODE CONFIGURATION ACTIF ---"));
    } 
    else if (event_red_long_press) { previousMode = MODE_STANDARD; currentMode = MODE_MAINTENANCE; } 
    else if (event_green_long_press) { currentMode = MODE_ECO; lastLogTime = millis(); } 
    
    if (millis() - lastLogTime >= log_interval) {
        log_data_to_sd();
        lastLogTime = millis();
    }
}

void mode_config_run() { 
    if (event_red_double_click || event_red_long_press) { 
        currentMode = MODE_STANDARD; 
        lastLogTime = millis(); 
        Serial.println(F("Retour au Mode Standard."));
    } 
    
    if (millis() - lastActivityTime > 1800000) { 
        currentMode = MODE_STANDARD; 
        lastLogTime = millis(); 
        Serial.println(F("Timeout : Retour au Mode Standard."));
    } 

    while (Serial.available() > 0) {
        char c = Serial.read();
        lastActivityTime = millis(); 

        if (c == '\n' || c == '\r') {
            if (cmdIndex > 0) {
                cmdBuffer[cmdIndex] = '\0'; 
                process_command(cmdBuffer); 
                cmdIndex = 0; 
            }
        } else if (cmdIndex < sizeof(cmdBuffer) - 1) {
            cmdBuffer[cmdIndex++] = c; 
        }
    }
}

void mode_eco_run() { 
    if (event_red_long_press) { previousMode = MODE_ECO; currentMode = MODE_MAINTENANCE;; lastLogTime = millis(); } 
    if (event_green_long_press) { currentMode = MODE_STANDARD; lastLogTime = millis(); } 
    
    if (millis() - lastLogTime >= (log_interval * 2)) {
        log_data_to_sd();
        lastLogTime = millis();
    }
}

void mode_maintenance_run() {
    if (event_red_long_press) { 
        lcd.clear(); lcd.setRGB(0,0,0); 
        if (sdAccessError || sdFull) {
            if (SD.begin(PIN_SD_CS)) { sdAccessError = false; sdFull = false; }
        }
        currentMode = previousMode; 
        lastLogTime = millis(); 
        return; 
    }

    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate > 1000) {
        lastUpdate = millis();
        
        lcd.setCursor(0, 0);
        if (!rtcError) {
            DateTime now = rtc.now(); 
            
            if(now.hour() < 10) { lcd.print('0'); }
            lcd.print(now.hour());
            lcd.print(F(":"));
            
            if(now.minute() < 10) { lcd.print('0'); }
            lcd.print(now.minute());
            lcd.print(F(":"));
            
            if(now.second() < 10) { lcd.print('0'); }
            lcd.print(now.second());
            
        } else {
            lcd.print(F("RTC ERR "));
        }
        lcd.print(F(" Maint."));

        lcd.setCursor(0, 1);
        lcd.print(F("                ")); 
        lcd.setCursor(0, 1);
        
        if ((millis() / 2000) % 2 == 0) {
            lcd.print(F("T:")); lcd.print(lastTemp, 0); lcd.print(F("C H:")); lcd.print(lastHum, 0); lcd.print(F("%"));
        } else {
            lcd.print(F("L:")); 
            if (cfg.active_lumin) {
                lcd.print(lastLum);
            } else {
                lcd.print(F("NA"));
            }
            lcd.print(nmea.isValid() ? F(" GPS:OK") : F(" GPS:NO"));
        }
    }
}

// ==========================================
// 9. SETUP & LOOP
// ==========================================
void setup() {
    Serial.begin(9600); gpsSerial.begin(9600);
    
    pinMode(PIN_BTN_RED, INPUT_PULLUP); 
    pinMode(PIN_BTN_GREEN, INPUT_PULLUP);
    pinMode(10, OUTPUT); 

    attachInterrupt(digitalPinToInterrupt(PIN_BTN_RED), isr_red, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_BTN_GREEN), isr_green, CHANGE);

    dht.begin(); lcd.begin(16, 2);
    
    if (!rtc.begin()) { rtcError = true; } 
    else if (!rtc.isrunning()) { rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); }

    if (!SD.begin(PIN_SD_CS)) {
        Serial.println(F("[Erreur] Init SD au demarrage."));
        sdAccessError = true;
    }

    currentMode = MODE_STANDARD;
}

void loop() {
    read_buttons();
    
    if (currentMode == MODE_STANDARD || currentMode == MODE_ECO) {
        while (gpsSerial.available() > 0) { 
            nmea.process(gpsSerial.read()); 
            gpsCharsCount++; 
        }
    }
    
    check_sensors(); 
    led_update();    
    
    switch (currentMode) {
        case MODE_STANDARD:    mode_standard_run(); break;
        case MODE_CONFIG:      mode_config_run(); break;
        case MODE_ECO:         mode_eco_run(); break;
        case MODE_MAINTENANCE: mode_maintenance_run(); break;
    }
}

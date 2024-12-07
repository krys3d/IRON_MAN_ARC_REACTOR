#include "TM1637Display.h"
#include "Adafruit_NeoPixel.h"
#include "WiFiManager.h"
#include "time.h"

// Broche de l'Arduino connectée aux NeoPixels
#define PIN 17
#define display_CLK 18
#define display_DIO 19

// Nombre de NeoPixels connectés à l'Arduino
#define NUMPIXELS 35

// ========================VARIABLES UTILES=============================
int Display_backlight = 0;           // Réglage de la luminosité de l'afficheur 0 à 7
int led_ring_brightness = 75;       // Réglage de la luminosité de la LED annulaire de 0 à 255
int led_ring_brightness_flash = 250; // Luminosité LED annulaire en mode flash

// =====================================================================

const int photoresistorPin = A0; // Broche A0 pour la photorésistance
const int ModeNuit = 150;        // Seuil luminosité Mode Nuit
const int ModeMini = 1500;       // Seuil luminosité Mode Mini
const int ModeNormal = 2500;     // Seuil luminosité Mode Normal
const int ModeMax = 3500;        // Seuil luminosité Mode Max

bool isNightModeActive = false;
bool cuckooPlayed = false; // Évite de jouer plusieurs fois par heure

// LEDs pour les effets
#define LED_25 25
#define LED_26 26

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
TM1637Display display(display_CLK, display_DIO);

// Configuration des offsets horaires
const long timeZoneOffset = 3600; // UTC+1
const long daylightOffset = 3600; // Décalage pour l'heure d'été

void setup() {
    pinMode(LED_25, OUTPUT);
    pinMode(LED_26, OUTPUT);
    digitalWrite(LED_25, HIGH);
    digitalWrite(LED_26, HIGH);

    Serial.begin(115200);
    Serial.println("\nDémarrage");

    WiFiManager manager;
    manager.setTimeout(180);
    if (!manager.autoConnect("ARC_REACTOR_00", "password")) {
        Serial.println("Échec de la connexion et délai d'attente dépassé");
        ESP.restart();
    }

    // Initialisation de l'heure via NTP
    configTime(timeZoneOffset, daylightOffset, "pool.ntp.org", "time.nist.gov");

    display.setBrightness(Display_backlight);
    pixels.begin();
    pixels.setBrightness(led_ring_brightness);

    // Initialisation des NeoPixels avec une couleur de base
    for (int i = 0; i < NUMPIXELS; i++) {
        pixels.setPixelColor(i, pixels.Color(0, 50, 255)); // Une couleur par défaut (Rouge, Vert, Bleu)
    }
    pixels.show();
}

void loop() {
    // Récupération de l'heure actuelle
    struct tm timeInfo;
    if (!getLocalTime(&timeInfo)) {
        Serial.println("Échec de la récupération de l'heure.");
        return;
    }

    // Gestion de la luminosité
    int photoresistorValue = analogRead(photoresistorPin);

    if (photoresistorValue <= ModeNuit) {
        if (!isNightModeActive) {
            isNightModeActive = true;
            display.clear(); // Éteindre l'afficheur
            turnOffNeopixels(); // Éteindre les NeoPixels
            updateLEDs(false);  // Éteindre LED_25 et LED_26
        }
    } else {
        if (isNightModeActive) {
            isNightModeActive = false;
            updateBrightness(led_ring_brightness, led_ring_brightness_flash);
            updateLEDs(true); // Allumer LED_25 et LED_26
        }

        if (photoresistorValue <= ModeMini) {
            updateBrightness(3, 50);
            updateDisplayBrightness(1); // Luminosité faible pour l'afficheur
            updateLEDsBrightness(64);  // Baisser l'intensité des LEDs
        } else if (photoresistorValue <= ModeNormal) {
            updateBrightness(12, 125);
            updateDisplayBrightness(4); // Luminosité moyenne pour l'afficheur
            updateLEDsBrightness(128); // Intensité moyenne pour les LEDs
        } else if (photoresistorValue <= ModeMax) {
            updateBrightness(75, 255);
            updateDisplayBrightness(7); // Luminosité maximale pour l'afficheur
            updateLEDsBrightness(255);  // Intensité maximale pour les LEDs
        }
    }

    // Affichage de l'heure (hh:mm) uniquement si le mode nuit n'est pas actif
    if (!isNightModeActive) {
        int hours = timeInfo.tm_hour;
        int minutes = timeInfo.tm_min;
        display.showNumberDecEx(hours, 0b01000000, true, 2, 0);
        display.showNumberDecEx(minutes, 0b00000000, true, 2, 2);
    }

    // Animation toutes les heures (cuckoo)
    if (timeInfo.tm_min == 0 && !cuckooPlayed) {
        if (!isNightModeActive) { // Ne pas jouer en mode nuit
            display_cuckoo();
        }
        cuckooPlayed = true; // Éviter de rejouer dans la même minute
    } else if (timeInfo.tm_min != 0) {
        cuckooPlayed = false; // Réinitialiser pour l'heure suivante
    }
}

void updateBrightness(int ringBrightness, int flashBrightness) {
    led_ring_brightness = ringBrightness;
    led_ring_brightness_flash = flashBrightness;
    pixels.setBrightness(led_ring_brightness);

    // Définir une couleur pour les NeoPixels après mise à jour de la luminosité
    for (int i = 0; i < NUMPIXELS; i++) {
        pixels.setPixelColor(i, pixels.Color(0, 50, 255)); // Couleur (Rouge, Vert, Bleu)
    }

    pixels.show();
}

void updateDisplayBrightness(int brightness) {
    Display_backlight = brightness;
    display.setBrightness(Display_backlight);
}

void updateLEDs(bool state) {
    digitalWrite(LED_25, state ? HIGH : LOW);
    digitalWrite(LED_26, state ? HIGH : LOW);
}

void updateLEDsBrightness(int brightness) {
    digitalWrite(LED_25, brightness);
    digitalWrite(LED_26, brightness);
}

void turnOffNeopixels() {
    pixels.clear();
    pixels.show();
}

void display_cuckoo() {
    // Défilement des chiffres de 0 à 9
    for (int i = 0; i <= 9; i++) {
        display.showNumberDecEx(i, 0b00000000, true, 1, 0);
        display.showNumberDecEx(i, 0b10000000, true, 1, 1);
        display.showNumberDecEx(i, 0b00000000, true, 1, 2);
        display.showNumberDecEx(i, 0b00000000, true, 1, 3);
        delay(30); // Ajustez la vitesse du défilement si nécessaire
    }

    // Flash lumineux
    for (int j = 0; j < 3; j++) { // Répéter le flash 3 fois
        for (int i = 0; i < NUMPIXELS; i++) {
            pixels.setPixelColor(i, pixels.Color(255, 255, 255)); // Lumière blanche intense
        }
        pixels.setBrightness(led_ring_brightness_flash); // Luminosité maximale
        pixels.show();
        delay(200); // Durée du flash en millisecondes

        // Éteindre les NeoPixels après le flash
        turnOffNeopixels();
        delay(200); // Temps avant le prochain flash
    }

    // Restaurer l'état normal des NeoPixels
    updateBrightness(led_ring_brightness, led_ring_brightness_flash);
}

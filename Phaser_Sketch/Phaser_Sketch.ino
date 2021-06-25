
#include <DFPlayer_Mini_Mp3.h>
#include <SoftwareSerial.h>
#include <Bounce2.h>
#include <Math.h>

// uncomment to enable output via Serial Port
#define PHASER_ENABLE_DEBUG_OUTPUT        1
#define DEBUG_SERIAL_DELAY                5

/*------------------------------------*/
/* Sofware Serial configuration       */
/*------------------------------------*/

const byte rxPin = 8;
const byte txPin = 9;

SoftwareSerial swSerial(rxPin, txPin);

/*------------------------------------*/
/* PIN configurations                 */
/*------------------------------------*/

const byte clockPin = 4;
const byte latchPin = 2;
const byte dataPin = 3;

const byte phaserLightPin = 6;

const byte increasePin = 5;
const byte firePin = 6;
const byte decreasePin = 7;

const byte batteryProbePIN = A0;

/*------------------------------------*/
/* Sound configuration                */
/*------------------------------------*/

const byte startupSound = 1;
const byte phaserAdjustSound = 2;
const byte fireSingleSound = 3;
const byte fireContStartSound = 4;
const byte fireContSound = 5;
const byte errorSound = 6;
const byte batteryLevelSound = 7;
const byte volumeControlSound = 8;
const byte acceptSound = 9;

/*------------------------------------*/
/* Data stored                        */
/*------------------------------------*/
byte powerLevel = 0;
byte volume = 15;
byte batteryLevel = 0;

/*------------------------------------*/
/*Timings and delays                  */
/*------------------------------------*/
byte startupStepTime = 2;
byte shiftOutDelay = 1;

/*-----------------------------------------------------------------*/
/* Solid State Machine konfiguration                               */
/*-----------------------------------------------------------------*/

boolean saveMode = true;
boolean fireMode = false;
boolean menuChangeVolume = false;
boolean menuShowBatteryLevel = false;


/*-----------------------------------------------------------------*/
/* Button measurement and control in ms unless stated otherwise    */
/*-----------------------------------------------------------------*/

float minBatteryVoltage = 3.0;
float maxBatteryVoltage = 4.2;

float minBatteryVoltageADValue = minBatteryVoltage * (1023.0 / 5.0);
float maxBatteryVoltageADValue = maxBatteryVoltage * (1023.0 / 5.0);
float voltageADRange = maxBatteryVoltageADValue - minBatteryVoltageADValue;

byte batterySamples = 15;

byte numberOfLEDs = 16;
byte debounceDelay = 2;

byte shortpressTime = 15;
byte longpressTime = 200;

bool firePressed = false;           // is BTN currently pressed
bool fireShortpress = false;        // short press event (after button was released)
bool fireLongpress = false;         // is a long press event currently in progress (longpress time)
bool fireLongpressOver = false;     // BTN long press event (after button was released)
unsigned long fireBTNPressTime = 0;       // time in millis() when the BTN was last pressed
unsigned long fireBTNReleaseTime = 0;     // time in millis() when the BTN was last released
unsigned long timeSinceFireBTNPress = 0;

bool firing = false;

bool increasePressed = false;
bool incLongpress = false;
bool incLongpressOver = false;
bool incShortpress = false;
unsigned long increaseBTNPressTime = 0;
unsigned long increaseBTNReleaseTime = 0;
unsigned long timeSinceIncreaseBTNPress = 0;

bool decreasePressed = false;
bool decLongpress = false;
bool decShortpress = false;
bool decLongpressOver = false;
unsigned long decreaseBTNPressTime = 0;
unsigned long decreaseBTNReleaseTime = 0;
unsigned long timeSinceDecreaseBTNPress = 0;

Bounce fireDebouncer = Bounce();
Bounce increaseDebouncer = Bounce();
Bounce decreaseDebouncer = Bounce();

void setup() {

#ifdef PHASER_ENABLE_DEBUG_OUTPUT
  Serial.begin(9600);
  Serial.println("Starte Setup");
  delay(DEBUG_SERIAL_DELAY);
#endif

  // initialise software serial control for sound module and sound module
  //  pinMode(rxPin, INPUT);
  //  pinMode(txPin, OUTPUT);

  swSerial.begin(9600);           // swSerial must be started first - only supported UART baud rate is 9600

  //  audioController.init();
  //  audioController.setVolume(volume);

  mp3_set_serial(swSerial);
  mp3_set_volume (15);

  // initialise control buttons
  pinMode(phaserLightPin, OUTPUT);

  pinMode(firePin, INPUT_PULLUP);
  pinMode(increasePin, INPUT_PULLUP);
  pinMode(decreasePin, INPUT_PULLUP);

  fireDebouncer.attach(firePin);
  fireDebouncer.interval(debounceDelay);
  increaseDebouncer.attach(increasePin);
  increaseDebouncer.interval(debounceDelay);
  decreaseDebouncer.attach(decreasePin);
  decreaseDebouncer.interval(debounceDelay);

  // Initialise display settings and clear any residual bits
  pinMode(clockPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  digitalWrite(clockPin, LOW);
  digitalWrite(latchPin, LOW);
  digitalWrite(dataPin, LOW);

  // Initialize Battery sense calculation boundaries
  /* analogRead gives a value between 0 and 1023, broken down to a value that corresponds to the number of display LEDs
     In addition the valid value range is between 613,8 (3 V = shutdown voltage of the battery protection circuit) and 859,32 (4.2 V max voltage of the LiIon Cell)
     The min and max voltage may be adjusted so the actual boundings are calculated in each cycle.
  */

  float minBatteryVoltageADValue = minBatteryVoltage * (1023.0 / 5.0);
  float maxBatteryVoltageADValue = maxBatteryVoltage * (1023.0 / 5.0);
  float voltageADRange = maxBatteryVoltageADValue - minBatteryVoltageADValue;

  setDisplay(powerLevel);

  // sound boot complete and system ready
  playSound(startupSound, true, false);

#ifdef PHASER_ENABLE_DEBUG_OUTPUT
  Serial.println("Setup beendet");
  delay(DEBUG_SERIAL_DELAY);
#endif
}

void loop() {

  updateInput();

  processInput();

}

void updateInput() {

  fireDebouncer.update();
  increaseDebouncer.update();
  decreaseDebouncer.update();

  // Flank fireBTN pressed
  if (fireDebouncer.fell()) {
#ifdef PHASER_ENABLE_DEBUG_OUTPUT
    Serial.println("Fire pressed");
    delay(DEBUG_SERIAL_DELAY);
#endif
    firePressed = true;
    fireBTNPressTime = millis();
  }

  // Flank fireBTN released
  if (fireDebouncer.rose()) {
#ifdef PHASER_ENABLE_DEBUG_OUTPUT
    Serial.println("Fire released");
    delay(DEBUG_SERIAL_DELAY);
#endif
    firePressed = false;
    timeSinceFireBTNPress = millis();
  }

  // Flank decreaseBTN pressed
  if (increaseDebouncer.fell()) {
#ifdef PHASER_ENABLE_DEBUG_OUTPUT
    Serial.println("Increase pressed");
    delay(DEBUG_SERIAL_DELAY);
#endif
    increasePressed = true;
    increaseBTNPressTime = millis();
  }

  // Flank decreaseBTN released
  if (increaseDebouncer.rose()) {
#ifdef PHASER_ENABLE_DEBUG_OUTPUT
    Serial.println("increase released");
    delay(DEBUG_SERIAL_DELAY);
#endif
    increasePressed = false;
    timeSinceIncreaseBTNPress = millis();
  }

  // Flank increaseBTN pressed
  if (decreaseDebouncer.fell()) {
#ifdef PHASER_ENABLE_DEBUG_OUTPUT
    Serial.println("decrease pressed");
    delay(DEBUG_SERIAL_DELAY);
#endif
    decreasePressed = true;
    decreaseBTNPressTime = millis();
  }

  // Flank increaseBTN released
  if (decreaseDebouncer.rose()) {
#ifdef PHASER_ENABLE_DEBUG_OUTPUT
    Serial.println("decrease released");
    delay(DEBUG_SERIAL_DELAY);
#endif
    decreasePressed = false;
    timeSinceDecreaseBTNPress = millis();
  }

  fireLongpress = (((millis() - fireBTNPressTime) > longpressTime) && firePressed == true);
  fireShortpress = (((millis() - timeSinceFireBTNPress) <= shortpressTime) && firePressed == false);
  fireLongpressOver = ((fireBTNReleaseTime - fireBTNPressTime)) > longpressTime && (((millis() - fireBTNReleaseTime) <= shortpressTime) && firePressed == false);

  incLongpress = (((millis() - increaseBTNPressTime) > longpressTime) && increasePressed == true);
  incShortpress = (((millis() - timeSinceIncreaseBTNPress) <= shortpressTime) && increasePressed == false);
  incLongpressOver = ((increaseBTNReleaseTime - increaseBTNPressTime)) > longpressTime && (((millis() - increaseBTNReleaseTime) <= shortpressTime) && increasePressed == false);

  decLongpress = (((millis() - decreaseBTNPressTime) > longpressTime) && decreasePressed == true);
  decShortpress = (((millis() - timeSinceDecreaseBTNPress) <= shortpressTime) && decreasePressed == false);
  decLongpressOver = ((decreaseBTNReleaseTime - decreaseBTNPressTime)) > longpressTime && (((millis() - decreaseBTNReleaseTime) <= shortpressTime) && decreasePressed == false);

}

void processInput() {

#ifdef PHASER_ENABLE_DEBUG_OUTPUT
  Serial.print("processInput saveMode:");
  Serial.print(saveMode);
  Serial.print(" fireMode:");
  Serial.print(fireMode);
  Serial.print(" menuChangeVolume:");
  Serial.println(menuChangeVolume);
  Serial.print(" menuShowBatteryLevel:");
  Serial.println(menuShowBatteryLevel);
  delay(DEBUG_SERIAL_DELAY);
#endif

  if (saveMode) {
    processSaveMode();
  }

  if (fireMode) {
    processFireMode();
  }

  if (menuChangeVolume) {
    processMenuChangeVolume();
  }

  if (menuShowBatteryLevel) {
    processShowBatteryLevel();
  }

}

void processSaveMode() {

#ifdef PHASER_ENABLE_DEBUG_OUTPUT
  Serial.println("processSaveMode");
  delay(DEBUG_SERIAL_DELAY);
#endif

  if (incLongpress) {

    startupSequence();

    saveMode = false;
    menuChangeVolume = false;
    menuShowBatteryLevel = false;
    fireMode = true;

    return;

  }

  if (decLongpress) {

    playSound(batteryLevelSound, true, false);

    saveMode = false;
    fireMode = false;
    menuChangeVolume = false;
    menuShowBatteryLevel = true;

    return;

  }

}

void processShowBatteryLevel() {

#ifdef PHASER_ENABLE_DEBUG_OUTPUT
  Serial.println("processShowBatteryLevel");
  delay(DEBUG_SERIAL_DELAY);
#endif

  updatePowerLevel();

  setDisplay(batteryLevel);

  if (fireShortpress) {

    playSound(volumeControlSound, true, false);

    saveMode = false;
    fireMode = false;
    menuChangeVolume = true;
    menuShowBatteryLevel = false;

    return;

  }

  if (fireLongpress) {

    playSound(acceptSound, true, false);

    saveMode = true;
    fireMode = false;
    menuChangeVolume = false;
    menuShowBatteryLevel = false;

    return;

  }

}

void processMenuChangeVolume() {

#ifdef PHASER_ENABLE_DEBUG_OUTPUT
  Serial.println("processMenuChangeVolume");
  delay(DEBUG_SERIAL_DELAY);
#endif

  byte volumeDisplay = volume * (numberOfLEDs / 30.0);

  setDisplay(volumeDisplay);

  if (fireShortpress) {

    playSound(batteryLevelSound, true, false);

    saveMode = false;
    fireMode = false;
    menuChangeVolume = false;
    menuShowBatteryLevel = true;

    return;

  }

  if (fireLongpress) {

    playSound(acceptSound, true, false);

    saveMode = true;
    fireMode = false;
    menuChangeVolume = false;
    menuShowBatteryLevel = false;

    return;

  }

  if (incShortpress) {

    if (volume < 30) {

      volume = volume + B01;

      if (volume == 16) {
        playSound(phaserAdjustSound, false, false);
      }

    } else {

      playSound(errorSound, false, false);

    }
  }

  if (decShortpress) {

    if (volume > 0) {

      volume = volume - B01;

      if (volume == 16) {
        playSound(phaserAdjustSound, false, false);
      }

    } else {

      playSound(errorSound, false, false);

    }
  }
}

void updatePowerLevel() {

  for (int i = 0; i < batterySamples; i++) {
    int batterySenseValue = analogRead(batteryProbePIN);

    // shift read AD value to a range starting at 0
    batterySenseValue = batterySenseValue - minBatteryVoltageADValue;

    float indicatorLevel = batterySenseValue * (numberOfLEDs / voltageADRange);
    indicatorLevel = indicatorLevel * 2;
    indicatorLevel = round(indicatorLevel);

    batteryLevel += indicatorLevel / 2;
  }

  batteryLevel = round(batteryLevel / batterySamples);

#ifdef PHASER_ENABLE_DEBUG_OUTPUT
  Serial.print(" batteryLevel: ");
  Serial.print(batteryLevel);
  delay(DEBUG_SERIAL_DELAY);
#endif

}

void processFireMode() {

#ifdef PHASER_ENABLE_DEBUG_OUTPUT
  Serial.println("processMenuMode");
  delay(DEBUG_SERIAL_DELAY);
#endif

  if (increasePressed && decreasePressed) {
    shutdownSequence();

    saveMode = true;
    fireMode = false;
    menuChangeVolume = false;
    menuShowBatteryLevel = false;

    return;
  }

  fire();

  if (incLongpress && !decreasePressed) {
    increaseLevel(false);
    return;
  } else if (incShortpress && !decreasePressed) {
    increaseLevel(true);
    return;
  }

  if (decLongpress && !increasePressed) {
    decreaseLevel(false);
    return;
  } else if (decShortpress && !increasePressed) {
    decreaseLevel(true);
    return;
  }

}

void fire() {

#ifdef PHASER_ENABLE_DEBUG_OUTPUT
  Serial.println("   Firing");
  delay(DEBUG_SERIAL_DELAY);
#endif

  if (fireShortpress && !firing) {
    digitalWrite(phaserLightPin, HIGH);
    playSound(fireSingleSound, true, false);
    digitalWrite(phaserLightPin, LOW);
  }

  if (fireLongpress) {

    if (!firing) {
      firing = true;
      digitalWrite(phaserLightPin, HIGH);
      playSound(fireContStartSound, false, false);
    } else {
      playSound(fireContSound, false, true);
    }

  }

  if (fireLongpressOver) {
    firing = false;
    digitalWrite(phaserLightPin, LOW);
    stopAllSound();
  }

}

void increaseLevel(bool sound) {

#ifdef PHASER_ENABLE_DEBUG_OUTPUT
  Serial.println("   Increase ");
  delay(DEBUG_SERIAL_DELAY);
#endif

  if (powerLevel < 16) {

    powerLevel = powerLevel + B01;

    setDisplay(powerLevel);

    if (sound || powerLevel == 16) {
      playSound(phaserAdjustSound, false, false);
    }

  } else {

    if (sound) {
      playSound(errorSound, false, false);
    }

  }

}

void decreaseLevel(bool sound) {

#ifdef PHASER_ENABLE_DEBUG_OUTPUT
  Serial.println("   Decrease");
  delay(DEBUG_SERIAL_DELAY);
#endif

  if (powerLevel > 1) {

    powerLevel = powerLevel - B01;

    setDisplay(powerLevel);

    if (sound || powerLevel == 1) {
      playSound(phaserAdjustSound, false, false);
    }

  } else {

    if (sound) {
      playSound(errorSound, false, false);
    }

  }

}

void startupSequence() {

#ifdef PHASER_ENABLE_DEBUG_OUTPUT
  Serial.println("startupSequence");
  delay(DEBUG_SERIAL_DELAY);
#endif

  while (powerLevel <= 16) {
    powerLevel = powerLevel + B01;
    setDisplay(powerLevel);
    delay(startupStepTime);
  }

  delay(startupStepTime);

  while (powerLevel > 1) {
    powerLevel = powerLevel - B01;
    setDisplay(powerLevel);
    delay(startupStepTime);
  }

  powerLevel = 1;

  playSound(phaserAdjustSound, false, false);
}

void shutdownSequence() {

#ifdef PHASER_ENABLE_DEBUG_OUTPUT
  Serial.println("shutdownSequence");
  delay(DEBUG_SERIAL_DELAY);
#endif

  while (powerLevel > 0) {
    powerLevel = powerLevel - B01;
    setDisplay(powerLevel);
    delay(startupStepTime);
  }

  playSound(phaserAdjustSound, false, false);
}


/*
   Method forces the current powerLevel to be displayed at the LED Display Panel
*/
void setDisplay(byte data) {

#ifdef PHASER_ENABLE_DEBUG_OUTPUT
  Serial.print("   Data");
  Serial.println(powerLevel);
  delay(DEBUG_SERIAL_DELAY);
#endif

  for (int i = 0; i < 16; i++) {
    digitalWrite(clockPin, HIGH);
    delay(shiftOutDelay);
    digitalWrite(clockPin, LOW);
  }

  digitalWrite(dataPin, HIGH);

  for (int i = 1; i <= data; i++) {
    digitalWrite(clockPin, HIGH);
    delay(shiftOutDelay);
    digitalWrite(clockPin, LOW);
  }

  digitalWrite(dataPin, LOW);

  digitalWrite(latchPin, HIGH);
  delay(shiftOutDelay);
  digitalWrite(latchPin, LOW);

}

/*
   Method plays a dedicated sound as stated in the sound configuration section.
   If waitBusy is set the method won't return until the player finished playback
*/
void playSound(short soundName, bool waitBusy, bool onlyNotActive) {

#ifdef PHASER_ENABLE_DEBUG_OUTPUT
  Serial.print("   Play Sound ");
  Serial.println(soundName);
  delay(DEBUG_SERIAL_DELAY);
#endif

  //  if(onlyNotActive && audioController.isPlaybackActive()) {
  //    return;
  //  }

  //  audioController.playFileIndex(soundName);

  if (waitBusy) {
    //    audioController.waitPlaybackFinished();
  }
}

void stopAllSound() {
  /*audioController.stop(false);*/
}

/*
  Clapster 1.0 - Interactive Rhythm Learning Device
  Voroshka Software - Hardware branch, 2025.
  */
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <string.h>
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// -------------------- Pin definitions --------------------
const int micSensor = A0;  // Microphone input 
const int programSwitch = 2; // Main button: play / record / cancel - in Learning Mode; Ready - in Gaming Mode
const int modeSwitch = 6; // Mode switch button: Learning / Gaming (INPUT_PULLUP)
const int buzzer = 3; // Buzzer pin

// -------------------- Detection constants --------------------
// Threshold for microphone to count a knock
const int threshold = 525;
// Tolerances for validating patterns in Learning mode (percentage)
const int rejectValue = 25; // Max difference per interval (%)
const int averageRejectValue = 15; // Max average difference (%)
// Debounce time for knocks (ms)
const int knockFadeTime = 80;
// Max number of intervals in a pattern and max silence between knocks
const int maximumKnocks = 20;
const int knockComplete = 1500; // ms of silence to end pattern recording

// -------------------- EEPROM layout --------------------
const int EEPROM_ADDR_COUNT = 0;
const int EEPROM_ADDR_DATA = 1;

// -------------------- Pattern storage --------------------
// secretCode is stored as intervals mapped to 0–100 (% of the longest interval)
int secretCode[maximumKnocks] = {
  50, 25, 25, 50, 100, 50,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};  // initial default pattern
int knockReadings[maximumKnocks]; // Raw recorded intervals in ms
int micSensorValue = 0; // Last microphone reading

// -------------------- Mode flags --------------------
// recordingMode: true when user is teaching a new pattern
bool recordingMode = false;
// gamingMode: false = Learning, true = Gaming
bool gamingMode = false;
// isPlayingBack: true when device is playing pattern (ignore mic during this)
bool isPlayingBack = false;

// -------------------- Program / Ready button state (D2) --------------------
bool buttonWasDown = false; // previous button state
unsigned long buttonDownTime = 0; // when button was pressed

// -------------------- Mode button state (D6, INPUT_PULLUP) --------------------
bool modeButtonLastState = HIGH;

// -------------------- Gaming mode configuration --------------------
const int maxGameAttempts = 3; // attempts per pattern
const int NUM_GAME_PATTERNS = 3;// number of predefined patterns
const int gamePatternMaxLength = 8;// maximum intervals in a game pattern
// Tolerances specifically for Gaming mode (slightly softer)
const int gameRejectValue = 30; // max interval difference (%)
const int gameAverageRejectValue = 20; // max average diff (%)
// gameState used as a simple state machine for the game:
// 0 = waiting for Ready to start first attempt on current pattern
// 1 = waiting for Ready to retry current pattern
// 2 = waiting for Ready to advance to next pattern and play it
// 3 = player has won the game, waiting for Ready to restart
// 4 = player has lost, waiting for Ready to restart
int currentPatternIndex = 0;// index of current game pattern
int currentAttempt = 0;// attempt count for current pattern
int gameState = 0;// current game state

// Number of intervals for each game pattern (not number of knocks)
const int gamePatternLengths[NUM_GAME_PATTERNS] = {5, 4, 5};

// Predefined game patterns: each value is 0–100 representing timing ratio
const int gamePatterns[NUM_GAME_PATTERNS][gamePatternMaxLength] = {
  {50,50,100,50,50,0,0,0}, // 1: 8-8-4-8-8-4
  {100,100,50,50,0,0,0,0}, // 2: 8-8-4-4-4
  {50,50,50, 50,100,0,0,0} // 3: 8-8-8-8-4-4
};

// Text labels for LCD title row in Gaming mode
const char* gamePatternTitles[NUM_GAME_PATTERNS] = {
  "1: 8-8-4-8-8-4",
  "2: 4-4-8-8-8",
  "3: 8-8-8-8-4-4"};

// -------------------- Async scrolling state (Gaming mode messages) --------------------
// This is used for long messages on the second LCD line.
bool scrollActive = false;
char scrollLine1[17]; // top line
char scrollBuffer[64]; // text to scroll on the second line
int  scrollIndex  = 0; // current scroll index
int  scrollLen = 0; // length of scrollBuffer
int  scrollDelay = 250; // ms per scroll step
unsigned long lastScrollTime = 0; // last time we scrolled

// -------------------- LCD helper functions --------------------
// Show two lines on LCD and clear previous content
void showMessage(const char* line1, const char* line2 = "") {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(line1);
  lcd.setCursor(0,1);
  lcd.print(line2);}

// Start asynchronous scrolling of a text on the second line.
// Actual scrolling is handled inside loop().
void scrollSecondLine(const char* line1, const char* text, int delayMs) {
  // Store first line
  strncpy(scrollLine1, line1, 16);
  scrollLine1[16] = '\0';
  // Store scrolling text
  strncpy(scrollBuffer, text, 63);
  scrollBuffer[63] = '\0';
  // Reset scrolling state
  scrollLen      = strlen(scrollBuffer);
  scrollIndex    = 0;
  scrollDelay    = delayMs;
  lastScrollTime = 0;
  scrollActive   = true;
  // Draw first line; second line will be updated gradually
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(scrollLine1);}

// Startup sequence with a short friendly melody
void showWelcome() {
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Welcome!");
  lcd.setCursor(0, 1);
  lcd.print("Clapster 1.0");
  // Simple 3-note motif for feedback at startup
  int motifFreqs[3] = {330, 392, 440};
  for (int i = 0; i < 3; i++) {
    lcd.setCursor(8 + (i % 3), 0);
    lcd.print(".");
    tone(buzzer, motifFreqs[i % 3], 180);
    delay(500);}
  noTone(buzzer);
  lcd.clear();}

// -------------------- Sound helper functions --------------------

// Short click sound used to represent a clap hit
void playKnockClick() {
  tone(buzzer, 2000, 80);  // 2 kHz, ~80 ms
}

// Siren-like sound for errors / wrong pattern
void playErrorSiren() {
  for (int i = 0; i < 3; i++) {
    tone(buzzer, 800, 150);
    delay(180);

    // While playing, user can still switch mode with D6
    if (gamingMode) {
      int tmpState = digitalRead(modeSwitch);
      if (tmpState == LOW && modeButtonLastState == HIGH) {}} // Mode change will be handled by checkModeSwitchImmediate() in loop
    tone(buzzer, 500, 150);
    delay(180);}}

// Triad sound for success in game/ correct pattern
void playSuccessTriad() {
  int freqs[3] = {262, 330, 392}; // C4–E4–G4
  for (int i = 0; i < 3; i++) {
    tone(buzzer, freqs[i], 200);
    delay(220);}}

// Short sound when pattern is successfully saved
void playPatternSavedTone() {
  tone(buzzer, 1200, 120);
  delay(150);
  tone(buzzer, 900, 120);
  delay(150);}

// -------------------- EEPROM helper functions --------------------
// Save current pattern from secretCode[] into EEPROM
void savePatternToEEPROM(int count) {
  // Clamp count to valid range
  if (count < 0) count = 0;
  if (count > maximumKnocks) count = maximumKnocks;
  // Save count of intervals
  EEPROM.update(EEPROM_ADDR_COUNT, (byte)count);
  // Save each interval (0–100)
  for (int i = 0; i < maximumKnocks; i++) {
    int v = secretCode[i];
    if (v < 0)   v = 0;
    if (v > 100) v = 100;
    EEPROM.update(EEPROM_ADDR_DATA + i, (byte)v);  }
  Serial.print("Pattern saved to EEPROM, count = ");
  Serial.println(count);
}
// Load pattern from EEPROM into secretCode
bool loadPatternFromEEPROM() {
  byte count = EEPROM.read(EEPROM_ADDR_COUNT);
  // If EEPROM is empty or invalid, fall back to default pattern
  if (count == 0xFF || count > maximumKnocks) {
    Serial.println("EEPROM empty/invalid, using default pattern from code.");
    int defaultCount = 0;
    for (int i = 0; i < maximumKnocks; i++) {
      if (secretCode[i] > 0) defaultCount++;    }
    savePatternToEEPROM(defaultCount);
    return false;  }

  // Load pattern into secretCode[]
  for (int i = 0; i < maximumKnocks; i++) {
    byte v = EEPROM.read(EEPROM_ADDR_DATA + i);
    if (v > 100) v = 100;
    secretCode[i] = (int)v;  }

  Serial.print("Pattern loaded from EEPROM, count = ");
  Serial.println(count);
  return true;}

// -------------------- Playback for Learning mode --------------------
// Plays pattern stored in secretCode[] as a series of clicks.
void playStoredPattern() {
  isPlayingBack = true;  // while true we ignore microphone input
  Serial.println("Playing stored pattern");
  showMessage("Playing back", "stored pattern");
  const int baseInterval = 200;
  const int minDelay     = 120;
  const int maxDelay     = baseInterval + 150;
  // Check if there is any non-zero interval
  bool hasPattern = false;
  for (int i = 0; i < maximumKnocks; i++) {
    if (secretCode[i] > 0) {
      hasPattern = true;
      break;}}
  // If no pattern yet, prompt user to record one
  if (!hasPattern) {
    showMessage("No pattern", "Hold A to rec");
    delay(1000);
    showMessage("Learning mode", "Listening...");
    isPlayingBack = false;
    return;  }
  // First knock
  playKnockClick();
  delay(50);
  // Remaining knocks based on stored intervals
  for (int i = 0; i < maximumKnocks; i++) {
    if (secretCode[i] > 0) {
      int waitTime = map(secretCode[i], 0, 100, minDelay, maxDelay);
      delay(waitTime);
      playKnockClick();}  }
  showMessage("Learning mode", "Listening...");
  isPlayingBack = false;}

// -------------------- Function prototypes --------------------
void listenToSecretKnock();
boolean validateKnock();
void triggerDoorUnlock();
// Gaming mode function prototypes
void enterGamingMode();
void resetGame();
void playGamePattern(int patternIndex);
boolean validateGamePattern(int patternIndex, int currentKnockCount);
boolean listenToGameKnockAndValidate();
void playGameRound();
void checkModeSwitchImmediate();// helper for immediate mode switching

// -------------------- Core setup and main loop --------------------
void setup() {
  // Program / Ready button uses INPUT
  pinMode(programSwitch, INPUT);
  // Mode button uses pull-up; LOW means pressed
  pinMode(modeSwitch, INPUT_PULLUP);
  modeButtonLastState = digitalRead(modeSwitch);
  Serial.begin(9600);
  Serial.println("Program start.");
  showWelcome();
  // Load pattern from EEPROM or save the default one
  bool loaded = loadPatternFromEEPROM();
  if (loaded) {
    Serial.println("Using pattern from EEPROM.");
  } else {
    Serial.println("Using default pattern (and saved it to EEPROM).");}
  showMessage("Learning mode", "Listening...");}

void loop() {
  // 1. Always check mode switch first so user can switch at any time
  checkModeSwitchImmediate();
  // 2. Handle button logic in LEARNING mode if not playing back
  if (!gamingMode && !isPlayingBack) {
    int buttonState = digitalRead(programSwitch);
    // Button press detected
    if (buttonState == HIGH && !buttonWasDown) {
      buttonWasDown  = true;
      buttonDownTime = millis();    }
    // Button release detected
    if (buttonState == LOW && buttonWasDown) {
      unsigned long pressDuration = millis() - buttonDownTime;
      buttonWasDown = false;
      if (recordingMode) {
        // While in recording mode, any press cancels recording
        Serial.println("Recording cancelled (loop).");
        showMessage("Recording", "Cancelled");
        delay(700);
        recordingMode = false;
        showMessage("Learning mode", "Listening...");
      } else {
        // Short press: playback stored pattern
        // Long press: enter recording mode
        if (pressDuration < 600) {
          playStoredPattern();
        } else if (pressDuration >= 1200) {
          recordingMode = true;
          showMessage("Recording mode", "Tap pattern");
          Serial.println("Mode: RECORDING");}}}}

  // 3. Handle Ready button / game logic in GAMING mode
  if (gamingMode && !isPlayingBack) {
    int buttonState = digitalRead(programSwitch);
    // Ready button press
    if (buttonState == HIGH && !buttonWasDown) {
      buttonWasDown  = true;
      buttonDownTime = millis();}
    // Ready button release
    if (buttonState == LOW && buttonWasDown) {
      unsigned long pressDuration = millis() - buttonDownTime;
      buttonWasDown = false;
      (void)pressDuration; // press duration not used in Gaming mode
      // Game state transitions based on Ready button
      if (gameState == 0 || gameState == 1) {
        // First attempt or retry current pattern
        playGameRound();
      } else if (gameState == 2) {
        // Pattern passed: move to next pattern and start it immediately
        currentPatternIndex++;
        if (currentPatternIndex >= NUM_GAME_PATTERNS) {
          currentPatternIndex = 0;}
        currentAttempt = 0;
        gameState      = 0;
        playGameRound();
      } else if (gameState == 3 || gameState == 4) {
        // Win or loss: restart whole game
        resetGame();}}}

  // 4. Read microphone input
  micSensorValue = analogRead(micSensor);
  // If not playing back, a loud knock may start pattern logic
  if (!isPlayingBack && micSensorValue >= threshold) {
    if (gamingMode) {
      // In Gaming mode the recording of knocks happen inside game logic;
      // here we do nothing to avoid conflicts.
    } else {
      // In Learning mode we record a full pattern and validate it
      listenToSecretKnock();}}

  // 5. Handle async scrolling for long messages
  if (scrollActive) {
    unsigned long nowScroll = millis();
    if (lastScrollTime == 0 || nowScroll - lastScrollTime >= (unsigned long)scrollDelay) {
      lastScrollTime = nowScroll;
      // Redraw first line (title)
      lcd.setCursor(0, 0);
      lcd.print("                ");
      lcd.setCursor(0, 0);
      lcd.print(scrollLine1);
      // Draw next window of 16 chars of scrolling text
      lcd.setCursor(0, 1);
      for (int i = 0; i < 16; i++) {
        int idx = scrollIndex + i;
        char c = (idx < scrollLen) ? scrollBuffer[idx] : ' ';
        lcd.print(c);}
      scrollIndex++;
      if (scrollIndex > scrollLen) {
        scrollActive = false; // stop scrolling when text is done
      }}}}

// -------------------- Immediate mode switch --------------------
// This function is called very often to allow switch modes at any time.
void checkModeSwitchImmediate() {
  int modeState = digitalRead(modeSwitch);
  // If button went from HIGH to LOW, user pressed mode switch
  if (modeState == LOW && modeButtonLastState == HIGH) {
    // Toggle between modes
    gamingMode    = !gamingMode;
    recordingMode = false;   // cannot stay in recording while switching mode
    scrollActive  = false;   // stop any scrolling
    if (gamingMode) {
      enterGamingMode();
      Serial.println("Mode: GAMING (immediate)");
    } else {
      showMessage("Learning mode", "Listening...");
      Serial.println("Mode: LEARNING (immediate)");}
    // Simple debounce delay
    delay(250);}
  modeButtonLastState = modeState;}

// -------------------- Gaming mode helper functions --------------------
// Initialize game state when entering Gaming mode
void enterGamingMode() {
  currentPatternIndex = 0;
  currentAttempt      = 0;
  gameState           = 0;
  scrollActive        = false;
  showMessage("Gaming mode", "Ready?");}
// Reset entire game (used after win or loss)
void resetGame() {
  currentPatternIndex = 0;
  currentAttempt      = 0;
  gameState           = 0;
  scrollActive        = false;
  showMessage("Gaming mode", "Ready?");}
// Play a predefined game pattern with click sounds
void playGamePattern(int patternIndex) {
  isPlayingBack = true;
  Serial.print("Playing game pattern ");
  Serial.println(patternIndex + 1);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(gamePatternTitles[patternIndex]);
  lcd.setCursor(0, 1);
  lcd.print("Playing...");
  int len = gamePatternLengths[patternIndex];
  // First knock
  playKnockClick();
  delay(50);
  // Play the sequence based on stored percentage values
  for (int i = 0; i < len; i++) {
    int val = gamePatterns[patternIndex][i];
    if (val <= 0) break; // extra safety: ignore trailing zeros
    int waitTime;
    // In this game, values close to 50 are eighth notes, 100 are quarter notes.
    if (val <= 60) {
      waitTime = 312;     // eighth note (average tempo)
    } else {
      waitTime = 625;     // quarter note (average tempo)
    }

    // Instead of a single delay, split into small steps
    // to allow mode switch during playback
    unsigned long start = millis();
    while (millis() - start < (unsigned long)waitTime) {
      checkModeSwitchImmediate();
      if (!gamingMode) {
        // If user switched mode, stop playback immediately
        isPlayingBack = false;
        return;}
      delay(10);}
    playKnockClick();}
  isPlayingBack = false;}
// Validate player's attempt against a specific game pattern
boolean validateGamePattern(int patternIndex, int currentKnockCount) {
  int expectedCount = gamePatternLengths[patternIndex];
  // Must have exact same number of intervals
  if (currentKnockCount != expectedCount) {
    return false;}
  // Find the maximum interval
  int maxKnockInterval = 0;
  for (int i = 0; i < currentKnockCount; i++) {
    if (knockReadings[i] > maxKnockInterval) {
      maxKnockInterval = knockReadings[i];}}
  if (maxKnockInterval == 0) {
    return false;}
  int totaltimeDifferences = 0;
  // Normalize and compare interval-by-interval
  for (int i = 0; i < currentKnockCount; i++) {
    knockReadings[i] = map(knockReadings[i], 0, maxKnockInterval, 0, 100);
    int timeDiff = abs(knockReadings[i] - gamePatterns[patternIndex][i]);
    // If any single interval is too different, fail
    if (timeDiff > gameRejectValue) {
      return false;}
    totaltimeDifferences += timeDiff;}
  // Average difference must also be within tolerance
  if (totaltimeDifferences / expectedCount > gameAverageRejectValue) {
    return false;}
  return true;}

// Listen to player's knocks in Gaming mode and validate them
boolean listenToGameKnockAndValidate() {
  Serial.println("Game: player attempt");
  // Reset knockReadings array
  for (int i = 0; i < maximumKnocks; i++) {
    knockReadings[i] = 0;}
  int  currentIntervalCount = 0;
  bool started              = false;
  unsigned long overallStart = millis();
  unsigned long startTime    = millis();
  unsigned long now          = millis();
  const int gameTotalTimeout  = 15000; // max time for entire attempt (ms)
  const int gameKnockComplete = 2500;  // silence after last knock to finish (ms)
  delay(knockFadeTime); // avoid counting the initial clap twice
  while ((now - overallStart < gameTotalTimeout) && (currentIntervalCount < maximumKnocks)) {
    // Allow mode switch while listening
    checkModeSwitchImmediate();
    if (!gamingMode) {
      return false;}

    micSensorValue = analogRead(micSensor);
    if (micSensorValue >= threshold) {
      Serial.println("Game knock.");
      playKnockClick();
      now = millis();
      if (!started) {
        // First knock: set reference time but do not store interval yet
        started   = true;
        startTime = now;
      } else {
        // Store interval between knocks
        int interval = now - startTime;
        if (interval < 15) interval = 15;   // avoid zero intervals
        knockReadings[currentIntervalCount] = interval;
        currentIntervalCount++;
        startTime = now;}
      delay(knockFadeTime);}

    now = millis();
    // If we already started and there is long silence, stop recording
    if (started && (now - startTime > gameKnockComplete)) {
      break;}}

  // If user did not really play a pattern, treat as failure
  if (currentIntervalCount == 0) {
    return false;}

  // Validate against current pattern
  return validateGamePattern(currentPatternIndex, currentIntervalCount);}

// One complete round in Gaming mode: play pattern, then listen and evaluate
void playGameRound() {
  if (currentAttempt < maxGameAttempts) {
    currentAttempt++;}

  // 1. Play the current game pattern
  playGamePattern(currentPatternIndex);

  if (!gamingMode) {
    // If user switched mode during playback, stop here
    return;}
  // 2. Short pause before listening to player
  unsigned long waitStart = millis();
  while (millis() - waitStart < 800) {
    checkModeSwitchImmediate();
    if (!gamingMode) {
      return;}
    delay(10);}
  // 3. Show attempt number on LCD
  char tryMsg[16];
  snprintf(tryMsg, sizeof(tryMsg), "your try %d/3", currentAttempt);
  showMessage(gamePatternTitles[currentPatternIndex], tryMsg);
  // 4. Listen to player and check result
  boolean success = listenToGameKnockAndValidate();
  if (!gamingMode) {
    // If mode changed during attempt, exit
    return;}
  if (success) {
    Serial.println("Game: success!");
    if (currentPatternIndex == NUM_GAME_PATTERNS - 1) {
      // This was the last pattern, full victory!!! nice
      gameState = 3;
      scrollSecondLine(
        "Congratulations!",
        "Press Ready to restart the game or press the other button for Learning Mode",
        250);
    } else {
      // Pattern passed, move on to the next one
      gameState = 2;
      scrollSecondLine(
        "Great job, buddy",
        "Press Ready to play the next pattern",
        250);}
    // Play success sound on top of the message
    playSuccessTriad();
  } else {
    Serial.println("Game: fail.");
    if (currentAttempt >= maxGameAttempts) {
      // No more attempts left → player lost
      gameState = 4;
      scrollSecondLine(
        "Sadly, you lost.",
        "Press Ready to restart the game or press the other button for Learning Mode",
        250);
    } else {
      // Player still has attempts left
      gameState = 1;
      scrollSecondLine(
        "You missed it...",
        "Press Ready to try again",
        250);}

    // Error sound after failure
    playErrorSiren();}}

// ---------------------------------------------------
// Recording clap timings in Learning mode
// ---------------------------------------------------
void listenToSecretKnock() {
  Serial.println("Knock starting");

  if (recordingMode) {
    showMessage("Recording mode", "Listening...");
  } else {
    showMessage("Learning mode", "Listening...");}

  // Reset intervals
  for (int i = 0; i < maximumKnocks; i++) {
    knockReadings[i] = 0;}
  int  currentKnockNumber   = 0;
  int  startTime            = millis();
  int  now                  = millis();
  bool cancelled            = false;
  bool cancelButtonWasDown  = false;
  delay(knockFadeTime); // avoid double counting the first knock
  // Collect intervals until time runs out or we reach maximumKnocks
  while ((now - startTime < knockComplete) && (currentKnockNumber < maximumKnocks)) {
    // Inside recordingMode we allow cancel via button
    if (recordingMode) {
      int cancelState = digitalRead(programSwitch);
      if (cancelState == HIGH && !cancelButtonWasDown) {
        cancelled = true;
        Serial.println("Recording cancelled (inside listen).");
        break;}
      cancelButtonWasDown = (cancelState == HIGH);}

    micSensorValue = analogRead(micSensor);
    if (micSensorValue >= threshold) {
      Serial.println("Knock.");
      playKnockClick();
      now = millis();
      int interval = now - startTime;
      if (interval < 15) interval = 15;
      knockReadings[currentKnockNumber] = interval;
      currentKnockNumber++;
      startTime = now;
      delay(knockFadeTime);}
    now = millis();}

  // If recording was cancelled, go back to idle
  if (recordingMode && cancelled) {
    showMessage("Recording", "Cancelled");
    delay(700);
    recordingMode = false;
    showMessage("Learning mode", "Listening...");
    return;}

  // In Learning mode: compare with stored pattern
  if (!recordingMode) {
    if (validateKnock()) {
      triggerDoorUnlock();
    } else {
      Serial.println("Secret knock failed.");
      showMessage("Wrong pattern", "Try again");
      playErrorSiren();
      showMessage("Learning mode", "Listening...");}
  } else {
    // In Recording mode: store new pattern into EEPROM
    validateKnock(); // this will update secretCode and save it
    Serial.println("New pattern stored.");

    delay(150);
    showMessage("Recording mode", "Pattern saved");
    playPatternSavedTone();

    recordingMode = false;
    showMessage("Learning mode", "Listening...");}}

// Called when pattern in Learning mode is correctly matched
void triggerDoorUnlock() {
  Serial.println("Door unlocked!");
  showMessage("Correct!", "Good job buddy!");
  playSuccessTriad();
  showMessage("Learning mode", "Listening...");}

// Validate a knock sequence in Learning mode.
// In recordingMode it converts knockReadings into secretCode and saves it.
boolean validateKnock() {
  int currentKnockCount = 0;
  int secretKnockCount  = 0;
  int maxKnockInterval  = 0;

  // Count recorded intervals and find the maximum
  for (int i = 0; i < maximumKnocks; i++) {
    if (knockReadings[i] > 0) {
      currentKnockCount++;
      if (knockReadings[i] > maxKnockInterval) {
        maxKnockInterval = knockReadings[i];}}
    if (secretCode[i] > 0) {
      secretKnockCount++;}}

  // If we are recording a new pattern, we ignore comparison
  if (recordingMode == true) {
    if (currentKnockCount == 0 || maxKnockInterval == 0) {
      Serial.println("Record failed: no knocks");
      return false;}
    // Clear previous pattern
    for (int i = 0; i < maximumKnocks; i++) {
      secretCode[i] = 0;}
    // Map intervals into 0–100 range and store to secretCode
    for (int i = 0; i < currentKnockCount; i++) {
      secretCode[i] = map(knockReadings[i], 0, maxKnockInterval, 0, 100);}

    // Debug print of new pattern
    Serial.print("New secretCode: ");
    for (int i = 0; i < maximumKnocks; i++) {
      Serial.print(secretCode[i]);
      Serial.print(" ");}
    Serial.println();
    // Save new pattern to EEPROM and show user playback
    savePatternToEEPROM(currentKnockCount);
    playStoredPattern();
    return false;}
  // In normal Learning mode we now compare with secretCode[]
  // If counts do not match, fail immediately
  if (currentKnockCount != secretKnockCount) {
    return false;}
  int totaltimeDifferences = 0;
  // Normalize knockReadings to 0–100 and compare per interval
  for (int i = 0; i < maximumKnocks; i++) {
    knockReadings[i] = map(knockReadings[i], 0, maxKnockInterval, 0, 100);
    int timeDiff = abs(knockReadings[i] - secretCode[i]);
    // If any interval too far off, fail
    if (timeDiff > rejectValue) {
      return false;}
    totaltimeDifferences += timeDiff;}

  // Average difference must be small enough
  if (totaltimeDifferences / secretKnockCount > averageRejectValue) {
    return false;}
  return true;}

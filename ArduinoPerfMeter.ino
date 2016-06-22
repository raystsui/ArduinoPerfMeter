#include <AsyncDelay.h>
#include <Filters.h>
#include <LcdBarGraph.h>
#include <LiquidCrystal.h>

#define bufferLength 100
#define numMeters 4
#define ledAdvLevel 45
#define USING_METERS 1
#define ALARM_RELAY_PIN 7
#define DELAY_NEWMAIL_LIGHT_MS 4000

// Four PWM pins to analogue meters
byte meterPin[] = {6, 9, 10, 11};
byte location = 0;
bool led = false;

// For TwoPolesFilters
float cornerFrequency[] = {0.6, 0.6, 0.35, 0.6};                  // corner frequency (Hz)
FilterTwoPole filterTwoLowpass[numMeters];    // create a two pole Lowpass filter

// For funciton: float logFreeMb(float freeMb)
double kBottom = log(25);
double kTop = log(4096);

int ledLevelTarget = 0;
int ledLevel = 0;


#ifndef USING_METERS
// Based on LCD Keypad Shield
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
LcdBarGraph lbg0(&lcd, 8, 0, 0);
LcdBarGraph lbg1(&lcd, 8, 0, 1);
LcdBarGraph lbg2(&lcd, 8, 8, 0);
LcdBarGraph lbg3(&lcd, 8, 8, 1);
#endif

AsyncDelay delay_newmail_light;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.setTimeout(1000);
  pinMode(13, OUTPUT);
  pinMode(ALARM_RELAY_PIN, OUTPUT);
  digitalWrite(ALARM_RELAY_PIN, LOW);

#ifndef USING_METERS
  lcd.begin(16, 2);
  lcd4WriteText(0, "Ready.");
#endif

  // standard Lowpass, set to the corner frequency
  for (int i = 0; i < numMeters; i++) {
    filterTwoLowpass[i].setAsFilter( LOWPASS_BUTTERWORTH, cornerFrequency[i], 50);     // Q-value=50
#ifdef USING_METERS
    pinMode(meterPin[i], OUTPUT);
    analogWrite(meterPin[i], 0);
#endif
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  String s;
  while (true) {
    if (delay_newmail_light.isExpired()) {
      digitalWrite(ALARM_RELAY_PIN, LOW);
    }

    if (Serial.available()) {
      s = serialReadln();
      if (s.length() <= 0) continue;
    } else continue;

    if (s.indexOf("sync") >= 0) {
      toggleLed();
      location = 0;
      continue;
    }

    if (s.indexOf("fish") >= 0) {
      location = 0;
      doHandshake();
#ifndef USING_METERS
      lcd4WriteText(2, "fish");
#endif
      continue;
    }


    if (s.indexOf("email") >= 0) {
      digitalWrite(ALARM_RELAY_PIN, HIGH);
      delay_newmail_light.start(DELAY_NEWMAIL_LIGHT_MS, AsyncDelay::MILLIS);
#ifndef USING_METERS
      lcd4WriteText(2, "mail");
#endif
      continue;
    }


    if (isNumeric(s)) {
      updateMeters(s);
    }
  }

}

void updateMeters(String s) {
  if (isNumeric(s)) {
    float f = s.toFloat();
    float f2;
    int meterLevel;

    filterTwoLowpass[location].input(f);

    switch (location) {
      case 2:     f2 = constrain(filterTwoLowpass[location].output(), 0, 4096); break;
      default:    f2 = constrain(filterTwoLowpass[location].output(), 0, 100); break;
    }
    meterLevel = map(f2, 0, 100, 0, 255);

#ifdef USING_METERS
    // Update to analogue meters
    switch (location) {
      case 0:
        analogWrite(meterPin[location], meterLevel);
        break;
      case 1:
        analogWrite(meterPin[location], meterLevel);
        break;
      case 2:
        //        meterLevel = map(logFreeMb(f), 0, 100, 0, 255);   // no Low-pass filtering
        meterLevel = map(logFreeMb(f2), 0, 100, 0, 255);   // With Low-pass filtering
        analogWrite(meterPin[location], meterLevel);
        break;
      case 3:
        analogWrite(meterPin[location], meterLevel);
        break;
    }
#else
    // Update to LCD display
    switch (location) {
      case 0:
        lbg0.drawValue(f2, 100);
        break;
      case 1:
        lbg1.drawValue(f2, 100);
        break;
      case 2:
        // lcd4WriteText(1, String(logFreeMb(f)));
        //        meterLevel = constrain(logFreeMb(f), 0, 100);     // no Low-pass filtering
        meterLevel = constrain(logFreeMb(f2), 0, 100);       // With Low-pass filtering
        lbg2.drawValue(meterLevel, 100);
        break;
      case 3:
        lbg3.drawValue(f2, 100);
        break;
    }
#endif
    advanceLocation();
  }
}

String serialReadln() {
  char buffer[bufferLength];
  int lf = 10;  // linefeed, chr(10)
  if (Serial.available()) {
    int numCharRead = Serial.readBytesUntil(lf, buffer, bufferLength);
    if (numCharRead == 0) return "";
    String s(buffer);
    s = s.substring(0, numCharRead);
    s.trim();
    return s;
  }
}

void toggleLed() {
  //  if (led) digitalWrite(13, HIGH); else digitalWrite(13, LOW);
  if (led) ledLevelTarget = 255; else ledLevelTarget = 0;
  led = !led;
  updateLed();
}

void updateLed() {
  if (ledLevel == ledLevelTarget) {
    return;
  }
  if (ledLevel > ledLevelTarget) {
    ledLevel -= ledAdvLevel;
  }
  if (ledLevel < ledLevelTarget) {
    ledLevel += ledAdvLevel;
  }
  constrain(ledLevel, 0, 255);
  analogWrite(13, ledLevel);
}

void advanceLocation() {
  location++;
  if (location >= numMeters) location = 0;
}

boolean isNumeric(String str) {
  for (char i = 0; i < str.length(); i++) {
    if ( !isDigit(str.charAt(i))) {
      return false;
    }
  }
  return true;
}

boolean isDigit(char c) {
  switch (c) {
    case '0': return true; break;
    case '1': return true; break;
    case '2': return true; break;
    case '3': return true; break;
    case '4': return true; break;
    case '5': return true; break;
    case '6': return true; break;
    case '7': return true; break;
    case '8': return true; break;
    case '9': return true; break;
    case '.': return true; break;
  }
  return false;
}

void doHandshake() {
  Serial.println("chips");
  Serial.flush();
}

float logFreeMb(float freeMb)
{
  double result = 0;
  result = (log(constrain(freeMb, 25, 4096)) - kBottom) / (kTop - kBottom);
  result = (1 - result) * 100;
  return float(result);
}

#ifndef USING_METERS

void lcd8WriteText(int loc, String msg) {
  int _col[] = {0, 4, 8, 12, 0, 4, 8, 12};
  int _row[] = {0, 0, 0, 0, 1, 1, 1, 1};
  char _empty[] = "    ";
  if (loc >= 0 && loc <= 7) {
    lcd.setCursor(_col[loc], _row[loc]);
    lcd.print(_empty);
    lcd.setCursor(_col[loc], _row[loc]);
    lcd.print(msg.substring(0, min(msg.length(), 4)));
  }
}

void lcd4WriteText(int loc, String msg) {
  int _col[] = {0, 8, 0, 8};
  int _row[] = {0, 0, 1, 1};
  char _empty[] = "        ";
  if (loc >= 0 && loc <= 4) {
    lcd.setCursor(_col[loc], _row[loc]);
    lcd.print(_empty);
    lcd.setCursor(_col[loc], _row[loc]);
    lcd.print(msg.substring(0, min(msg.length(), 8)));
  }
}
#endif

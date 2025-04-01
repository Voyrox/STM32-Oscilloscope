#include "SPI.h"
#include <EEPROM.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ST7796S_kbv.h"

#define maxSamples 1000
uint16_t buffer[maxSamples];
int data[maxSamples], data_old[maxSamples], data1[maxSamples];

#define TFT_CS PB6
#define TFT_DC PC7
#define TFT_RST PA9
Adafruit_ST7796S_kbv tft = Adafruit_ST7796S_kbv(TFT_CS, TFT_DC, TFT_RST);

int setting = 0, zoomFactor = 1;
int i, x, y, i2, u_max, u_min, mn = 2, raz, per, razv = 0;
int u1 = 0, u2 = 0, t1 = 0, t2 = 0, zap, ux = 1, uxx = 1, fun;
long times = 0, times3 = 0;
byte w = 0, hold = 0, oldHold = 0;
float del = 1.0f;

void setup() {
  pinMode(PA0, INPUT_ANALOG);
  pinMode(PC0, INPUT_PULLUP);  // HOLD button
  pinMode(PA4, INPUT_PULLUP);  // UP button
  pinMode(PB0, INPUT_PULLUP);  // DOWN button
  pinMode(PC1, INPUT_PULLUP);  // SET button

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(ST7796S_BLACK);

  razv = EEPROM.read(0);

  tft.setTextSize(1);
  tft.setTextColor(ST7796S_WHITE);

  tft.setCursor(0, 0);
  tft.print("Vmax = ");
  tft.setCursor(0, 10);
  tft.print("Vmin = ");
  tft.setCursor(0, 20);
  tft.print("Vpp  = ");

  tft.setCursor(220, 0);
  tft.print("OSCILLOSCOPE");

  tft.setCursor(220, 10);
  tft.print("VERSION 1.0");

  tft.setCursor(220, 20);
  tft.print("ewenmacculoch.com");

  GenPWM();
}

/* -------------------------------------------------------------------------
   Generate a PWM (testing). 
   ------------------------------------------------------------------------- */
void GenPWM() {
  pinMode(PA8, OUTPUT);
  analogWriteResolution(12);
  analogWrite(PA8, 2048);
}

/* -------------------------------------------------------------------------
   Capture samples from the ADC.
   ------------------------------------------------------------------------- */
void sampleWaveform() {
  unsigned long startTime = micros();
  for (int i = 0; i < maxSamples; i++) {
    buffer[i] = analogRead(PA0);
    delayMicroseconds(10);
  }
  times3 = micros() - startTime;
}

void updateHoldText() {
  tft.fillRect(220, 0, 110, 8, ST7796S_BLACK);
  tft.setCursor(220, 0);
  tft.setTextColor(ST7796S_WHITE);
  tft.print(hold ? "HOLD" : "OSCILLOSCOPE");
}

/* -------------------------------------------------------------------------
   Draw the small highlight rectangles & numeric text for:
    - time scale => x=110,y=0
    - amplitude => x=110,y=10
    - zoom => x=110,y=20
   ------------------------------------------------------------------------- */
void drawSettingsBar() {
  tft.fillRect(110, 0, 60, 8, ST7796S_BLACK);

  if (setting == 0) {
    tft.fillRect(110, 0, 60, 8, ST7796S_RED);
    tft.setCursor(110, 0);
    tft.setTextColor(ST7796S_BLACK);
  } else {
    tft.setCursor(110, 0);
    tft.setTextColor(ST7796S_WHITE);
  }

  tft.print((float)times3 / 10.0 / mn, 1);
  tft.print("uS");

  // AMPLITUDE (U x <ux>)
  tft.fillRect(110, 10, 40, 8, ST7796S_BLACK);
  if (setting == 1) {
    tft.fillRect(110, 10, 40, 8, ST7796S_RED);
    tft.setCursor(110, 10);
    tft.setTextColor(ST7796S_BLACK);
  } else {
    tft.setCursor(110, 10);
    tft.setTextColor(ST7796S_WHITE);
  }
  tft.print("U x");
  tft.print(ux);

  // ZOOM (Z x <zoomFactor>)
  tft.fillRect(110, 20, 40, 8, ST7796S_BLACK);
  if (setting == 2) {
    tft.fillRect(110, 20, 40, 8, ST7796S_RED);
    tft.setCursor(110, 20);
    tft.setTextColor(ST7796S_BLACK);
  } else {
    tft.setCursor(110, 20);
    tft.setTextColor(ST7796S_WHITE);
  }
  tft.print("Z x");
  tft.print(zoomFactor);
}

/* -------------------------------------------------------------------------
   Buttons: HOLD, SET, UP, DOWN
   ------------------------------------------------------------------------- */
void processButtons() {
  static bool lastUp   = HIGH, lastSet  = HIGH;
  static bool lastHold = HIGH, lastDown = HIGH;
  static unsigned long lastDownRepeatTime = 0;
  static bool downWasPressed = false;

  bool currentUp   = digitalRead(PA4);
  bool currentSet  = digitalRead(PC1);
  bool currentHold = digitalRead(PC0);
  bool currentDown = digitalRead(PB0);

  if (lastHold == HIGH && currentHold == LOW) {
    hold = !hold;
  }

  // Cycle through settings: 0..2
  if (lastSet == HIGH && currentSet == LOW) {
    setting++;
    if (setting > 2) setting = 0;
    w = 1;  // force a screen redraw of the waveform area
  }

  // UP button
  if (lastUp == HIGH && currentUp == LOW) {
    if (setting == 0) {
      // Increase time scale
      razv++;
      if (razv > 12) razv = 12;
      EEPROM.update(0, razv);
      w = 1;
    }
    else if (setting == 1) {
      // Increase amplitude scale
      uxx++;
      ux = uxx;
      del = 1;
      w = 1;
    }
    else if (setting == 2) {
      // Increase zoom factor
      zoomFactor++;
      if (zoomFactor > 8) zoomFactor = 8;
      w = 1;
    }
  }

  // DOWN button logic
  if (setting == 1) {
    // special repeated press logic for amplitude scale
    if (currentDown == LOW) {
      if (!downWasPressed) {
        uxx--;
        if (uxx <= 0) {
          uxx = 0; 
          ux = 1; 
          del = 2;
        }
        downWasPressed = true;
        w = 1;
      }
      else if (millis() - lastDownRepeatTime >= 500 
               && millis() - lastDownRepeatTime >= 100) {
        uxx--;
        if (uxx <= 0) {
          uxx = 0; 
          ux = 1; 
          del = 2;
        }
        lastDownRepeatTime = millis();
        w = 1;
      }
    } else {
      downWasPressed = false;
    }
  }
  else {
    if (lastDown == HIGH && currentDown == LOW) {
      if (setting == 0) {
        // Decrease time scale
        razv--;
        if (razv < 0) razv = 0;
        EEPROM.update(0, razv);
        w = 1;
      }
      else if (setting == 2) {
        // Decrease zoom factor
        zoomFactor--;
        if (zoomFactor < 1) zoomFactor = 1;
        w = 1;
      }
    }
  }

  lastUp   = currentUp;
  lastSet  = currentSet;
  lastHold = currentHold;
  lastDown = currentDown;
}

/* -------------------------------------------------------------------------
   Based on 'razv', set 'mn' (time scale multiplier).
   ------------------------------------------------------------------------- */
void razmer() {
  switch (razv) {
    case 0: mn = 4; break;
    case 1: mn = 2; break;
    default: mn = 1; break;
  }
}

/* -------------------------------------------------------------------------
   Find min and max every 500 ms
   ------------------------------------------------------------------------- */
void arr() {
  if (millis() - times > 500) {
    u_max = 0;
    u_min = 4100;

    for (int mmm = 0; mmm < 640; mmm++) {
      u_min = min(u_min, (int)buffer[mmm]);
      u_max = max(u_max, (int)buffer[mmm]);
    }
    times = millis();

    tft.fillRect(55, 0, 56, 8, ST7796S_BLACK);
    tft.setCursor(55, 0);
    tft.setTextColor(ST7796S_WHITE);
    tft.print(u_max * 3.3 / 4095 * del, 2);

    tft.fillRect(55, 10, 56, 8, ST7796S_BLACK);
    tft.setCursor(55, 10);
    tft.print(u_min * 3.3 / 4095 * del, 2);

    int amplitude = (u_max - u_min);
    tft.fillRect(55, 20, 56, 8, ST7796S_BLACK);
    tft.setCursor(55, 20);
    tft.print(amplitude * 3.3 / 4095 * del, 2);

    // If we've effectively hit 3.3 V, reduce scale automatically
    float maxVoltage = (u_max * 3.3f / 4095.0f);
    if (maxVoltage >= 3.3f) {
      uxx = 0;
      ux  = 1;
      del = 2;

      // Show "U x 0.5" or some message:
      tft.fillRect(70, 10, 65, 8, ST7796S_BLACK);
      tft.setCursor(90, 10);
      tft.print("U x ");
      tft.print(0.5, 1);
    }
  }
}

void loop() {
  processButtons();

  if (hold != oldHold) {
    updateHoldText();
    oldHold = hold;
  }

  // Update the small highlight rectangles + numeric text for
  // time scale, amplitude, zoom factor
  drawSettingsBar();

  if (!hold) {
    razmer();

    // On changing any setting, if w=1, clear the waveform area
    if (w) {
      tft.fillRect(0, 30, tft.width(), tft.height() - 30, ST7796S_BLACK);
      w = 0;
    }

    sampleWaveform();

    // Convert raw buffer[] -> data[] mapped for vertical display
    for (int x = 0; x < maxSamples; x++) {
      data[x] = map(buffer[x], 0, 4095 / ux, 200, 0) + 30;
    }

    // Update and print the floating Vmax, Vmin, Vpp every 500 ms
    arr();

    for (i = 1; i < 1000; i++) {
      if (data[i + 5] > 200 && data[i + 3] < 200) {
        fun = i;
        break;
      }
    }

    for (i = 0; i < 1000 - fun; i++) {
      data1[i] = data[fun + i];
    }

    // Plot wave
    int maxDrawPoints = tft.width() / (mn * zoomFactor);
    for (i = 0; i < maxDrawPoints - 1; i++) {
      // Erase old line
      tft.drawLine(
        i * mn * zoomFactor,
        data_old[i],
        i * mn * zoomFactor + (mn * zoomFactor) - 1,
        data_old[i + 1],
        ST7796S_BLACK
      );
      // Draw new line
      tft.drawLine(
        i * mn * zoomFactor,
        data1[i],
        i * mn * zoomFactor + (mn * zoomFactor) - 1,
        data1[i + 1],
        ST7796S_RED
      );
    }

    // Save new data for next erase
    for (i = 0; i < maxDrawPoints - 1; i++) {
      data_old[i] = data1[i];
    }
  }
}

#include "SPI.h"
#include <EEPROM.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ST7796S_kbv.h"
#include <math.h>

#define maxSamples 1000
uint16_t buffer[maxSamples];
int data[maxSamples], data_old[maxSamples], data1[maxSamples];

#define TFT_CS PB6
#define TFT_DC PC7
#define TFT_RST PA9
Adafruit_ST7796S_kbv tft = Adafruit_ST7796S_kbv(TFT_CS, TFT_DC, TFT_RST);

#define GRID_COLOR 0x7BEF

int setting = 0, zoomFactor = 1;
int i, x, y, i2, u_max, u_min, mn = 2, raz, per, razv = 0;
int u1 = 0, u2 = 0, t1 = 0, t2 = 0, zap, ux = 1, uxx = 1, fun;
long times = 0, times3 = 0;
byte w = 0, hold = 0, oldHold = 0;
float del = 1.0f;

int yOffset = 30;
String Version = "1.0";

bool gridToggle = true;
bool clearScreen = false;
bool invToggle = false;

void showLoadingScreen() {
  tft.fillScreen(ST7796S_BLACK);
  
  tft.setTextColor(ST7796S_WHITE);
  tft.setTextSize(2);
  int titleLength = 13;
  int titleWidth  = titleLength * 6 * 2;
  int titleX = (tft.width() - titleWidth) / 2;
  tft.setCursor(titleX, 20);
  tft.print("Initializing...");

  tft.setTextSize(1);
  String versionText = "Version " + Version;
  int versionWidth = versionText.length() * 6; 
  int versionX = (tft.width() - versionWidth) / 2;
  tft.setCursor(versionX, 40);
  tft.print(versionText);

  int barWidth = 200;
  int barHeight = 10;
  int barX = (tft.width() - barWidth) / 2;
  int barY = 80;

  tft.drawRect(barX, barY, barWidth, barHeight, ST7796S_WHITE);

  int textX = barX;
  int textY = barY + barHeight + 15;

  auto updateBar = [&](int percent) {
    int filled = map(percent, 0, 100, 0, barWidth);
    tft.fillRect(barX + 1, barY + 1, filled - 2, barHeight - 2, ST7796S_WHITE);
  };

  tft.setCursor(textX, textY);
  tft.print("Initializing RAM...");
  for (int i = 0; i <= 25; i++) {
    updateBar(i);
    delay(20);
  }

  tft.fillRect(textX, textY, 200, 10, ST7796S_BLACK);
  tft.setCursor(textX, textY);
  tft.print("Initializing EEPROM...");
  for (int i = 26; i <= 50; i++) {
    updateBar(i);
    delay(20);
  }

  tft.fillRect(textX, textY, 200, 10, ST7796S_BLACK);
  tft.setCursor(textX, textY);
  tft.print("Initializing I/O...");
  for (int i = 51; i <= 75; i++) {
    updateBar(i);
    delay(20);
  }

  tft.fillRect(textX, textY, 200, 10, ST7796S_BLACK);
  tft.setCursor(textX, textY);
  tft.print("Finalizing...");
  for (int i = 76; i <= 100; i++) {
    updateBar(i);
    delay(20);
  }

  delay(500);
  tft.fillScreen(ST7796S_BLACK);
}

void GenPWM() {
  pinMode(PA8, OUTPUT);
  analogWriteResolution(12);
  analogWrite(PA8, 2048);
}

void setup() {
  pinMode(PA0, INPUT_ANALOG);
  pinMode(PC0, INPUT_PULLUP);  // HOLD button                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               
  pinMode(PA4, INPUT_PULLUP);  // UP button
  pinMode(PB0, INPUT_PULLUP);  // DOWN button
  pinMode(PC1, INPUT_PULLUP);  // SET button

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(ST7796S_BLACK);
  
  showLoadingScreen();

  razv = EEPROM.read(0);

  tft.setTextSize(1);
  tft.setTextColor(ST7796S_WHITE);

  tft.setCursor(0, 0);
  tft.print("Vmax = ");
  tft.setCursor(0, 10);
  tft.print("Vmin = ");
  tft.setCursor(0, 20);
  tft.print("Vpp  = ");

  tft.setCursor(280, 0);
  tft.print("OSCILLOSCOPE");
  tft.setCursor(280, 10);
  tft.print("VERSION " + Version);
  tft.setCursor(280, 20);
  tft.print("ewenmacculloch.com");

  GenPWM();
}

void drawGridSegment(int xStart, int xEnd) {
  for (int y = 35; y < tft.height(); y += 50) {
    for (int x = 10; x < tft.width(); x += 5) {
      if (x >= xStart && x < xEnd) {
        tft.drawPixel(x, y, GRID_COLOR);
      }
    }
  }
  for (int x = 0; x < tft.width(); x += 64) {
    if (x >= xStart && x < xEnd) {
      for (int y = 35; y < tft.height(); y += 10) {
        tft.drawPixel(x, y, GRID_COLOR);
      }
    }
  }
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
  tft.fillRect(280, 0, 110, 8, ST7796S_BLACK);
  tft.setCursor(280, 0);
  tft.setTextColor(ST7796S_WHITE);
  tft.print(hold ? "HOLD" : "OSCILLOSCOPE");
}

void drawSettingsBar() {
  // TIME SCALE
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

  // AMPLITUDE
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

  // ZOOM
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

void drawControlColumn() {
  // Y OFFSET
  tft.fillRect(190, 0, 40, 8, ST7796S_BLACK);
  if (setting == 3) {
    tft.fillRect(190, 0, 40, 8, ST7796S_RED);
    tft.setCursor(190, 0);
    tft.setTextColor(ST7796S_BLACK);
  } else {
    tft.setCursor(190, 0);
    tft.setTextColor(ST7796S_WHITE);
  }
  tft.print("Y x");
  tft.print(yOffset);

  // GRID TOGGLE
  tft.fillRect(190, 10, 60, 8, ST7796S_BLACK);
  if (setting == 4) {
    tft.fillRect(190, 10, 60, 8, ST7796S_RED);
    tft.setCursor(190, 10);
    tft.setTextColor(ST7796S_BLACK);
  } else {
    tft.setCursor(190, 10);
    tft.setTextColor(ST7796S_WHITE);
  }
  tft.print("Grid: ");
  tft.print(gridToggle ? "On" : "Off");

  // INV TOGGLE
  tft.fillRect(190, 20, 60, 8, ST7796S_BLACK);
  if (setting == 5) {
    tft.fillRect(190, 20, 60, 8, ST7796S_RED);
    tft.setCursor(190, 20);
    tft.setTextColor(ST7796S_BLACK);
  } else {
    tft.setCursor(190, 20);
    tft.setTextColor(ST7796S_WHITE);
  }
  tft.print("Inv: ");
  tft.print(invToggle ? "On" : "Off");
}

/* -------------------------------------------------------------------------
   Process buttons: HOLD, SET, UP, DOWN.
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

  // Cycle through settings: 0..5
  if (lastSet == HIGH && currentSet == LOW) {
    setting++;
    if (setting > 5) setting = 0;
    w = 1;  // force a redraw of settings
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
    else if (setting == 3) {
      // Increase Y offset by 10
      yOffset += 10;
      w = 1;
    }
    else if (setting == 4) {
      // Toggle Grid mode
      gridToggle = !gridToggle;
      clearScreen = true;
      w = 1;
    }
    else if (setting == 5) {
      // Toggle Inversion mode
      invToggle = !invToggle;
      w = 1;
    }
  }

  // DOWN button logic
  if (setting == 1) { // for amplitude with repeat logic
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
      else if (millis() - lastDownRepeatTime >= 500 &&
               millis() - lastDownRepeatTime >= 100) {
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
      else if (setting == 3) {
        // Decrease Y offset by 10
        yOffset -= 10;
        w = 1;
      }
      // No DOWN action for settings 4 & 5 (Grid & Inv)
    }
  }

  lastUp   = currentUp;
  lastSet  = currentSet;
  lastHold = currentHold;
  lastDown = currentDown;
}

/* -------------------------------------------------------------------------
   Set time scale multiplier 'mn' based on 'razv'.
   ------------------------------------------------------------------------- */
void razmer() {
  switch (razv) {
    case 0: mn = 4; break;
    case 1: mn = 2; break;
    default: mn = 1; break;
  }
}

/* -------------------------------------------------------------------------
   Every 500ms, compute min and max values and update measurement texts.
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

    tft.fillRect(55, 0, 54, 8, ST7796S_BLACK);
    tft.setCursor(55, 0);
    tft.setTextColor(ST7796S_WHITE);
    tft.print(u_max * 3.3 / 4095 * del, 2);

    tft.fillRect(55, 10, 54, 8, ST7796S_BLACK);
    tft.setCursor(55, 10);
    tft.print(u_min * 3.3 / 4095 * del, 2);

    int amplitude = (u_max - u_min);
    tft.fillRect(55, 20, 54, 8, ST7796S_BLACK);
    tft.setCursor(55, 20);
    tft.print(amplitude * 3.3 / 4095 * del, 2);
    
    if (setting != 1) {
      int signalRange = u_max - u_min;
      if (signalRange > 0) {
        ux = 4095 / signalRange / 1.5;
        if (ux < 1) ux = 1;
      }
    }      
  }
}

void loop() {
  processButtons();

  if (hold != oldHold) {
    updateHoldText();
    oldHold = hold;
  }

  drawSettingsBar();
  drawControlColumn();

  if (!hold) {
    razmer();

    sampleWaveform();

    for (int x = 0; x < maxSamples; x++) {
      data[x] = map(buffer[x], 0, 4095 / ux, 200, 0) + yOffset;
    }

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

    int maxDrawPoints = tft.width() / (mn * zoomFactor);
    tft.startWrite();
    if (gridToggle) {

      for (i = 0; i < maxDrawPoints - 1; i++) {
        int xStart = i * mn * zoomFactor;
        int xEnd = xStart + (mn * zoomFactor);

        tft.fillRect(xStart, 60, (mn * zoomFactor), tft.height() - 60, ST7796S_BLACK);
        drawGridSegment(xStart, xEnd);

        tft.drawLine(xStart, data1[i],
                     xEnd - 1, data1[i + 1],
                     ST7796S_RED);
      }
      for (i = 0; i < maxDrawPoints - 1; i++) {
        data_old[i] = data1[i];
      }
    } else {

      if (clearScreen) {
        tft.fillRect(0, 35, tft.width(), tft.height(), ST7796S_BLACK);
        clearScreen = false;
      }

      static int prevZoom = 1;
      if (zoomFactor != prevZoom) {
        tft.fillRect(0, 60, tft.width(), tft.height() - 60, ST7796S_BLACK);
        for (i = 0; i < maxDrawPoints; i++) {
          data_old[i] = data1[i];
        }
        prevZoom = zoomFactor;
      }

      for (i = 0; i < maxDrawPoints - 1; i++) {
        tft.drawLine(i * mn * zoomFactor, data_old[i],
                     i * mn * zoomFactor + (mn * zoomFactor) - 1, data_old[i + 1],
                     ST7796S_BLACK);
        tft.drawLine(i * mn * zoomFactor, data1[i],
                     i * mn * zoomFactor + (mn * zoomFactor) - 1, data1[i + 1],
                     ST7796S_RED);
      }
      for (i = 0; i < maxDrawPoints - 1; i++) {
        data_old[i] = data1[i];
      }
    }
    tft.endWrite();
  }
}
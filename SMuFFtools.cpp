/**
 * SMuFF Firmware
 * Copyright (C) 2019 Technik Gegg
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * Module containing helper functions
 */

#include "SMuFF.h"
#include "SMuFFBitmaps.h"
#include "Config.h"
#include "ZTimerLib.h"
#include "ZStepperLib.h"
#include "ZServo.h"

extern ZStepper       steppers[];
extern ZServo         servo;
extern char           tmp[128];

SMuFFConfig           smuffConfig;
int                   lastEncoderTurn = 0;
byte                  toolSelected = -1;
static bool           feederJamed = false;
PositionMode          positionMode = RELATIVE;
static bool           displayingUserMessage = false;
static unsigned int   userMessageTime = 0;
char                  _sel[128];
char                  _wait[128];
char                  _title[128];
char                  _msg1[256];
char                  _msg2[128];
char                  _btn[128];


const char brand[] = VERSION_STRING;

void setupDisplay() {
  display.begin(/*Select=*/ ENCODER_BUTTON_PIN,  /* menu_next_pin= */ U8X8_PIN_NONE, /* menu_prev_pin= */ U8X8_PIN_NONE, /* menu_home_pin= */ U8X8_PIN_NONE);
  display.enableUTF8Print();
  resetDisplay();
  if (smuffConfig.lcdContrast < MIN_CONTRAST || smuffConfig.lcdContrast > MAX_CONTRAST) {
    smuffConfig.lcdContrast = DSP_CONTRAST;
    EEPROM.put(EEPROM_CONTRAST, smuffConfig.lcdContrast);
  }
  display.setContrast(smuffConfig.lcdContrast);
}

void drawLogo() {
  display.setBitmapMode(1);
  display.drawXBMP(0, 0, logo_width, logo_height, logo_bits);
  display.setFont(LOGO_FONT);
  display.setFontMode(0);
  display.setFontDirection(0);
  display.setDrawColor(1);
  display.setCursor(display.getDisplayWidth() - display.getStrWidth(brand) - 1, display.getDisplayHeight() - display.getMaxCharHeight());
  display.print(brand);
}

void drawStatus() {
  display.setFont(STATUS_FONT);
  display.setFontMode(0);
  display.setDrawColor(1);
  sprintf_P(tmp, P_CurrentTool);
  display.drawStr(display.getDisplayWidth() - display.getStrWidth(tmp) - 5, 14, tmp);
  display.drawStr(display.getDisplayWidth() - display.getStrWidth("X") - 5, 14, (toolSelected >= 0 && toolSelected < smuffConfig.toolCount) ? String(toolSelected).c_str() : "-");
  sprintf_P(tmp, P_Feed);
  display.drawStr(display.getDisplayWidth() - display.getStrWidth(tmp) - 5, 34, tmp);
  display.setFontMode(1);
  display.setFont(SMALL_FONT);
  display.setDrawColor(2);
  display.drawBox(0, display.getDisplayHeight()-display.getMaxCharHeight()+2, display.getDisplayWidth(), display.getMaxCharHeight());
  sprintf_P(_wait, parserBusy ? P_Busy : P_Ready);
  sprintf(tmp, "M:%d | %-4s | %-5s ", freeMemory(), traceSerial2.c_str(), _wait);
  display.drawStr(1, display.getDisplayHeight(), tmp);
  display.setFontMode(0);
  display.setDrawColor(1);
  display.setFont(ICONIC_FONT);
  display.drawGlyph(110, 38, feederEndstop() ? 0x41 : 0x42);
  display.setFont(BASE_FONT);
}

void resetDisplay() {
    display.clearDisplay();
    display.setFont(BASE_FONT);
    display.setFontMode(0);
    display.setDrawColor(1);
}

void drawSelectingMessage(int tool) {
  display.firstPage();
  do {
    resetDisplay();
    sprintf_P(_sel, P_Selecting);
    sprintf_P(_wait, P_Wait);
    if(*smuffConfig.materials[tool] != NULL) {
      sprintf(tmp,"%s", smuffConfig.materials[tool]);
    }
    else {
      sprintf_P(tmp, "%s%d", P_Tool, tool);
    }
    display.drawStr((display.getDisplayWidth() - display.getStrWidth(_sel))/2, (display.getDisplayHeight() - display.getMaxCharHeight())/2-10, _sel);
    display.setFont(BASE_FONT_BIG);
    display.drawStr((display.getDisplayWidth() - display.getStrWidth(tmp))/2, (display.getDisplayHeight() - display.getMaxCharHeight())/2+9, tmp);
    display.setFont(BASE_FONT);
    display.drawStr((display.getDisplayWidth() - display.getStrWidth(_wait))/2, (display.getDisplayHeight() - display.getMaxCharHeight())/2 + display.getMaxCharHeight()+10, _wait);
  } while(display.nextPage());
}

int splitStringLines(char lines[][MAX_LINE_LENGTH], String message) {
  int pos1 = 0;
  int pos2 = message.indexOf('\n') == -1 ? message.length() : message.indexOf('\n');
  int ln = 0;
  do {
    int len = pos2+1-pos1 > MAX_LINE_LENGTH-1 ? MAX_LINE_LENGTH-1 : pos2+1-pos1;
    //__debug("Line: %d-%d\t%s", pos1, pos2, message.substring(pos1, pos2).c_str());
    message.substring(pos1).toCharArray(lines[ln++], len);
    if(ln >= MAX_LINES)
      break;
    pos1 = pos2+1;
  } while((pos2 = message.substring(pos1).indexOf('\n')) != -1);
  if(message.substring(pos2+1).length()>0 && ln < MAX_LINES) {
    int len = message.substring(pos1).length()+1;
    message.substring(pos1).toCharArray(lines[ln++], len);
  }
  return ln;
}

void drawUserMessage(String message) {
  char lines[MAX_LINES][MAX_LINE_LENGTH];
  int lineCnt = splitStringLines(lines, message);

  display.firstPage();
  do {
    resetDisplay();
    display.setFont(BASE_FONT_BIG);
    int y = (display.getDisplayHeight()-(lineCnt-1)*display.getMaxCharHeight())/2;
    display.firstPage();
    display.drawFrame(0, 0, display.getDisplayWidth(), display.getDisplayHeight());
    do {
      for(int i=0; i< lineCnt; i++) {
        display.drawStr((display.getDisplayWidth() - display.getStrWidth(lines[i]))/2, y, lines[i]);
        y += display.getMaxCharHeight();
      }
    } while(display.nextPage());
    display.setFont(BASE_FONT);
  } while(display.nextPage());  
  displayingUserMessage  = true;
  userMessageTime = millis();
}


void drawSDStatus(int stat) {
  resetDisplay();
  switch(stat) {
    case SD_ERR_INIT:
      sprintf_P(tmp, P_SD_InitError);
      break;
    case SD_ERR_NOCONFIG:
      sprintf_P(tmp, P_SD_NoConfig);
      break;
    case SD_READING_CONFIG:
      sprintf_P(tmp, P_SD_ReadingConfig);
      break;
  }
  display.firstPage();
  do {
    drawLogo();
    display.setCursor((display.getDisplayWidth() - display.getStrWidth(tmp))/2, display.getDisplayHeight());
    display.print(tmp);
  } while(display.nextPage());
}

bool selectorEndstop() {
  return steppers[SELECTOR].getEndstopHit();
}

bool revolverEndstop() {
  return steppers[REVOLVER].getEndstopHit();
}

bool feederEndstop() {
  if(smuffConfig.externalControl_Z) {
    return steppers[FEEDER].getEndstopHitAlt();
  }
  return steppers[FEEDER].getEndstopHit();
}

uint8_t u8x8_GetMenuEvent(u8x8_t *u8x8)
{
  int stat = 0;
  int button = digitalRead(ENCODER_BUTTON_PIN);
  int turn = encoder.read();

  if (button == LOW) {
    delay(20);
    button = digitalRead(ENCODER_BUTTON_PIN);
    if (button == LOW && u8x8->debounce_state == HIGH) {
      stat = U8X8_MSG_GPIO_MENU_SELECT;
      resetAutoClose();
      turn = encoder.read();
      lastEncoderTurn = turn;
    }
  }
  else {
    if (turn != lastEncoderTurn) {
      if (turn % ENCODER_DELAY == 0) {
        int delta = turn < lastEncoderTurn ? -1 : 1;
        lastEncoderTurn = turn;
        resetAutoClose();

        switch (delta)
        {
          case 1:
            stat = U8X8_MSG_GPIO_MENU_NEXT;
            break;
          case -1:
            stat =  U8X8_MSG_GPIO_MENU_PREV;
            break;
        }
      }
    }
  }
  u8x8->debounce_state = button;
  serialEvent();
  serialEvent2();
  //wireReceiveEvent(0);
  if(checkAutoClose()) {
    stat = U8X8_MSG_GPIO_MENU_HOME;
  }
  return stat;
}

bool moveHome(int index, bool showMessage = true, bool checkFeeder = true) {
  if(!steppers[index].getEnabled())
    steppers[index].setEnabled(true);

  if(feederJamed) {
    beep(4);
    return;
  }
  parserBusy = true;
  if (checkFeeder && feederEndstop()) {
    if (showMessage) {
      if (!showFeederLoadedMessage()) {
        parserBusy = false;
        return false;
      }
    }
    else {
      if (feederEndstop()) {
        unloadFilament();
      }
    }
  }
  //__debug("Stepper home");
  steppers[index].home();
  //__debug("DONE Stepper home");
  if (index == SELECTOR) {
    toolSelected = -1;
  }
  long pos = steppers[index].getStepPosition();
  if (index == SELECTOR || index == REVOLVER) {
    EEPROM.update(EEPROM_TOOL, toolSelected);
  }
  EEPROM.put(index * sizeof(long), pos);
  parserBusy = false;
  return true;
}

bool showFeederLoadedMessage() {
  bool state = false;
  lastEncoderButtonTime = millis();
  beep(1);
  int button = showDialog(P_TitleWarning, P_FeederLoaded, P_AskUnload, P_YesNoButtons);
  if (button == 1) {
    drawStatus();
    unloadFilament();
    state = true;
  }
  display.clearDisplay();
  return state;
}

bool showFeederLoadMessage() {
  bool state = false;
  lastEncoderButtonTime = millis();
  beep(1);
  int button = showDialog(P_TitleSelected, P_SelectedTool, P_AskLoad, P_YesNoButtons);
  if (button == 1) {
    drawStatus();
    loadFilament();
    state = true;
  }
  display.clearDisplay();
  return state;
}

bool showFeederFailedMessage(int state) {
  lastEncoderButtonTime = millis();
  beep(3);
  int button = showDialog(P_TitleWarning, state == 1 ? P_CantLoad : P_CantUnload, P_CheckUnit, P_OkButtonOnly);
  display.clearDisplay();
}

int showDialog(PGM_P title, PGM_P message, PGM_P addMessage, PGM_P buttons) {
  sprintf_P(_title, title);
  sprintf_P(_msg1, message);
  sprintf_P(_msg2, addMessage);
  sprintf_P(_btn, buttons);
  return display.userInterfaceMessage(_title, _msg1, _msg2, _btn);
}

void signalNoTool() {
  userBeep();
  sprintf_P(_msg1, P_NoTool);
  strcat_P(_msg1, P_Aborting);
  drawUserMessage(_msg1);
}

bool loadFilament(bool showMessage = true) {
  if (toolSelected == 255) {
    signalNoTool();
    return false;
  }
  if(smuffConfig.externalControl_Z) {
    resetRevolver();
    signalLoadFilament();
    return true;
  }
  parserBusy = true;
  if(!steppers[FEEDER].getEnabled())
    steppers[FEEDER].setEnabled(true);
  if(smuffConfig.resetBeforeFeed_Y)
    resetRevolver();
  int n = 100;
  unsigned int curSpeed = steppers[FEEDER].getMaxSpeed();
  steppers[FEEDER].setMaxSpeed(smuffConfig.insertSpeed_Z);
  while (!feederEndstop()) {
    prepSteppingRelMillimeter(FEEDER, 2.5, true);
    runAndWait(FEEDER);
    if (n == 50) {
      resetRevolver();
      prepSteppingRelMillimeter(FEEDER, -15.0, true);
      runAndWait(FEEDER);
    }
    if (n <= 0) {
      if (showMessage)
        showFeederFailedMessage(1);
      steppers[FEEDER].setMaxSpeed(curSpeed);
      feederJamed = true;
      parserBusy = false;
      return false;
    }
    n--;
  }
  feederJamed = false;
  steppers[FEEDER].setMaxSpeed(curSpeed);
  prepSteppingRelMillimeter(FEEDER, smuffConfig.bowdenLength*.95, true);
  runAndWait(FEEDER);
  steppers[FEEDER].setMaxSpeed(smuffConfig.insertSpeed_Z);
  prepSteppingRelMillimeter(FEEDER, smuffConfig.bowdenLength*.05, true);
  runAndWait(FEEDER);
  
  if(smuffConfig.reinforceLength > 0) {
    resetRevolver();
    prepSteppingRelMillimeter(FEEDER, smuffConfig.reinforceLength, true);
    runAndWait(FEEDER);
  }
  
  steppers[FEEDER].setMaxSpeed(curSpeed);
  EEPROM.put(EEPROM_FEEDER_POS, steppers[FEEDER].getStepPosition());
  if(smuffConfig.homeAfterFeed)
    steppers[REVOLVER].home();
  parserBusy = false;
  return true;
}

bool unloadFilament() {
  if (toolSelected == 255) {
    signalNoTool();
    return false;
  }
  if(smuffConfig.externalControl_Z) {
    signalUnloadFilament();
    return true;
  }
  parserBusy = true;
  if(!steppers[FEEDER].getEnabled())
    steppers[FEEDER].setEnabled(true);
  if(smuffConfig.resetBeforeFeed_Y)
    resetRevolver();
  unsigned int curSpeed = steppers[FEEDER].getMaxSpeed();
  steppers[FEEDER].setEndstopState(!steppers[FEEDER].getEndstopState());
  if(smuffConfig.unloadRetract != 0) {
    prepSteppingRelMillimeter(FEEDER, smuffConfig.unloadRetract);
    runAndWait(FEEDER);
    if(smuffConfig.unloadPushback != 0) {
      steppers[FEEDER].setMaxSpeed(smuffConfig.insertSpeed_Z);
      prepSteppingRelMillimeter(FEEDER, smuffConfig.unloadPushback);
      runAndWait(FEEDER);
      delay(smuffConfig.pushbackDelay*1000);
      steppers[FEEDER].setMaxSpeed(curSpeed);
    }
  }
  prepSteppingRelMillimeter(FEEDER, -(smuffConfig.bowdenLength*3));
  runAndWait(FEEDER);
  steppers[FEEDER].setMaxSpeed(smuffConfig.insertSpeed_Z);
  int n = 200;
  do {
    prepSteppingRelMillimeter(FEEDER, -20.0, true);
    runAndWait(FEEDER);
    if(!feederEndstop()) {
      if((n > 0 && n < 200) && n % 50 == 0) {
        resetRevolver();
        prepSteppingRelMillimeter(FEEDER, 15.0, true);
        runAndWait(FEEDER);
      }
      if (n <= 0) {
        showFeederFailedMessage(0);
        steppers[FEEDER].setMaxSpeed(curSpeed);
        feederJamed = true;
        parserBusy = false;
        return false;
      }
    }
    n--;
  } while (!feederEndstop());
  feederJamed = false;
  steppers[FEEDER].setMaxSpeed(curSpeed);
  steppers[FEEDER].setEndstopState(!steppers[FEEDER].getEndstopState());
  steppers[FEEDER].setStepPosition(0);
  EEPROM.put(EEPROM_FEEDER_POS, steppers[FEEDER].getStepPosition());
  parserBusy = false;
  return true;
}

bool selectTool(int ndx, bool showMessage = true) {
  char msg[256];
  if(feederJamed) {
    beep(4);
    sprintf_P(msg, P_FeederJamed);
    strcat_P(msg, P_Aborting);
    drawUserMessage(msg);
    return;
  }
  signalSelectorBusy();
  if(toolSelected == ndx) {
    userBeep();
    sprintf_P(msg, P_ToolAlreadySet);
    drawUserMessage(msg);
    if(smuffConfig.externalControl_Z) {
      resetRevolver();
      signalSelectorReady();
    }
    return;
  }
  if(!steppers[SELECTOR].getEnabled())
    steppers[SELECTOR].setEnabled(true);
  
  if (showMessage) {
    while(!smuffConfig.externalControl_Z && feederEndstop()) {
      if (!showFeederLoadedMessage())
        return;
    }
  }
  else {
    if (!smuffConfig.externalControl_Z && feederEndstop()) {
      unloadFilament();
    }
  }
  //__debug("Selecting tool: %d", ndx);
  parserBusy = true;
  drawSelectingMessage(ndx);
  prepSteppingAbsMillimeter(SELECTOR, smuffConfig.firstToolOffset + (ndx * smuffConfig.toolSpacing));
  remainingSteppersFlag |= _BV(SELECTOR);
  if(!smuffConfig.resetBeforeFeed_Y) {
    prepSteppingAbs(REVOLVER, smuffConfig.firstRevolverOffset + (ndx *smuffConfig.revolverSpacing), true);
    remainingSteppersFlag |= _BV(REVOLVER);
  }
  runAndWait(-1);
  toolSelected = ndx;
  EEPROM.put(EEPROM_TOOL, toolSelected);
  for (int i = 0; i < NUM_STEPPERS; i++) {
    EEPROM.put(i * sizeof(long), steppers[i].getStepPosition());
  }
  if (!smuffConfig.externalControl_Z && showMessage) {
    showFeederLoadMessage();
  }
  if(smuffConfig.externalControl_Z) {
    resetRevolver();
    signalSelectorReady();
  }
  if(testMode)
    Serial2.print("T"); Serial2.println(ndx);
  parserBusy = false;
  return true;
}

void resetRevolver() {
  //__debug("resetting revolver");
  moveHome(REVOLVER, false, false);
  //__debug("DONE resetting revolver");
  if (toolSelected > -1) {
    prepSteppingAbs(REVOLVER, smuffConfig.firstRevolverOffset + (toolSelected*smuffConfig.revolverSpacing), true);
    runAndWait(REVOLVER);
  }
}

void setStepperSteps(int index, long steps, bool ignoreEndstop) {
  if (steps != 0)
    steppers[index].prepareMovement(steps, ignoreEndstop);
}

void prepSteppingAbs(int index, long steps, bool ignoreEndstop = false) {
  long pos = steppers[index].getStepPosition();
  long _steps = steps - pos;
  setStepperSteps(index, _steps, ignoreEndstop);
}

void prepSteppingAbsMillimeter(int index, float millimeter, bool ignoreEndstop = false) {
  unsigned int stepsPerMM = steppers[index].getStepsPerMM();
  long steps = (long)((float)millimeter * stepsPerMM);
  long pos = steppers[index].getStepPosition();
  setStepperSteps(index, steps - pos, ignoreEndstop);
}

void prepSteppingRel(int index, long steps, bool ignoreEndstop = false) {
  setStepperSteps(index, steps, ignoreEndstop);
}

void prepSteppingRelMillimeter(int index, float millimeter, bool ignoreEndstop = false) {
  unsigned int stepsPerMM = steppers[index].getStepsPerMM();
  long steps = (long)((float)millimeter * stepsPerMM);
  setStepperSteps(index, steps, ignoreEndstop);
}

void printEndstopState(int serial) {
  sprintf(tmp, "Selector: %s\tRevolver: %s\tFeeder: %s\n",
          selectorEndstop()  ? "triggered" : "open",
          revolverEndstop()  ? "triggered" : "open",
          feederEndstop()    ? "triggered" : "open");
  printResponse(tmp, serial);
}

void printSpeeds(int serial) {
  sprintf_P(tmp, P_AccelSpeed,
          String(steppers[SELECTOR].getMaxSpeed()).c_str(),
          String(steppers[REVOLVER].getMaxSpeed()).c_str(),
          smuffConfig.externalControl_Z ? "external" : String(steppers[FEEDER].getMaxSpeed()).c_str());
  printResponse(tmp, serial);
}

void printAcceleration(int serial) {
  sprintf_P(tmp, P_AccelSpeed,
          String(steppers[SELECTOR].getAcceleration()).c_str(),
          String(steppers[REVOLVER].getAcceleration()).c_str(),
          smuffConfig.externalControl_Z ? "external" : String(steppers[FEEDER].getAcceleration()).c_str());
  printResponse(tmp, serial);
}

void printOffsets(int serial) {
  sprintf_P(tmp, P_AccelSpeed,
          String((int)(smuffConfig.firstToolOffset*10)).c_str(),
          String(smuffConfig.firstRevolverOffset).c_str(),
          "--");
  printResponse(tmp, serial);
}

void printPos(int index, int serial) {
  sprintf(buf, "Pos. '%s': %d\n", steppers[index].getDescriptor(), steppers[index].getStepPosition());
  printResponse(buf, serial);
}

void beep(int count) {
  for (int i = 0; i < count; i++) {
    tone(BEEPER_PIN, BEEPER_FREQUENCY, BEEPER_DURATION);
    delay(BEEPER_DURATION*2);
  }
}

void userBeep() {
  tone(BEEPER_PIN, BEEPER_FREQUENCY, BEEPER_DURATION);
  delay(BEEPER_DURATION*2);
  tone(BEEPER_PIN, BEEPER_UFREQUENCY, BEEPER_UDURATION);
  delay(BEEPER_UDURATION*2);
  tone(BEEPER_PIN, BEEPER_UFREQUENCY, BEEPER_UDURATION);
}

bool setServoPos(int degree) {
  return servo.setServoPos(degree);
}

void getEepromData() {
  long pos;

  EEPROM.get(EEPROM_SELECTOR_POS, pos);
  steppers[SELECTOR].setStepPosition(pos);
  EEPROM.get(EEPROM_REVOLVER_POS, pos);
  steppers[REVOLVER].setStepPosition(pos);
  EEPROM.get(EEPROM_FEEDER_POS, pos);
  steppers[FEEDER].setStepPosition(pos);

  EEPROM.get(EEPROM_TOOL, toolSelected);
  EEPROM.get(EEPROM_CONTRAST, smuffConfig.lcdContrast);

  EEPROM.get(EEPROM_TOOL_COUNT, smuffConfig.toolCount);
  if (smuffConfig.toolCount < MIN_TOOLS || smuffConfig.toolCount > MAX_TOOLS) {
    smuffConfig.toolCount = 5;
    EEPROM.put(EEPROM_TOOL_COUNT, smuffConfig.toolCount);
  }

}

void setSignalPort(int port, bool state) {
  sprintf(tmp,"%c%c%s", 0x1b, port, state ? "1" : "0");
  Serial2.write(tmp);
}

void signalSelectorReady() {
  setSignalPort(SELECTOR_SIGNAL, false);
  //__debug("Signalling Selector ready");
}

void signalSelectorBusy() {
  setSignalPort(SELECTOR_SIGNAL, true);
  //__debug("Signalling Selector busy");
}

void signalLoadFilament() {
  setSignalPort(FEEDER_SIGNAL, true);
  //__debug("Signalling load filament");
}

void signalUnloadFilament() {
  setSignalPort(FEEDER_SIGNAL, false);
  //__debug("Signalling unload filament");
}

void listDir(File root, int numTabs, int serial) {
  while (true) {
    File entry =  root.openNextFile();
    if (!entry)
      break;

    for (int i = 1; i < numTabs; i++) {
      printResponse("\t", serial);
    }
    printResponse(entry.name(), serial);
    if (entry.isDirectory()) {
      printResponse("/\r\n", serial);
      //listDir(entry, numTabs + 1, serial);
    } 
    else {
      sprintf(tmp, "\t\t%ld\r\n", entry.size());
      printResponse(tmp, serial);
    }
    entry.close();
  }
}

void __debug(const char* fmt, ...) {
#ifdef DEBUG
  char _tmp[1024];
  va_list arguments;
  va_start(arguments, fmt); 
  vsnprintf(_tmp, 1024, fmt, arguments);
  va_end (arguments); 
  Serial.println(_tmp);
#endif
}

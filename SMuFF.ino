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
#include "SMuFF.h"
#include "Config.h"
#include "ZTimerLib.h"
#include "ZStepperLib.h"
#include "ZServo.h"

ZStepper                        steppers[NUM_STEPPERS];
ZTimer                          stepperTimer;
ZServo                          servo(SERVO1_PIN);
U8G2_ST7565_64128N_F_4W_HW_SPI  display(U8G2_R2, /* cs=*/ DSP_CS_PIN, /* dc=*/ DSP_DC_PIN, /* reset=*/ DSP_RESET_PIN);
Encoder                         encoder(ENCODER1_PIN, ENCODER2_PIN);

volatile byte           nextStepperFlag = 0;
volatile byte           remainingSteppersFlag = 0;
volatile unsigned long  lastEncoderButtonTime = 0;
bool                    testMode = false;
int                     toolSelections[MAX_TOOLS]; 
unsigned long           pwrSaveTime;
bool                    isPwrSave = false;

String serialBuffer0, serialBuffer2, serialBuffer9; 
String mainList;
String toolsList;
String offsetsList;
String traceSerial2;
char   tmp[128];

extern char _title[128];


void overrideStepX() {
  STEP_HIGH_X
  // if(smuffConfig.delayBetweenPulses) __asm__ volatile ("nop");
  STEP_LOW_X
}

void overrideStepY() {
  STEP_HIGH_Y
  // if(smuffConfig.delayBetweenPulses) __asm__ volatile ("nop");
  STEP_LOW_Y
}

void overrideStepZ() {
  STEP_HIGH_Z
  // if(smuffConfig.delayBetweenPulses) __asm__ volatile ("nop");
  STEP_LOW_Z
}

void endstopYevent() {
  //__debug("Endstop Revolver: %d", steppers[REVOLVER].getStepPosition());
}

void endstopZevent() {
  //__debug("Endstop Revolver: %d", steppers[REVOLVER].getStepPosition());
}

void setup() {

  serialBuffer0.reserve(80);
  serialBuffer2.reserve(80);
  serialBuffer9.reserve(80);

  setupDisplay(); 
  readConfig();

  steppers[SELECTOR] = ZStepper(SELECTOR, "Selector", X_STEP_PIN, X_DIR_PIN, X_ENABLE_PIN, smuffConfig.acceleration_X, smuffConfig.maxSpeed_X);
  steppers[SELECTOR].setEndstop(X_END_PIN, smuffConfig.endstopTrigger_X, ZStepper::MIN);
  steppers[SELECTOR].stepFunc = overrideStepX;
  steppers[SELECTOR].setMaxStepCount(smuffConfig.maxSteps_X);
  steppers[SELECTOR].setStepsPerMM(smuffConfig.stepsPerMM_X);
  steppers[SELECTOR].setInvertDir(smuffConfig.invertDir_X);

  steppers[REVOLVER] = ZStepper(REVOLVER, "Revolver", Y_STEP_PIN, Y_DIR_PIN, Y_ENABLE_PIN, smuffConfig.acceleration_Y, smuffConfig.maxSpeed_Y);
  steppers[REVOLVER].setEndstop(Y_END_PIN, smuffConfig.endstopTrigger_Y, ZStepper::ORBITAL);
  steppers[REVOLVER].stepFunc = overrideStepY;
  steppers[REVOLVER].setMaxStepCount(smuffConfig.stepsPerRevolution_Y);
  steppers[REVOLVER].endstopFunc = endstopYevent;
  steppers[REVOLVER].setInvertDir(smuffConfig.invertDir_Y);
  
  steppers[FEEDER] = ZStepper(FEEDER, "Feeder", Z_STEP_PIN, Z_DIR_PIN, Z_ENABLE_PIN, smuffConfig.acceleration_Z, smuffConfig.maxSpeed_Z);
  steppers[FEEDER].setEndstop(Z_END_PIN, smuffConfig.endstopTrigger_Z, ZStepper::MIN);
  steppers[FEEDER].stepFunc = overrideStepZ;
  steppers[FEEDER].setStepsPerMM(smuffConfig.stepsPerMM_Z);
  steppers[FEEDER].endstopFunc = endstopZevent;
  steppers[FEEDER].setInvertDir(smuffConfig.invertDir_Z);

  stepperTimer.setupTimer(ZTimer::TIMER4, ZTimer::PRESCALER1);
  stepperTimer.setupTimerHook(isrTimerHandler);

  Serial.begin(smuffConfig.serial1Baudrate);
  Serial2.begin(smuffConfig.serial2Baudrate);
  
  getEepromData();
  //__debug("DONE reading EEPROM");

  char menu[256];
  sprintf_P(menu, P_MenuItemBack);
  strcat_P(menu, P_MenuItems);
  mainList = String(menu);
  //__debug("DONE setting Main menu");
  sprintf_P(menu, P_MenuItemBack);
  strcat_P(menu, P_OfsMenuItems);
  offsetsList = String(menu);
  

  for(int i=0; i < NUM_STEPPERS; i++) {
      steppers[i].runAndWaitFunc = runAndWait;
      steppers[i].runNoWaitFunc = runNoWait;
      steppers[i].setEnabled(true);
  }
  //__debug("DONE enabling steppers");

  pinMode(FAN_PIN, OUTPUT);
  if(smuffConfig.fanSpeed > 0 && smuffConfig.fanSpeed <= 255)
    analogWrite(FAN_PIN, smuffConfig.fanSpeed);

  if(smuffConfig.i2cAddress != 0) {
    Wire.begin(smuffConfig.i2cAddress);
    Wire.onReceive(wireReceiveEvent);
  }
  //__debug("DONE I2C init");
  
  resetRevolver();
  //__debug("DONE reset Revolver");
  
  servo.setServoPos(0);
  sendStartResponse(0);
  //sendStartResponse(2);
  pwrSaveTime = millis();
}

void setupToolsMenu() {
  char menu[256];
  sprintf_P(menu, P_MenuItemBack);
  memset(toolSelections, 0, sizeof(int)*MAX_TOOLS);
  int n = 0;
  for(int i=0; i< smuffConfig.toolCount; i++) {
    if(i == toolSelected)
      continue;
    toolSelections[n] = i;
    sprintf_P(tmp, P_ToolMenu, i);
    strcat(menu, tmp);
    strcat(menu, "\n");
    n++;
  }
  menu[strlen(menu)-1] = '\0';
  toolsList = String(menu);
}

void setNextInterruptInterval() {

  unsigned int minDuration = 999999;
  for(int i = 0; i < NUM_STEPPERS; i++) {
    if((_BV(i) & remainingSteppersFlag) && steppers[i].getDuration() < minDuration ) {
      minDuration = steppers[i].getDuration();
    }
  }

  nextStepperFlag = 0;
  for(int i = 0; i < NUM_STEPPERS; i++) {

    if ( (_BV(i) & remainingSteppersFlag) && steppers[i].getDuration() == minDuration )
      nextStepperFlag |= _BV(i);
  }

  if (remainingSteppersFlag == 0) {
    stepperTimer.setOCRxA(65500);
  }
  //__debug("minDuration: %d", minDuration);
  stepperTimer.setNextInterruptInterval(minDuration);
}


void isrTimerHandler() {
  unsigned int tmp = stepperTimer.getOCRxA(); 
  stepperTimer.setOCRxA(65500);

  for (int i = 0; i < NUM_STEPPERS; i++) {
    if(!(_BV(i) & remainingSteppersFlag))
      continue;

    if(!(nextStepperFlag & _BV(i))) {
      steppers[i].setDuration(steppers[i].getDuration() - tmp);
      continue;
    }
    
    steppers[i].handleISR();
    if(steppers[i].getMovementDone())
      remainingSteppersFlag &= ~_BV(i); 
  }
  //__debug("ISR(): %d", remainingSteppersFlag);
  setNextInterruptInterval();
}

void runNoWait(int index) {
  if(index != -1)
    remainingSteppersFlag |= _BV(index);
  setNextInterruptInterval();
}

void runAndWait(int index) {
  runNoWait(index);
  while(remainingSteppersFlag);
}

static int lastTurn;
static bool showMenu = false; 
static bool lastZEndstopState = 0;

void loop() {

  if(feederEndstop() != lastZEndstopState) {
    lastZEndstopState = feederEndstop();
    setSignalPort(1, feederEndstop());
  }
  //__debug("Mem: %d", freeMemory());

  if(!checkUserMessage()) {
    if(!isPwrSave) {
      display.firstPage();
      do {
        drawLogo();
        drawStatus();
      } while(display.nextPage());
    }
  }
  else {
    if(millis()-userMessageTime > USER_MESSAGE_RESET*1000) {
      displayingUserMessage = false;
    }
  }

  int button = digitalRead(ENCODER_BUTTON_PIN);
  if(button == LOW && isPwrSave) {
    setPwrSave(0);
  }
  else {
    int turn = encoder.read();
    if(turn % ENCODER_DELAY_MENU == 0) {
      if(!showMenu && turn != lastTurn) {
        if(isPwrSave) {
          setPwrSave(0);
        }
        else {
          displayingUserMessage = false;
          showMenu = true;
          if(turn < lastTurn) {
            showMainMenu();
          }
          else {
            showToolsMenu();
          }
          turn = encoder.read();
          showMenu = false;
        }
      }
      lastTurn = turn;
    }
  }
  delay(100);
  if((millis() - pwrSaveTime)/1000 >= smuffConfig.powerSaveTimeout && !isPwrSave) {
    //__debug("Power save mode after %d seconds (%d)", (millis() - pwrSaveTime)/1000, smuffConfig.powerSaveTimeout);
    setPwrSave(1);
  }
}

void setPwrSave(int state) {
  display.setPowerSave(state);
  isPwrSave = state == 1;
  if(!isPwrSave)
    pwrSaveTime = millis();
}

bool checkUserMessage() {
  int button = digitalRead(ENCODER_BUTTON_PIN);
  if(button == LOW && displayingUserMessage) {
    displayingUserMessage = false;
  }
  if(millis()-userMessageTime > USER_MESSAGE_RESET*1000) {
    displayingUserMessage = false;
    //__debug("%ld", (millis()-userMessageTime)/1000);
  }
  return displayingUserMessage;
}

void showMainMenu() {

  bool stopMenu = false;
  unsigned int startTime = millis();
  uint8_t current_selection = 0;
  sprintf_P(_title, P_TitleMainMenu);
  do {
    resetAutoClose();
    while(checkUserMessage());
    current_selection = display.userInterfaceSelectionList(_title, current_selection, mainList.c_str());

    if(current_selection == 0)
      return;

    else {
      switch(current_selection) {
        case 1:
          stopMenu = true;
          break;
        
        case 2:
          moveHome(SELECTOR);
          moveHome(REVOLVER, true, false);
          startTime = millis();
          break;
        
        case 3:
          steppers[SELECTOR].setEnabled(false);
          steppers[REVOLVER].setEnabled(false);
          steppers[FEEDER].setEnabled(false);
          startTime = millis();
          break;

        case 4:
          showOffsetsMenu();
          break;
          
        case 5:
          loadFilament();
          startTime = millis();
          break;
        
        case 6:
          unloadFilament();
          startTime = millis();
          break;
      }
    }
    if (millis() - startTime > smuffConfig.menuAutoClose * 1000) {
      stopMenu = true;
    }
  } while(!stopMenu);
}

void showOffsetsMenu() {
  bool stopMenu = false;
  unsigned int startTime = millis();
  uint8_t current_selection = 0;
  sprintf_P(_title, P_TitleOffsetsMenu);
  
  do {
    resetAutoClose();
    while(checkUserMessage());
    current_selection = display.userInterfaceSelectionList(_title, current_selection, offsetsList.c_str());

    if(current_selection == 0)
      return;

    else {
      switch(current_selection) {
        case 1:
          stopMenu = true;
          break;
        
        case 2:
          changeOffset(SELECTOR);
          startTime = millis();
          break;
        
        case 3:
          changeOffset(REVOLVER);
          startTime = millis();
          break;
      }
    }
    if (millis() - startTime > smuffConfig.menuAutoClose * 1000) {
      stopMenu = true;
    }
  } while(!stopMenu);  
}

void changeOffset(int index) {
  int turn = encoder.read();
  int lastTurn = 0;
  int lastButton = HIGH;
  int button;
  int steps = (smuffConfig.stepsPerRevolution_Y / 360);
  float stepsF = 0.1f;

  while((button = digitalRead(ENCODER_BUTTON_PIN)) == LOW)
    delay(20);
  
  moveHome(index);
  long pos = steppers[index].getStepPosition();
  float posF = steppers[index].getStepPositionMM();

  drawPosition(index);

  while(1) {
    lastButton = button;
    button = digitalRead(ENCODER_BUTTON_PIN);
    if(button == LOW && lastButton == HIGH)
      break;
    turn = encoder.read();
    if(turn == lastTurn)
      continue;
    if(turn < lastTurn) {
      pos = -steps;
      posF = -stepsF;
    }
    else {
      pos = steps;
      posF = stepsF;
    }
    lastTurn = turn;
    if(index == REVOLVER) {
      prepSteppingRel(index, pos, true);
    }
    if(index == SELECTOR) {
      prepSteppingRelMillimeter(SELECTOR, posF, true);
    }
    runAndWait(index);
    drawPosition(index);
  }
}

void drawPosition(int index) {
  if(index == REVOLVER) {
    sprintf(tmp, "%s\n%ld", steppers[index].getDescriptor(), steppers[index].getStepPosition());
  }
  if(index == SELECTOR) {
    sprintf(tmp, "%s\n%s", steppers[index].getDescriptor(), String(steppers[index].getStepPositionMM()).c_str());
  }
  drawUserMessage(String(tmp));
}

void showToolsMenu() {

  bool stopMenu = false;
  unsigned int startTime = millis();
  uint8_t current_selection = 0;
  sprintf_P(_title, P_TitleToolsMenu);
  do {
    setupToolsMenu();
    resetAutoClose();
    while(checkUserMessage());
    uint8_t startPos = toolSelected == 255 ? 0 : toolSelected+1;
    current_selection = display.userInterfaceSelectionList(_title, startPos, toolsList.c_str());

    if(current_selection <= 1)
      stopMenu = true;
    else {
      int tool = toolSelections[current_selection-2];
      selectTool(tool);
      startTime = millis();
    }
    if (millis() - startTime > smuffConfig.menuAutoClose * 1000) {
      stopMenu = true;
    }
  } while(!stopMenu);
}

void resetAutoClose() {
  lastEncoderButtonTime = millis();
}

bool checkAutoClose() {
  //__debug("Home: %ld", millis()-lastEncoderButtonTime);
  if (millis() - lastEncoderButtonTime >= smuffConfig.menuAutoClose*1000) {
    return true;
  }
  return false;
}

void serialEvent() {
  while (Serial.available()) {
    char in = (char)Serial.read();
    if (in == '\n') {
      //__debug("Received: %s", serialBuffer0.c_str());
      parseGcode(serialBuffer0, 0);
      serialBuffer0 = "";
    }
    else
      serialBuffer0 += in;
  }
}

void serialEvent2() {
  while (Serial2.available()) {
    char in = (char)Serial2.read();
    if (in == '\n') {
      Serial.println(serialBuffer2);
      parseGcode(serialBuffer2, 2);
      traceSerial2 = String(serialBuffer2);
      serialBuffer2 = "";
    }
    else
      serialBuffer2 += in;
  }
}

void wireReceiveEvent(int numBytes) {
  while (Wire.available()) {
    char in = (char)Wire.read();
    if (in == '\n') {
      parseGcode(serialBuffer9, 9);
      serialBuffer9 = "";
    }
    else
        serialBuffer9 += in;
  }
}

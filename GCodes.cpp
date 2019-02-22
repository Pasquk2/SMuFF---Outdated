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
 * Module for handling all the G-Codes supported
 */
 
#include "SMuFF.h"
#include "ZTimerLib.h"
#include "ZStepperLib.h"
#include "GCodes.h"

extern ZStepper steppers[NUM_STEPPERS];

const char* S_Param = "S";
const char* P_Param = "P";
const char* X_Param = "X";
const char* Y_Param = "Y";
const char* Z_Param = "Z";
const char* E_Param = "E";
const char* F_Param = "F";
const char* C_Param = "C";
const char* T_Param = "T";
const char* N_Param = "N";

GCodeFunctions gCodeFuncsM[] = {
  {  80, dummy },
  {  81, dummy },
  { 104, dummy },
  { 105, dummy },
  { 108, dummy },
  { 109, dummy },
  { 220, dummy },
  { 221, dummy },

  {  18, M18 },
  {  20, M20 },
  {  42, M42 },
  {  84, M18 },
  { 106, M106 },
  { 107, M107 }, 
  { 110, M110 },
  { 111, M111 },
  { 114, M114 },
  { 115, M115 },
  { 117, M117 },
  { 119, M119 },
  { 201, M201 },
  { 203, M203 },
  { 206, M206 },
  { 250, M250 },
  { 280, M280 },
  { 300, M300 },
  { 500, M500 },
  { 503, M503 },
  { 700, M700 },
  { 701, M701 },
  { 999, M999 },
  { 2000, M2000 },
  { 2001, M2001 },
  { -1, NULL }
};

GCodeFunctions gCodeFuncsG[] = {
  {   0, G0 },
  {   1, G1 },
  {   4, G4 },
  {  12, G12 },
  {  28, G28 },
  {  90, G90 },
  {  91, G91 },
  { -1, NULL},
};

int param;
extern char tmp[128];

/*========================================================
 * Class G
 ========================================================*/
bool dummy(const char* msg, String buf, int serial) {
  int code = buf.toInt();
  __debug("Ignored M-Code: M%d", code);
  return true;
}

bool M18(const char* msg, String buf, int serial) {
  bool stat = true;
  printResponse(msg, serial); 
  if(buf.length()==0) {
    steppers[SELECTOR].setEnabled(false);
    steppers[REVOLVER].setEnabled(false);
    steppers[FEEDER].setEnabled(false);
  }
  else {
    if(buf.indexOf(X_Param) != -1) {
      steppers[SELECTOR].setEnabled(false);
    }
    else if(buf.indexOf(Y_Param) != -1) {
      steppers[REVOLVER].setEnabled(false);
    }
    else if(buf.indexOf(Z_Param) != -1) {
      steppers[FEEDER].setEnabled(false);
    }
    else {
      stat = false;
    }
  }
  return stat;
}

bool M20(const char* msg, String buf, int serial) {
  
  if(!getParamString(buf, S_Param, tmp, sizeof(tmp))){
    sprintf(tmp,"/");
  }
  if (SD.begin(SD_SS_PIN)) {
    File root = SD.open(tmp);
    listDir(root, 1, serial);
    root.close();
    return true;
  }
  sprintf_P(tmp, P_SD_InitError);
  printResponse(tmp, serial); 
  return false;
}

bool M42(const char* msg, String buf, int serial) {
  bool stat = true;
  int pin;
  printResponse(msg, serial); 
  if((pin = getParam(buf, P_Param)) == -1) {
    pinMode(pin, OUTPUT);
    if((param = getParam(buf, S_Param)) == -1) {
      if(param >= 0 && param <= 255)
        analogWrite(pin, param);
    }
  }
  return stat;
}

bool M106(const char* msg, String buf, int serial) {
  printResponse(msg, serial); 
  if((param = getParam(buf, S_Param)) == -1) {
    param = 255;
  }
  analogWrite(FAN_PIN, param);
  return true;
}
 
bool M107(const char* msg, String buf, int serial) {
  printResponse(msg, serial); 
  analogWrite(FAN_PIN, 0);
  return true;
}

bool M110(const char* msg, String buf, int serial) {
  printResponse(msg, serial); 
  if((param = getParam(buf, N_Param)) != -1) {
    currentLine = param;
  }
  return true;
}

bool M111(const char* msg, String buf, int serial) {
  if((param = getParam(buf, S_Param)) != -1) {
    testMode = param == 1;
  }      
  return true;
}

bool M114(const char* msg, String buf, int serial) {
  printResponse(msg, serial); 
  sprintf_P(tmp, P_AccelSpeed, 
  String(steppers[SELECTOR].getStepPositionMM()).c_str(),
  String(steppers[REVOLVER].getStepPosition()).c_str(),
  String(steppers[FEEDER].getStepPositionMM()).c_str());
  printResponse(tmp, serial); 
  return true;
}

bool M115(const char* msg, String buf, int serial) {
  sprintf_P(tmp, P_GVersion, VERSION_STRING, VERSION_DATE);
  printResponse(tmp, serial); 
  return true;
}

bool M117(const char* msg, String buf, int serial) {
  String umsg = buf;
  umsg.replace("_", " ");
  beep(1);
  drawUserMessage(umsg);
  return true;
}

bool M119(const char* msg, String buf, int serial) {
  printResponse(msg, serial); 
  if((param = getParam(buf, Z_Param)) != -1) {
    steppers[FEEDER].setEndstopHit(param);
  }
  printEndstopState(serial); 
  return true;
}

bool M201(const char* msg, String buf, int serial) {
  bool stat = true;
  printResponse(msg, serial); 
  if(buf.length()==0) {
    printAcceleration(serial);
    return stat;
  }
  if((param = getParam(buf, X_Param))  != -1) {
    if(param >= 200 && param <= 15000)
      steppers[SELECTOR].setAcceleration(param);
    else stat = false;
  }
  if((param = getParam(buf, Y_Param))  != -1) {
    if(param >= 200 && param <= 15000)
      steppers[REVOLVER].setAcceleration(param);
    else stat = false;
  }
  if((param = getParam(buf, Z_Param))  != -1) {
    if(param >= 200 && param <= 15000)
      steppers[FEEDER].setAcceleration(param);
    else stat = false;
  }
  return stat;
}

bool M203(const char* msg, String buf, int serial) {
  bool stat = true;
  printResponse(msg, serial); 
  if(buf.length()==0) {
    printSpeeds(serial);
    return stat;
  }
  if((param = getParam(buf, X_Param))  != -1) {
    if(param > 0 && param <= 10000)
      steppers[SELECTOR].setMaxSpeed(param);
    else stat = false;
  }
  if((param = getParam(buf, Y_Param))  != -1) {
    if(param > 0 && param <= 10000) {
      steppers[REVOLVER].setMaxSpeed(param);
      //__debug("Revolver max speed: %d", steppers[REVOLVER].getMaxSpeed());
    }
    else stat = false;
  }
  if((param = getParam(buf, Z_Param))  != -1) {
    if(param > 0 && param <= 10000)
      steppers[FEEDER].setMaxSpeed(param);
    else stat = false;
  }
  return stat;
}

bool M206(const char* msg, String buf, int serial) {
  bool stat = true;
  printResponse(msg, serial); 
  if(buf.length()==0) {
    printOffsets(serial);
    return stat;
  }
  if((param = getParam(buf, X_Param))  != -1) {
    if(param > 0 && param <= 10000)
      smuffConfig.firstToolOffset = (float)param/10;
    else stat = false;
  }
  if((param = getParam(buf, Y_Param))  != -1) {
    if(param > 0 && param <= 8640) {
      smuffConfig.firstRevolverOffset = param;
    }
    else stat = false;
  }
}

bool M250(const char* msg, String buf, int serial) {
  bool stat = true;
  if((param = getParam(buf, C_Param)) != -1) {
    if(param >= 60 && param < 256) {
      display.setContrast(param);
      EEPROM.put(EEPROM_CONTRAST, param);
      printResponse(msg, serial); 
    }
    else
      stat = false;
  }
  else {
      printResponse(msg, serial);
      char tmp[50];
      sprintf_P(tmp, P_M250Response, smuffConfig.lcdContrast);
      printResponse(tmp, serial); 
  }
  return stat;
}

bool M280(const char* msg, String buf, int serial) {
  bool stat = true;
  printResponse(msg, serial);
  if((param = getParam(buf, S_Param)) != -1) {
    if(!setServoPos(param))
      stat = false;
  }
  else stat = false;
  return stat;
}

bool M300(const char* msg, String buf, int serial) {
  bool stat = true;
  printResponse(msg, serial);
  if((param = getParam(buf, S_Param)) != -1) {
    int frequency = param;
    if((param = getParam(buf, P_Param)) != -1) {
      tone(BEEPER_PIN, frequency, param);
    }
    else 
      stat = false;
  }
  else 
    stat = false;
  return stat;
}

bool M500(const char* msg, String buf, int serial) {
  printResponse(msg, serial);
  saveSettings(serial);
  return true;
}

bool M503(const char* msg, String buf, int serial) {
  printResponse(msg, serial);
  reportSettings(serial);
  return true;
}

bool M700(const char* msg, String buf, int serial) {
  bool stat = true;
  printResponse(msg, serial);
  if(toolSelected > 0 && toolSelected <= MAX_TOOLS) {
    getParamString(buf, S_Param, smuffConfig.materials[toolSelected], sizeof(smuffConfig.materials[0]));
    //__debug("Material: %s\n",smuffConfig.materials[toolSelected]);
    return loadFilament();
  }
  else 
    stat = false;
  return stat;
}

bool M701(const char* msg, String buf, int serial) {
  printResponse(msg, serial);
  return unloadFilament();
}

bool M999(const char* msg, String buf, int serial) {
  printResponse(msg, serial); 
  delay(500); 
  __asm__ volatile ("jmp 0x0000"); 
  return true;
}

bool M2000(const char* msg, String buf, int serial) {
  char s[80];
  printResponse(msg, serial); 
  getParamString(buf, S_Param, tmp, sizeof(tmp));
  if(strlen(tmp)>0) {
    printResponse("B", serial);
    for(int i=0; i< strlen(tmp); i++) {
      sprintf(s,"%d:", (char)tmp[i]);
      printResponse(s, serial); 
    }
    printResponse("10\n", serial);
  }
  return true;
}

bool M2001(const char* msg, String buf, int serial) {
  printResponse(msg, serial); 
  getParamString(buf, S_Param, tmp, sizeof(tmp));
  String data = String(tmp);
  data.trim();
  if(data.length() > 0) {
    int ndx = 0;
    int pos = 0;
    if(data.startsWith("B")) {
      printResponse(">>", serial); 
      ndx++;
      do {
        pos = data.indexOf(":", ndx);
        int c;
        if(pos != -1) {
          c = data.substring(ndx, pos).toInt();
        }
        else {
          c = data.substring(ndx).toInt();
        }
        if(c == 10) {
          printResponse("\\n", serial); 
        }
        else {
          sprintf(tmp, "%c", c);
          printResponse(tmp, serial); 
        }
        ndx = pos + 1;
      } while(pos != -1);
      printResponse("<<\n", serial); 
    }
    else {
      printResponseP(P_WrongFormat, serial);
      return false;
    }
  }
  else 
    return false;
  return true;
}

/*========================================================
 * Class G
 ========================================================*/
bool G0(const char* msg, String buf, int serial) {
  printResponse(msg, serial);
  if((param = getParam(buf, Y_Param)) != -1) {
    steppers[REVOLVER].setEnabled(true);
    prepSteppingAbs(REVOLVER, smuffConfig.firstRevolverOffset + ((param)*smuffConfig.revolverSpacing), true);
    runAndWait(REVOLVER);
  }
  if((param = getParam(buf, X_Param)) != -1) {
    steppers[SELECTOR].setEnabled(true);
    prepSteppingAbsMillimeter(SELECTOR, smuffConfig.firstToolOffset + (param * smuffConfig.toolSpacing));
    runAndWait(SELECTOR);
  }
  return true;
}

bool G1(const char* msg, String buf, int serial) {
  printResponse(msg, serial);
  bool isMill = true;
  if((param = getParam(buf, T_Param)) != -1) {
    isMill = (param == 1);
  }
  if((param = getParam(buf, Y_Param)) != -1) {
    //__debug("G1 moving Y: %d", param);
    steppers[REVOLVER].setEnabled(true);
    prepStepping(REVOLVER, (long)param, isMill);
  }
  if((param = getParam(buf, X_Param)) != -1) {
    //__debug("G1 moving X: %d", param);
    steppers[SELECTOR].setEnabled(true);
    prepStepping(SELECTOR, (long)param, isMill, true);
  }
  if((param = getParam(buf, Z_Param)) != -1) {
    //__debug("G1 moving Z: %d", param);
    steppers[FEEDER].setEnabled(true);
    prepStepping(FEEDER, (long)param, isMill);
  }
  runAndWait(-1);
  return true;
}

bool G4(const char* msg, String buf, int serial) {
  bool stat = true;
  printResponse(msg, serial);
  if((param = getParam(buf, S_Param)) != -1) {
    if(param > 0 && param < 500)
      delay(param*1000);
  }
  else if((param = getParam(buf, P_Param)) != -1) {
      delay(param);
  }
  else {
    stat = false;
  }
  return stat;
}

bool G12(const char* msg, String buf, int serial) {
  printResponse(msg, serial);
  setServoPos(180);
  delay(500);
  setServoPos(0);
  return true;
}

bool G28(const char* msg, String buf, int serial) {
  bool stat = true;
  printResponse(msg, serial);
  if(buf.length()==0) {
    stat = moveHome(SELECTOR, false, true); 
    if(stat)
      moveHome(REVOLVER, false, false); 
  }
  else {
    if(buf.indexOf(X_Param) != -1) {
      stat = moveHome(SELECTOR, false, false); 
    }
    if(buf.indexOf(Y_Param) != -1) {
      stat = moveHome(REVOLVER, false, false); 
    }
  }
  return stat;
}

bool G90(const char* msg, String buf, int serial) {
  printResponse(msg, serial);
  positionMode = ABSOLUTE;
  return true;
}

bool G91(const char* msg, String buf, int serial) {
  printResponse(msg, serial);
  positionMode = RELATIVE;
  return true;
}

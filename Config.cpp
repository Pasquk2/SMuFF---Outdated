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
  * Module for reading configuration file (JSON-Format) from SD-Card
  */

#include "Config.h"
#include "SMuFF.h"
#include <ArduinoJson.h>

void readConfig()
{
  StaticJsonBuffer<2048> jsonBuffer;
  
  if (!SD.begin(SD_SS_PIN)) {
    drawSDStatus(SD_ERR_INIT);
    delay(5000);
    return;
  }
  File cfg = SD.open(CONFIG_FILE, FILE_READ);
  if (!cfg){
    drawSDStatus(SD_ERR_NOCONFIG);
    delay(3000);
  } 
  else {
    size_t fsize = cfg.size();
    if(fsize > sizeof(jsonBuffer)) {
      showDialog(P_TitleConfigError, P_ConfigFail1, P_ConfigFail3, P_OkButtonOnly);
      cfg.close();
      return;
    }
    JsonObject& root = jsonBuffer.parseObject(cfg);
    if (!root.success()) {
      showDialog(P_TitleConfigError, P_ConfigFail1, P_ConfigFail2, P_OkButtonOnly);
    } 
    else {
      char* selector =  "Selector";
      char* revolver =  "Revolver";
      char* feeder   =  "Feeder";
      drawSDStatus(SD_READING_CONFIG);
      int toolCnt =                     root["ToolCount"];
      smuffConfig.toolCount = (toolCnt > MIN_TOOLS && toolCnt < MAX_TOOLS) ? toolCnt : 5;
      smuffConfig.firstToolOffset =     root[selector]["Offset"];
      smuffConfig.toolSpacing =         root[selector]["Spacing"];
      smuffConfig.stepsPerMM_X =        root[selector]["StepsPerMillimeter"];
      smuffConfig.maxSteps_X = ((smuffConfig.toolCount-1)*smuffConfig.toolSpacing+smuffConfig.firstToolOffset) * smuffConfig.stepsPerMM_X;
      smuffConfig.maxSpeed_X =          root[selector]["MaxSpeed"];
      smuffConfig.acceleration_X =      root[selector]["Acceleration"];
      smuffConfig.invertDir_X =         root[selector]["InvertDir"];
      smuffConfig.endstopTrigger_X =    root[selector]["EndstopTrigger"];
      smuffConfig.stepsPerRevolution_Y= root[revolver]["StepsPerRevolution"];
      smuffConfig.firstRevolverOffset = root[revolver]["Offset"];
      smuffConfig.revolverSpacing =     smuffConfig.stepsPerRevolution_Y / 10;
      smuffConfig.maxSpeed_Y =          root[revolver]["MaxSpeed"];
      smuffConfig.acceleration_Y =      root[revolver]["Acceleration"];
      smuffConfig.resetBeforeFeed_Y =   root[revolver]["ResetBeforeFeed"];
      smuffConfig.homeAfterFeed =       root[revolver]["HomeAfterFeed"];
      smuffConfig.invertDir_Y =         root[revolver]["InvertDir"];
      smuffConfig.endstopTrigger_Y =    root[revolver]["EndstopTrigger"];
      smuffConfig.externalControl_Z =   root[feeder]["ExternalControl"];
      smuffConfig.stepsPerMM_Z =        root[feeder]["StepsPerMillimeter"];
      smuffConfig.acceleration_Z =      root[feeder]["Acceleration"];
      smuffConfig.maxSpeed_Z =          root[feeder]["MaxSpeed"];
      smuffConfig.insertSpeed_Z =       root[feeder]["InsertSpeed"];
      smuffConfig.invertDir_Z =         root[feeder]["InvertDir"];
      smuffConfig.endstopTrigger_Z =    root[feeder]["EndstopTrigger"];
      smuffConfig.reinforceLength =     root[feeder]["ReinforceLength"];
      smuffConfig.unloadRetract =       root[feeder]["UnloadRetract"];
      smuffConfig.unloadPushback =      root[feeder]["UnloadPushback"];
      smuffConfig.pushbackDelay =       root[feeder]["PushbackDelay"];
      int contrast =                    root["LCDContrast"];
      smuffConfig.lcdContrast = (contrast > MIN_CONTRAST && contrast < MAX_CONTRAST) ? contrast : DSP_CONTRAST;
      smuffConfig.bowdenLength =        root["BowdenLength"];
      int i2cAdr =                      root["I2CAddress"];
      smuffConfig.i2cAddress = (i2cAdr > 0 && i2cAdr < 255) ? i2cAdr : I2C_SLAVE_ADDRESS;
      smuffConfig.menuAutoClose =       root["MenuAutoClose"];
      smuffConfig.delayBetweenPulses =  root["DelayBetweenPulses"];
      smuffConfig.serial1Baudrate =     root["Serial1Baudrate"];
      smuffConfig.serial2Baudrate =     root["Serial2Baudrate"];
      smuffConfig.fanSpeed =            root["FanSpeed"];
      smuffConfig.powerSaveTimeout =    root["PowerSaveTimeout"];

      for(int i=0; i < smuffConfig.toolCount; i++) {
        char tmp[10];
        sprintf(tmp,"Tool%d", i);
        memset(smuffConfig.materials[i], 0, sizeof(smuffConfig.materials[i]));
        strlcpy(smuffConfig.materials[i], root["Materials"][tmp], sizeof(smuffConfig.materials[i])); 
      }
      //__debug("DONE reading config");
    }
    cfg.close();
  }
}

#ifndef __TALLYBOXOUTPUT_HPP__
#define __TALLYBOXOUTPUT_HPP__
#include "TallyBoxConfiguration.hpp"
#include "Arduino.h"

#define MAX_BRIGHTNESS                1023
#define DEFAULT_RED_BRIGHTNESS_PCT    20
#define DEFAULT_GREEN_BRIGHTNESS_PCT  80

typedef enum
{
  OUTPUT_NONE,    /*used by terminal interface*/
  OUTPUT_GREEN,
  OUTPUT_RED,
  OUTPUT_LINKED   /*used for visualizing the brightnetss setting mode*/
} tallyBoxOutput_t;


void getOutputTxData(tallyBoxConfig_t& c, uint8_t& bsmEnabled, uint16_t& bsmCounter, uint16_t& bsmChannel, uint16_t& greenBrightness, uint16_t& redBrightness);
void putOutputRxData(tallyBoxConfig_t& c, uint8_t& bsmEnabled, uint16_t& bsmCounter, uint16_t& bsmChannel, uint16_t& greenBrightness, uint16_t& redBrightness);

void setBrightnessSettingMode(tallyBoxOutput_t ch, bool enable);
bool getBrightnessSettingMode(tallyBoxOutput_t& ch);

void outputUpdate(tallyBoxConfig_t& c, uint16_t currentTick, bool dataIsValid, bool tallyPreview, bool tallyProgram, bool inTransition);
void outputUpdate(tallyBoxConfig_t& c, uint16_t currentTick, bool dataIsValid, bool inTransition);

uint16_t convertBrightnessValueToRaw(float percent);
float convertBrightnessValueToPercent(uint16_t raw);


#endif
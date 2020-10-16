#ifndef __TALLYBOXOUTPUT_HPP__
#define __TALLYBOXOUTPUT_HPP__
#include "Arduino.h"

typedef enum
{
  OUTPUT_NONE,    /*used by terminal interface*/
  OUTPUT_GREEN,
  OUTPUT_RED,
  OUTPUT_LINKED   /*used for visualizing the brightnetss setting mode*/
} tallyBoxOutput_t;


void getOutputTxData(uint8_t& bsmEnabled, uint16_t& bsmCounter, uint16_t& bsmChannel, uint16_t& greenBrightness, uint16_t& redBrightness);
void putOutputRxData(uint8_t& bsmEnabled, uint16_t& bsmCounter, uint16_t& bsmChannel, uint16_t& greenBrightness, uint16_t& redBrightness);

void setBrightnessSettingMode(tallyBoxOutput_t ch, bool enable);
void setOutputBrightness(uint16_t percent, tallyBoxOutput_t ch);
uint16_t getOutputBrightness(tallyBoxOutput_t ch);
void outputUpdate(uint16_t currentTick, bool dataIsValid, bool tallyPreview, bool tallyProgram);
void outputUpdate(uint16_t currentTick, bool dataIsValid);

#endif
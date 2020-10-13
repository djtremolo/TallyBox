#ifndef __TALLYBOXOUTPUT_HPP__
#define __TALLYBOXOUTPUT_HPP__
#include "Arduino.h"

void setOutputBrightness(uint16_t percent);
void outputUpdate(bool dataIsValid, bool tallyPreview, bool tallyProgram);
void outputUpdate(bool dataIsValid);

#endif
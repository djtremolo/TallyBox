#ifndef __TALLYBOXINFRA_HPP__
#define __TALLYBOXINFRA_HPP__
#include "Arduino.h"

uint16_t getCurrentTick(bool nonCompensated=false);
int32_t getTickCompensationValue();
void setTickCompensationValue(int32_t comp);

#endif
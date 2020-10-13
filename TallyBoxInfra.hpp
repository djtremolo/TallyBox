#ifndef __TALLYBOXINFRA_HPP__
#define __TALLYBOXINFRA_HPP__
#include "Arduino.h"

uint16_t getCurrentTick(bool nonCompensated=false);
void setTickCompensationValue(int32_t comp);

#endif
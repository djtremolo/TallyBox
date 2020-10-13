#include "TallyBoxInfra.hpp"
#include "Arduino.h"
#include "TallyBoxPeerNetwork.hpp"

#define TIME_TICK_PRESCALER           10
#define TIME_SPLITS                   32    /*must be 32 because of the 32-bit led sequence values*/
#define TIME_FULL_ROUND               (TIME_TICK_PRESCALER*TIME_SPLITS)

int32_t myTickCompensationValue = 0;

void setTickCompensationValue(int32_t comp)
{
  myTickCompensationValue = comp;
}

uint16_t getCurrentTick(bool nonCompensated)
{
  int64_t myMillis = millis();
  int32_t compensationInMs = (nonCompensated ? 0 : (myTickCompensationValue*TIME_TICK_PRESCALER));
  int64_t myCompensatedMillis = (myMillis > TIME_FULL_ROUND ? (myMillis+compensationInMs) : myMillis);
  uint16_t currentTick = (myCompensatedMillis/TIME_TICK_PRESCALER) % TIME_FULL_ROUND;

  return currentTick;
}




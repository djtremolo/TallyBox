#ifndef __TALLYBOXSTATEMACHINE_HPP__
#define __TALLYBOXSTATEMACHINE_HPP__
#include <arduino.h>
#include "TallyBoxConfiguration.hpp"

typedef enum
{
  CONNECTING_TO_WIFI = 0,
  CONNECTING_TO_ATEM_HOST,
  CONNECTING_TO_PEERNETWORK_HOST,
  RUNNING_ATEM,
  RUNNING_PEERNETWORK,
  ERROR,
  /**************/
  STATE_MAX
} tallyBoxState_t;

void tallyBoxStateMachineInitialize(tallyBoxConfig_t& c);
void tallyBoxStateMachineUpdate(tallyBoxConfig_t& c, tallyBoxState_t switchToState = STATE_MAX);
bool tallyDataIsValid();

#endif

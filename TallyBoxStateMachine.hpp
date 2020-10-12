#ifndef __TALLYBOXSTATEMACHINE_HPP__
#define __TALLYBOXSTATEMACHINE_HPP__
#include <arduino.h>
#include "TallyBoxConfiguration.hpp"

typedef enum
{
  CONNECTING_TO_WIFI = 0,
  CONNECTING_TO_ATEM_HOST,
  CONNECTING_TO_TALLYBOX_HOST,
  RUNNING_ATEM,
  RUNNING_TALLYBOX,
  ERROR,
  /**************/
  STATE_MAX
} tallyBoxState_t;

typedef enum
{
  PREVIEW_ON,
  PREVIEW_OFF,
  PROGRAM_ON,
  PROGRAM_OFF
} tallyBoxCameraEvent_t;

void tallyBoxStateMachineFeedEvent(tallyBoxCameraEvent_t e);

void tallyBoxStateMachineUpdate(tallyBoxConfig_t& c, tallyBoxState_t switchToState = STATE_MAX);

#endif

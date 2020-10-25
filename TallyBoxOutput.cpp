#include "TallyBoxOutput.hpp"
#include "Arduino.h"

#define PIN_GREEN               D7
#define PIN_RED                 D8

static bool myGreenState = false;
static bool myRedState = false;
static bool brightnessSettingModeEnabled = false;
static uint16_t brightnessSettingModeCounter = 0;
static tallyBoxOutput_t brightnessSettingModeChannel;

static uint16_t myGreenBrightnessValue = DEFAULT_BRIGHTNESS;
static uint16_t myRedBrightnessValue = DEFAULT_BRIGHTNESS;

void setOutputState(tallyBoxOutput_t ch, bool outputState);
bool getOutputState(tallyBoxOutput_t ch);
void setOutputBrightness(uint16_t percent);

void getOutputTxData(uint8_t& bsmEnabled, uint16_t& bsmCounter, uint16_t& bsmChannel, uint16_t& greenBrightness, uint16_t& redBrightness)
{
  /*PeerNetwork master: sending out data to slaves*/
  bsmEnabled = (uint8_t)brightnessSettingModeEnabled;
  bsmCounter = brightnessSettingModeCounter;  /*10 us ticks -> 1000 = 10 seconds*/
  bsmChannel = (uint16_t)brightnessSettingModeChannel;
  greenBrightness = myGreenBrightnessValue;
  redBrightness = myRedBrightnessValue;
}

void putOutputRxData(uint8_t& bsmEnabled, uint16_t& bsmCounter, uint16_t& bsmChannel, uint16_t& greenBrightness, uint16_t& redBrightness)
{
  /*PeerNetwork slave: receiving data from master*/
  brightnessSettingModeEnabled = (bool)bsmEnabled;
  brightnessSettingModeCounter = bsmCounter;  /*10 us ticks -> 1000 = 10 seconds*/
  brightnessSettingModeChannel = (tallyBoxOutput_t)bsmChannel;
  myGreenBrightnessValue = greenBrightness;
  myRedBrightnessValue = redBrightness;
}

void setBrightnessSettingMode(tallyBoxOutput_t ch, bool enable)
{
  if(enable)
  {
    brightnessSettingModeEnabled = true;
    brightnessSettingModeCounter = 1000;  /*10 us ticks -> 1000 = 10 seconds*/
    brightnessSettingModeChannel = ch;
  }
  else
  {
    brightnessSettingModeEnabled = false;
    brightnessSettingModeCounter = 0;
    brightnessSettingModeChannel = OUTPUT_NONE;
  }
}

bool getBrightnessSettingMode(tallyBoxOutput_t& ch)
{
  bool ret = false;
  ret = brightnessSettingModeEnabled;
  ch = brightnessSettingModeChannel;
  
  return ret;
}


void setOutputState(tallyBoxOutput_t ch, bool outputState)
{
  switch(ch)
  {
    case OUTPUT_GREEN: 
      myGreenState = outputState;
      break;
    case OUTPUT_RED: 
      myRedState = outputState;
      break;
    default:
      break;
  }
}

bool getOutputState(tallyBoxOutput_t ch)
{
  return ((ch==OUTPUT_GREEN) ? myGreenState : myRedState);
}

void setOutputBrightness(uint16_t percent)
{
  setOutputBrightness(percent, OUTPUT_GREEN);
  setOutputBrightness(percent, OUTPUT_RED);
}

void setOutputBrightness(uint16_t percent, tallyBoxOutput_t ch)
{
  uint16_t aoValue = map(percent, 0, 100, 0, MAX_BRIGHTNESS);

  switch(ch)
  {
    case OUTPUT_GREEN: 
      myGreenBrightnessValue = aoValue;
      break;
    case OUTPUT_RED: 
      myRedBrightnessValue = aoValue;
      break;
    default:
      break;
  }
}

uint16_t getOutputBrightness(tallyBoxOutput_t ch)
{
  uint16_t raw = (ch==OUTPUT_GREEN ? myGreenBrightnessValue : myRedBrightnessValue);
  uint16_t percent = map(raw, 0, MAX_BRIGHTNESS, 0, 100);

  return percent;
}

void outputUpdate(uint16_t currentTick, bool dataIsValid, bool tallyPreview, bool tallyProgram, bool inTransition)
{
  if(dataIsValid)
  {
    setOutputState(OUTPUT_GREEN, tallyPreview);
    setOutputState(OUTPUT_RED, tallyProgram);
  }
  
  outputUpdate(currentTick, dataIsValid, inTransition);
}

static void getWarningLevels(uint16_t currentTick, int32_t& greenLevel, int32_t& redLevel)
{
  static uint16_t prevTick = 0;
  const int32_t wcMaxRed = 480;  /*0...1023*/
  const int32_t wcMinRed = 0;  /*0...1023*/
  const int32_t wcMaxGrn = 1023;  /*0...1023*/
  const int32_t wcMinGrn = 0;  /*0...1023*/

  if(currentTick<160)
  {
    /*the first half*/
    /*green: increasing tick causes raising the output value
      red:   inverted */
    greenLevel = map(currentTick, 0, 159, wcMinGrn, wcMaxGrn);
    redLevel = map(currentTick, 0, 159, wcMaxRed, wcMinRed);
  }
  else
  {
    /*red:   increasing tick causes raising the output value
      green: inverted */
    greenLevel = map(currentTick, 160, 319, wcMaxGrn, wcMinGrn);
    redLevel = map(currentTick, 160, 319, wcMinRed, wcMaxRed);
  }
}

bool handleBrightnessSettingMode()
{
  bool skipRealOutput = false;
  
  if(brightnessSettingModeEnabled)
  {
    if(brightnessSettingModeCounter > 0)
    {
      uint16_t testG = 0;
      uint16_t testR = 0;

      brightnessSettingModeCounter--;

      /*mask the channel that is not being set right now*/
      switch(brightnessSettingModeChannel)
      {
        case OUTPUT_NONE:
          break;
        case OUTPUT_GREEN:
          testG = myGreenBrightnessValue;
          break;
        case OUTPUT_RED:
          testR = myRedBrightnessValue;
          break;
        case OUTPUT_LINKED:
          testG = myGreenBrightnessValue;
          testR = myRedBrightnessValue;
          break;
      }

      if(brightnessSettingModeChannel == OUTPUT_GREEN) testR = 0;
      if(brightnessSettingModeChannel == OUTPUT_RED) testG = 0;
      
      analogWrite(PIN_GREEN, testG);
      analogWrite(PIN_RED, testR);
      
      skipRealOutput = true;  /*force calling function to return after this*/
    }
    else
    {
      /*timeout exceeded, disable setting mode*/
      setBrightnessSettingMode(OUTPUT_NONE, false);
    }
  } 
  return skipRealOutput;
}

void outputUpdate(uint16_t currentTick, bool dataIsValid, bool inTransition)
{
  if(handleBrightnessSettingMode())
  {
    /*we are in the mode that visualizes the brightness setting*/
    return;
  }
 
  /*control the light output for the user*/
  if(dataIsValid)
  {
    if(inTransition)
    {
      if(myGreenState || myRedState)
      {
        /*while in transition - either on program/preview - we are actually in program, so let's show RED*/
        analogWrite(PIN_GREEN, 0);
        analogWrite(PIN_RED, myRedBrightnessValue);
      }
      else
      {
        /*otherwise, show nothing*/
        analogWrite(PIN_GREEN, 0);
        analogWrite(PIN_RED, 0);
      }
    }
    else
    {
      /*NORMAL case:*/
      analogWrite(PIN_GREEN, (myGreenState ? myGreenBrightnessValue : 0));
      analogWrite(PIN_RED, (myRedState ? myRedBrightnessValue : 0));
    }
  }
  else
  {
    /*WARNING case: smoothly wave between green and red to indicate disconnection*/
    int32_t wGreen, wRed;
    getWarningLevels(currentTick, wGreen, wRed);
    analogWrite(PIN_GREEN, wGreen);
    analogWrite(PIN_RED, wRed);
  }
}

#include "TallyBoxOutput.hpp"
#include "Arduino.h"

#define MAX_BRIGHTNESS          1023

#define PIN_GREEN               D7
#define PIN_RED                 D8

#define DEFAULT_BRIGHTNESS      ((1 * MAX_BRIGHTNESS) / 2)

typedef enum
{
    OUTPUT_GREEN = 0x00000001,
    OUTPUT_RED = 0x00000002
} tallyBoxOutput_t;


static bool myGreenState = false;
static bool myRedState = false;

static uint16_t myGreenBrightnessValue = DEFAULT_BRIGHTNESS;
static uint16_t myRedBrightnessValue = DEFAULT_BRIGHTNESS;

void setOutputState(tallyBoxOutput_t ch, bool outputState);
bool getOutputState(tallyBoxOutput_t ch);

void setOutputBrightness(uint16_t percent, tallyBoxOutput_t ch);
uint16_t getOutputBrightness(tallyBoxOutput_t ch);


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
}

void outputUpdate(uint16_t currentTick, bool dataIsValid, bool tallyPreview, bool tallyProgram)
{
  if(dataIsValid)
  {
    setOutputState(OUTPUT_GREEN, tallyPreview);
    setOutputState(OUTPUT_RED, tallyProgram);
  }
  
  outputUpdate(currentTick, dataIsValid);
}

static void getWarningLevels(uint16_t currentTick, int32_t& greenLevel, int32_t& redLevel)
{
  static uint16_t prevTick = 0;
  static bool goingUp = true;
  const int32_t wcMax = 480;  /*0...1023*/
  const int32_t wcMin = 0;  /*0...1023*/

  /*keep warning sequence running in case it will be needed due to disconnection*/
  /*check if this is a new round: currentTick goes from 0...319 in 10ms steps -> full round = 3.2 seconds*/
  if(currentTick < prevTick)
  {
    /*reverse direction*/
    goingUp = !goingUp;
  }
  prevTick = currentTick;

  /*update warningCounter value*/
  if(goingUp)
  {
    /*green: increasing tick causes raising the output value
      red:   inverted */
    greenLevel = map(currentTick, 0, 319, wcMin, wcMax);
    redLevel = map(currentTick, 0, 319, wcMax, wcMin);
  }
  else
  {
    /*green: increasing tick causes lowering the output value
      red:   inverted */
    greenLevel = map(currentTick, 0, 319, wcMax, wcMin);
    redLevel = map(currentTick, 0, 319, wcMin, wcMax);
  }
}

void outputUpdate(uint16_t currentTick, bool dataIsValid)
{
  /*control the light output for the user*/
  if(dataIsValid)
  {
    analogWrite(PIN_GREEN, (myGreenState ? myGreenBrightnessValue : 0));
    analogWrite(PIN_RED, (myRedState ? myRedBrightnessValue : 0));
  }
  else
  {
    int32_t wGreen, wRed;
    getWarningLevels(currentTick, wGreen, wRed);
    analogWrite(PIN_GREEN, wGreen);
    analogWrite(PIN_RED, wRed);
  }
}

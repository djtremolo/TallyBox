#include "TallyBoxOutput.hpp"
#include "Arduino.h"

#define MAX_BRIGHTNESS          1023

#define PIN_GREEN               D7
#define PIN_RED                 D8

typedef enum
{
    OUTPUT_GREEN = 0x00000001,
    OUTPUT_RED = 0x00000002
} tallyBoxOutput_t;


static bool myGreenState = false;
static bool myRedState = false;

static uint16_t myGreenBrightnessValue = (MAX_BRIGHTNESS/2);
static uint16_t myRedBrightnessValue = (MAX_BRIGHTNESS/2);

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

void outputUpdate(bool tallyPreview, bool tallyProgram)
{
  setOutputState(OUTPUT_GREEN, tallyPreview);
  setOutputState(OUTPUT_RED, tallyProgram);
  outputUpdate();
}

void outputUpdate()
{
  analogWrite(PIN_GREEN, (myGreenState ? myGreenBrightnessValue : 0));
  analogWrite(PIN_RED, (myRedState ? myRedBrightnessValue : 0));
}

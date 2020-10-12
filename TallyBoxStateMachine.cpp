#include "TallyBoxStateMachine.hpp"
#include "Arduino.h"
#include <ATEMbase.h>
#include <ATEMstd.h>
#include <SkaarhojPgmspace.h>

ATEMstd AtemSwitcher;

static bool isMaster = true;
static bool tallyPreview = false;
static bool tallyProgram = false;


const uint32_t ledSequence[STATE_MAX] = 
{
  0x00000001,   /*CONNECTING_TO_WIFI*/
  0x00000005,   /*CONNECTING_TO_ATEM_HOST*/
  0x00000015,   /*CONNECTING_TO_TALLYBOX_HOST*/

  0x01010101,   /*RUNNING_ATEM*/

//  0x00000000,   /*RUNNING_ATEM*/
  0x00000000,   /*RUNNING_TALLYBOX*/
  0x0000000F    /*ERROR*/
};

tallyBoxState_t myState = CONNECTING_TO_WIFI; /*start from here*/

uint32_t getLedSequenceForRunState()
{
  uint32_t outMask = 0;
  if(tallyPreview)
    outMask |= 0x55555555;
  if(tallyProgram)
    outMask |= 0xFFFFFFFF;

  return outMask;
}

void updateLed(uint16_t tick)
{
  bool isRunning = (myState==RUNNING_ATEM || myState==RUNNING_TALLYBOX);
  uint32_t seq = (isRunning ? getLedSequenceForRunState() : ledSequence[myState]);
  bool state = (seq >> tick) & 0x00000001;
  int ledState = (state ? LOW : HIGH);

  digitalWrite(LED_BUILTIN, ledState);
}


void tallyBoxStateMachineFeedEvent(tallyBoxCameraEvent_t e)
{

}



void tallyBoxStateMachineUpdate(tallyBoxConfig_t& c, tallyBoxState_t switchToState)
{
  uint16_t currentTick = (millis() / 100) % 32; /*tick will count 0...31,0...31 etc*/
  static uint8_t internalState[STATE_MAX] = {};
  static uint16_t prevTick = 0;

  static bool prevTallyPreview = false;
  static bool prevTallyProgram = false;



  if(currentTick == prevTick)
  {
    /*run only once per tick*/
    return; 
  }

  prevTick = currentTick;

  /*external transition required?*/
  if(switchToState != STATE_MAX)
  {
    myState = switchToState;
    internalState[myState] = 0;
  }

  uint8_t is = internalState[myState];




  updateLed(currentTick);

  switch(myState)
  {
    case CONNECTING_TO_WIFI:
      switch(internalState[CONNECTING_TO_WIFI])
      {
        case 0: /*init wifi device*/
          WiFi.begin(c.wifiSSID, c.wifiPasswd);
          internalState[CONNECTING_TO_WIFI] = 1;
          break;

        case 1: /*wait*/
          if(WiFi.status() != WL_CONNECTED)
          {
            Serial.println("Connecting to WiFi..."); 
          }
          else
          {
            internalState[CONNECTING_TO_WIFI] = 2;
          }
          break;

        case 2: /*advance to next*/
          Serial.print("Wifi connected: ");
          Serial.println(WiFi.localIP());
          internalState[CONNECTING_TO_WIFI] = 0;
          myState = (isMaster ? CONNECTING_TO_ATEM_HOST : CONNECTING_TO_TALLYBOX_HOST);
          break;

        default:
          Serial.println("*** Error: illegal internalstate (CONNECTING_TO_WIFI) ***");
          internalState[CONNECTING_TO_WIFI] = 0;
          break;
      }      
      break;

    case CONNECTING_TO_ATEM_HOST:
      switch(internalState[CONNECTING_TO_ATEM_HOST])
      {
        case 0:
          AtemSwitcher.begin(IPAddress(c.hostAddressU32));
          AtemSwitcher.serialOutput(0x80);
          AtemSwitcher.connect();
          internalState[CONNECTING_TO_ATEM_HOST] = 1;
          break;

        case 1:
          AtemSwitcher.runLoop();
          if(!AtemSwitcher.isConnected())
          {
            Serial.println("Connecting to ATEM..."); 
          }
          else
          {
            internalState[CONNECTING_TO_ATEM_HOST] = 2;
          }          
          break;
        
        case 2: /*advance to next*/
          Serial.println("Connected to ATEM host!");
          internalState[CONNECTING_TO_ATEM_HOST] = 0;
          myState = RUNNING_ATEM;
          break;

        default:
          Serial.println("*** Error: illegal internalstate (CONNECTING_TO_ATEM_HOST) ***");
          internalState[CONNECTING_TO_WIFI] = 0;
          break;
      }
      break;

    case CONNECTING_TO_TALLYBOX_HOST:
      myState = RUNNING_TALLYBOX;
      break;

    case RUNNING_ATEM:
      AtemSwitcher.runLoop();

      tallyPreview = AtemSwitcher.getPreviewTally(1); //c.cameraId);
      tallyProgram = AtemSwitcher.getProgramTally(1); //c.cameraId);


      if(tallyPreview != prevTallyPreview)
      {
        Serial.println("PREVIEW!!!");
        prevTallyPreview = tallyPreview;
      }

      if(tallyProgram != prevTallyProgram)
      {
        Serial.println("PROGRAM!!!");
        prevTallyProgram = tallyProgram;
      }



      break;

    case RUNNING_TALLYBOX:
      break;

    case ERROR:
      Serial.println("*** ERROR ***");
      break;

    default:
      Serial.println("*** INVALID STATE ***");
      break;
  }
}

//void tallyBoxStateMachine


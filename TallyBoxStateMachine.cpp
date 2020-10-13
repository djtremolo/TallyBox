#include "TallyBoxStateMachine.hpp"
#include "Arduino.h"
#include <ATEMbase.h>
#include <ATEMstd.h>
#include <SkaarhojPgmspace.h>
#include "OTAUpgrade.hpp"
#include "TallyBoxOutput.hpp"
#include "TallyBoxPeerNetwork.hpp"


#define SEQUENCE_SINGLE_SHORT       0x00000001
#define SEQUENCE_DOUBLE_SHORT       0x00000005
#define SEQUENCE_TRIPLE_SHORT       0x00000015
#define SEQUENCE_SINGLE_LONG        0x000000FF
#define SEQUENCE_BLINKING_LONG      0x00FF00FF
#define SEQUENCE_BLINKING_SHORT     0x55555555
#define SEQUENCE_CONSTANT_OFF       0x00000000
#define SEQUENCE_CONSTANT_ON        0xFFFFFFFF


ATEMstd AtemSwitcher;
static bool isMaster = false;
static bool tallyPreview = false;
static bool tallyProgram = false;
static bool masterCommunicationFrozen = false;
static tallyBoxState_t myState = CONNECTING_TO_WIFI; /*start from here*/
static uint32_t lastReceivedMasterMessageInTicks = 0;
static uint32_t cumulativeTickCounter = 0;


const uint32_t ledSequence[STATE_MAX] = 
{
  SEQUENCE_SINGLE_SHORT,   /*CONNECTING_TO_WIFI*/
  SEQUENCE_DOUBLE_SHORT,   /*CONNECTING_TO_ATEM_HOST*/
  SEQUENCE_TRIPLE_SHORT,   /*CONNECTING_TO_PEERNETWORK_HOST*/
  SEQUENCE_CONSTANT_OFF,   /*RUNNING_ATEM*/
  SEQUENCE_CONSTANT_OFF,   /*RUNNING_PEERNETWORK*/
  SEQUENCE_SINGLE_LONG    /*ERROR*/
};



/*** INTERNAL FUNCTIONS **************************************/
static uint32_t getLedSequenceForRunState();
static void updateLed(uint16_t tick);
static void setTallySignals(tallyBoxConfig_t& c, uint16_t greenChannel, uint16_t redChannel);
static void stateConnectingToWifi(tallyBoxConfig_t& c, uint8_t *internalState);
static void stateConnectingToAtemHost(tallyBoxConfig_t& c, uint8_t *internalState);
static void stateConnectingToPeerNetworkHost(tallyBoxConfig_t& c, uint8_t *internalState);
static void stateRunningAtem(tallyBoxConfig_t& c, uint8_t *internalState);
static void stateRunningPeerNetwork(tallyBoxConfig_t& c, uint8_t *internalState);
/*************************************************************/


static uint32_t getLedSequenceForRunState()
{
  uint32_t outMask = 0;

  if(masterCommunicationFrozen)
  {
    outMask = SEQUENCE_BLINKING_LONG;
  }
  else
  {
    if(tallyPreview)
      outMask |= SEQUENCE_BLINKING_SHORT;
    if(tallyProgram)
      outMask |= SEQUENCE_CONSTANT_ON;
  }

  return outMask;
}

static void updateLed(uint16_t tick)
{
  bool isRunning = (myState==RUNNING_ATEM || myState==RUNNING_PEERNETWORK);
  uint32_t seq = (isRunning ? getLedSequenceForRunState() : ledSequence[myState]);
  bool state = (seq >> tick) & 0x00000001;
  int ledState = (state ? LOW : HIGH);

  digitalWrite(LED_BUILTIN, ledState);
}


static void stateConnectingToWifi(tallyBoxConfig_t& c, uint8_t *internalState)
{
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

      if(isMaster)
      {
        myState = CONNECTING_TO_ATEM_HOST;
      }
      else
      {
        /*start listening for peerNetwork updates from master box*/
        peerNetworkInitialize(7493);
        myState = CONNECTING_TO_PEERNETWORK_HOST;
      }  

      /*initialize Over-the-Air upgdade mechanism*/
      WiFi.mode(WIFI_STA);
      OTAInitialize();

      break;

    default:
      Serial.println("*** Error: illegal internalstate (CONNECTING_TO_WIFI) ***");
      internalState[CONNECTING_TO_WIFI] = 0;
      break;
  }      
}

static void stateConnectingToAtemHost(tallyBoxConfig_t& c, uint8_t *internalState)
{
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
}

static void stateConnectingToPeerNetworkHost(tallyBoxConfig_t& c, uint8_t *internalState)
{
  myState = RUNNING_PEERNETWORK;
}

static void setTallySignals(tallyBoxConfig_t& c, uint16_t greenChannel, uint16_t redChannel)
{
  tallyPreview = (greenChannel == c.cameraId);
  tallyProgram = (redChannel == c.cameraId);
}

static void stateRunningAtem(tallyBoxConfig_t& c, uint8_t *internalState)
{
  static bool prevCommFrozen = false;
  AtemSwitcher.runLoop();

  if(AtemSwitcher.isConnected())
  {
    masterCommunicationFrozen = false;
    lastReceivedMasterMessageInTicks = cumulativeTickCounter;
  }

  if(cumulativeTickCounter - lastReceivedMasterMessageInTicks > 10)
  {
    masterCommunicationFrozen = true;

    /*jump to reconnection*/
    myState = CONNECTING_TO_ATEM_HOST;
    internalState[CONNECTING_TO_ATEM_HOST] = 0;
  }

  if(!masterCommunicationFrozen)
  {
    uint16_t greenChannel = AtemSwitcher.getPreviewInput();
    uint16_t redChannel = AtemSwitcher.getProgramInput();

    setTallySignals(c, greenChannel, redChannel);
    peerNetworkSend(c, greenChannel, redChannel);
  }

  /*report state changes*/
  if(masterCommunicationFrozen != prevCommFrozen)
  {
    if(masterCommunicationFrozen)
    {
      Serial.println("No connection to ATEM, trying to reconnect...");
    }
    else
    {
      Serial.println("Connection with ATEM established!");
    }    

    prevCommFrozen = masterCommunicationFrozen;
  }
}

static void stateRunningPeerNetwork(tallyBoxConfig_t& c, uint8_t *internalState)
{
  static bool prevCommFrozen = false;
  uint16_t greenChannel, redChannel;

  if(peerNetworkReceive(greenChannel, redChannel))
  {
    setTallySignals(c, greenChannel, redChannel);
    lastReceivedMasterMessageInTicks = cumulativeTickCounter;
    masterCommunicationFrozen = false;
  }

  if(cumulativeTickCounter - lastReceivedMasterMessageInTicks > 10)
  {
    masterCommunicationFrozen = true;
  }

  /*report state changes*/
  if(masterCommunicationFrozen != prevCommFrozen)
  {
    if(masterCommunicationFrozen)
    {
      Serial.println("No message received from master, waiting...");
    }
    else
    {
      Serial.println("Message received from master!");      
    }    

    prevCommFrozen = masterCommunicationFrozen;
  }
}

void tallyBoxStateMachineInitialize(tallyBoxConfig_t& c)
{
  randomSeed(analogRead(5));  /*random needed by ATEM library*/

  isMaster = (c.cameraId == 1); /*ID 1 behaves as master: connects to ATEM and forwards the information via PeerNetwork*/
}


void tallyBoxStateMachineUpdate(tallyBoxConfig_t& c, tallyBoxState_t switchToState)
{
  uint16_t currentTick = (millis() / 10) % 320; /*tick will count 0...31,0...31 etc*/
  static uint8_t internalState[STATE_MAX] = {};
  static uint16_t prevTick = 0;

  /*call over-the-air update mechanism from here to provide faster speed*/
  OTAUpdate();

  /*update main output: Red&Green tally lights*/
  outputUpdate(tallyPreview, tallyProgram);

  /*only run state machine once per tick*/
  if(currentTick == prevTick)
  {
    return; 
  }
  prevTick = currentTick;

  /*cumulative tick counter (10ms ticks) for timeout handling*/
  cumulativeTickCounter++;

  /*external transition required?*/
  if(switchToState != STATE_MAX)
  {
    myState = switchToState;
    internalState[myState] = 0;
  }

  /*update diagnostic led to indicate running state*/
  updateLed(currentTick/10);

  /*process functionality*/
  switch(myState)
  {
    case CONNECTING_TO_WIFI:
      stateConnectingToWifi(c, internalState);
      break;

    case CONNECTING_TO_ATEM_HOST:
      stateConnectingToAtemHost(c, internalState);
      break;

    case CONNECTING_TO_PEERNETWORK_HOST:
      stateConnectingToPeerNetworkHost(c, internalState);
      break;

    case RUNNING_ATEM:
      stateRunningAtem(c, internalState);
      break;

    case RUNNING_PEERNETWORK:
      stateRunningPeerNetwork(c, internalState);
      break;

    case ERROR:
      Serial.println("*** ERROR ***");
      break;

    default:
      Serial.println("*** INVALID STATE ***");
      break;
  }
}


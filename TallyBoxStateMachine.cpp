#include "TallyBoxStateMachine.hpp"
#include "Arduino.h"
#include <ATEMbase.h>
#include <ATEMstd.h>
#include <SkaarhojPgmspace.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>

#include "OTAUpgrade.hpp"
#include "TallyBoxOutput.hpp"
#include "TallyBoxPeerNetwork.hpp"
#include "TallyBoxInfra.hpp"
#include "TallyBoxTerminal.hpp"
#include "TallyBoxWebServer.hpp"


#define SEQUENCE_SINGLE_SHORT       0x00000001
#define SEQUENCE_DOUBLE_SHORT       0x00000005
#define SEQUENCE_TRIPLE_SHORT       0x00000015
#define SEQUENCE_SINGLE_LONG        0x000000FF
#define SEQUENCE_BLINKING_LONG      0x00FF00FF
#define SEQUENCE_BLINKING_SHORT     0x55555555
#define SEQUENCE_CONSTANT_OFF       0x00000000
#define SEQUENCE_CONSTANT_ON        0xFFFFFFFF


ATEMstd AtemSwitcher;
static bool tallyPreview = false;
static bool tallyProgram = false;
static bool tallyInTransition = false;
static bool masterCommunicationFrozen = false;
static tallyBoxState_t myState = CONNECTING_TO_WIFI; /*start from here*/
static uint32_t lastReceivedMasterMessageInTicks = 0;
static uint32_t cumulativeTickCounter = 0;
static bool mDnsInitialized = false;

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
static void MDnsInitialize(tallyBoxConfig_t& c);
static void MDnsUpdate();
static void setTallySignals(tallyBoxNetworkConfig_t& c, uint16_t greenChannel, uint16_t redChannel);
static void stateConnectingToWifi(tallyBoxNetworkConfig_t& c, uint8_t *internalState);
static void stateConnectingToAtemHost(tallyBoxNetworkConfig_t& c, uint8_t *internalState);
static void stateConnectingToPeerNetworkHost(tallyBoxNetworkConfig_t& c, uint8_t *internalState);
static void stateRunningAtem(tallyBoxNetworkConfig_t& c, uint8_t *internalState);
static void stateRunningPeerNetwork(tallyBoxNetworkConfig_t& c, uint8_t *internalState);
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

static void updateLed(uint16_t currentTick)
{
  uint16_t tick = currentTick / 10;
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
      Serial.print("Connecting to WiFi"); 
      WiFi.mode(WIFI_STA);
      WiFi.begin(c.network.wifiSSID, c.network.wifiPasswd);
      internalState[CONNECTING_TO_WIFI] = 1;
      break;

    case 1: /*wait*/
      if(WiFi.status() != WL_CONNECTED)
      {
        Serial.print(".");
      }
      else
      {
        internalState[CONNECTING_TO_WIFI] = 2;
      }
      break;

    case 2: /*advance to next*/
      Serial.print("\r\nWifi connected: ");
      Serial.println(WiFi.localIP());
      internalState[CONNECTING_TO_WIFI] = 0;

      if(c.user.isMaster)
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
      delay(100);
      OTAInitialize();
      MDnsInitialize(c);
      tallyBoxTerminalInitialize();
      tallyBoxWebServerInitialize(c);
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
      AtemSwitcher.begin(IPAddress(c.network.hostAddressU32));
      AtemSwitcher.serialOutput(0x80);
      AtemSwitcher.connect();
      internalState[CONNECTING_TO_ATEM_HOST] = 1;
      Serial.println("Connecting to ATEM"); 
      break;

    case 1:
      AtemSwitcher.runLoop();
      if(AtemSwitcher.isConnected())
      {
        internalState[CONNECTING_TO_ATEM_HOST] = 2;
      }          
      break;
    
    case 2: /*advance to next*/
      Serial.println("\r\nConnected to ATEM host!");
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

static void setTallySignals(tallyBoxConfig_t& c, uint16_t greenChannel, uint16_t redChannel, bool inTransition)
{
  tallyPreview = (greenChannel == c.user.cameraId);
  tallyProgram = (redChannel == c.user.cameraId);
  tallyInTransition = inTransition;
}

#define INCOMING_FAULT_TOLERANCE_IN_10MS_TICKS                200


static void stateRunningAtem(tallyBoxConfig_t& c, uint8_t *internalState)
{
  static bool prevCommFrozen = false;
  AtemSwitcher.runLoop();

  if(AtemSwitcher.isConnected())
  {
    masterCommunicationFrozen = false;
    lastReceivedMasterMessageInTicks = cumulativeTickCounter;
  }
  else
  {
    Serial.println("ATEM Disconnection!!");   
  }

  if(cumulativeTickCounter - lastReceivedMasterMessageInTicks > INCOMING_FAULT_TOLERANCE_IN_10MS_TICKS)
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
    bool inTransition = AtemSwitcher.getTransitionInTransition(0);

    setTallySignals(c, greenChannel, redChannel, inTransition);
    peerNetworkSend(c, greenChannel, redChannel, inTransition);
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
  bool inTransition;

  if(peerNetworkReceive(greenChannel, redChannel, inTransition))
  {
    setTallySignals(c, greenChannel, redChannel, inTransition);
    lastReceivedMasterMessageInTicks = cumulativeTickCounter;
    masterCommunicationFrozen = false;
  }

  if(cumulativeTickCounter - lastReceivedMasterMessageInTicks > INCOMING_FAULT_TOLERANCE_IN_10MS_TICKS)
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

bool tallyDataIsValid()
{
  bool ret = false;
  if(((myState==RUNNING_ATEM) || (myState==RUNNING_PEERNETWORK)) && (!masterCommunicationFrozen))
  {
    ret = true;
  }
  return ret;
}

#define CPU_TIME_DEBUG                  false

#if CPU_TIME_DEBUG
#define DIAG_LED_LOOP_FULL              D0
#define DIAG_LED_LOOP_OTA               D1
#define DIAG_LED_LOOP_STATEMACHINE      D3
#define DIAG_LED_LOOP_TALLY_OUTPUT      D4
#define DIAG_LED_LOOP_WEB_SERVER        D5

#define LED_ON                          HIGH
#define LED_OFF                         LOW

#define DEBUG_PULSE_START(x)            digitalWrite((x), LED_ON);
#define DEBUG_PULSE_STOP(x)             digitalWrite((x), LED_OFF);

#else
#define DEBUG_PULSE_START(x)
#define DEBUG_PULSE_STOP(x)
#endif

void tallyBoxStateMachineInitialize(tallyBoxConfig_t& c)
{
  randomSeed(analogRead(5));  /*random needed by ATEM library*/

#if CPU_TIME_DEBUG
  pinMode(DIAG_LED_LOOP_FULL, OUTPUT);
  pinMode(DIAG_LED_LOOP_STATEMACHINE, OUTPUT);
  pinMode(DIAG_LED_LOOP_OTA, OUTPUT);
  pinMode(DIAG_LED_LOOP_TALLY_OUTPUT, OUTPUT);
  pinMode(DIAG_LED_LOOP_WEB_SERVER, OUTPUT);
#endif

}

static void MDnsInitialize(tallyBoxConfig_t& c)
{
  if(!MDNS.begin(c.network.mdnsHostName)) 
  {
    Serial.println("Error setting up MDNS responder!");
  }
  else
  {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS responder started");
    mDnsInitialized = true;
  }
}


static void MDnsUpdate()
{
  if(mDnsInitialized)
  {
    MDNS.update();
  }
}


void tallyBoxStateMachineUpdate(tallyBoxConfig_t& c, tallyBoxState_t switchToState)
{
  static tallyBoxState_t prevState = STATE_MAX; /*force printing out the first state*/
  static uint8_t internalState[STATE_MAX] = {};
  static uint16_t prevTick = 0;
  uint16_t currentTick = getCurrentTick();  /*0...319,0...319...*/
  bool printStateName = false;

  DEBUG_PULSE_START(DIAG_LED_LOOP_FULL);

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

  /*check whether to print out the state name*/
  printStateName = (myState != prevState);
  prevState = myState;

  /*process functionality*/
  DEBUG_PULSE_START(DIAG_LED_LOOP_STATEMACHINE);
  switch(myState)
  {
    case CONNECTING_TO_WIFI:
      if(printStateName) Serial.println("CONNECTING_TO_WIFI");

      stateConnectingToWifi(c, internalState);
      break;

    case CONNECTING_TO_ATEM_HOST:
      if(printStateName) Serial.println("CONNECTING_TO_ATEM_HOST");
      stateConnectingToAtemHost(c, internalState);
      break;

    case CONNECTING_TO_PEERNETWORK_HOST:
      if(printStateName) Serial.println("CONNECTING_TO_PEERNETWORK_HOST");
      stateConnectingToPeerNetworkHost(c, internalState);
      break;

    case RUNNING_ATEM:
      if(printStateName) Serial.println("RUNNING_ATEM");
      stateRunningAtem(c, internalState);
      break;

    case RUNNING_PEERNETWORK:
      if(printStateName) Serial.println("RUNNING_PEERNETWORK");
      stateRunningPeerNetwork(c, internalState);
      break;

    case ERROR:
      Serial.println("*** ERROR ***");
      break;

    default:
      Serial.println("*** INVALID STATE ***");
      break;
  }
  DEBUG_PULSE_STOP(DIAG_LED_LOOP_STATEMACHINE);

  /*update main output: Red&Green tally lights*/
  DEBUG_PULSE_START(DIAG_LED_LOOP_TALLY_OUTPUT);
  outputUpdate(currentTick, tallyDataIsValid(), tallyPreview, tallyProgram, tallyInTransition);
  DEBUG_PULSE_STOP(DIAG_LED_LOOP_TALLY_OUTPUT);

  /*update diagnostic led to indicate running state*/
  updateLed(currentTick);

  /*run terminal here*/
  tallyBoxTerminalUpdate();

  /*run webserver here*/
  DEBUG_PULSE_START(DIAG_LED_LOOP_WEB_SERVER);
  tallyBoxWebServerUpdate();
  DEBUG_PULSE_STOP(DIAG_LED_LOOP_WEB_SERVER);

  /*call over-the-air update mechanism from here to provide faster speed*/
  DEBUG_PULSE_START(DIAG_LED_LOOP_OTA);
  OTAUpdate();
  DEBUG_PULSE_STOP(DIAG_LED_LOOP_OTA);

  /*run mDns*/
  MDnsUpdate();

  DEBUG_PULSE_STOP(DIAG_LED_LOOP_FULL);
}


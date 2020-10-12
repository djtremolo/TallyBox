#include "TallyBoxStateMachine.hpp"
#include "Arduino.h"
#include <ATEMbase.h>
#include <ATEMstd.h>
#include <SkaarhojPgmspace.h>
#include <WiFiUdp.h>

ATEMstd AtemSwitcher;
WiFiUDP Udp;

static bool isMaster = true;
static bool tallyPreview = false;
static bool tallyProgram = false;
static bool masterCommunicationFrozen = false;


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

  if(masterCommunicationFrozen)
  {
    outMask = 0xFF00FF00;
  }
  else
  {
    if(tallyPreview)
      outMask |= 0x55555555;
    if(tallyProgram)
      outMask |= 0xFFFFFFFF;
  }

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

void peerNetworkInitialize(uint16_t localPort)
{
  Udp.begin(localPort);  
}


uint16_t peerNetworkSerialize(uint16_t& grn, uint16_t& red, uint8_t *buf, uint16_t maxLen)
{
  uint16_t ret = 0;
  if(maxLen >= 4)
  {
    uint16_t idx = 0;
    buf[idx++] = (uint8_t)(grn >> 8) & 0x00ff;
    buf[idx++] = (uint8_t)(grn) & 0x00ff;
    buf[idx++] = (uint8_t)(red >> 8) & 0x00ff;
    buf[idx++] = (uint8_t)(red) & 0x00ff;
    ret = idx;
  }
  return ret;
}

bool peerNetworkDeSerialize(uint16_t& grn, uint16_t& red, uint8_t *buf, uint16_t len)
{
  bool ret = false;
  if(len == 4)
  {
    grn = (uint16_t)buf[0];
    grn <<= 8;
    grn |= (uint16_t)buf[1];

    red = (uint16_t)buf[2];
    red <<= 8;
    red |= (uint16_t)buf[3];

    ret = true;
  }
  return ret;
}


void peerNetworkSend(tallyBoxConfig_t& c, uint16_t greenChannel, uint16_t redChannel)
{
  uint8_t buf[4];

  uint16_t bufLen = peerNetworkSerialize(greenChannel, redChannel, buf, sizeof(buf));
  if(bufLen > 0)
  {
    Udp.beginPacket(IPAddress(0,0,0,0), 7493);
    Udp.write(buf, bufLen);
    Udp.endPacket();
  }

  tallyPreview = (greenChannel == c.cameraId);
  tallyProgram = (redChannel == c.cameraId);

  Serial.println("Program="+String(redChannel)+", Preview="+String(greenChannel)+".");
}

bool peerNetworkReceive(uint16_t& greenChannel, uint16_t& redChannel)
{
  bool ret = false;

  Serial.print("A");
  //if(Udp.available())
  {
    int packetSize = Udp.parsePacket();
    Serial.print("B");
    if(packetSize)
    {
      uint8_t buf[255];
      int len = Udp.read(buf, 255);
      uint16_t tmpGreen;
      uint16_t tmpRed;
      Serial.print("C");
      
      if(peerNetworkDeSerialize(tmpGreen, tmpRed, buf, packetSize))
      {
        Serial.print("D");
        greenChannel = tmpGreen;
        redChannel = tmpRed;

        ret = true;
      }
    }
  }
  return ret;
}

void stateConnectingToWifi()
{
  
}

void tallyBoxStateMachineUpdate(tallyBoxConfig_t& c, tallyBoxState_t switchToState)
{
  uint16_t currentTick = (millis() / 100) % 32; /*tick will count 0...31,0...31 etc*/
  static uint8_t internalState[STATE_MAX] = {};
  static uint16_t prevTick = 0;
  static uint32_t tickCounter = 0;
  static uint32_t lastReceivedMasterMessageInTicks = 0;
  uint16_t programCh, previewCh;
  uint16_t tmpA, tmpB;

  if(currentTick == prevTick)
  {
    /*run only once per tick*/
    return; 
  }
  prevTick = currentTick;

  tickCounter++;
  isMaster = c.cameraId == 1; 

  /*external transition required?*/
  if(switchToState != STATE_MAX)
  {
    myState = switchToState;
    internalState[myState] = 0;
  }

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

          if(isMaster)
          {
            myState = CONNECTING_TO_ATEM_HOST;
          }
          else
          {
            /*start listening for peerNetwork updates from master box*/
            peerNetworkInitialize(7493);
            myState = CONNECTING_TO_TALLYBOX_HOST;
          }  
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
      Serial.println("RUNNING_ATEM");

      AtemSwitcher.runLoop();

      if(AtemSwitcher.isConnected())
      {
        masterCommunicationFrozen = false;
        lastReceivedMasterMessageInTicks = tickCounter;
      }

      if(tickCounter - lastReceivedMasterMessageInTicks > 10)
      {
        masterCommunicationFrozen = true;

        /*jump to reconnection*/
        myState = CONNECTING_TO_ATEM_HOST;
        internalState[CONNECTING_TO_ATEM_HOST] = 0;
      }

      if(!masterCommunicationFrozen)
      {
        peerNetworkSend(c, AtemSwitcher.getPreviewInput(), AtemSwitcher.getProgramInput());
      }
      break;

    case RUNNING_TALLYBOX:
      Serial.println("RUNNING_TALLYBOX");

      if(peerNetworkReceive(tmpA, tmpB))
      {
        Serial.println("Received master frame");
        tallyPreview = (tmpA == c.cameraId);
        tallyProgram = (tmpB == c.cameraId);
        lastReceivedMasterMessageInTicks = tickCounter;
        masterCommunicationFrozen = false;
      }

      if(tickCounter - lastReceivedMasterMessageInTicks > 10)
      {
        masterCommunicationFrozen = true;
      }
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


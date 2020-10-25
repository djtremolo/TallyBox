#include "ESP8266WiFi.h"
#include "Arduino.h"
#include "TallyBoxInfra.hpp"
#include <Arduino_CRC32.h>
#include "TallyBoxOutput.hpp"
#include "TallyBoxStateMachine.hpp"

static WiFiServer server(7493);
//WiFiClient client;
static bool initialized = false;

void tallyBoxTerminalInitialize()
{
  server.begin();
  initialized = true;
}

WiFiClient handleClientConnection()
{
  static WiFiClient client;

  if(client)
  {
    if(client.connected())
    {
      /*we already have a connected client*/    
      return client;
    }

    /*disable, just in case*/
    setBrightnessSettingMode(OUTPUT_NONE, false);

    /*drop zombie connection*/
    client.stop();
    Serial.println("client disconnected");
  }

  /*check if we have new client connecting*/
  client = server.available();
  if(client)
  {
    Serial.println("client connected");
  }
  return client;
}


bool getUserInput(WiFiClient client, String& userInput)
{
  bool somethingReceived = false;
  bool frameEndReceived = false;

  if(client.available())
  {
    somethingReceived = true;
    while(client.available())
    {
      char c = client.read();
      userInput += c;
      if(c == '\n')
        frameEndReceived = true;
    }
  }
  return somethingReceived;
}

typedef enum
{
  USER_EVENT_NONE,
  USER_EVENT_EMPTY,
  USER_EVENT_COMMAND,
  USER_EVENT_COMMAND_WITH_VALUE
} terminalUserEvent_t;

terminalUserEvent_t parseUserInput(WiFiClient client, String& cmd, String& val)
{
  terminalUserEvent_t event = USER_EVENT_NONE;
  String ui = "";
  bool uiReceived = getUserInput(client, ui);

  if(!uiReceived)
  {
    cmd = "";
    val = "";
    return USER_EVENT_NONE;
  }

  int delimiterFoundAt = ui.indexOf('=');

  if(delimiterFoundAt >= 0)
  {
    cmd = ui.substring(0, delimiterFoundAt);
    val = ui.substring(delimiterFoundAt+1);
    cmd.trim();
    val.trim();

    if(cmd.length() == 0)
    {
      event = USER_EVENT_EMPTY; /*cmd is empty after whitespaces are removed*/
    }
    else if(val.length() == 0)
    {
      event = USER_EVENT_COMMAND; /*cmd was fine, but value was empty*/
    }
    else
    {
      event = USER_EVENT_COMMAND_WITH_VALUE;
    }
  }
  else
  {
    val = ""; /*no value found*/
    cmd = ui;
    cmd.trim();
    if(cmd.length() == 0)
    {
      event = USER_EVENT_EMPTY; /*cmd is empty after whitespaces are removed*/
    }
    else
    {
      event = USER_EVENT_COMMAND; /*cmd was fine, but value was empty*/
    }
  }

  return event;
}

typedef enum 
{
  MENU_MAIN,
  MENU_RESTART,
  MENU_BRIGHTNESS,
  /*************/
  MENU_MAX
} terminalMenuId_t;

typedef struct
{
  String menuText;
} terminalMenuItem_t;


const terminalMenuItem_t terminalMenu[MENU_MAX] = 
{
  {"Main menu:\r\n  1 = Restart\r\n  2 = Brightness\r\n  -> "},
  {"Restart\r\n  y/Y = yes\r\n  others = Return to main menu\r\n "},
  {"Brightness\r\n  g/G = Preview\r\n  r/R = Program\r\n  l/L = Both channels linked\r\n  m/M = Return to main menu\r\n "}
};


const String channelNames[] = {"None", "Green", "Red", "Linked"};

float myRation = 1.0;

float calculateLinkedRatio(float green, float red)
{
  if(green < 1)
  {
    return 1.0;
  }

  /*red = ratio * green*/
  float ratio = red / green;

  /*limit strength*/
  ratio = min(ratio, 10.0f);
  ratio = max(ratio, 0.1f);

  return ratio;
}

void saturateAdjustmentValue(float oldValue, float& adjustmentValue)
{
  if(oldValue+adjustmentValue > 100.0) adjustmentValue = 100.0-oldValue;
  if(oldValue+adjustmentValue < 0.0) adjustmentValue = -oldValue;
}

static float myG = 50.0;
static float myR = 50.0;
static float myRatio = 1.0;
static bool brightnessValuesInitialized = false;

void sampleBrightnessValues()
{
  static bool rawValuesInitialized = false;
  if(!rawValuesInitialized)
  {
    myG = getOutputBrightness(OUTPUT_GREEN);
    myR = getOutputBrightness(OUTPUT_RED);
    rawValuesInitialized = true;
  }
  myRatio = calculateLinkedRatio(myG, myR);
  brightnessValuesInitialized = true;
}

void normalizeAdjustmentWithRatio(float ratio, float commonAdjValue, float& gAdj, float& rAdj)
{
  if(ratio > 1.0)
  {
    gAdj = commonAdjValue/ratio;
    rAdj = commonAdjValue;
  }
  else
  {
    gAdj = commonAdjValue;
    rAdj = ratio*commonAdjValue;    
  }

  float adjustedG = myG+gAdj;
  float adjustedR = myR+rAdj;

  /*check limits together*/
  if((adjustedG > 100.0) || (adjustedG < 0.0)
      || (adjustedR > 100.0) || (adjustedR < 0.0))
  {
    gAdj = 0.0;
    rAdj = 0.0;
  }
}

void adjustChannel(tallyBoxOutput_t ch, String& cmd)
{
  float adjustment = 0.0;
  char first = cmd.charAt(0);
  
  if(!brightnessValuesInitialized)
  {
    return;
  }

  if(cmd == "+")
  {
    adjustment = 1.0;
  }
  else if(cmd == "-")
  {
    adjustment = -1.0;
  }
  else if((first=='+' || first=='-') && (cmd.length() > 1))
  {
    adjustment = cmd.substring(1).toFloat();

    if(first == '-') adjustment *= -1.0;
  }
  else
  {
    return;
  }

  if(adjustment > 100.0 || adjustment < -100.0)
  {
    return;
  }

  float gAdj;
  float rAdj;

  switch(ch)
  {
    case OUTPUT_GREEN:
      saturateAdjustmentValue(myG, adjustment);
      myG += adjustment;
      break;
    case OUTPUT_RED:
      saturateAdjustmentValue(myR, adjustment);
      myR += adjustment;
      break;
    case OUTPUT_LINKED:
      normalizeAdjustmentWithRatio(myRatio, adjustment, gAdj, rAdj);
      myG += gAdj;
      myR += rAdj;
      break;
    default:
      break;
  }

  setOutputBrightness((uint16_t)myG, OUTPUT_GREEN);
  setOutputBrightness((uint16_t)myR, OUTPUT_RED);
}


void userInterface(WiFiClient client)
{
  static terminalMenuId_t myState = MENU_MAIN;
  static tallyBoxOutput_t selectedBrightnessChannel = OUTPUT_NONE;
  static String prevCommand;
  static String prevVal;
  static terminalUserEvent_t prevEvent;
  static terminalMenuId_t prevState = MENU_MAIN;

  if(!client || !(client.connected()))
  {
    return;
  }


  /*removeme begin: report master status*/
  static bool prevMasterConnection = false;
  bool masterConnection = tallyDataIsValid();

  if(masterConnection != prevMasterConnection)
  {
    client.print("*** " + String(millis()));
    if(masterConnection)
      client.println(": Master CONNECTED");
    else
      client.println(": Master connection BROKEN");

    prevMasterConnection = masterConnection;
  }
  /*removeme end: report master status*/


  /*removeme begin: report tick compensation status*/
  static int32_t prevCompensationValue = false;
  int32_t compensationValue = getTickCompensationValue();

  if(compensationValue != prevCompensationValue)
  {
    client.print("*** " + String(millis()));

    client.println(": TickCompensation="+String(compensationValue));

    prevCompensationValue = compensationValue;
  }
  /*removeme end: report tick compensation status*/

  String cmd;
  String val;
  terminalUserEvent_t ev = parseUserInput(client, cmd, val);

  if(ev!=USER_EVENT_NONE)
  {
    /*repeat previous command if just enter was pressed. This eases up setting the levels etc.*/
    if((ev==USER_EVENT_EMPTY) && (myState == prevState) && (prevCommand != ""))
    {
      cmd = prevCommand;
      val = prevVal;
      ev = prevEvent;
    }

    /*store state before proceeding to state machine*/
    prevState = myState;

    char cmdFirst = cmd.charAt(0);
    switch(myState)
    {
      case MENU_MAIN:
        switch(ev)
        {
          case USER_EVENT_COMMAND:
            if(cmd=="1")
            {
              myState = MENU_RESTART;
            }
            else if(cmd=="2")
            {
              sampleBrightnessValues();
              selectedBrightnessChannel = OUTPUT_NONE;
              myState = MENU_BRIGHTNESS;
            }
            break;
          default:
            break;
        }
        break;
      case MENU_RESTART:
        switch(ev)
        {
          case USER_EVENT_COMMAND:
            if(cmdFirst=='y' || cmdFirst=='Y')
            {
              client.println("\r\nRESTARTING\r\n");
              delay(1000);
              ESP.restart();
            }
            else
            {
              myState = MENU_MAIN;
            }
            break;
          default:
            break;
        }
        break;
      case MENU_BRIGHTNESS:
        switch(ev)
        {
          case USER_EVENT_COMMAND:
            if(cmdFirst=='g' || cmdFirst=='G')
            {
              selectedBrightnessChannel = OUTPUT_GREEN;
            }
            else if(cmdFirst=='r' || cmdFirst=='R')
            {
              selectedBrightnessChannel = OUTPUT_RED;
            }
            else if(cmdFirst=='l' || cmdFirst=='L')
            {
              sampleBrightnessValues();
              selectedBrightnessChannel = OUTPUT_LINKED;
            }
            else if(cmdFirst=='m' || cmdFirst=='M')
            {
              myState = MENU_MAIN;
            }
            else if(cmdFirst=='+' || cmdFirst=='-')
            {
              adjustChannel(selectedBrightnessChannel, cmd);
            }
            break;
          default:
            break;
        }
        break;
      default:
        myState = MENU_MAIN;
        break;
    }
    /*store previous command so that we can repeat it by just "enter"*/
    prevCommand = cmd;
    prevVal = val;
    prevEvent = ev;

    /*print prompt*/
    client.print("\r\n"+terminalMenu[myState].menuText);

    /*keep visualization active in this state*/
    setBrightnessSettingMode(selectedBrightnessChannel, myState == MENU_BRIGHTNESS);

    /*state dependent appending of prompt*/
    switch(myState)
    {
      case MENU_BRIGHTNESS:
        switch(selectedBrightnessChannel)
        {
          case OUTPUT_GREEN:
          case OUTPUT_RED:
          case OUTPUT_LINKED:
            client.print("[G="+String((uint16_t)myG)+"%, R="+String((uint16_t)myR)+"%] "+channelNames[selectedBrightnessChannel]+" -> ");
            break;
          default:
            client.print("Select channel -> ");
            break;
        }
        break;
    }
  }
}


void tallyBoxTerminalUpdate()
{
  if(!initialized)
  {
    return;
  }

  auto client = handleClientConnection();
  userInterface(client);

}

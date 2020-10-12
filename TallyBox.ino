#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "TallyBoxConfiguration.hpp"
#include "TallyBoxStateMachine.hpp"


#define TALLYBOX_FIRMWARE_VERSION               "0.1.0"

tallyBoxConfig_t myConf;

//eth.addr==ec:fa:bc:c0:a9:cd

void setup() 
{ 

  pinMode(LED_BUILTIN, OUTPUT);

  randomSeed(analogRead(5));  // For random port selection

  Serial.begin(115200);
  Serial.println("\n- - - - - - - -\nTallyBox version Jee "+String(TALLYBOX_FIRMWARE_VERSION)+".\n- - - - - - - -\n");

  EEPROM.begin(512);

  /*handle configuration write/read. Will block in case of non-recoverable failure.*/
  tallyBoxConfiguration(myConf);




  // Initialize a connection to the switcher:
}

void loop() 
{
  tallyBoxStateMachineUpdate(myConf);
    
}

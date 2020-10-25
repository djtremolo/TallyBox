#include "TallyBoxConfiguration.hpp"
#include "TallyBoxStateMachine.hpp"
#include "LittleFS.h"

#define TALLYBOX_FIRMWARE_VERSION               "0.1.2"

tallyBoxConfig_t myConf;
static FS* filesystem = &LittleFS;

void setup() 
{ 
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(0, INPUT);

  Serial.println("\n- - - - - - - -\nTallyBox version "+String(TALLYBOX_FIRMWARE_VERSION)+".\n- - - - - - - -\n");

  filesystem->begin();

  /*handle configuration write/read. Will block in case of non-recoverable failure.*/
  tallyBoxConfiguration(myConf);

  /*initialize main functionality*/
  tallyBoxStateMachineInitialize(myConf);
}

void loop()
{
  tallyBoxStateMachineUpdate(myConf);
  delay(1);
}

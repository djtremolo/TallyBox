#include "TallyBoxConfiguration.hpp"
#include "TallyBoxStateMachine.hpp"

#define TALLYBOX_FIRMWARE_VERSION               "0.1.0"

tallyBoxConfig_t myConf;

void setup() 
{ 
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println("\n- - - - - - - -\nTallyBox version "+String(TALLYBOX_FIRMWARE_VERSION)+".\n- - - - - - - -\n");

  /*handle configuration write/read. Will block in case of non-recoverable failure.*/
  tallyBoxConfiguration(myConf);
}

void loop()
{
  tallyBoxStateMachineUpdate(myConf);
}

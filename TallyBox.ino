#include <ATEMbase.h>
#include <ATEMstd.h>
#include <SkaarhojPgmspace.h>

#define TALLYBOX_FIRMWARE_VERSION               "0.1.0"
#define TALLYBOX_PROGRAM_EEPROM                 0

ATEMstd AtemSwitcher;

//eth.addr==ec:fa:bc:c0:a9:cd

/*configuration mechanism included as a cpp file*/
#include "TallyBoxConfiguration.cpp"


void setup() 
{ 
  tallyBoxConfig_t myConf;

  randomSeed(analogRead(5));  // For random port selection

  Serial.begin(115200);
  Serial.println("\n- - - - - - - -\nTallyBox version "+String(TALLYBOX_FIRMWARE_VERSION)+".\n- - - - - - - -\n");

  EEPROM.begin(512);

  /*handle configuration write/read. Will block in case of non-recoverable failure.*/
  tallyBoxConfiguration(myConf);

  // Initialize a connection to the switcher:
  AtemSwitcher.begin(IPAddress(myConf.hostAddressU32));
  AtemSwitcher.serialOutput(0x80);
  AtemSwitcher.connect();
}

void loop() 
{
  AtemSwitcher.runLoop();
    
}

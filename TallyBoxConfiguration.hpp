#ifndef __TALLYBOXCONFIGURATION_HPP__
#define __TALLYBOXCONFIGURATION_HPP__

/*please create the following header file with content:

#define TALLYBOX_CONFIGURATION_DEFAULT_SSID     "set-to-your-ssid"
#define TALLYBOX_CONFIGURATION_DEFAULT_PASSWD   "set-to-your-pwd"

*/
#include "TallyBoxDefaultConfiguration.hpp"


#define TALLYBOX_CONFIGURATION_VERSION          1
#define TALLYBOX_EEPROM_STORAGE_ADDRESS         0


typedef struct
{
  size_t sizeOfConfiguration;
  uint8_t versionOfConfiguration;
  uint8_t cameraId;
  char wifiSSID[32+1];
  char wifiPasswd[32+1];
  uint32_t hostAddressU32;

  /*must be last*/
  uint32_t checkSum;
} tallyBoxConfig_t;


#endif

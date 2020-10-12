#ifndef __TALLYBOXCONFIGURATION_HPP__
#define __TALLYBOXCONFIGURATION_HPP__

#include <arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
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


void setDefaults(tallyBoxConfig_t& c);
uint32_t calcChecksum(tallyBoxConfig_t& c);
bool validateConfiguration(tallyBoxConfig_t& c);
bool configurationGet(tallyBoxConfig_t& c);
bool configurationPut(tallyBoxConfig_t& c);
void handleConfigurationWrite(tallyBoxConfig_t& c);
void handleConfigurationRead(tallyBoxConfig_t& c);
void dumpConf(String confName, tallyBoxConfig_t& c);
void tallyBoxConfiguration(tallyBoxConfig_t& c);


#endif

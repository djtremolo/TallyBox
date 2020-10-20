#ifndef __TALLYBOXCONFIGURATION_HPP__
#define __TALLYBOXCONFIGURATION_HPP__

#include <arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "TallyBoxDefaultConfiguration.hpp"

#define TALLYBOX_CONFIGURATION_VERSION          1
#define TALLYBOX_EEPROM_STORAGE_ADDRESS         0


#define CONF_NETWORK_NAME_LEN_SSID              20
#define CONF_NETWORK_NAME_LEN_PASSWD            20
#define CONF_NETWORK_NAME_LEN_MDNS_NAME         20

typedef struct
{
  size_t sizeOfConfiguration;
  uint8_t versionOfConfiguration;

  char wifiSSID[CONF_NETWORK_NAME_LEN_SSID+1];
  char wifiPasswd[CONF_NETWORK_NAME_LEN_PASSWD+1];
  uint32_t hostAddressU32;
  uint32_t ownAddressU32;
  bool hasStaticIp;
  bool isMaster;
  char mdnsHostName[CONF_NETWORK_NAME_LEN_MDNS_NAME+1];

  /*must be last*/
  uint32_t checkSum;
} tallyBoxNetworkConfig_t;

typedef struct
{
  size_t sizeOfConfiguration;
  uint8_t versionOfConfiguration;

  uint8_t cameraId;
  uint16_t greenBrightnessPercent;
  uint16_t redBrightnessPercent;

  /*must be last*/
  uint32_t checkSum;
} tallyBoxUserConfig_t;





void setDefaults(tallyBoxNetworkConfig_t& c);
uint32_t calcChecksum(tallyBoxNetworkConfig_t& c);
bool validateConfiguration(tallyBoxNetworkConfig_t& c);
bool configurationGet(tallyBoxNetworkConfig_t& c);
bool configurationPut(tallyBoxNetworkConfig_t& c);
void handleConfigurationWrite(tallyBoxNetworkConfig_t& c);
void handleConfigurationRead(tallyBoxNetworkConfig_t& c);
void dumpConf(String confName, tallyBoxNetworkConfig_t& c);
void tallyBoxConfiguration(tallyBoxNetworkConfig_t& c);

void setDefaults(tallyBoxUserConfig_t& c);
uint32_t calcChecksum(tallyBoxUserConfig_t& c);
bool validateConfiguration(tallyBoxUserConfig_t& c);
bool configurationGet(tallyBoxUserConfig_t& c);
bool configurationPut(tallyBoxUserConfig_t& c);
void handleConfigurationWrite(tallyBoxUserConfig_t& c);
void handleConfigurationRead(tallyBoxUserConfig_t& c);
void dumpConf(String confName, tallyBoxUserConfig_t& c);
void tallyBoxConfiguration(tallyBoxUserConfig_t& c);


#endif

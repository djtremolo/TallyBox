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
  char mdnsHostName[CONF_NETWORK_NAME_LEN_MDNS_NAME+1];

  /*must be last*/
  uint32_t checkSum;
} tallyBoxNetworkConfig_t;

typedef struct
{
  size_t sizeOfConfiguration;
  uint8_t versionOfConfiguration;

  uint16_t cameraId;
  uint16_t greenBrightnessValue;
  uint16_t redBrightnessValue;
  bool isMaster;

  /*must be last*/
  uint32_t checkSum;
} tallyBoxUserConfig_t;


typedef struct
{
  tallyBoxNetworkConfig_t network;
  tallyBoxUserConfig_t user;
} tallyBoxConfig_t;


void tallyBoxConfiguration(tallyBoxConfig_t& c);

#endif

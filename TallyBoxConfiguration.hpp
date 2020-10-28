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
  IPAddress hostAddress;
  IPAddress ownAddress;
  IPAddress subnetMask;
  IPAddress defaultGateway;
  bool hasStaticIp;
  char mdnsHostName[CONF_NETWORK_NAME_LEN_MDNS_NAME+1];
} tallyBoxNetworkConfig_t;

typedef struct
{
  size_t sizeOfConfiguration;
  uint8_t versionOfConfiguration;

  uint16_t cameraId;
  float greenBrightnessPercent;
  float redBrightnessPercent;
  bool isMaster;
} tallyBoxUserConfig_t;


typedef struct
{
  tallyBoxNetworkConfig_t network;
  tallyBoxUserConfig_t user;
} tallyBoxConfig_t;


void tallyBoxConfiguration(tallyBoxConfig_t& c);

#endif

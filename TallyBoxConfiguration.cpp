#include "TallyBoxConfiguration.hpp"
#include "TallyBoxOutput.hpp"   /*for default brightness*/
#include "Arduino.h"
#include <Arduino_CRC32.h>
#include "LittleFS.h"
#include <ArduinoJson.h>

static FS* filesystem = &LittleFS;

const char fileNameNetworkConfig[] = "config_network.json";
const char fileNameUserConfig[] = "config_user.json";

void setDefaults(tallyBoxNetworkConfig_t& c);
void setDefaults(tallyBoxUserConfig_t& c);
void dumpConf(String confName, tallyBoxNetworkConfig_t& c);
void dumpConf(String confName, tallyBoxUserConfig_t& c);
const char* getFileName(tallyBoxNetworkConfig_t& c);
const char* getFileName(tallyBoxUserConfig_t& c);
void serializeToByteArray(tallyBoxNetworkConfig_t& c, char* jsonBuf, size_t maxBytes);
void serializeToByteArray(tallyBoxUserConfig_t& c, char* jsonBuf, size_t maxBytes);
bool deSerializeFromJson(tallyBoxNetworkConfig_t& c, char* jsonBuf);
bool deSerializeFromJson(tallyBoxUserConfig_t& c, char* jsonBuf);


template <typename T>
uint32_t calcChecksum(T& c);

template <typename T>
bool validateConfiguration(T& c);

template <typename T>
bool configurationGet(T& c);

template <typename T>
bool configurationPut(T& c);

template <typename T>
void writeFactoryDefaultConfiguration(T& c);

template <typename T>
void handleConfigurationRead(T& c);

void tallyBoxConfiguration(tallyBoxConfig_t& c);



void setDefaults(tallyBoxNetworkConfig_t& c)
{
  c.sizeOfConfiguration = sizeof(tallyBoxNetworkConfig_t);
  c.versionOfConfiguration = TALLYBOX_CONFIGURATION_VERSION;

  strlcpy(c.wifiSSID, TALLYBOX_CONFIGURATION_DEFAULT_SSID, sizeof(c.wifiSSID));
  strlcpy(c.wifiPasswd, TALLYBOX_CONFIGURATION_DEFAULT_PASSWD, sizeof(c.wifiPasswd));

  c.isMaster = TALLYBOX_CONFIGURATION_DEFAULT_ISMASTER; 

  IPAddress ip;
  ip.fromString(String(TALLYBOX_CONFIGURATION_DEFAULT_HOSTIP));
  c.hostAddress = ip;

  ip.fromString(String(TALLYBOX_CONFIGURATION_DEFAULT_OWNIP));
  c.ownAddress = ip;

  ip.fromString(String(TALLYBOX_CONFIGURATION_DEFAULT_SUBNET));
  c.subnetMask = ip;

  ip.fromString(String(TALLYBOX_CONFIGURATION_DEFAULT_GATEWAY));
  c.defaultGateway = ip;

  c.hasStaticIp = TALLYBOX_CONFIGURATION_DEFAULT_HASOWNIP; 

  strlcpy(c.mdnsHostName, "tallybox", sizeof(c.mdnsHostName));
}

void setDefaults(tallyBoxUserConfig_t& c)
{
  c.sizeOfConfiguration = sizeof(tallyBoxUserConfig_t);
  c.versionOfConfiguration = TALLYBOX_CONFIGURATION_VERSION;

  c.cameraId = TALLYBOX_CONFIGURATION_DEFAULT_CAMERA_ID;
  c.greenBrightnessPercent = DEFAULT_GREEN_BRIGHTNESS_PCT;
  c.redBrightnessPercent = DEFAULT_RED_BRIGHTNESS_PCT;
}

void dumpConf(String confName, tallyBoxNetworkConfig_t& c)
{
  Serial.println("Configuration: '" + confName + "'.");
  Serial.println(" - Version            = "+String(c.versionOfConfiguration));
  Serial.println(" - Size               = "+String(c.sizeOfConfiguration));
  Serial.println(" - Master Device      = "+String(c.isMaster));
  Serial.println(" - ATEM Host IP       = "+c.hostAddress.toString());
  Serial.println(" - Uses Static IP     = "+String(c.hasStaticIp));
  Serial.println(" - Own IP             = "+c.ownAddress.toString());
  Serial.println(" - Subnet mask        = "+c.subnetMask.toString());
  Serial.println(" - Default gateway    = "+c.defaultGateway.toString());
  Serial.println(" - MDNS Host Name     = "+String(c.mdnsHostName));
  Serial.print(" - WifiSSID           = ");
  Serial.println(c.wifiSSID);
  Serial.println(" - Password           = <not shown>");
}

void dumpConf(String confName, tallyBoxUserConfig_t& c)
{
  Serial.println("Configuration: '" + confName + "'.");
  Serial.println(" - Version            = "+String(c.versionOfConfiguration));
  Serial.println(" - Size               = "+String(c.sizeOfConfiguration));
  Serial.println(" - Camera ID          = "+String(c.cameraId));
  Serial.println(" - Green Brightness   = "+String(c.greenBrightnessPercent, 0)+"%");
  Serial.println(" - Red Brightness     = "+String(c.redBrightnessPercent, 0)+"%");
}


template <typename T>
bool validateConfiguration(T& c)
{
  bool ret = false;
  if(c.versionOfConfiguration == TALLYBOX_CONFIGURATION_VERSION)
  {
    if(c.sizeOfConfiguration == sizeof(T))
    {
      ret = true;
    }
    else
    {
      Serial.println("validateConfiguration: size check failed");
    }
  }
  else
  {
    Serial.println("validateConfiguration: version check failed");
  }

  return ret;
}

const char* getFileName(tallyBoxNetworkConfig_t& c)
{
  return fileNameNetworkConfig;
}

const char* getFileName(tallyBoxUserConfig_t& c)
{
  return fileNameUserConfig;
}

template <typename T>
bool configurationGet(T& c)
{
  bool ret = false;
  const char* fileName = getFileName(c);

  File f = filesystem->open(fileName, "r");

  if(f.size() > 0)
  {
    char buf[512];

    if(f.read((uint8_t*)buf, sizeof(buf)) > 0)
    {
      if(deSerializeFromJson(c, buf))
      {
        ret = validateConfiguration(c);
      }
      else
      {
        Serial.println("configurationGet(): json deserialization failed");
      }
    }
    else
    {
      Serial.printf("configurationGet(): read failed\r\n");
    }
  }
  else
  {
    Serial.printf("configurationGet(): file is empty\r\n");
  }

  f.close();

  return ret;
}


template <typename T>
bool configurationPut(T& c)
{
  bool ret = false;
  const char* fileName = getFileName(c);

  if(validateConfiguration(c))
  {
    File f = filesystem->open(fileName, "w");
    char bufToBeStored[512];

    serializeToByteArray(c, bufToBeStored, sizeof(bufToBeStored));

    size_t len = strlen(bufToBeStored);

    Serial.println("configurationPut: preparing to write "+String(len)+" bytes.");

    if(f.write(bufToBeStored, len) == len)
    {
      Serial.println("success!");
      ret = true;
    }
    else
    {
      Serial.printf("configurationPut(): write failed\r\n");
    }
  }
  return ret;
}


void serializeToByteArray(tallyBoxNetworkConfig_t& c, char* jsonBuf, size_t maxBytes)
{
  StaticJsonDocument<512> doc;
  
  doc["sizeOfConfiguration"] = c.sizeOfConfiguration;
  doc["versionOfConfiguration"] = c.versionOfConfiguration;
  doc["wifiSSID"] = String(c.wifiSSID);
  doc["wifiPasswd"] = String(c.wifiPasswd);
  doc["isMaster"] = c.isMaster;
  doc["hostAddress"] = c.hostAddress.toString();
  doc["ownAddress"] = c.ownAddress.toString();
  doc["subnetMask"] = c.subnetMask.toString();
  doc["defaultGateway"] = c.defaultGateway.toString();
  doc["hasStaticIp"] = c.hasStaticIp;
  doc["mdnsHostName"] = String(c.mdnsHostName);

  serializeJsonPretty(doc, jsonBuf, maxBytes);

  Serial.println("json='"+String(jsonBuf)+"'");

}

void serializeToByteArray(tallyBoxUserConfig_t& c, char* jsonBuf, size_t maxBytes)
{
  StaticJsonDocument<512> doc;
  
  doc["sizeOfConfiguration"] = c.sizeOfConfiguration;
  doc["versionOfConfiguration"] = c.versionOfConfiguration;
  doc["cameraId"] = c.cameraId;
  doc["greenBrightnessPercent"] = c.greenBrightnessPercent;
  doc["redBrightnessPercent"] = c.redBrightnessPercent;

  serializeJsonPretty(doc, jsonBuf, maxBytes);

  Serial.println("json='"+String(jsonBuf)+"'");
}



bool deSerializeFromJson(tallyBoxNetworkConfig_t& c, char* jsonBuf)
{
  bool ret = false;
  StaticJsonDocument<512> doc;

  DeserializationError error = deserializeJson(doc, jsonBuf);

  if(error == DeserializationError::Ok)
  {
    c.sizeOfConfiguration = doc["sizeOfConfiguration"];
    c.versionOfConfiguration = doc["versionOfConfiguration"];
    
    String ssid = doc["wifiSSID"];
    strlcpy(c.wifiSSID, ssid.c_str(), CONF_NETWORK_NAME_LEN_SSID);
    String pwd = doc["wifiPasswd"];
    strlcpy(c.wifiPasswd, pwd.c_str(), CONF_NETWORK_NAME_LEN_PASSWD);

    c.isMaster = doc["isMaster"];

    String ha = doc["hostAddress"];
    c.hostAddress.fromString(ha);

    String oa = doc["ownAddress"];
    c.ownAddress.fromString(oa);

    String snm = doc["subnetMask"];
    c.subnetMask.fromString(snm);

    String dg = doc["defaultGateway"];
    c.defaultGateway.fromString(dg);

    c.hasStaticIp = doc["hasStaticIp"];
    String mdnshost = doc["mdnsHostName"];
    strlcpy(c.mdnsHostName, mdnshost.c_str(), CONF_NETWORK_NAME_LEN_MDNS_NAME);

    ret = true;
  }

  return ret;
}

bool deSerializeFromJson(tallyBoxUserConfig_t& c, char* jsonBuf)
{
  bool ret = false;
  StaticJsonDocument<512> doc;

  DeserializationError error = deserializeJson(doc, jsonBuf);

  if(error == DeserializationError::Ok)
  {
    c.sizeOfConfiguration = doc["sizeOfConfiguration"];
    c.versionOfConfiguration = doc["versionOfConfiguration"];

    c.cameraId = doc["cameraId"];
    c.greenBrightnessPercent = doc["greenBrightnessPercent"];
    c.redBrightnessPercent = doc["redBrightnessPercent"];

    ret = true;
  }

  return ret;
}


template <typename T>
void writeFactoryDefaultConfiguration(T& c)
{
  const char* fName = getFileName(c);

  setDefaults(c);

  if(!configurationPut(c))
  {
    Serial.println("Write FAILED!");
  }
}


template <typename T>
void handleConfigurationRead(T& c)
{
  const char* fName = getFileName(c);

  configurationGet(c);

  if(!validateConfiguration(c))
  {
    #if T == tallyBoxUserConfig_t
    Serial.printf("Loading configuration '%s' failed. Trying anyway.\r\n", fName);
    #else
    Serial.printf("Loading configuration '%s' failed. Cannot continue without networking settings.\r\n", fName);
    while(1);
    #endif
  }
}

void writeFactoryDefault(tallyBoxConfig_t& c)
{
  writeFactoryDefaultConfiguration(c.network);
  writeFactoryDefaultConfiguration(c.user);
}

#define BUTTON_ACTIVE   LOW

bool tallyBoxWriteConfiguration(tallyBoxNetworkConfig_t& c)
{
  return configurationPut(c);
}

bool tallyBoxWriteConfiguration(tallyBoxUserConfig_t& c)
{
  return configurationPut(c);
}

void tallyBoxConfiguration(tallyBoxConfig_t& c)
{
  /*give user the possibility to interrupt and load the default configuration*/
  Serial.print("Press FLASH button for default configuration....");
  delay(5000);
  if(digitalRead(0) == BUTTON_ACTIVE)
  {
    uint32_t counter = 0;
    Serial.println("Ok, keep the button active for more than 10 seconds");
    while(digitalRead(0) == BUTTON_ACTIVE)
    {
      digitalWrite(LED_BUILTIN, (counter % 2));
      Serial.print("-");      
      if(++counter > 20)
      {
        break;
      }
      delay(500);
    }
    /*still active after 10s?*/
    if(digitalRead(0) == BUTTON_ACTIVE)
    {
      Serial.println("Writing default configuration!");
      writeFactoryDefault(c);
    }
    else
    {
      Serial.println("\r\nCancelled, going forward with stored configuration - if any.");
    }
  }
  else
  {
    Serial.println("\r\nProceeding to normal boot.");
  }

  #if TALLYBOX_PROGRAM_FORCE_WRITE_DEFAULTS
  writeFactoryDefault(c);
  #endif

  /*handle network configuration part*/
  handleConfigurationRead(c.network);

  /*handle user configuration part*/
  handleConfigurationRead(c.user);

  Serial.println("*******************\r\nStarting with configuration:");
  dumpConf("Network", c.network);
  dumpConf("User", c.user);
  Serial.println("*******************\r\n");

}

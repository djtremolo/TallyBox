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
void handleConfigurationWrite(T& c);

template <typename T>
void handleConfigurationRead(T& c);

void tallyBoxConfiguration(tallyBoxConfig_t& c);



void setDefaults(tallyBoxNetworkConfig_t& c)
{
  c.sizeOfConfiguration = sizeof(tallyBoxNetworkConfig_t);
  c.versionOfConfiguration = TALLYBOX_CONFIGURATION_VERSION;

  strcpy(c.wifiSSID, TALLYBOX_CONFIGURATION_DEFAULT_SSID);
  strcpy(c.wifiPasswd, TALLYBOX_CONFIGURATION_DEFAULT_PASSWD);

  IPAddress hostIp(TALLYBOX_CONFIGURATION_DEFAULT_HOSTIP_BYTE0,
                TALLYBOX_CONFIGURATION_DEFAULT_HOSTIP_BYTE1,
                TALLYBOX_CONFIGURATION_DEFAULT_HOSTIP_BYTE2,
                TALLYBOX_CONFIGURATION_DEFAULT_HOSTIP_BYTE3);
  c.hostAddressU32 = hostIp.v4();

  IPAddress ownIp(TALLYBOX_CONFIGURATION_DEFAULT_OWNIP_BYTE0,
                TALLYBOX_CONFIGURATION_DEFAULT_OWNIP_BYTE1,
                TALLYBOX_CONFIGURATION_DEFAULT_OWNIP_BYTE2,
                TALLYBOX_CONFIGURATION_DEFAULT_OWNIP_BYTE3);
  c.ownAddressU32 = ownIp.v4();

  c.hasStaticIp = TALLYBOX_CONFIGURATION_DEFAULT_HASOWNIP; 

  strcpy(c.mdnsHostName, "tallybox");

  c.checkSum = calcChecksum(c);
}

void setDefaults(tallyBoxUserConfig_t& c)
{
  c.sizeOfConfiguration = sizeof(tallyBoxUserConfig_t);
  c.versionOfConfiguration = TALLYBOX_CONFIGURATION_VERSION;

  c.cameraId = TALLYBOX_CONFIGURATION_DEFAULT_CAMERA_ID;
  c.greenBrightnessValue = DEFAULT_GREEN_BRIGHTNESS;
  c.redBrightnessValue = DEFAULT_RED_BRIGHTNESS;
  c.isMaster = TALLYBOX_CONFIGURATION_DEFAULT_ISMASTER; 

  c.checkSum = calcChecksum(c);
}

void dumpConf(String confName, tallyBoxNetworkConfig_t& c)
{
  Serial.println("Configuration: '" + confName + "'.");
  Serial.println(" - Version            = "+String(c.versionOfConfiguration));
  Serial.println(" - Size               = "+String(c.sizeOfConfiguration));
  Serial.print(" - HostAddress        = ");
  IPAddress ip(c.hostAddressU32);
  Serial.println(ip);
  Serial.print(" - WifiSSID           = ");
  Serial.println(c.wifiSSID);
  Serial.println(" - Password           = <not shown>");
  Serial.print(" - Checksum           = 0x");
  Serial.println(c.checkSum, HEX);
}

void dumpConf(String confName, tallyBoxUserConfig_t& c)
{
  Serial.println("Configuration: '" + confName + "'.");
  Serial.println(" - Version            = "+String(c.versionOfConfiguration));
  Serial.println(" - Size               = "+String(c.sizeOfConfiguration));

  Serial.println(" - Camera ID          = "+String(c.cameraId));
  Serial.println(" - Green Brightness   = "+String(c.greenBrightnessValue));
  Serial.println(" - Red Brightness     = "+String(c.redBrightnessValue));
  Serial.println(" - Master Device      = "+String(c.isMaster));

  Serial.print(" - Checksum           = 0x");
  Serial.println(c.checkSum, HEX);
}


template <typename T>
uint32_t calcChecksum(T& c)
{
  Arduino_CRC32 crc32;
  uint16_t lenToCheck = sizeof(T) - sizeof(c.checkSum);
  uint8_t *buf = (uint8_t*)&c;

  return crc32.calc(buf, lenToCheck);
}

template <typename T>
bool validateConfiguration(T& c)
{
  bool ret = false;
  if(c.versionOfConfiguration == TALLYBOX_CONFIGURATION_VERSION)
  {
    if(c.sizeOfConfiguration == sizeof(T))
    {
      /*check CRC but allow manual editing by setting the crc to a magic value*/
      if((c.checkSum == calcChecksum(c)) || (c.checkSum == 12345678))
      {
        ret = true;
      }
      else
      {
        Serial.println("validateConfiguration: CRC check failed");
      }
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

    if(f.write(bufToBeStored, len) == len)
    {
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
  doc["hostAddressU32"] = c.hostAddressU32;
  doc["ownAddressU32"] = c.ownAddressU32;
  doc["hasStaticIp"] = c.hasStaticIp;
  doc["mdnsHostName"] = String(c.mdnsHostName);
  doc["checkSum"] = c.checkSum;

  serializeJsonPretty(doc, jsonBuf, maxBytes);

  Serial.println("json='"+String(jsonBuf)+"'");

}

void serializeToByteArray(tallyBoxUserConfig_t& c, char* jsonBuf, size_t maxBytes)
{
  StaticJsonDocument<512> doc;
  
  doc["sizeOfConfiguration"] = c.sizeOfConfiguration;
  doc["versionOfConfiguration"] = c.versionOfConfiguration;
  doc["cameraId"] = c.cameraId;
  doc["greenBrightnessValue"] = c.greenBrightnessValue;
  doc["redBrightnessValue"] = c.redBrightnessValue;
  doc["isMaster"] = c.isMaster;
  doc["checkSum"] = c.checkSum;

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
    strncpy(c.wifiSSID, ssid.c_str(), CONF_NETWORK_NAME_LEN_SSID);
    String pwd = doc["wifiPasswd"];
    strncpy(c.wifiPasswd, pwd.c_str(), CONF_NETWORK_NAME_LEN_PASSWD);
    c.hostAddressU32 = doc["hostAddressU32"];
    c.ownAddressU32 = doc["ownAddressU32"];
    c.hasStaticIp = doc["hasStaticIp"];
    String mdnshost = doc["mdnsHostName"];
    strncpy(c.mdnsHostName, mdnshost.c_str(), CONF_NETWORK_NAME_LEN_MDNS_NAME);
    c.checkSum = doc["checkSum"];

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
    c.greenBrightnessValue = doc["greenBrightnessValue"];
    c.redBrightnessValue = doc["redBrightnessValue"];
    c.isMaster = doc["isMaster"];
    
    c.checkSum = doc["checkSum"];

    ret = true;
  }

  return ret;
}


template <typename T>
void handleConfigurationWrite(T& c)
{
  const char* fName = getFileName(c);

  setDefaults(c);

  /*avoid overwriting the data if it is already identical*/
  T readConf;
  bool avoidWriting = false;
  configurationGet(readConf);
  if(validateConfiguration(readConf))
  {
    if(memcmp((void*)&c, (void*)&readConf, sizeof(T)) == 0)
    {
      /*identical!*/
      avoidWriting = true;
    }
  }

  dumpConf(fName, c);

  if(!avoidWriting)
  {
    if(!configurationPut(c))
    {
      Serial.println("Write FAILED!");
    }
  }
  else
  {
    Serial.println("<not written as the existing content was identical>");
  }  
}


template <typename T>
void handleConfigurationRead(T& c)
{
  const char* fName = getFileName(c);

  configurationGet(c);

  if(validateConfiguration(c))
  {
    dumpConf(fName, c);
  }
  else
  {
    #if T == tallyBoxUserConfig_t
    Serial.printf("Loading configuration '%s' failed. Trying anyway.\r\n", fName);
    #else
    Serial.printf("Loading configuration '%s' failed. Cannot continue without networking settings.\r\n", fName);
    while(1);
    #endif
  }
}

void loadDefaults(tallyBoxConfig_t& c)
{
  handleConfigurationWrite(c.network);
  handleConfigurationWrite(c.user);
}

#define BUTTON_ACTIVE   LOW

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
      Serial.println("Using default configuration!");
      loadDefaults(c);
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
  loadDefaults(c);
  #endif

  /*handle network configuration part*/
  handleConfigurationRead(c.network);

  /*handle user configuration part*/
  handleConfigurationRead(c.user);
}

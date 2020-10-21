#include "TallyBoxConfiguration.hpp"
#include "TallyBoxOutput.hpp"   /*for default brightness*/
#include <Arduino_CRC32.h>
#include "LittleFS.h"

static FS* filesystem = &LittleFS;

const char fileNameNetworkConfig[] = "network.cfg";
const char fileNameUserConfig[] = "user.cfg";


void setDefaults(tallyBoxNetworkConfig_t& c);
void setDefaults(tallyBoxUserConfig_t& c);
void dumpConf(String confName, tallyBoxNetworkConfig_t& c);
void dumpConf(String confName, tallyBoxUserConfig_t& c);
const char* getFileName(tallyBoxNetworkConfig_t& c);
const char* getFileName(tallyBoxUserConfig_t& c);

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

  strcpy(c.mdnsHostName, "tallybox.local");

  c.checkSum = calcChecksum(c);
}

void setDefaults(tallyBoxUserConfig_t& c)
{
  c.sizeOfConfiguration = sizeof(tallyBoxUserConfig_t);
  c.versionOfConfiguration = TALLYBOX_CONFIGURATION_VERSION;

  c.cameraId = TALLYBOX_CONFIGURATION_DEFAULT_CAMERA_ID;
  c.greenBrightnessPercent = DEFAULT_BRIGHTNESS;
  c.redBrightnessPercent = DEFAULT_BRIGHTNESS;
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
  Serial.println(" - Green Brightness   = "+String(c.greenBrightnessPercent));
  Serial.println(" - Red Brightness     = "+String(c.redBrightnessPercent));
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
      if(c.checkSum == calcChecksum(c))
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
  Serial.println("configurationGet(): opening file '"+String(fileName)+"'\r\n");

  File f = filesystem->open(fileName, "r");

  if(f.size() > 0)
  {
    Serial.printf("configurationGet(): size=%u\r\n", f.size());

    if(f.read((uint8_t*)&c, sizeof(T)) == sizeof(T))
    {
      Serial.printf("configurationGet(): successfully read data, validating... ");
      ret = validateConfiguration(c);
      Serial.printf("ret = %s\r\n", ret?"true":"false");
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
    Serial.printf("configurationPut(): opening file '%s'\r\n", fileName);
    
    File f = filesystem->open(fileName, "w");

    if(f.write((uint8_t*)&c, sizeof(T)) == sizeof(T))
    {
      Serial.printf("configurationPut(): successfully wrote data\r\n");
      ret = true;
    }
    else
    {
      Serial.printf("configurationPut(): write failed\r\n");
    }
  }
  return ret;
}

template <typename T>
void handleConfigurationWrite(T& c)
{
#if TALLYBOX_PROGRAM_WRITE_DEFAULTS  
  const char* fName = getFileName(c);
  Serial.printf("TALLYBOX_PROGRAM_WRITE_DEFAULTS enabled\r\nhandleConfigurationWrite('%s')\r\n", fName);

  setDefaults(c);

  dumpConf("default settings", c);

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
    if(configurationPut(c))
    {
      Serial.println("Written successfully!");
    } 
    else
    {
      Serial.println("Write FAILED!");
    }
  }
  else
  {
    Serial.println("<not written as the existing content was identical>");
  }  
#endif
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
    Serial.printf("Loading configuration '%s' failed, cannot continue!\r\n", fName);
    while(1);
  }
}


void tallyBoxConfiguration(tallyBoxConfig_t& c)
{
  /*Note: both functions are blocking. If the writing or reading fails, the program stops and will wait for reset*/

  Serial.println("************ NETWORK CONFIGURATION ****");

  /*handle network configuration part*/
  handleConfigurationWrite(c.network);
  handleConfigurationRead(c.network);

  Serial.println("************ USER CONFIGURATION ****");

  /*handle user configuration part*/
  handleConfigurationWrite(c.user);
  handleConfigurationRead(c.user);

  Serial.println("************ DONE ****");
}

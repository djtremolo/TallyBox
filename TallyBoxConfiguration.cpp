#include "TallyBoxConfiguration.hpp"
#include <Arduino_CRC32.h>

void setDefaults(tallyBoxConfig_t& c)
{
  c.sizeOfConfiguration = sizeof(tallyBoxConfig_t);
  c.versionOfConfiguration = TALLYBOX_CONFIGURATION_VERSION;

  c.cameraId = 0;
  IPAddress ip(TALLYBOX_CONFIGURATION_DEFAULT_HOSTIP_BYTE0,
                TALLYBOX_CONFIGURATION_DEFAULT_HOSTIP_BYTE1,
                TALLYBOX_CONFIGURATION_DEFAULT_HOSTIP_BYTE2,
                TALLYBOX_CONFIGURATION_DEFAULT_HOSTIP_BYTE3);
  c.hostAddressU32 = ip.v4();
  strcpy(c.wifiSSID, TALLYBOX_CONFIGURATION_DEFAULT_SSID);
  strcpy(c.wifiPasswd, TALLYBOX_CONFIGURATION_DEFAULT_PASSWD);

  c.checkSum = calcChecksum(c);
}

uint32_t calcChecksum(tallyBoxConfig_t& c)
{
  Arduino_CRC32 crc32;
  uint16_t lenToCheck = sizeof(tallyBoxConfig_t) - sizeof(c.checkSum);
  uint8_t *buf = (uint8_t*)&c;

  return crc32.calc(buf, lenToCheck);
}

bool validateConfiguration(tallyBoxConfig_t& c)
{
  bool ret = false;
  if(c.versionOfConfiguration == TALLYBOX_CONFIGURATION_VERSION)
  {
    if(c.sizeOfConfiguration == sizeof(tallyBoxConfig_t))
    {
      if(c.checkSum == calcChecksum(c))
      {
        ret = true;
      }
    }
  }
  return ret;
}

bool configurationGet(tallyBoxConfig_t& c)
{
  bool ret;

  EEPROM.get(TALLYBOX_EEPROM_STORAGE_ADDRESS, c);
  ret = validateConfiguration(c);
}

bool configurationPut(tallyBoxConfig_t& c)
{
  bool ret = false;

  if(validateConfiguration(c))
  {
    EEPROM.put(TALLYBOX_EEPROM_STORAGE_ADDRESS, c);
    EEPROM.commit();
    ret = true;
  }
  return ret;
}

void handleConfigurationWrite(tallyBoxConfig_t& c)
{
#if TALLYBOX_PROGRAM_EEPROM
  Serial.println("Programming EEPROM...");

  setDefaults(c);

  /*avoid overwriting the data if it is already identical*/
  tallyBoxConfig_t readConf;
  bool avoidWriting = false;
  configurationGet(readConf);
  if(validateConfiguration(readConf))
  {
    if(memcmp((void*)&c, (void*)&readConf, sizeof(tallyBoxConfig_t)) == 0)
    {
      /*identical!*/
      avoidWriting = true;
    }
  }

  dumpConf("writing default data", c);

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

  Serial.println("Blocked after writing, waiting for reset.");
  while(1);
#endif
}

void handleConfigurationRead(tallyBoxConfig_t& c)
{
  configurationGet(c);
  if(validateConfiguration(c))
  {
    dumpConf(String("read from EEPROM"), c);
  }
  else
  {
    Serial.println("Loading configuration failed, cannot continue!");
    while(1);
  }
}

void dumpConf(String confName, tallyBoxConfig_t& c)
{
  Serial.println("Configuration: '" + confName + "'.");
  Serial.println(" - Version =     "+String(c.versionOfConfiguration));
  Serial.println(" - Size    =     "+String(c.sizeOfConfiguration));
  Serial.println(" - CameraId =    "+String(c.cameraId));
  Serial.print(" - HostAddress = ");
  IPAddress ip(c.hostAddressU32);
  Serial.println(ip);
  Serial.print(" - WifiSSID =    ");
  Serial.println(c.wifiSSID);
  Serial.println(" - Password =    <not shown>");
  Serial.print(" - Checksum =    0x");
  Serial.println(c.checkSum, HEX);
}

void tallyBoxConfiguration(tallyBoxConfig_t& c)
{
  /*Note: both functions are blocking. If the writing or reading fails, the program stops and will wait for reset*/
  handleConfigurationWrite(c);
  handleConfigurationRead(c);
}

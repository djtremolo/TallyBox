#include "ESP8266WiFi.h"
#include "Arduino.h"
#include <WiFiUdp.h>
#include "TallyBoxPeerNetwork.hpp"

WiFiUDP Udp;

static uint16_t peerNetworkSerialize(uint16_t& grn, uint16_t& red, uint8_t *buf, uint16_t maxLen);
static bool peerNetworkDeSerialize(uint16_t& grn, uint16_t& red, uint8_t *buf, uint16_t len);

static uint16_t peerNetworkSerialize(uint16_t& grn, uint16_t& red, uint8_t *buf, uint16_t maxLen)
{
  uint16_t ret = 0;
  if(maxLen >= 4)
  {
    uint16_t idx = 0;
    buf[idx++] = (uint8_t)(grn >> 8) & 0x00ff;
    buf[idx++] = (uint8_t)(grn) & 0x00ff;
    buf[idx++] = (uint8_t)(red >> 8) & 0x00ff;
    buf[idx++] = (uint8_t)(red) & 0x00ff;
    ret = idx;
  }
  return ret;
}

static bool peerNetworkDeSerialize(uint16_t& grn, uint16_t& red, uint8_t *buf, uint16_t len)
{
  bool ret = false;
  if(len == 4)
  {
    grn = (uint16_t)buf[0];
    grn <<= 8;
    grn |= (uint16_t)buf[1];

    red = (uint16_t)buf[2];
    red <<= 8;
    red |= (uint16_t)buf[3];

    ret = true;
  }
  return ret;
}


void peerNetworkSend(tallyBoxConfig_t& c, uint16_t greenChannel, uint16_t redChannel)
{
  uint8_t buf[4];

  uint16_t bufLen = peerNetworkSerialize(greenChannel, redChannel, buf, sizeof(buf));
  if(bufLen > 0)
  {
    Udp.beginPacket(IPAddress(0,0,0,0), 7493);
    Udp.write(buf, bufLen);
    Udp.endPacket();
  }

}

bool peerNetworkReceive(uint16_t& greenChannel, uint16_t& redChannel)
{
  bool ret = false;

  int packetSize = Udp.parsePacket();
  if(packetSize)
  {
    uint8_t buf[255];
    int len = Udp.read(buf, 255);
    uint16_t tmpGreen;
    uint16_t tmpRed;
    
    if(peerNetworkDeSerialize(tmpGreen, tmpRed, buf, packetSize))
    {
      greenChannel = tmpGreen;
      redChannel = tmpRed;

      ret = true;
    }
  }
  return ret;
}


void peerNetworkInitialize(uint16_t localPort)
{
  Udp.begin(localPort);  
}


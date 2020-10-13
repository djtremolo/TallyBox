#include "ESP8266WiFi.h"
#include "Arduino.h"
#include <WiFiUdp.h>
#include "TallyBoxPeerNetwork.hpp"
#include "TallyBoxInfra.hpp"
#include <Arduino_CRC32.h>

#define PEERNETWORK_PROTOCOL_VERSION_U8                  1
#define PEERNETWORK_PROTOCOL_IDENTIFIER_U32             0x7A61696D
#define PEERNETWORK_TALLY_BROADCAST_IDENTIFIER_U16      0x0001

WiFiUDP Udp;

/*** INTERNAL FUNCTIONS **************************************/
static uint16_t peerNetworkSerialize(uint16_t& grn, uint16_t& red, uint8_t *buf, uint16_t maxLen);
static bool peerNetworkDeSerialize(uint16_t& grn, uint16_t& red, uint8_t *buf, uint16_t len);
/*************************************************************/


static void syncLocalTick(uint16_t tick)
{
  if(tick == 0)
  {
    int32_t localCurrentTick = (int32_t)getCurrentTick(true);
    int32_t masterCurrentTick = (int32_t)tick;
    int32_t comp = 0;

    if(masterCurrentTick == localCurrentTick)
    {
      comp = 0;
    }
    else if(masterCurrentTick > localCurrentTick)
    {
      comp = (masterCurrentTick-localCurrentTick); /*we are in the past -> positive compensation*/
    }
    else
    {
      comp = (masterCurrentTick-localCurrentTick); /*we are in the future -> negative compensation*/
    }
    setTickCompensationValue(comp);
  }
}


static void putU8(uint8_t** bufPtr, uint8_t value)
{
  *((*bufPtr)++) = value;
}

static uint8_t getU8(uint8_t** bufPtr)
{
  uint8_t value = *(*bufPtr);
  (*bufPtr)++;
  return value;
}

static void putU16(uint8_t** bufPtr, uint16_t value)
{
  putU8(bufPtr, (value>>8)&0x00FF);
  putU8(bufPtr, value&0x00FF);
}

static uint16_t getU16(uint8_t** bufPtr)
{
  uint16_t val;

  val = getU8(bufPtr);
  val <<= 8;
  val |= getU8(bufPtr);

  return val;
}

static void putU32(uint8_t** bufPtr, uint32_t value)
{
  putU8(bufPtr, (value>>24)&0x000000FF);
  putU8(bufPtr, (value>>16)&0x000000FF);
  putU8(bufPtr, (value>>8)&0x000000FF);
  putU8(bufPtr, value&0x000000FF);
}

static uint32_t getU32(uint8_t** bufPtr)
{
  uint32_t val;

  val = getU8(bufPtr);
  val <<= 8;
  val |= getU8(bufPtr);
  val <<= 8;
  val |= getU8(bufPtr);
  val <<= 8;
  val |= getU8(bufPtr);

  return val;
}

static uint16_t getBufLength(uint8_t* ptrFirst, uint8_t* ptrAfterLast)
{
  uint16_t len = (uint16_t)(((size_t)(ptrAfterLast))-((size_t)(ptrFirst)));
  return len;
}

static uint16_t peerNetworkSerialize(uint16_t& grn, uint16_t& red, uint8_t *buf, uint16_t maxLen)
{
  uint16_t ret = 0;
  const uint16_t headerSize = 9;
  const uint16_t footerSize = 4;
  const uint16_t msgSize = headerSize + 2 + 2 + footerSize;
  Arduino_CRC32 crc32;

  if(maxLen >= msgSize)
  {
    uint8_t *p = buf;
    uint16_t myTick = getCurrentTick();

    /*header*/
    putU32(&p, PEERNETWORK_PROTOCOL_IDENTIFIER_U32);
    putU8(&p, PEERNETWORK_PROTOCOL_VERSION_U8);
    putU16(&p, PEERNETWORK_TALLY_BROADCAST_IDENTIFIER_U16);
    putU16(&p, myTick);

    /*payload*/
    putU16(&p, grn);
    putU16(&p, red);

    /*crc*/
    uint16_t lenWithoutCrc = getBufLength(buf, p);
    uint32_t crc = crc32.calc(buf, lenWithoutCrc);

    putU32(&p, crc);

    ret = lenWithoutCrc + 4;
  }

  return ret;
}

static bool peerNetworkDeSerialize(uint16_t& grn, uint16_t& red, uint8_t *buf, uint16_t len)
{
  bool ret = false;
  const uint16_t headerSize = 9;
  const uint16_t footerSize = 4;
  const uint16_t msgSize = headerSize + 2 + 2 + footerSize;
  Arduino_CRC32 crc32;
  uint16_t masterTick;
  
  if(len == msgSize)
  {
    uint8_t *p = buf;

    /*check incoming header*/
    if(getU32(&p) == PEERNETWORK_PROTOCOL_IDENTIFIER_U32)
    {
      if(getU8(&p) == PEERNETWORK_PROTOCOL_VERSION_U8)
      {
        if(getU16(&p) == PEERNETWORK_TALLY_BROADCAST_IDENTIFIER_U16)
        {
          masterTick = getU16(&p);

          /*header check passed, handle payload*/
          grn = getU16(&p);
          red = getU16(&p);


          /*crc check*/
          uint16_t lenWithoutCrc = getBufLength(buf, p);
          uint32_t calculatedCrc = crc32.calc(buf, lenWithoutCrc);
          uint32_t receivedCrc = getU32(&p);

          if(receivedCrc == calculatedCrc)
          {
            /*successfully handled message!*/
            /*provide basis for local time concept*/
            syncLocalTick(masterTick);
            ret = true;
          }
          else
          {
            Serial.print("PeerNetwork: CRC failure in reception. Received: 0x");
            Serial.print(receivedCrc, HEX);
            Serial.print(", Calculated: 0x");
            Serial.println(calculatedCrc, HEX);

          }
        }
        else
        {
          Serial.println("PeerNetwork: Unknown message identifier");
        }
      }
      else
      {
        Serial.println("PeerNetwork: Unknown protocol version");
      }
    }
    else
    {
      Serial.println("PeerNetwork: Unknown protocol");
    }
  }
  else
  {
    Serial.println("PeerNetwork: Illegal message length");
  }
  return ret;
}

void peerNetworkSend(tallyBoxConfig_t& c, uint16_t greenChannel, uint16_t redChannel)
{
  uint8_t buf[32];

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
    uint8_t buf[32];
    int len = Udp.read(buf, 32);
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


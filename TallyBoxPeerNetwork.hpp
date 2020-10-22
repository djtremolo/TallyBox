#ifndef __TALLYBOXPEERNETWORK_HPP__
#define __TALLYBOXPEERNETWORK_HPP__
#include "Arduino.h"
#include <WiFiUdp.h>
#include "TallyBoxConfiguration.hpp"

void peerNetworkInitialize(uint16_t localPort);
void peerNetworkSend(tallyBoxConfig_t& c, uint16_t greenChannel, uint16_t redChannel, bool inTransition);
bool peerNetworkReceive(uint16_t& greenChannel, uint16_t& redChannel, bool& inTransition);

#endif
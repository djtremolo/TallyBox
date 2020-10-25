# TallyBox

## Overview
The TallyBox unit that is attached to each video camera in multi-camera setup. The TallyBox provides a visual signal for both camera operator and the actor(s) of the scene. The TallyBox has three main states:
- Dark: the camera in question is not being used in program
- Green: the camera in question is on preview state, i.e. it will most probably be the next camera that will be used in program. This signal informs the camera operator to prepare for a steady shot.
- Red: this camera is currently used in program. The camera operator shall keep the shot steady. For the use cases, where the actor(s) benefit from it, this signal provides the awareness of which camera is the active one.

## Usage
The TallyBox communicates with an ATEM-compatible video switcher, which provides the status of the currently used camera channel. The communication uses WiFi connection, i.e. it requires an access point that is on the same network where the switcher is.

## System set-up

## User interfaces

## Configuration

## Tested system

## Security
Briefly, TallyBox does not implement *any* security practices. It relies completely on the isolated network concept, preferably with no Internet connection at all. It is worth of noting that if the network is publicly available, the security concern is not limited to TallyBox devices, as such network would expose direct access to the ATEM switcher as well.

# Software
## Architectural overview
### TallyBox.ino

### TallyBoxConfiguration

### TallyBoxInfra

### TallyBoxOutput

### TallyBoxPeerNetwork

### TallyBoxStateMachine

### TallyBoxTerminal

### TallyBoxWebServer

## Third-party libraries

### Basic Arduino framework
https://www.arduino.cc/

### Arduino_CRC32
https://github.com/arduino-libraries/Arduino_CRC32

### ArduinoJson
https://arduinojson.org/

### ArduinoOTA
https://github.com/jandrassy/ArduinoOTA

### SKAARHOJ Arduino Libraries for ATEM
Used ATEMbase and ATEMstd from:
https://github.com/kasperskaarhoj/SKAARHOJ-Open-Engineering/tree/master/ArduinoLibs


# Hardware
tbd

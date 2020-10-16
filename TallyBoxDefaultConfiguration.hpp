#if 1
#define TALLYBOX_CONFIGURATION_DEFAULT_SSID             "myTallyNetSSID"        /*customize*/
#define TALLYBOX_CONFIGURATION_DEFAULT_PASSWD           ""                      /*customize*/
#else
#include "myPrivateCredentials.hpp"
#endif

#define TALLYBOX_CONFIGURATION_DEFAULT_HOSTIP_BYTE0     192
#define TALLYBOX_CONFIGURATION_DEFAULT_HOSTIP_BYTE1     168
#define TALLYBOX_CONFIGURATION_DEFAULT_HOSTIP_BYTE2     1
#define TALLYBOX_CONFIGURATION_DEFAULT_HOSTIP_BYTE3     100

#define TALLYBOX_CONFIGURATION_DEFAULT_CAMERA_ID        1

#define TALLYBOX_PROGRAM_EEPROM                         0       /*enable this for writing the default values to EEPROM, disable for normal operation*/


#if 1
#define TALLYBOX_CONFIGURATION_DEFAULT_SSID             "myTallyNetSSID"        /*customize*/
#define TALLYBOX_CONFIGURATION_DEFAULT_PASSWD           "myTallyNetPassword"    /*customize*/
#else
#include "myPrivateCredentials.hpp"
#endif

#define TALLYBOX_CONFIGURATION_DEFAULT_HOSTIP          "192.168.1.100"
#define TALLYBOX_CONFIGURATION_DEFAULT_OWNIP           "192.168.1.90"
#define TALLYBOX_CONFIGURATION_DEFAULT_SUBNET          "255.255.255.0"
#define TALLYBOX_CONFIGURATION_DEFAULT_GATEWAY         "192.168.1.254"

#define TALLYBOX_CONFIGURATION_DEFAULT_HASOWNIP        false

#define TALLYBOX_CONFIGURATION_DEFAULT_CAMERA_ID       1
#define TALLYBOX_CONFIGURATION_DEFAULT_ISMASTER        (TALLYBOX_CONFIGURATION_DEFAULT_CAMERA_ID==1)

#define TALLYBOX_PROGRAM_FORCE_WRITE_DEFAULTS          0       /*enable this for writing the default values to network config file, disable for normal operation*/


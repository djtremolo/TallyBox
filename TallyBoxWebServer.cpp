#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <FS.h>
#include <LittleFS.h>
#include "TallyBoxWebServer.hpp"
#include "TallyBoxOutput.hpp"
#include <malloc.h>
#include <math.h>

static FS* filesystem = &LittleFS;

extern const char* TallyboxFirmwareVersion;

#define DBG_OUTPUT_PORT Serial

static ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

static char *myIndexHtmBuf = NULL;
static char *myRestartHtmBuf = NULL;
static char *myConfigNetworkHtmBuf = NULL;
static char *myConfigUserHtmBuf = NULL;

//holds the current upload
File fsUploadFile;


static bool initialized = false;

String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

String getContentType(String filename) {
  if (server.hasArg("download")) {
    return "application/octet-stream";
  } else if (filename.endsWith(".htm")) {
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".json")) {
    return "application/json";
  } else if (filename.endsWith(".jsn")) {
    return "text/plain";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  }
  return "text/plain";
}


void handleFileCreate() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  String path = server.arg(0);
  DBG_OUTPUT_PORT.println("handleFileCreate: " + path);
  if (path == "/") {
    return server.send(500, "text/plain", "BAD PATH");
  }
  if (filesystem->exists(path)) {
    return server.send(500, "text/plain", "FILE EXISTS");
  }
  File file = filesystem->open(path, "w");
  if (file) {
    file.close();
  } else {
    return server.send(500, "text/plain", "CREATE FAILED");
  }
  server.send(200, "text/plain", "");
  path = String();
}
void handleFileList() {
  if (!server.hasArg("dir")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = server.arg("dir");
  DBG_OUTPUT_PORT.println("handleFileList: " + path);
  Dir dir = filesystem->openDir(path);
  path = String();

  String output = "[";
  while (dir.next()) {
    File entry = dir.openFile("r");
    if (output != "[") {
      output += ',';
    }
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    if (entry.name()[0] == '/') {
      output += &(entry.name()[1]);
    } else {
      output += entry.name();
    }
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}
bool handleFileRead(String path) {
  DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if (path.endsWith("/")) {
    path += "index.htm";
  }
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (filesystem->exists(pathWithGz) || filesystem->exists(path)) {
    if (filesystem->exists(pathWithGz)) {
      path += ".gz";
    }
    File file = filesystem->open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

bool getRawHtm(const char* fileName, char *bufContent, size_t maxLen)
{
  bool dataValid = false;
  static size_t bufLen = 0;

  if(filesystem->exists(fileName))
  {
    File file = filesystem->open(fileName, "r");

    bufLen = file.readBytes(bufContent, maxLen);
    if(bufLen > 0)
    {
      bufContent[bufLen] = 0; /*terminate*/
      dataValid = true;
    }
    file.close();
  }

  return dataValid;
}


#define MAX_HTML_FILE_SIZE    4000

bool handleIndexHtm(tallyBoxConfig_t& c, ESP8266WebServer& s, bool useServerArgs) {
  static bool fileHasBeenRead = false;
  char *bufContent = myIndexHtmBuf;
  String path = s.uri();

  DBG_OUTPUT_PORT.println("handleIndexHtm: " + path);
  if (path.endsWith("/")) {
    path += "index.htm";
  }

  String contentType = getContentType(path);
  if(!fileHasBeenRead)
  {
    if(!getRawHtm("index.htm", bufContent, MAX_HTML_FILE_SIZE))
    {
      Serial.println("file access failed");
      return false;
    }
    fileHasBeenRead = true;
  }

  /*allocate buffer for final html*/
  char *myBuf = (char*)malloc(MAX_HTML_FILE_SIZE);

  if(myBuf)
  {
    snprintf(myBuf, MAX_HTML_FILE_SIZE, bufContent, 
            TallyboxFirmwareVersion
            );

    server.send(200, "text/html", myBuf);

    free(myBuf);
  }
  else
  {
    Serial.println("malloc failed");
    server.send(404, "text/html", "malloc failed");
  }

  return true;
}

bool handleConfigUserHtm(tallyBoxConfig_t& c, ESP8266WebServer& s, bool useServerArgs) {
  //char bufContent[MAX_HTML_FILE_SIZE] = "";
  char *bufContent = myConfigUserHtmBuf;
  String path = s.uri();
  bool validated = false;
  String userFeedback = "";
  static bool fileHasBeenRead = false;

  DBG_OUTPUT_PORT.println("handleConfigUserHtm: " + path);
  if (path.endsWith("/")) {
    path += "config_user.htm";
  }

  String contentType = getContentType(path);

  if(!fileHasBeenRead)
  {
    if(!getRawHtm("config_user.htm", bufContent, MAX_HTML_FILE_SIZE))
    {
      Serial.println("file access failed");
      return false;
    }
    fileHasBeenRead = true;
  }

  if(useServerArgs)
  {
    c.user.cameraId = (uint16_t)(server.arg("cameraId").toInt());

    tallyBoxOutput_t ch = OUTPUT_NONE;
    float oldGreen = c.user.greenBrightnessPercent;
    float oldRed = c.user.redBrightnessPercent;

    c.user.greenBrightnessPercent = round(server.arg("greenBrightnessPercent").toFloat());
    c.user.redBrightnessPercent = round(server.arg("redBrightnessPercent").toFloat());

    if(c.user.greenBrightnessPercent != oldGreen)
    {
      ch = OUTPUT_GREEN;
    }
    if(c.user.redBrightnessPercent != oldRed)
    {
      if(ch == OUTPUT_GREEN)
      {
        ch = OUTPUT_LINKED;
      }
      else
      {
        ch = OUTPUT_RED;
      }
    }

    setBrightnessSettingMode(ch, (ch != OUTPUT_NONE));

    if(server.arg("verify") == "Verify")
    {
      userFeedback += "Verified. The configuration can now be stored.";
      validated = true;
    }

    if(server.arg("store") == "Store parameters")
    {
      if(tallyBoxWriteConfiguration(c.user))
      {
        /*success!*/
        userFeedback += "Configuration stored successfully!";
      }
      else
      {
        userFeedback += "Configuration storing FAILED!";
      }
    }
  }

  /*allocate buffer for final html*/
  char *myBuf = (char*)malloc(MAX_HTML_FILE_SIZE);

  if(myBuf)
  {
    snprintf(myBuf, MAX_HTML_FILE_SIZE, bufContent, 
            TallyboxFirmwareVersion,
            c.user.cameraId,
            (uint16_t)round(c.user.greenBrightnessPercent),
            (c.network.isMaster ? "enabled" : "disabled"),
            (uint16_t)round(c.user.redBrightnessPercent),
            (c.network.isMaster ? "enabled" : "disabled"),
            (validated?"enabled":"disabled"),
            userFeedback.c_str()
            );

    server.send(200, "text/html", myBuf);

    free(myBuf);
  }
  else
  {
    Serial.println("malloc failed");
    server.send(404, "text/html", "malloc failed");
  }
  return true;
}


bool handleConfigNetworkHtm(tallyBoxConfig_t& c, ESP8266WebServer& s, bool useServerArgs) {
  char *bufContent = myConfigNetworkHtmBuf;
  String path = s.uri();
  bool validated = false;
  String userFeedback = "";

  DBG_OUTPUT_PORT.println("handleConfigNetworkHtm: " + path);
  if (path.endsWith("/")) {
    path += "config_network.htm";
  }

  String contentType = getContentType(path);
  if(!getRawHtm("config_network.htm", bufContent, MAX_HTML_FILE_SIZE))
  {
    return false;
  }
  
  if(useServerArgs)
  {
    strlcpy(c.network.wifiSSID, server.arg("wifiSSID").c_str(), sizeof(c.network.wifiSSID));
    strlcpy(c.network.wifiPasswd, server.arg("wifiPassword").c_str(), sizeof(c.network.wifiPasswd));

    c.network.isMaster = (server.arg("connectionType") == "master");

    String ha = server.arg("hostAddress");
    c.network.hostAddress.fromString(ha);

    c.network.hasStaticIp = (server.arg("staticIpInUse") == "staticIpInUse");

    if(c.network.hasStaticIp)
    {
      String oa = server.arg("ownAddress");
      c.network.ownAddress.fromString(oa);

      String snm = server.arg("subnetMask");
      c.network.subnetMask.fromString(snm);

      String dg = server.arg("defaultGateway");
      c.network.defaultGateway.fromString(dg);
    }


    Serial.println("MDNSHOST = '"+server.arg("mDnsHostName")+"'");


    String hostName = server.arg("mDnsHostName");
    strlcpy(c.network.mdnsHostName, hostName.c_str(), sizeof(c.network.mdnsHostName));

    if(server.arg("verify") == "Verify")
    {
      userFeedback += "Verified. The configuration can now be stored.";
      validated = true;
    }

    if(server.arg("store") == "Store parameters")
    {
      if(tallyBoxWriteConfiguration(c.network))
      {
        /*success!*/
        userFeedback += "Configuration stored successfully!";
      }
      else
      {
        userFeedback += "Configuration storing FAILED!";
      }
    }
  }

  /*allocate buffer for final html*/
  char *myBuf = (char*)malloc(MAX_HTML_FILE_SIZE);

  if(myBuf)
  {
    snprintf(myBuf, MAX_HTML_FILE_SIZE, bufContent, 
            TallyboxFirmwareVersion,
            c.network.wifiSSID,
            c.network.wifiPasswd,
            (c.network.isMaster ? "checked" : ""),
            (c.network.isMaster ? "" : "checked"),
            c.network.hostAddress.toString().c_str(),
            (c.network.isMaster ? "enabled" : "disabled"),
            (c.network.hasStaticIp ? "checked" : ""),
            (c.network.hasStaticIp ? 
                c.network.ownAddress.toString().c_str() 
                : WiFi.localIP().toString().c_str()),
            (c.network.hasStaticIp ? "enabled" : "disabled"),
            (c.network.hasStaticIp ? 
                c.network.subnetMask.toString().c_str() 
                : WiFi.subnetMask().toString().c_str()),
            (c.network.hasStaticIp ? "enabled" : "disabled"),
            (c.network.hasStaticIp ? 
                c.network.defaultGateway.toString().c_str() 
                : WiFi.gatewayIP().toString().c_str()),
            (c.network.hasStaticIp ? "enabled" : "disabled"),
            c.network.mdnsHostName,
            (validated ? "enabled" : "disabled"),
            userFeedback.c_str()
            );

    server.send(200, "text/html", myBuf);
    free(myBuf);
  }
  else
  {
    Serial.println("malloc failed");
    server.send(404, "text/html", "malloc failed");
  }

  return true;
}













bool handleRestartHtm(tallyBoxConfig_t& c, ESP8266WebServer& s, bool useServerArgs) {
  char *bufContent = myRestartHtmBuf;
  String path = s.uri();
  bool restartEnabled = false;
  String userFeedback = "";

  DBG_OUTPUT_PORT.println("handleRestartHtm: " + path);
  if (path.endsWith("/")) {
    path += "restart.htm";
  }

  String contentType = getContentType(path);
  if(!getRawHtm("restart.htm", bufContent, MAX_HTML_FILE_SIZE))
  {
    return false;
  }
  
  if(useServerArgs)
  {
    restartEnabled = (server.arg("action") == "restart");

    if(server.arg("submit") == "Submit")
    {
      if(restartEnabled)
      {
        Serial.println("*******************************************");
        Serial.println("*******  RESTART  *************************");
        Serial.println("*******************************************");
        server.send(200, "text/html", "<html>Restarting.... wait 30 seconds before reconnecting</html>");
        delay(1000);
        ESP.restart();
      }
    }
  }

  /*allocate buffer for final html*/
  char *myBuf = (char*)malloc(MAX_HTML_FILE_SIZE);

  if(myBuf)
  {
    snprintf(myBuf, MAX_HTML_FILE_SIZE, bufContent, 
            TallyboxFirmwareVersion,
            (restartEnabled ? "checked" : ""),
            (restartEnabled ? "" : "checked"),
            "enabled"
            );

    server.send(200, "text/html", myBuf);
    free(myBuf);
  }
  else
  {
    Serial.println("malloc failed");
    server.send(404, "text/html", "malloc failed");
  }

  return true;
}














void handleFileUpload() {
  if (server.uri() != "/edit") {
    return;
  }
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    DBG_OUTPUT_PORT.print("handleFileUpload Name: "); DBG_OUTPUT_PORT.println(filename);
    fsUploadFile = filesystem->open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
    }
    DBG_OUTPUT_PORT.print("handleFileUpload Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}
void handleFileDelete() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  String path = server.arg(0);
  DBG_OUTPUT_PORT.println("handleFileDelete: " + path);
  if (path == "/") {
    return server.send(500, "text/plain", "BAD PATH");
  }
  if (!filesystem->exists(path)) {
    return server.send(404, "text/plain", "FileNotFound");
  }
  filesystem->remove(path);
  server.send(200, "text/plain", "");
  path = String();
}




void tallyBoxWebServerInitialize(tallyBoxConfig_t& c)
{

  if((myIndexHtmBuf = (char*)malloc(MAX_HTML_FILE_SIZE)) == NULL)
  {
    Serial.println("malloc failed: myIndexHtmBuf");
    return;
  }
  if((myRestartHtmBuf = (char*)malloc(MAX_HTML_FILE_SIZE)) == NULL)
  {
    Serial.println("malloc failed: myRestartHtmBuf");
    return;
  }
  if((myConfigNetworkHtmBuf = (char*)malloc(MAX_HTML_FILE_SIZE)) == NULL)
  {
    Serial.println("malloc failed: myConfigNetworkHtmBuf");
    return;
  }
  if((myConfigUserHtmBuf = (char*)malloc(MAX_HTML_FILE_SIZE)) == NULL)
  {
    Serial.println("malloc failed: myConfigUserHtmBuf");
    return;
  }

  Dir dir = filesystem->openDir("/");
  while (dir.next()) 
  {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
  }
  DBG_OUTPUT_PORT.printf("\n");

  DBG_OUTPUT_PORT.print("Open http://");
  DBG_OUTPUT_PORT.print(c.network.mdnsHostName);
  DBG_OUTPUT_PORT.println(".local/edit to see the file browser");

  //SERVER INIT
  //list directory
  server.on("/list", HTTP_GET, handleFileList);


  server.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.htm")) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {
    server.send(200, "text/plain", "");
  }, handleFileUpload);

  server.on("/", HTTP_GET, [&]() {
    if (!handleIndexHtm(c, server, false)) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  server.on("/index.htm", HTTP_GET, [&]() {
    if (!handleIndexHtm(c, server, false)) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  server.on("/index.htm", HTTP_POST, [&]() {
    if (!handleIndexHtm(c, server, true)) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });


  server.on("/config_user.htm", HTTP_GET, [&]() {
    if (!handleConfigUserHtm(c, server, false)) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  server.on("/config_user.htm", HTTP_POST, [&]() {
    if (!handleConfigUserHtm(c, server, true)) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  server.on("/config_network.htm", HTTP_GET, [&]() {
    if (!handleConfigNetworkHtm(c, server, false)) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  server.on("/config_network.htm", HTTP_POST, [&]() {
    if (!handleConfigNetworkHtm(c, server, true)) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  server.on("/restart.htm", HTTP_GET, [&]() {
    if (!handleRestartHtm(c, server, false)) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  server.on("/restart.htm", HTTP_POST, [&]() {
    if (!handleRestartHtm(c, server, true)) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });


  server.on("/config_network.json", HTTP_GET, []() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  server.on("/config_network.json", HTTP_POST, []() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([]() {
    Serial.println("not found, trying from FS");
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/all", HTTP_GET, []() {
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"analog\":" + String(analogRead(A0));
    json += ", \"gpio\":" + String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });


  httpUpdater.setup(&server);

  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");

  initialized = true;
}

void tallyBoxWebServerUpdate()
{
  if(!initialized)
  {
      return;
  }

  server.handleClient();
}

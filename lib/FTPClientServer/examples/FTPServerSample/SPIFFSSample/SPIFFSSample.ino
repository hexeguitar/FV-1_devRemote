/*
   This is an example sketch to show the use of the espFTP server.

   Please replace
     YOUR_SSID and YOUR_PASS
   with your WiFi's values and compile.

   If you want to see debugging output of the FTP server, please
   select select an Serial Port in the Arduino IDE menu Tools->Debug Port

   Send L via Serial Monitor, to display the contents of the FS
   Send F via Serial Monitor, to fromat the FS

   This example is provided as Public Domain
   Daniel Plasa <dplasa@gmail.com>

*/
#if (defined ESP32)
#include <WiFi.h>
#include <SPIFFS.h>
#define BAUDRATE 115200
#elif (defined ESP8266)
#include <ESP8266WiFi.h>
#include <FS.h>
#define BAUDRATE 74880
#endif

#include <FTPServer.h>

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASS";

// Since SPIFFS is becoming deprecated but might still be in
// use in your Projects, tell the FtpServer to use SPIFFS
FTPServer ftpSrv(SPIFFS);

void setup(void)
{
  Serial.begin(BAUDRATE);
  WiFi.begin(ssid, password);

  bool fsok = SPIFFS.begin();
  Serial.printf_P(PSTR("FS init: %s\n"), fsok ? PSTR("ok") : PSTR("fail!"));

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.printf_P(PSTR("."));
  }
  Serial.printf_P(PSTR("\nConnected to %s, IP address is %s\n"), ssid, WiFi.localIP().toString().c_str());

  // setup the ftp server with username and password
  // ports are defined in FTPCommon.h, default is
  //   21 for the control connection
  //   50009 for the data connection (passive mode by default)
  ftpSrv.begin(F("ftp"), F("ftp"));
}

enum consoleaction
{
  show,
  wait,
  format,
  list
};

consoleaction action = show;

void loop(void)
{
  // this is all you need
  // make sure to call handleFTP() frequently
  ftpSrv.handleFTP();

  //
  // Code below just for debugging in Serial Monitor
  //
  if (action == show)
  {
    Serial.printf_P(PSTR("Enter 'F' to format, 'L' to list the contents of the FS\n"));
    action = wait;
  }
  else if (action == wait)
  {
    if (Serial.available())
    {
      char c = Serial.read();
      if (c == 'F')
        action = format;
      else if (c == 'L')
        action = list;
      else if (!(c == '\n' || c == '\r'))
        action = show;
    }
  }
  else if (action == format)
  {
    uint32_t startTime = millis();
    SPIFFS.format();
    Serial.printf_P(PSTR("FS format done, took %lu ms!\n"), millis() - startTime);
    action = show;
  }
  else if (action == list)
  {
    Serial.printf_P(PSTR("Listing contents...\n"));
    uint16_t dirCount = ListDir("/");
    Serial.printf_P(PSTR("%d files total\n"), dirCount);
    action = show;
  }
}

#if (defined ESP8266)
uint16_t ListDir(const char *path)
{
  uint16_t dirCount = 0;
  Dir dir = SPIFFS.openDir(path, "r");
  while (dir.next())
  {
    ++dirCount;
    Serial.printf_P(PSTR("%6ld  %s\n"), (uint32_t)dir.fileSize(), dir.fileName().c_str());
  }
  return dirCount;
}
#elif (defined ESP32)
uint16_t ListDir(const char *path)
{
  uint16_t dirCount = 0;
  File root = SPIFFS.open(path);
  if (!root)
  {
    Serial.println(F("failed to open root"));
    return 0;
  }
  if (!root.isDirectory())
  {
    Serial.println(F("/ not a directory"));
    return 0;
  }

  File file = root.openNextFile();
  while (file)
  {
    ++dirCount;
    if (file.isDirectory())
    {
      Serial.print(F("  DIR : "));
      Serial.println(file.name());
      dirCount += ListDir(file.name());
    }
    else
    {
      Serial.print(F("  FILE: "));
      Serial.print(file.name());
      Serial.print(F("\tSIZE: "));
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
  return dirCount;
}
#endif
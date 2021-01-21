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
#error "No LittlFS on the ESP32"
#endif

#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <FTPServer.h>
#define BAUDRATE 74880

const char *ssid PROGMEM = "YOUR_SSID";
const char *password PROGMEM = "YOUR_PASS";

// tell the FtpServer to use LittleFS
FTPServer ftpSrv(LittleFS);

void setup(void)
{
  Serial.begin(BAUDRATE);
  WiFi.begin(ssid, password);

  bool fsok = LittleFS.begin();
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
  ftpSrv.begin(F("ftp"), F("ftp")); //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)
}

enum consoleaction
{
  show,
  wait,
  format,
  list
};
consoleaction action = show;
uint16_t listDir(String indent, String path);

void loop(void)
{
  // this is all you need
  // make sure to call handleFTP() frequently
  ftpSrv.handleFTP();

  //
  // Code below just to debug in Serial Monitor
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
    LittleFS.format();
    Serial.printf_P(PSTR("FS format done, took %lu ms!\n"), millis() - startTime);
    action = show;
  }
  else if (action == list)
  {
    Serial.printf_P(PSTR("Listing contents...\n"));
    uint16_t fileCount = listDir("", "/");
    Serial.printf_P(PSTR("%d files/dirs total\n"), fileCount);
    action = show;
  }
}

uint16_t listDir(String indent, String path)
{
  uint16_t dirCount = 0;
  Dir dir = LittleFS.openDir(path);
  while (dir.next())
  {
    ++dirCount;
    if (dir.isDirectory())
    {
      Serial.printf_P(PSTR("%s%s [Dir]\n"), indent.c_str(), dir.fileName().c_str());
      dirCount += listDir(indent + "  ", path + dir.fileName() + "/");
    }
    else
      Serial.printf_P(PSTR("%s%-16s (%ld Bytes)\n"), indent.c_str(), dir.fileName().c_str(), (uint32_t)dir.fileSize());
  }
  return dirCount;
}

/*
   This is an example sketch to show the use of the FTP Client.

   Please replace
     YOUR_SSID and YOUR_PASS
   with your WiFi's values and compile.

   If you want to see debugging output of the FTP Client, please
   select select an Serial Port in the Arduino IDE menu Tools->Debug Port

   Send L via Serial Monitor, to display the contents of the FS
   Send F via Serial Monitor, to fromat the FS
   Send G via Serial Monitor, to GET a file from a FTP Server

   This example is provided as Public Domain
   Daniel Plasa <dplasa@gmail.com>

*/
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <FTPClient.h>

const char *ssid PROGMEM = "YOUR_SSID";
const char *password PROGMEM = "YOUR_PASS";

// tell the FTP Client to use LittleFS
FTPClient ftpClient(LittleFS);

// provide FTP servers credentials and servername
FTPClient::ServerInfo ftpServerInfo("user", "password", "hostname_or_ip");

void setup(void)
{
  Serial.begin(74880);
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

  ftpClient.begin(ftpServerInfo);
}

enum consoleaction
{
  show,
  get,
  put,
  wait,
  format,
  list
};
consoleaction action = show;

String fileName;
bool transferStarted = false;
uint32_t startTime;

void loop()
{
  // this is all you need
  // make sure to call handleFTP() frequently
  ftpClient.handleFTP();

  //
  // Code below just to debug in Serial Monitor
  //
  if (action == show)
  {
    Serial.printf_P(PSTR("Enter 'F' to format, 'L' to list the contents of the FS, 'G'/'P' + File to GET/PUT from/to FTP Server\n"));
    action = wait;
  }
  else if (action == wait)
  {
    if (transferStarted )
    {
      const FTPClient::Status &r = ftpClient.check();
      if (r.result == FTPClient::OK)
      {
        Serial.printf_P(PSTR("Transfer complete, took %lu ms\n"), millis() - startTime);
        transferStarted = false;
      }
      else if (r.result == FTPClient::ERROR)
      {
        Serial.printf_P(PSTR("Transfer failed after %lu ms, code: %u, descr=%s\n"), millis() - startTime, ftpClient.check().code, ftpClient.check().desc.c_str());
        transferStarted = false;
      }
    }

    if (Serial.available())
    {
      char c = Serial.read();
      if (c == 'F')
        action = format;
      else if (c == 'L')
        action = list;
      else if (c == 'G')
      {
        action = get;
        fileName = Serial.readStringUntil('\n');
        fileName.trim();
      }
      else if (c == 'P')
      {
        action = put;
        fileName = Serial.readStringUntil('\n');
        fileName.trim();
      }
      else if (!(c == '\n' || c == '\r'))
        action = show;
    }
  }

  else if (action == format)
  {
    startTime = millis();
    LittleFS.format();
    Serial.printf_P(PSTR("FS format done, took %lu ms!\n"), millis() - startTime);
    action = show;
  }

  else if ((action == get) || (action == put))
  {
    startTime = millis();
    ftpClient.transfer(fileName, fileName, action == get ?
                       FTPClient::FTP_GET_NONBLOCKING : FTPClient::FTP_PUT_NONBLOCKING);
    Serial.printf_P(PSTR("transfer started, took %lu ms!\n"), millis() - startTime);
    transferStarted = true;
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

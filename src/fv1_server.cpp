/*
 * FV-1 devRemote - remote programmer for the SpinSemi FV1 DSP
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "fv1_server.h"
#include <list>
#include <tuple>
#include "fv1.h"

const char *ssid = "FV1remote";
const char *password = "Nadszyszkownik";

const char *const PROGMEM BTN_NAME[]{"0", "1", "2", "3", "4", "5", "6", "7"};
uint8_t btn_pressed;

String fw_enabled = "";
String fw_enabled_last = "";

bool refresh_request = false;

const char WARNING[] PROGMEM = R"(<h2>No File System found!</h2>)";
const char HELPER[] PROGMEM = R"(<h2>Please upload index.html to the /htm folder</h2>)";

ESP8266WebServer server(80);
FTPServer ftpSrv(LittleFS);

void enable_file(void);
void sendResponse();
void burn_eeprom();
void enable_eeprom(void);
bool handleList(bool bypasshtm);
void deleteRecursive(const String &path);
bool handleFile(String &&path);
void handleUpload();
void formatFS();
const String formatBytes(size_t const &bytes);

// -----------------------------------------------------------------------------------------------------
void server_init(void)
{
    // set up filesystem
    if (!LittleFS.begin())
    {
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }
    // Set up wifi
    WiFi.mode(WIFI_AP);
#ifdef CONFIG
    WiFi.softAPConfig(apIPv4, apIPv4, subnet);
#endif
    if (WiFi.softAP(ssid, password))
    {
        Serial.printf(PSTR("Connect with the network \"%s\"\ntype the IP %s in browser.\n\n"), ssid, WiFi.softAPIP().toString().c_str());
        Serial.println(PSTR("Mac address = ") + WiFi.softAPmacAddress());
    }
    else
    {
        Serial.println(PSTR("Error while creating the AP."));
    }
    Serial.println("mDNS responder started");
    server.on("/format", formatFS);
    server.on("/upload", HTTP_POST, sendResponse, handleUpload);
    server.on("/uploadhex", HTTP_POST, sendResponse, handleUpload);
    server.onNotFound([]() {
        if (!handleFile(server.urlDecode(server.uri())))
            server.send(404, "text/plain", "FileNotFound");
    });
    // enable hex file
    server.on("/enable", HTTP_GET, enable_file);
    // patch number buttons
    server.on("/press", HTTP_GET, []() {
        String temp = "[";
        for (auto &el : BTN_NAME)
        {
            if (temp != "[")
                temp += ',';
            temp += (String) "\"" + el + "\"";
        }
        temp += "]";
        server.send(200, "application/json", temp);
    });
    server.on("/press", HTTP_POST, []() {
        bool result = false;
        if (server.args())
        {
            btn_pressed = server.argName(0).toInt();
            result = fv1.set_prg(btn_pressed);
        }
        // Http reply
        String temp = "\"";
        for (byte i = 0; i < 8; i++)
        {
            if (i == btn_pressed && result)
                temp += 1;
            else
                temp += 0;
        }
        temp += "\"";
        server.send(200, "application/json", temp);
    });
    // burn the EEPROM using currently loaded/parsed hex file
    server.on("/burn", burn_eeprom);

    server.on("/eepen", enable_eeprom);
    // show the ip address
    server.on("/getip", HTTP_GET, []() {
        String temp = "[";
        temp += (String) "\"" + WiFi.softAPIP().toString().c_str() + "\"";
        temp += "]";
        server.send(200, "application/json", temp);
    });
    // used to reload the site
    server.on("/refresh", HTTP_GET, []() {
        String reply = refresh_request ? "1" : "0";
        String temp = "[";
        temp +=  refresh_request ;
        temp += "]";
        server.send(200, "application/json", temp);
        refresh_request = false;
    });

    server.on("/trigrefresh", HTTP_GET, []() {
        refresh_request = true;
        sendResponse();
    });

    server.begin();
    ftpSrv.begin("fv1", "fv1");
    if (!MDNS.begin("fv1"))
    {
        Serial.println("Error setting up MDNS responder!");
    }
}
// -----------------------------------------------------------------------------------------------------
void server_process(void)
{
    server.handleClient();
    ftpSrv.handleFTP(); 
    MDNS.update();
}
// -----------------------------------------------------------------------------------------------------
void enable_file(void)
{
    String server_reply = "";

    if (server.hasArg("file"))
    {
        fw_enabled = server.arg("file");
        server.sendHeader("Location", "/htm/index.html");
        server.send(303, "message/http");
        return;
    }
    Serial.print(F("Loading file: "));
    Serial.println(fw_enabled);
    FV1_result_t reply = fv1.load_file(fw_enabled);
    fv1.print_result(reply);
    switch (reply)
    {
    case FV1_OK:
        server_reply = fw_enabled;
        fw_enabled_last = fw_enabled;
        break;
    case FV1_INPUT_FILE_WRONG:
    case FV1_INPUT_FILE_CHKSUM_ERR:
        server_reply = "Not a valid FV-1 hex file!";
        break;
    case FV1_INPUT_FILE_NOT_FOUND:
        server_reply = "File not found!";
        break;
    default:
        server_reply = "Error!";
        break;
    }
    String temp = "[";
    temp += (String) "\"" + server_reply + "\"";
    temp += "]";
    server.send(200, "application/json", temp);
}
// -----------------------------------------------------------------------------------------------------
void burn_eeprom(void)
{
    String server_reply = "";

    bool eep_result = fv1.write_eep(0x51);

    String temp = "[";
    temp += (String) "\"" + "EEPROM burn: " + (eep_result ? "OK" : "ERROR!") + "\"";
    temp += "]";
    server.send(200, "application/json", temp);
}
// -----------------------------------------------------------------------------------------------------
void enable_eeprom(void)
{
    uint8_t eep_result = fv1.toggle_slave_i2c();
    Serial.print(F("Onboard EEPROM "));
    Serial.println(eep_result ? F("enabled") : F("disabled"));
    
    String temp = "[";
    temp += (String) "\"" + "EEPROM enable: " + (eep_result ? "ON" : "OFF") + "\"";
    temp += "]";
    server.send(200, "application/json", temp);
}
// -----------------------------------------------------------------------------------------------------
bool handleList(bool bypasshtm)
{
    FSInfo fs_info;
    LittleFS.info(fs_info);
    Dir dir = LittleFS.openDir("/");
    using namespace std;
    using records = tuple<String, String, int>;
    list<records> dirList;

    while (dir.next())
    {
        if (dir.isDirectory())
        {
            if (dir.fileName() != String("htm") || !bypasshtm)
            {
                uint8_t ran{0};
                Dir fold = LittleFS.openDir(dir.fileName());
                while (fold.next())
                {
                    ran++;
                    dirList.emplace_back(dir.fileName(), fold.fileName(), fold.fileSize());
                }
                if (!ran)
                {
                    dirList.emplace_back(dir.fileName(), "", 0);
                }
            }
        }
        else
        {
            dirList.emplace_back("", dir.fileName(), dir.fileSize());
        }
    }
    dirList.sort([](const records &f, const records &l) {
        if (server.arg(0) == "1")
        {
            for (uint8_t i = 0; i < 31; i++)
            {
                if (tolower(get<1>(f)[i]) < tolower(get<1>(l)[i]))
                    return true;
                else if (tolower(get<1>(f)[i]) > tolower(get<1>(l)[i]))
                    return false;
            }
            return false;
        }
        else
        {
            for (uint8_t i = 0; i < 31; i++)
            {
                if (tolower(get<1>(f)[i]) > tolower(get<1>(l)[i]))
                    return true;
                else if (tolower(get<1>(f)[i]) < tolower(get<1>(l)[i]))
                    return false;
            }
            return false;
        }
    });
    dirList.sort([](const records &f, const records &l) {
        if (get<0>(f)[0] != 0x00 || get<0>(l)[0] != 0x00)
        {
            for (uint8_t i = 0; i < 31; i++)
            {
                if (tolower(get<0>(f)[i]) < tolower(get<0>(l)[i]))
                    return true;
                else if (tolower(get<0>(f)[i]) > tolower(get<0>(l)[i]))
                    return false;
            }
        }
        return false;
    });
    String temp = "[";
    for (auto &t : dirList)
    {
        if (temp != "[")
            temp += ',';
        temp += "{\"folder\":\"" + get<0>(t) + "\",\"name\":\"" + get<1>(t) + "\",\"size\":\"" + formatBytes(get<2>(t)) + "\"}";
    }
    temp += ",{\"usedBytes\":\"" + formatBytes(fs_info.usedBytes) +
            "\",\"totalBytes\":\"" + formatBytes(fs_info.totalBytes) +
            "\",\"freeBytes\":\"" + (fs_info.totalBytes - fs_info.usedBytes) + "\"}]";
    server.send(200, "application/json", temp);
    return true;
}
// -----------------------------------------------------------------------------------------------------
void deleteRecursive(const String &path)
{
    Serial.print("deleting: ");
    Serial.println(path);
    if (LittleFS.remove(path))
    {
        LittleFS.open(path.substring(0, path.lastIndexOf('/')) + "/", "w");
        return;
    }
    Dir dir = LittleFS.openDir(path);
    while (dir.next())
    {
        deleteRecursive(path + '/' + dir.fileName());
    }
    LittleFS.rmdir(path);
}
// -----------------------------------------------------------------------------------------------------
bool handleFile(String &&path)
{
    if (server.hasArg("new"))
    {
        String folderName{server.arg("new")};
        for (auto &c : {34, 37, 38, 47, 58, 59, 92})
            for (auto &e : folderName)
                if (e == c)
                    e = 95;
        LittleFS.mkdir(folderName);
    }
    if (server.hasArg("sort"))
        return handleList(false);
    if (server.hasArg("sortHex"))
        return handleList(true);
    if (server.hasArg("delete"))
    {
        deleteRecursive(server.arg("delete"));
        sendResponse();
        return true;
    }
    if (!LittleFS.exists("htm/index.html"))
        server.send(200, "text/html", LittleFS.begin() ? HELPER : WARNING);
    if (path.endsWith("/"))
        path += "htm/index.html";

    return (LittleFS.exists(path) ? ({File f = LittleFS.open(path, "r"); server.streamFile(f, mime::getContentType(path)); f.close(); true; }) : false);
}
// -----------------------------------------------------------------------------------------------------
void handleUpload()
{
    static File fsUploadFile;
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        if (upload.filename.length() > 31)
        {
            upload.filename = upload.filename.substring(upload.filename.length() - 31, upload.filename.length());
        }
        printf(PSTR("handleFileUpload Name: /%s\n"), upload.filename.c_str());
        fsUploadFile = LittleFS.open(server.arg(0) + "/" + server.urlDecode(upload.filename), "w");
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        printf(PSTR("handleFileUpload Data: %u\n"), upload.currentSize);
        fsUploadFile.write(upload.buf, upload.currentSize);
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        printf(PSTR("handleFileUpload Size: %u\n"), upload.totalSize);
        fsUploadFile.close();
    }
}
// -----------------------------------------------------------------------------------------------------
void formatFS()
{
    LittleFS.format();
    sendResponse();
}
// -----------------------------------------------------------------------------------------------------
void sendResponse()
{
    server.sendHeader("Location", "/htm/index.html");
    server.send(303, "message/http");
}
// -----------------------------------------------------------------------------------------------------
const String formatBytes(size_t const &bytes)
{
    return (bytes < 1024 ? static_cast<String>(bytes) + " Byte" : bytes < 1048576 ? static_cast<String>(bytes / 1024.0) + 
        " KB" : static_cast<String>(bytes / 1048576.0) + " MB");
}
// -----------------------------------------------------------------------------------------------------
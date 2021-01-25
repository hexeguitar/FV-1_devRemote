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
#ifndef _SERVER_H
#define _SERVER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h> 
#include <FTPServer.h>

void server_init(void);
void server_process(void);


extern ESP8266WebServer server;
extern FTPServer ftpSrv;
extern String fw_enabled;       // currently enabled hex file

#endif // _SERVER_H

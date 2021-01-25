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
 * 
 * Copyright (c) 2021 by Piotr Zapart
 * 
 * Credits: 
 * - Jens Fleischer https://fipsok.de - for the ESP8266 web interface examples
 * - Perttu Haimi https://github.com/n3ws/fvduino - for the fv-1 optimized i2c slave implementation
 */
#include <Arduino.h>
#include "fv1_server.h"
#include "fv1.h"

const uint8_t fv1_reset_pin = 14;
const uint8_t fv1_eeprom_select_pin = 12;

FV1 fv1(fv1_reset_pin, fv1_eeprom_select_pin);

//#define CONFIG                            

#ifdef CONFIG
IPAddress apIPv4(10, 0, 0, 0); 
IPAddress subnet(255, 0, 0, 0);
#endif

void setup()
{
    Serial.begin(115200);
    delay(100);
    server_init();
    fw_enabled = fv1.begin();
    Serial.print("Boot: load last used file: ");
    Serial.println(fw_enabled);
}

void loop()
{
    server_process();
}

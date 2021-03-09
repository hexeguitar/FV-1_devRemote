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

#ifndef _FV1_H
#define _FV1_H

#include <Arduino.h>
#include <LittleFS.h>

typedef enum
{
    FV1_OK,
    FV1_INPUT_FILE_NOT_FOUND,
    FV1_INPUT_FILE_WRONG,
    FV1_INPUT_FILE_CHKSUM_ERR,
    FV1_OTHER_ERR
}FV1_result_t;

class FV1
{
public:
    FV1(uint8_t dspP, uint8_t eepselP);
    String begin();
    FV1_result_t load_file(const String& path);
    bool set_prg(uint8_t prg_no);
    void print_result(FV1_result_t result);
    bool write_eep(uint8_t slaveAddr);
    bool toggle_slave_i2c();
    bool get_slave_i2c_state(void) {return slave_i2c_state;}
private:
    uint8_t dsprst_pin;
    uint8_t eep_select_pin;
    uint8_t current_program;
    uint8_t *dsp_fw_ptr;
    uint8_t dsp_fw_bf[4096];
    uint8_t slave_i2c_state = 1;
    uint8_t boot_complete = 0;
    uint8_t get_record_length(uint8_t* record);
    uint16_t get_record_address(uint8_t* record);
    uint8_t get_record_type(uint8_t* record);
    uint16_t extract_data(uint8_t* record, uint16_t len, uint8_t *rawBufPtr);
    uint8_t get_record_chksum(uint8_t* record);
    bool parse_record(uint8_t* record);
    bool eep_verify(void);
    void print_file(void);
};

extern FV1 fv1;

#endif // _FV1_H

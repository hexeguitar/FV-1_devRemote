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
#include "fv1.h"
#include "Wire.h"
#include "SparkFun_External_EEPROM.h"

#define FV1_HEXFILE_SIZE_WIN            (21517u) // length of the SpinASM output hex file
#define FV1_HEXFILE_SIZE_UNIX           (20492u)   
#define IHEX_START ':'
#define I2C_SLAVE_TIMEOUT_TICKS 0x8000

bool IRAM_ATTR trig_read(uint8_t *dataPtr, uint8_t rst);

ExternalEEPROM eep;

// -----------------------------------------------------------------------------------------------------
FV1::FV1(uint8_t dspP, uint8_t eepselP)
{
    dsprst_pin = dspP;
    eep_select_pin = eepselP;
    dsp_fw_ptr = NULL;
}
// -----------------------------------------------------------------------------------------------------
String FV1::begin(void)
{
    eep.setMemorySize(32768 / 8); // 24LC32A
    eep.setPageSize(32);          //In bytes.

    GPOC = (1 << SDA); // set SDA low
    pinMode(SCL, INPUT_PULLUP);
    pinMode(SDA, INPUT_PULLUP);
    pinMode(dsprst_pin, OUTPUT_OPEN_DRAIN);
    digitalWrite(dsprst_pin, HIGH);
    pinMode(eep_select_pin, OUTPUT);
    digitalWrite(eep_select_pin, HIGH);
    GPOC = (1 << SDA); // set output SDA low
    GPEC = (1 << SDA); // SDA output OFF (= Open Drain Hi)
    GPEC = (1 << SCL); // SDA High
    // load last used file
    File last_used = LittleFS.open("/htm/last.ini", "r");
    String data = last_used.readString();
    last_used.close();
    Serial.print("last used file = ");
    Serial.println(data);
    FV1_result_t result = load_file(data);
    print_result(result);

    return data;
}
// -----------------------------------------------------------------------------------------------------
bool FV1::set_prg(uint8_t prg_no)
{
    if (prg_no > 7 || !dsp_fw_ptr)
        return false;
    bool result = false;
    current_program = prg_no;
    dsp_fw_ptr = &dsp_fw_bf[512 * current_program];
    Serial.print("Setting program: ");
    Serial.println(prg_no);
    result = trig_read(dsp_fw_ptr, dsprst_pin);
    if (!result) {Serial.print(F("Error loading program ")); Serial.println(prg_no);}
    return result;
}
// -----------------------------------------------------------------------------------------------------
bool FV1::toggle_slave_i2c()
{
    slave_i2c_state ^= 1;
    if (slave_i2c_state)
    {
        digitalWrite(eep_select_pin, HIGH);
    }
    else
    {
        digitalWrite(eep_select_pin, LOW);
        digitalWrite(dsprst_pin, LOW);
        delay(100);
        digitalWrite(dsprst_pin, HIGH);
    }
    return !slave_i2c_state;
}
// -----------------------------------------------------------------------------------------------------
FV1_result_t FV1::load_file(const String &path)
{
    uint8_t buffer[24];
    uint16_t sum = 0, chksum = 0;
    uint8_t byte_count = 0;
    uint16_t data_addr = 0x0000;
    uint8_t type = 0xFF;
    bool eof_reached = false;
    dsp_fw_ptr = NULL;

    if (!LittleFS.exists(path))
    {
        return FV1_INPUT_FILE_NOT_FOUND;
    }

    File hexfile = LittleFS.open(path, "r"); // read mode

    if (hexfile.size() == FV1_HEXFILE_SIZE_WIN || hexfile.size() == FV1_HEXFILE_SIZE_UNIX)
    {
        // file legth ok
    }
    else
    {
        hexfile.close();
        boot_complete = 1;
        return FV1_INPUT_FILE_WRONG;
    }
    // Now let's handle the OS dependant line endings
    // line end:    Windows:        CRLF \r\n
    //              Linux           LF     \n
    //              Mac (preOSX)    CR   \r  
    hexfile.setTimeout(10);
    char term = '\r';
    bool t_found = hexfile.findUntil("\r", &term);
    if (t_found)
    {
        hexfile.seek(0);
        term = '\n';
        t_found = hexfile.findUntil("\n", &term);
        if (t_found)
        {
           Serial.println(PSTR("Win type file ending detected"));
        }
        else
        {
            term = '\r';
            Serial.println(PSTR("Mac (old) type line ending detected"));
        }  
    }
    else
    {
        hexfile.seek(0);
        term = '\n';
        t_found = hexfile.findUntil("\n", &term);
        if (t_found)
        {
            Serial.println(PSTR("Unix type file ending detected"));
        }    
    }
    hexfile.setTimeout(1000);
    hexfile.seek(0);
    while (hexfile.available())
    {
        String data = hexfile.readStringUntil('\n'); // get a new line
        data.trim();
        if (data[0] != IHEX_START)
        {
            hexfile.close();
            boot_complete = 1;
            return FV1_INPUT_FILE_WRONG;
        }
        // each line is one FV1 instruction,
        data.getBytes(buffer, data.length()+1); // convert to byte array
        type = get_record_type(buffer);
        byte_count = get_record_length(buffer);
        chksum = get_record_chksum(buffer);
        data_addr = get_record_address(buffer);
        sum = byte_count + (data_addr >> 8) + (data_addr & 0xFF) + chksum + type;
        //Serial.printf("addr = %04d, cnt = %04d\r\n", data_addr, byte_count);
        switch (type)
        {
        case 0x00: // data byte
            sum += extract_data(buffer, byte_count, &dsp_fw_bf[data_addr]);
            break;
        case 0x01: // end of file
            eof_reached = true;
            break;
        default:
            hexfile.close();
            boot_complete = 1;
            return FV1_INPUT_FILE_WRONG;
            break;
        }
        if (sum & 0xFF) // checksum mismatch!
        {
            hexfile.close();
            boot_complete = 1;
            return FV1_INPUT_FILE_CHKSUM_ERR;
        }
    }
    if (!eof_reached)
    {
        hexfile.close();
        boot_complete = 1;
        return FV1_INPUT_FILE_WRONG;
    }
    current_program = 0;
    dsp_fw_ptr = &dsp_fw_bf[512 * current_program];
    hexfile.close();
    if (boot_complete)      // do not save at boot
    {
        File last_used = LittleFS.open("/htm/last.ini", "w");
        last_used.print(path);
        last_used.close();       
    }
    boot_complete = 1;
    return FV1_OK;
}
// -----------------------------------------------------------------------------------------------------
bool FV1::write_eep(uint8_t slaveAddr)
{
    bool result = false;
    Wire.begin();
    if (dsp_fw_ptr)
    {
        while (eep.begin(0x51) == false)
        {
            Serial.println(F("No memory detected."));
            delay(250);
        }

        Serial.println(F("Writing EEPROM..."));
        eep.write(0x00, dsp_fw_bf, sizeof(dsp_fw_bf));
        Serial.println(F("Veryfing EEPROM..."));
        result = eep_verify();
    }
    else
    {
        result = false;
    }
    if (result) Serial.println(F("EEPROM write success!"));
    else        Serial.println(F("EEPROM write error!"));
    begin(); // reinit the slave i2c

    return result;
}
// -----------------------------------------------------------------------------------------------------
void FV1::print_result(FV1_result_t result)
{
    Serial.print(PSTR("Hex file "));
    switch (result)
    {
    case FV1_OK:
        Serial.println(PSTR("load success!"));
        break;
    case FV1_INPUT_FILE_NOT_FOUND:
        Serial.println(PSTR("not found!"));
        break;
    case FV1_INPUT_FILE_WRONG:
        Serial.println(PSTR("wrong!"));
        break;
    case FV1_INPUT_FILE_CHKSUM_ERR:
        Serial.println(PSTR("checksum error!"));
        break;
    case FV1_OTHER_ERR:
        Serial.println(PSTR("other error!"));
        break;
    default:
        break;
    }
}
// -----------------------------------------------------------------------------------------------------
uint8_t FV1::get_record_length(uint8_t *record)
{
    char buf[3];

    buf[0] = record[1];
    buf[1] = record[2];
    buf[2] = '\0';

    return strtol(buf, 0, 16);
}
// -----------------------------------------------------------------------------------------------------
uint16_t FV1::get_record_address(uint8_t *record)
{
    char buf[3];
    uint16_t addr;
    buf[2] = '\0';

    buf[0] = record[3];
    buf[1] = record[4];
    addr = (strtol(buf, 0, 16)) << 8;

    buf[0] = record[5];
    buf[1] = record[6];
    addr += strtol(buf, 0, 16);

    return addr;
}
// -----------------------------------------------------------------------------------------------------
uint8_t FV1::get_record_type(uint8_t *record)
{
    char buf[3];

    buf[0] = record[7];
    buf[1] = record[8];
    buf[2] = '\0';

    return strtol(buf, 0, 16);
}
// -----------------------------------------------------------------------------------------------------
uint8_t FV1::get_record_chksum(uint8_t *record)
{
    char buf[3];
    uint8_t addr = 9 + get_record_length(record) * 2;

    buf[0] = record[addr];
    buf[1] = record[addr + 1];
    buf[2] = '\0';

    return strtol(buf, 0, 16);
}
// -----------------------------------------------------------------------------------------------------
uint16_t FV1::extract_data(uint8_t *record, uint16_t len, uint8_t *rawBufPtr)
{
    int begin = 9;               // start position of data in record
    int end = (len * 2) + begin; //data is in sets of 2
    char buf[3];
    buf[2] = '\0';
    uint8_t value;
    uint16_t sum = 0;
    for (int i = begin; i < end; i = i + 2)
    {
        buf[0] = record[i];
        buf[1] = record[i + 1];
        value = strtol(buf, 0, 16);
        *rawBufPtr++ = value;
        sum += value;
    }
    return sum;
}

// -----------------------------------------------------------------------------------------------------
bool IRAM_ATTR trig_read(uint8_t *dataPtr, uint8_t rst)
{
    volatile uint32_t dly = 20000;
    uint8_t prev_clk = 1;

    uint16_t pos = 0;
    uint8_t curr_byte = *dataPtr;

    uint8_t bit_mask = 0b10000000;
    uint8_t clk_count = 0;
    uint32_t timeout = I2C_SLAVE_TIMEOUT_TICKS;
    GPEC = (1 << SDA) | (1 << SCL);
    // Undivided attention for FV-1 requests
    noInterrupts();

    // Notify FV-1 of patch change by toggling the notify pin
    digitalWrite(rst, LOW);
    while (dly)
    {
        dly--;
        __asm__ __volatile__("nop");
        ESP.wdtFeed();
    }
    GPEC = (1 << SDA);
    digitalWrite(rst, HIGH);

    while (clk_count < 37 && --timeout) // Handle the header
    {
        uint8_t clk = (GPI & (1 << SCL)) != 0; // read SCL
        if (!clk && prev_clk)
        { // SCL went down
            switch (clk_count)
            {
            case 8:
            case 17:
            case 26:
            case 36:
                GPES = (1 << SDA); // SDA low
                break;
            default:
                GPEC = (1 << SDA); // SDA high
                break;
            }
            clk_count++;
        }
        prev_clk = clk;
    }
    timeout = I2C_SLAVE_TIMEOUT_TICKS;
    clk_count = 0;
    while (pos < 512 && --timeout) // Send the data
    {
        uint8_t clk = (GPI & (1 << SCL)) != 0; // read SCL

        if (!clk && prev_clk) // falling edge
        {
            if (clk_count != 8) // Sending byte
            {
                if (curr_byte & bit_mask)
                    GPEC = (1 << SDA); //SDA_HIGH(SDA);
                else
                    GPES = (1 << SDA); //SDA_LOW(SDA);
                bit_mask >>= 1;
                clk_count++;
            }
            else
            {
                GPEC = (1 << SDA); // SDA_HIGH(SDA);
                clk_count = 0;
                bit_mask = 0b10000000;
                pos++;
                curr_byte = *(++dataPtr);
            }
        }
        prev_clk = clk;
        ESP.wdtFeed();
    }
    interrupts();
    if (timeout)
        return true;
    else
        return false;
}
// -----------------------------------------------------------------------------------------------------
bool FV1::eep_verify(void)
{
    if (!dsp_fw_ptr)
    {
        Serial.print(F("Verification error! HEX file not enabled!'"));
        return (false);
    }

    int error = 0;
    uint32_t EEPROMLocation = 0;
    uint8_t *onEEPROM = (uint8_t *)malloc(eep.getPageSize()); //Create a buffer to hold a page

    while (EEPROMLocation < 4096)
    {
        ESP.wdtFeed();
        uint8_t bytesToRead = eep.getPageSize();

        if (EEPROMLocation + bytesToRead > eep.getMemorySize())
            bytesToRead = eep.getMemorySize() - EEPROMLocation;

        eep.read(EEPROMLocation, onEEPROM, eep.getPageSize()); //Location, data
        //Verify what was read from the EEPROM matches the file
        for (int x = 0; x < bytesToRead; x++)
        {
            uint8_t onFile = dsp_fw_bf[EEPROMLocation + x];
            if (onEEPROM[x] != onFile)
            {
                Serial.print(F("Verify failed at location 0x"));
                Serial.print(EEPROMLocation + x, HEX);
                Serial.print(F(". Read 0x"));
                Serial.print(onEEPROM[x], HEX);
                Serial.print(F(", expected 0x"));
                Serial.print(onFile, HEX);
                Serial.println(F("."));
                if (error++)
                {
                    return (false);
                }
            }
        }
        EEPROMLocation += bytesToRead;
        if (EEPROMLocation % (eep.getPageSize()*16) == 0)
            Serial.print(F("."));
    }
    Serial.println(F("\r\nVerification PASSED!"));
    free(onEEPROM);
    return (true);
}

void FV1::print_file(void)
{
    uint16_t i = 0;
    while(i < 4096)
    {
        Serial.printf("%04d:\t%02x:%02x:%02x:%02x\t%04d:\t%02x:%02x:%02x:%02x\r\n", 
                    i/4,
                    dsp_fw_bf[i],  dsp_fw_bf[i + 1],
                    dsp_fw_bf[i + 2],  dsp_fw_bf[i + 3],
                    (i+4)/4,
                    dsp_fw_bf[i+4],  dsp_fw_bf[i + 5],
                    dsp_fw_bf[i + 6],  dsp_fw_bf[i + 7]);
        i = i + 8;
    }
}
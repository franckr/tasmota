/*
  xnrg_16_teleinfo.ino - France Teleinfo energy sensor support for Sonoff-Tasmota
  
  Copyright (C) 2020  Nicolas Bernaerts

    05/05/2019 - v1.0   - Creation
    16/05/2019 - v1.1   - Add Tempo and EJP contracts
    08/06/2019 - v1.2   - Handle active and apparent power
    05/07/2019 - v2.0   - Rework with selection thru web interface
    02/01/2020 - v3.0   - Functions rewrite for Tasmota 8.x compatibility
    05/02/2020 - v3.1   - Add support for 3 phases meters
    14/03/2020 - v3.2   - Add apparent power graph
    05/04/2020 - v3.3   - Add Timezone management
    13/05/2020 - v3.4   - Add overload management per phase
    19/05/2020 - v3.6   - Add configuration for first NTP server
    26/05/2020 - v3.7   - Add Information JSON page
    07/07/2020 - v3.7.1 - Enable discovery (mDNS)
    29/07/2020 - v3.8   - Add Meter section to JSON
    05/08/2020 - v4.0   - Major code rewrite, JSON section is now TIC, numbered like new official Teleinfo module
    24/08/2020 - v4.0.1 - Web sensor display update
    18/09/2020 - v4.1   - Based on Tasmota 8.4
    07/10/2020 - v5.0   - Handle different graph periods and javascript auto update
    18/10/2020 - v5.1   - Expose icon on web server
    25/10/2020 - v5.2   - Real time graph page update
    30/10/2020 - v5.3   - Add TIC message page
    02/11/2020 - v5.4   - Tasmota 9.0 compatibility
    09/11/2020 - v6.0   - Handle ESP32 ethernet devices with board selection
    11/11/2020 - v6.1   - Add data.json page
    20/11/2020 - v6.2   - Correct checksum bug
    29/12/2020 - v6.3   - Strengthen message error control
    20/02/2021 - v6.4   - Add sub-totals (HCHP, HCHC, ...) to JSON

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_ENERGY_SENSOR
#ifdef USE_TELEINFO

#ifdef ESP32
#include <HardwareSerial.h>
#endif

/*********************************************************************************************\
 * Teleinfo historical
 * docs https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf
 * Teleinfo hardware will be enabled if 
 *     Hardware RX = [Teleinfo 1200] or [Teleinfo 9600]
 *     Hardware TX = [Teleinfo TX]
\*********************************************************************************************/

/*************************************************\
 *               Variables
\*************************************************/

// declare teleinfo energy driver and sensor
#define XNRG_15   15
#define XSNS_15   15

// teleinfo constant
#define TELEINFO_VOLTAGE             230        // default contract voltage is 200V

// web published pages
#define D_PAGE_TELEINFO_CONFIG       "teleinfo"
#define D_PAGE_TELEINFO_TIC          "tic"
#define D_PAGE_TELEINFO_DATA         "data.json"
#define D_PAGE_TELEINFO_GRAPH        "graph"
#define D_PAGE_TELEINFO_BASE_SVG     "base.svg"
#define D_PAGE_TELEINFO_DATA_SVG     "data.svg"
#define D_CMND_TELEINFO_MODE         "mode"
#define D_CMND_TELEINFO_ETH          "eth"

// graph data
#define TELEINFO_GRAPH_SAMPLE        365         // 1 day per year
#define TELEINFO_GRAPH_WIDTH         800      
#define TELEINFO_GRAPH_HEIGHT        500 
#define TELEINFO_GRAPH_PERCENT_START 10     
#define TELEINFO_GRAPH_PERCENT_STOP  90

// JSON message
#define TELEINFO_JSON_TIC            "TIC"
#define TELEINFO_JSON_PHASE          "PHASE"
#define TELEINFO_JSON_ADCO           "ADCO"
#define TELEINFO_JSON_ISOUSC         "ISOUSC"
#define TELEINFO_JSON_SSOUSC         "SSOUSC"
#define TELEINFO_JSON_IINST          "IINST"
#define TELEINFO_JSON_SINSTS         "SINSTS"
#define TELEINFO_JSON_ADIR           "ADIR"

#define TELEINFO_JSON_BASE           "BASE"
#define TELEINFO_JSON_HCHC           "HCHC"
#define TELEINFO_JSON_HCHP           "HCHP"
#define TELEINFO_JSON_BBRHCJB        "BBRHCJB"
#define TELEINFO_JSON_BBRHPJB        "BBRHPJB"
#define TELEINFO_JSON_BBRHCJW        "BBRHCJW"
#define TELEINFO_JSON_BBRHPJW        "BBRHPJW"
#define TELEINFO_JSON_BBRHCJR        "BBRHCJR"
#define TELEINFO_JSON_BBRHPJR        "BBRHPJR"
#define TELEINFO_JSON_EJPHN          "EJPHN"
#define TELEINFO_JSON_EJPHPM         "EJPHPM"

#define D_TELEINFO_MODE              "Teleinfo"
#define D_TELEINFO_CONFIG            "Configure Teleinfo"
#define D_TELEINFO_BOARD             "Ethernet board"
#define D_TELEINFO_GRAPH             "Graph"
#define D_TELEINFO_MESSAGE           "Message"
#define D_TELEINFO_REFERENCE         "Contract n°"
#define D_TELEINFO_TIC               "TIC messages"
#define D_TELEINFO_ERROR             "TIC errors"
#define D_TELEINFO_DISABLED          "Désactivé"
#define D_TELEINFO_SPEED_DEFAULT     "bauds"
#define D_TELEINFO_SPEED_HISTO       "bauds (Historique)"
#define D_TELEINFO_SPEED_STD         "bauds (Standard)"

// others
#define TELEINFO_PHASE_MAX           3      

// form strings
const char TELEINFO_INPUT_TEXT[] PROGMEM  = "<p><input type='radio' name='%s' value='%d' %s>%s</p>\n";
const char TELEINFO_INPUT_VALUE[] PROGMEM = "<p><input type='radio' name='%s' value='%d' %s>%d %s</p>\n";
const char TELEINFO_FORM_START[] PROGMEM  = "<form method='get' action='%s'>\n";
const char TELEINFO_FORM_STOP[] PROGMEM   = "</form>\n";
const char TELEINFO_FIELD_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
const char TELEINFO_FIELD_STOP[] PROGMEM  = "</fieldset></p><br>\n";
const char TELEINFO_HTML_POWER[] PROGMEM  = "<text class='power' x='%d%%' y='%d%%'>%d</text>\n";
const char TELEINFO_HTML_DASH[] PROGMEM   = "<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n";

// graph colors
const char *const arr_color_phase[] PROGMEM = { "ph1", "ph2", "ph3" };

// week days name
const char *const arr_week_day[] PROGMEM = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

// TIC message parts
enum TeleinfoMessagePart { TELEINFO_NONE, TELEINFO_ETIQUETTE, TELEINFO_DONNEE, TELEINFO_CHECKSUM };

// overload states
enum TeleinfoGraphPeriod { TELEINFO_LIVE, TELEINFO_DAY, TELEINFO_WEEK, TELEINFO_MONTH, TELEINFO_YEAR, TELEINFO_PERIOD_MAX };
const char *const arr_period_cmnd[] PROGMEM = { "live", "day", "week", "month", "year" };
const char *const arr_period_label[] PROGMEM = { "Live", "Day", "Week", "Month", "Year" };
const long arr_period_sample[] = { 5, 236, 1657, 7338, 86400 };       // number of seconds between samples

// teleinfo driver status
bool teleinfo_enabled = false;

// teleinfo data
struct {
  uint8_t phase  = 1;           // number of phases
  long    isousc = 0;           // contract max current per phase
  long    ssousc = 0;           // contract max power per phase
  String  adco;
} teleinfo_contract;

// teleinfo message
struct {
  long   count = 0;             // number of received messages
  long   error = 0;             // number of checksum errors
  String buffer;
  String content;
} teleinfo_message;

// teleinfo current line
struct {
  uint8_t part = TELEINFO_NONE;
  char    checksum;
  String  buffer;
  String  etiquette;
  String  donnee;
} teleinfo_line;

// teleinfo update trigger
struct {
bool updated    = false;        // data have been updated
long papp_last  = 0;            // last published apparent power
long papp_delta = 0;            // apparent power delta to publish
} teleinfo_json;

// teleinfo power counters
struct {
  long papp    = 0;             // total apparent power
  long base    = 0;
  long total   = 0;
  long hchc    = 0;
  long hchp    = 0;
  long bbrhcjb = 0;
  long bbrhpjb = 0;
  long bbrhcjw = 0;
  long bbrhpjw = 0;
  long bbrhcjr = 0;
  long bbrhpjr = 0;
  long ejphn   = 0;
  long ejphpm  = 0;
} teleinfo_counter;

// data per phase
struct tic_phase {
  bool adir_set;                          // adir set in the message
  long iinst;                             // instant current
  long papp;                              // instant apparent power
  long adir;                              // percentage of contract power overconsumption
  long last_papp;                         // last published apparent power
  uint32_t last_diff;                     // last time a power difference was published
}; 
tic_phase teleinfo_phase[TELEINFO_PHASE_MAX];

// graph
struct tic_period {
  bool     updated;                       // flag to ask for graph update
  int      index;                         // current array index per refresh period
  long     counter;                       // counter in seconds per refresh period
  uint16_t pmax;                          // graph maximum power
  uint16_t papp[TELEINFO_PHASE_MAX];      // current apparent power per refresh period and per phase
  uint16_t arr_papp[TELEINFO_PHASE_MAX][TELEINFO_GRAPH_SAMPLE];
}; 
tic_period teleinfo_graph[TELEINFO_PERIOD_MAX];

#ifdef ESP32
// serial port
HardwareSerial *teleinfo_serial = nullptr;
#endif

/****************************************\
 *               Icons
\****************************************/

// icon : teleinfo
const char teleinfo_icon_tic_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x04, 0x03, 0x00, 0x00, 0x00, 0x31, 0x10, 0x7c, 0xf8, 0x00, 0x00, 0x00, 0x12, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x6f, 0x6c, 0x00, 0x00, 0x00, 0x4d, 0x82, 0xbd, 0x61, 0x83, 0xb5, 0x67, 0x83, 0xb1, 0xf7, 0xe4, 0xb9, 0xab, 0xda, 0xea, 0xec, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x2e, 0x23, 0x00, 0x00, 0x2e, 0x23, 0x01, 0x78, 0xa5, 0x3f, 0x76, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x12, 0x17, 0x07, 0x09, 0xde, 0x55, 0xef, 0x17, 0x00, 0x00, 0x02, 0x37, 0x49, 0x44, 0x41, 0x54, 0x68, 0xde, 0xed, 0x99, 0x59, 0x6e, 0xc4, 0x20, 0x0c, 0x86, 0x11, 0x9e, 0x83, 0x58, 0x70, 0x01, 0x64, 0xee, 0x7f, 0xb7, 0x66, 0x99, 0x25, 0xac, 0xc6, 0x38, 0xe9, 0xb4, 0x52, 0xfc, 0x42, 0x35, 0xc4, 0x5f, 0x7e, 0x13, 0xc0, 0x86, 0x1a, 0xf3, 0xf7, 0x0d, 0x9c, 0xf3, 0xb3, 0xbe, 0x71, 0x73, 0x9f, 0x06, 0xac, 0xaf, 0xa6, 0xcd, 0x3f, 0xcc, 0xfa, 0xef, 0x36, 0x1b, 0x00, 0xbd, 0x00, 0x68, 0x74, 0x02, 0x9c, 0x56, 0xc0, 0x2c, 0xc0, 0x29, 0x01, 0x1f, 0x01, 0x5e, 0x0b, 0x40, 0xe5, 0x10, 0x4e, 0x02, 0xec, 0xd7, 0x01, 0xa4, 0x05, 0xb8, 0x13, 0x01, 0x4e, 0x0d, 0xc0, 0xff, 0x09, 0xa0, 0x33, 0x01, 0xe6, 0x2b, 0x00, 0xab, 0x05, 0x80, 0x16, 0x60, 0xd4, 0x00, 0x3b, 0xf1, 0x11, 0x62, 0x5d, 0xc2, 0x38, 0xc0, 0x05, 0xdd, 0xae,
  0x6e, 0x53, 0x00, 0x88, 0x01, 0x90, 0x3d, 0x2a, 0x1e, 0xc3, 0x1c, 0x60, 0xa5, 0x00, 0x9b, 0x3f, 0x4b, 0xd2, 0x31, 0x74, 0x55, 0x09, 0x47, 0x8d, 0x71, 0xcb, 0x13, 0xb1, 0x3d, 0xf7, 0xb0, 0x9c, 0x4c, 0x98, 0xb9, 0xb7, 0xd3, 0xbd, 0x65, 0xe2, 0x8d, 0xc7, 0x2d, 0xa2, 0x24, 0xc0, 0x5e, 0x4a, 0xe0, 0xd0, 0xf2, 0xac, 0x11, 0x88, 0x19, 0xf2, 0xcc, 0xbf, 0x20, 0x40, 0x5b, 0x5b, 0xb1, 0xba, 0xab, 0xd3, 0x8b, 0x29, 0x67, 0xc0, 0x95, 0xd6, 0x52, 0x18, 0x86, 0x02, 0x28, 0x24, 0x50, 0x6f, 0x80, 0xab, 0x02, 0x32, 0xad, 0xc7, 0x9e, 0x18, 0x29, 0xeb, 0xaf, 0x02, 0x5c, 0x13, 0x50, 0x2a, 0xb9, 0x08, 0x80, 0x0c, 0x00, 0x45, 0x00, 0xba, 0x16, 0xb0, 0x76, 0xfb, 0xd8, 0x05, 0x00, 0xf7, 0x9d, 0x2b, 0xcf, 0x21, 0x33, 0x55, 0x90, 0xcb, 0x17, 0xc8, 0xcc, 0x15, 0xe4, 0x32, 0x4e, 0x30, 0xfd, 0x0f, 0xc9, 0x66, 0x1c, 0x66, 0xbd, 0x21, 0x93, 0xf4, 0x3c, 0xb7, 0x62, 0x99, 0xac, 0xe9, 0xb9, 0x25, 0x87, 0x4c, 0xe9, 0x80, 0x4c, 0x42, 0xee, 0x57, 0xe8, 0x80, 0x57, 0x9d, 0x31, 0x5e, 0x22, 0x82, 0x51, 0x58, 0x34, 0xb7, 0x35, 0xc6, 0x16, 0x55, 0xfe, 0x56, 0xf7, 0x69, 0xdf, 0x33, 0x4c, 0x5c, 0x6b, 0x7a, 0xcc, 0x56, 0x89, 0x2c, 0x0e, 0x78, 0xbf, 0xd3, 0xce, 0x4d, 0xd0, 0xc3, 0x2b, 0xa7, 0x8a, 0x56, 0x38, 0x00, 0x66, 0xaa, 0xd6, 0x74, 0xd1, 0xce, 0x1c, 0x3e, 0xd2, 0x55, 0x2d, 0x0f, 0x02, 0xd2, 0xc7, 0xe5, 0x41, 0x50, 0xf6, 0x3e, 0x69, 0x10, 0x90, 0x0b, 0xb6, 0xc2, 0x20, 0xca, 0x9a, 0x4b, 0x26, 0x81, 0xdc, 0x14, 0xe0, 0x11, 0x3a, 0x7b, 0xe3, 0x48, 0x0c, 0xe0, 0x8a, 0xfd, 0x5d, 0x78, 0xa1, 0x40, 0x4b, 0x12, 0x8f, 0xd4, 0x48, 0x2f, 0x23, 0x49, 0x87, 0x7a,
  0x09, 0xf2, 0x4c, 0x00, 0x8e, 0x1c, 0x57, 0xcf, 0x01, 0xa0, 0xf0, 0x4e, 0x43, 0x0d, 0x80, 0xd3, 0x01, 0xd2, 0x3b, 0x0d, 0xe8, 0x08, 0xb0, 0x5a, 0xc0, 0xd8, 0xad, 0x4a, 0xaf, 0xce, 0xb2, 0x72, 0x00, 0x0e, 0x96, 0x88, 0x4d, 0x42, 0xbb, 0x77, 0xf6, 0xf6, 0xc7, 0xf2, 0x80, 0xcf, 0xe9, 0xd4, 0x75, 0x66, 0xda, 0xd0, 0xdd, 0x49, 0xa7, 0x77, 0x28, 0x99, 0x77, 0xfa, 0x94, 0xa5, 0x86, 0xae, 0xd2, 0xb8, 0xed, 0xb6, 0xdb, 0x7e, 0xcd, 0xe2, 0x62, 0xe1, 0xdd, 0x94, 0x3f, 0xae, 0xff, 0x7c, 0xdb, 0x9b, 0xce, 0x5e, 0xe2, 0x9f, 0xf9, 0x21, 0xdf, 0x86, 0x9e, 0x07, 0xba, 0xee, 0xb9, 0xee, 0x79, 0xb2, 0xa8, 0x01, 0x5e, 0x9e, 0x0f, 0x06, 0xe0, 0xcd, 0x06, 0x08, 0xcb, 0xfe, 0xfc, 0x01, 0xf8, 0xa5, 0xee, 0x59, 0x00, 0x1e, 0xf6, 0x26, 0xbf, 0x7d, 0x4d, 0x00, 0xb8, 0x7a, 0xe6, 0x80, 0xf5, 0xc7, 0xb7, 0x27, 0x03, 0x08, 0xbb, 0x82, 0xe5, 0x4f, 0x7f, 0x04, 0x58, 0x97, 0x36, 0x78, 0x25, 0x80, 0xfc, 0x06, 0x20, 0x9f, 0x86, 0x20, 0x00, 0x3c, 0xc7, 0xa0, 0x06, 0x20, 0xad, 0x02, 0x16, 0x40, 0x2f, 0x80, 0x58, 0xc1, 0xe1, 0x6e, 0x87, 0xf6, 0x49, 0x94, 0xe6, 0x69, 0x4a, 0x1a, 0x5b, 0xde, 0x04, 0x0d, 0x02, 0x6c, 0x06, 0xb8, 0xf3, 0xdc, 0x6e, 0x3f, 0x61, 0x43, 0x14, 0x0c, 0xfe, 0x63, 0x6f, 0x4d, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int teleinfo_icon_tic_len = 720;

/*******************************************\
 *               Accessor
\*******************************************/

// get teleinfo mode (baud rate)
uint16_t TeleinfoGetMode ()
{
  uint16_t actual_mode;

  // read actual teleinfo mode
  actual_mode = Settings.sbaudrate;

  // if outvalue, set to disabled
  if (actual_mode > 19200) actual_mode = 0;
  
  return actual_mode;
}

// set teleinfo mode (baud rate)
void TeleinfoSetMode (uint16_t new_mode)
{
  // if within range, set baud rate
  if (new_mode <= 19200)
  {
    // if mode has changed
    if (Settings.sbaudrate != new_mode)
    {
      // save mode
      Settings.sbaudrate = new_mode;

      // ask for restart
      TasmotaGlobal.restart_flag = 2;
    }
  }
}

/*********************************************\
 *               Functions
\*********************************************/

bool TeleinfoValidateChecksum (const char *pstr_etiquette, const char *pstr_donnee, const char tic_checksum) 
{
  bool    result = false;
  uint8_t valid_checksum = 32;

  // check pointers 
  if ((pstr_etiquette != nullptr) && (pstr_donnee != nullptr))
  {
    // if etiquette and donnee are defined and checksum is exactly one caracter
    if (strlen(pstr_etiquette) && strlen(pstr_donnee))
    {
      // add every char of etiquette and donnee to checksum
      while (*pstr_etiquette) valid_checksum += *pstr_etiquette++ ;
      while(*pstr_donnee) valid_checksum += *pstr_donnee++ ;
      valid_checksum = (valid_checksum & 63) + 32;
      
      // compare given checksum and calculated one
      result = (valid_checksum == tic_checksum);
      if (!result) teleinfo_message.error++;
    }
  }

  return result;
}

bool TeleinfoIsValueNumeric (const char *pstr_value) 
{
  bool result = false;

  // check pointer 
  if (pstr_value != nullptr)
  {
    // check that all are digits
    result = true;
    while(*pstr_value) if (isDigit(*pstr_value++) == false) result = false;
  }

  return result;
}

void TeleinfoPreInit ()
{
  // if PIN defined, set energy driver
  if (!TasmotaGlobal.energy_driver && PinUsed(GPIO_TELEINFO_RX))
  {
    // energy driver is teleinfo meter
    TasmotaGlobal.energy_driver = XNRG_15;

    // voltage not available in teleinfo
    Energy.voltage_available = false;
  }
}

void TeleinfoInit ()
{
  uint16_t teleinfo_mode;

  // get teleinfo speed
  teleinfo_mode = TeleinfoGetMode ();

  // if sensor has been pre initialised
  teleinfo_enabled = ((TasmotaGlobal.energy_driver == XNRG_15) && (teleinfo_mode > 0));
  if (teleinfo_enabled)
  {
#ifdef ESP8266
    // start ESP8266 serial port
    Serial.flush ();
    Serial.begin (teleinfo_mode, SERIAL_7E1);
    ClaimSerial ();
#else  // ESP32
    // start ESP32 serial port
    teleinfo_serial = new HardwareSerial(2);
    teleinfo_serial->begin(teleinfo_mode, SERIAL_7E1, Pin(GPIO_TELEINFO_RX), -1);
#endif // ESP286 & ESP32

    // log
    AddLog_P2(LOG_LEVEL_INFO, PSTR("TIC: Teleinfo Serial ready"));
  }

  // else disable energy driver
  else TasmotaGlobal.energy_driver = ENERGY_NONE;
}

void TeleinfoGraphInit ()
{
  uint8_t phase, period;
  int     index;

  // initialise phase data
  for (phase = 0; phase < TELEINFO_PHASE_MAX; phase++)
  {
    // set default voltage
    Energy.voltage[index] = TELEINFO_VOLTAGE;

    // default values
    teleinfo_phase[phase].iinst     = 0;
    teleinfo_phase[phase].papp      = 0;
    teleinfo_phase[phase].adir      = 0;
    teleinfo_phase[phase].adir_set  = false;
    teleinfo_phase[phase].last_papp = 0;
    teleinfo_phase[phase].last_diff = UINT32_MAX;
  }

  // initialise graph data
  for (period = 0; period < TELEINFO_PERIOD_MAX; period++)
  {
    // init counter
    teleinfo_graph[period].index   = 0;
    teleinfo_graph[period].counter = 0;

    // loop thru phase
    for (phase = 0; phase < TELEINFO_PHASE_MAX; phase++)
    {
      // init max power per period
      teleinfo_graph[period].papp[phase] = 0;

      // loop thru graph values
      for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++) teleinfo_graph[period].arr_papp[phase][index] = UINT16_MAX;
    }
  }
}

void TeleinfoEvery50ms ()
{
  bool     checksum_ok, is_numeric;
  uint8_t  recv_serial, phase;
  uint32_t teleinfo_delta, teleinfo_newtotal;
  int      string_size;
  long     current_inst, current_total;
  float    power_apparent;

  // loop as long as serial port buffer is not empty and timeout not reached
#ifdef ESP8266
  while (Serial.available() > 0) 
#else  // ESP32
  while (teleinfo_serial->available() > 0) 
#endif // ESP286 & ESP32
  {
    // read caracter
#ifdef ESP8266
    recv_serial = Serial.read ();
#else  // ESP32
    recv_serial = teleinfo_serial->read(); 
#endif // ESP286 & ESP32
    switch (recv_serial)
    {
      // ---------------------------
      // 0x02 : Beginning of message 
      // ---------------------------
      case 2:
        // reset teleinformation message
        teleinfo_message.buffer = "";

        // reset adir overload flags
        for (phase = 0; phase < teleinfo_contract.phase; phase++) teleinfo_phase[phase].adir_set = false;
        break;
          
      // ------------------------
      // 0x0A : Beginning of line
      // ------------------------
      case 10:
        teleinfo_line.part = TELEINFO_ETIQUETTE;
        teleinfo_line.buffer    = "";
        teleinfo_line.etiquette = "";
        teleinfo_line.donnee    = "";
        teleinfo_line.checksum  = 0;
        break;

      // ---------------------------
      // \t or SPACE : new line part
      // ---------------------------
      case 9:
      case ' ':
        // update current line part
        switch (teleinfo_line.part)
        {
          case TELEINFO_ETIQUETTE:
            teleinfo_line.etiquette = teleinfo_line.buffer;
            break;
          case TELEINFO_DONNEE:
            teleinfo_line.donnee = teleinfo_line.buffer;
            break;
          case TELEINFO_CHECKSUM:
            teleinfo_line.checksum = teleinfo_line.buffer[0];
        }

        // prepare next part of line
        teleinfo_line.part ++;
        teleinfo_line.buffer = "";
        break;
        
      // ------------------
      // 0x0D : End of line
      // ------------------
      case 13:
        // retrieve checksum
        if (teleinfo_line.part == TELEINFO_CHECKSUM)
        {
          string_size = teleinfo_line.buffer.length ();
          if (string_size > 0) teleinfo_line.checksum = teleinfo_line.buffer[0];
        }

        // reset line part
        teleinfo_line.part = TELEINFO_NONE;

        // calculate size of complete message with current line
        string_size  = teleinfo_message.buffer.length ();
        string_size += teleinfo_line.etiquette.length ();
        string_size += teleinfo_line.donnee.length ();
        string_size += 4;

        // if size permits, add last line received to tic message
        if (string_size <= LOGSZ) teleinfo_message.buffer += teleinfo_line.etiquette + " " + teleinfo_line.donnee + " " + String (teleinfo_line.checksum) + "\n";

        // validate checksum and numeric format
        checksum_ok = TeleinfoValidateChecksum (teleinfo_line.etiquette.c_str (), teleinfo_line.donnee.c_str (), teleinfo_line.checksum);
        is_numeric  = TeleinfoIsValueNumeric (teleinfo_line.donnee.c_str ());

        // if checksum is ok, handle the line
        if (checksum_ok && is_numeric)
        {
          // detect 3 phase counter
          if (teleinfo_line.etiquette == "IINST3") teleinfo_contract.phase = 3;

          // handle etiquettes
          if (teleinfo_line.etiquette == "ADCO") teleinfo_contract.adco = teleinfo_line.donnee;
          else if (teleinfo_line.etiquette == "IINST")  teleinfo_phase[0].iinst = teleinfo_line.donnee.toInt ();
          else if (teleinfo_line.etiquette == "IINST1") teleinfo_phase[0].iinst = teleinfo_line.donnee.toInt ();
          else if (teleinfo_line.etiquette == "IINST2") teleinfo_phase[1].iinst = teleinfo_line.donnee.toInt ();
          else if (teleinfo_line.etiquette == "IINST3") teleinfo_phase[2].iinst = teleinfo_line.donnee.toInt ();
          else if (teleinfo_line.etiquette == "ADPS")
          {
            teleinfo_phase[0].adir_set = true;
            teleinfo_phase[0].adir = teleinfo_line.donnee.toInt ();
          }
          else if (teleinfo_line.etiquette == "ADIR1")
          {
            teleinfo_phase[0].adir_set = true;
            teleinfo_phase[0].adir = teleinfo_line.donnee.toInt ();
          }
          else if (teleinfo_line.etiquette == "ADIR2")
          {
            teleinfo_phase[1].adir_set = true;
            teleinfo_phase[1].adir = teleinfo_line.donnee.toInt ();
          } 
          else if (teleinfo_line.etiquette == "ADIR3")
          {
            teleinfo_phase[2].adir_set = true;
            teleinfo_phase[2].adir = teleinfo_line.donnee.toInt ();
          }
          else if (teleinfo_line.etiquette == "ISOUSC")
          {
            teleinfo_contract.isousc = teleinfo_line.donnee.toInt ();
            if (teleinfo_contract.isousc > 0) teleinfo_contract.ssousc = teleinfo_contract.isousc * 200;
          }
          else if (teleinfo_line.etiquette == "PAPP")    teleinfo_counter.papp    = teleinfo_line.donnee.toInt ();
          else if (teleinfo_line.etiquette == "BASE")    teleinfo_counter.base    = teleinfo_line.donnee.toInt ();
          else if (teleinfo_line.etiquette == "HCHC")    teleinfo_counter.hchc    = teleinfo_line.donnee.toInt ();
          else if (teleinfo_line.etiquette == "HCHP")    teleinfo_counter.hchp    = teleinfo_line.donnee.toInt ();
          else if (teleinfo_line.etiquette == "BBRHCJB") teleinfo_counter.bbrhcjb = teleinfo_line.donnee.toInt ();
          else if (teleinfo_line.etiquette == "BBRHPJB") teleinfo_counter.bbrhpjb = teleinfo_line.donnee.toInt ();
          else if (teleinfo_line.etiquette == "BBRHCJW") teleinfo_counter.bbrhcjw = teleinfo_line.donnee.toInt ();
          else if (teleinfo_line.etiquette == "BBRHPJW") teleinfo_counter.bbrhpjw = teleinfo_line.donnee.toInt ();
          else if (teleinfo_line.etiquette == "BBRHCJR") teleinfo_counter.bbrhcjr = teleinfo_line.donnee.toInt ();
          else if (teleinfo_line.etiquette == "BBRHPJR") teleinfo_counter.bbrhpjr = teleinfo_line.donnee.toInt ();
          else if (teleinfo_line.etiquette == "EJPHN")   teleinfo_counter.ejphn   = teleinfo_line.donnee.toInt ();
          else if (teleinfo_line.etiquette == "EJPHPM")  teleinfo_counter.ejphpm  = teleinfo_line.donnee.toInt ();
        }
        break;

      // ---------------------
      // Ox03 : End of message
      // ---------------------
      case 3:
        // if needed, declare 3 phases
        if (Energy.phase_count != teleinfo_contract.phase) Energy.phase_count = teleinfo_contract.phase;

        // loop to calculate total current
        current_total = 0;
        for (phase = 0; phase < teleinfo_contract.phase; phase++) current_total += teleinfo_phase[phase].iinst;

        // loop to update current and power
        for (phase = 0; phase < teleinfo_contract.phase; phase++)
        {
          // if adir not set, reset to 0
          if (!teleinfo_phase[phase].adir_set) teleinfo_phase[phase].adir = 0;

          // calculate phase apparent power
          if (current_total == 0) teleinfo_phase[phase].papp = teleinfo_counter.papp / teleinfo_contract.phase;
          else teleinfo_phase[phase].papp = (teleinfo_counter.papp * teleinfo_phase[phase].iinst) / current_total;

          // update phase active power and instant current
          power_apparent = (float)teleinfo_phase[phase].papp;
          Energy.current[phase]        = (float)teleinfo_phase[phase].iinst;
          Energy.apparent_power[phase] = power_apparent;
          Energy.active_power[phase]   = power_apparent;
        } 

        // update total energy counter
        teleinfo_newtotal = teleinfo_counter.base + teleinfo_counter.hchc + teleinfo_counter.hchp;
        teleinfo_newtotal += teleinfo_counter.bbrhcjb + teleinfo_counter.bbrhpjb + teleinfo_counter.bbrhcjw + teleinfo_counter.bbrhpjw + teleinfo_counter.bbrhcjr + teleinfo_counter.bbrhpjr;
        teleinfo_newtotal += teleinfo_counter.ejphn + teleinfo_counter.ejphpm;
        teleinfo_delta = teleinfo_newtotal - teleinfo_counter.total;
        teleinfo_counter.total = teleinfo_newtotal;
        Energy.kWhtoday += (unsigned long)(teleinfo_delta * 100);
        EnergyUpdateToday ();

        // message update : if papp above ssousc
        if (teleinfo_counter.papp > teleinfo_contract.ssousc) teleinfo_json.updated = true;

        // message update : if more than 1% power change
        teleinfo_json.papp_delta = teleinfo_contract.ssousc / 100;
        if (abs (teleinfo_json.papp_last - teleinfo_counter.papp) > teleinfo_json.papp_delta) teleinfo_json.updated = true;

        // message update : if ADIR is above 100%
        for (phase = 0; phase < teleinfo_contract.phase; phase++) if (teleinfo_phase[phase].adir >= 100) teleinfo_json.updated = true;

        // increment message counter
        teleinfo_message.count++;

        // update teleinfo raw message
        teleinfo_message.content  = "COUNTER " + String (teleinfo_message.count) + "\n";
        teleinfo_message.content += teleinfo_message.buffer;
        teleinfo_message.buffer  = "";
        break;

      // if caracter is anything else printable, add it to current message part
      default:
         if ((teleinfo_line.part > TELEINFO_NONE) && isprint (recv_serial))
         {
           string_size = teleinfo_line.buffer.length ();
           if (string_size < CMDSZ) teleinfo_line.buffer += (char) recv_serial;
         }
        break;
    }
  }
}

void TeleinfoEverySecond ()
{
  uint8_t  period, phase;
  uint16_t current_papp;

  // loop thru the periods and the phases, to update apparent power to the max on the period
  for (period = 0; period < TELEINFO_PERIOD_MAX; period++)
  {
    // loop thru phases to update max value
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // get current apparent power
      current_papp = (uint16_t)teleinfo_phase[phase].papp;

      // if within range and greater, update phase apparent power
      if((current_papp != UINT16_MAX) && (teleinfo_graph[period].papp[phase] < current_papp)) teleinfo_graph[period].papp[phase] = current_papp;
    }

    // increment graph period counter and update graph data if needed
    if (teleinfo_graph[period].counter == 0) TeleinfoUpdateGraphData (period);
    teleinfo_graph[period].counter ++;
    teleinfo_graph[period].counter = teleinfo_graph[period].counter % arr_period_sample[period];
  }

  // if current or overload has been updated, publish teleinfo data
  if (teleinfo_json.updated) TeleinfoShowJSON (false);
}

// update graph history data
void TeleinfoUpdateGraphData (uint8_t graph_period)
{
  uint8_t phase;
  int     index;

  // set indexed graph values with current values
  index = teleinfo_graph[graph_period].index;
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // save graph data for current phase
    teleinfo_graph[graph_period].arr_papp[phase][index] = teleinfo_graph[graph_period].papp[phase];
    teleinfo_graph[graph_period].papp[phase] = 0;
  }

  // increase data index in the graph
  teleinfo_graph[graph_period].index++;
  teleinfo_graph[graph_period].index = teleinfo_graph[graph_period].index % TELEINFO_GRAPH_SAMPLE;

  // set update flag
  teleinfo_graph[graph_period].updated = 1;
}

// Show JSON status (for MQTT)
//  "TIC":{ "PHASE":3,"ADCO":"1234567890","ISOUSC":30,"SSOUSC":6000,"SINSTS":2567,"SINSTS1":1243,"IINST1":14.7,"ADIR1":0,"SINSTS2":290,"IINST2":4.4,"ADIR2":0,"SINSTS3":6500,"IINST3":33,"ADIR3":110 }
void TeleinfoShowJSON (bool append)
{
  uint8_t phase;
  long    power_percent;
  String  str_json, str_phase;

  // reset update flag and update published apparent power
  teleinfo_json.updated   = false;
  teleinfo_json.papp_last = teleinfo_counter.papp;

  // if not in append mode, add current time
  if (append == false) str_json = "\"" + String (D_JSON_TIME) + "\":\"" + GetDateAndTime(DT_LOCAL) + "\",";

  // start TIC section
  str_json += "\"" + String (TELEINFO_JSON_TIC) + "\":{";

  // if in append mode, add contract data
  if (append == true)
  {
    str_json += "\"" + String (TELEINFO_JSON_PHASE) + "\":" + String (teleinfo_contract.phase);
    str_json += ",\"" + String (TELEINFO_JSON_ADCO) + "\":\"" + teleinfo_contract.adco + "\"";
    str_json += ",\"" + String (TELEINFO_JSON_ISOUSC) + "\":" + String (teleinfo_contract.isousc);
    str_json += ",";
  }
 
  // add instant values
  str_json += "\"" + String (TELEINFO_JSON_SSOUSC) + "\":" + String (teleinfo_contract.ssousc);
  str_json += ",\"" + String (TELEINFO_JSON_SINSTS) + "\":" + String (teleinfo_counter.papp);
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // calculate adir (percentage)
    if (teleinfo_contract.ssousc > teleinfo_phase[phase].papp) power_percent = teleinfo_phase[phase].papp * 100 / teleinfo_contract.ssousc;
    else power_percent = 100;

    // add to JSON
    str_phase = String (phase + 1);
    str_json += ",\"" + String (TELEINFO_JSON_SINSTS) + str_phase + "\":" + String (teleinfo_phase[phase].papp);
    str_json += ",\"" + String (TELEINFO_JSON_IINST)  + str_phase + "\":" + String (teleinfo_phase[phase].iinst);
    str_json += ",\"" + String (TELEINFO_JSON_ADIR)   + str_phase + "\":" + String (power_percent);
  }

  // if needed, add Teleinfo sub-totals
  if (teleinfo_counter.base    != 0) str_json += ",\"" + String (TELEINFO_JSON_BASE)    + "\":" + String (teleinfo_counter.base);
  if (teleinfo_counter.hchc    != 0) str_json += ",\"" + String (TELEINFO_JSON_HCHC)    + "\":" + String (teleinfo_counter.hchc);
  if (teleinfo_counter.hchp    != 0) str_json += ",\"" + String (TELEINFO_JSON_HCHP)    + "\":" + String (teleinfo_counter.hchp);
  if (teleinfo_counter.bbrhcjb != 0) str_json += ",\"" + String (TELEINFO_JSON_BBRHCJB) + "\":" + String (teleinfo_counter.bbrhcjb);
  if (teleinfo_counter.bbrhpjb != 0) str_json += ",\"" + String (TELEINFO_JSON_BBRHPJB) + "\":" + String (teleinfo_counter.bbrhpjb);
  if (teleinfo_counter.bbrhcjw != 0) str_json += ",\"" + String (TELEINFO_JSON_BBRHCJW) + "\":" + String (teleinfo_counter.bbrhcjw);
  if (teleinfo_counter.bbrhpjw != 0) str_json += ",\"" + String (TELEINFO_JSON_BBRHPJW) + "\":" + String (teleinfo_counter.bbrhpjw);
  if (teleinfo_counter.bbrhcjr != 0) str_json += ",\"" + String (TELEINFO_JSON_BBRHCJR) + "\":" + String (teleinfo_counter.bbrhcjr);
  if (teleinfo_counter.bbrhpjr != 0) str_json += ",\"" + String (TELEINFO_JSON_BBRHPJR) + "\":" + String (teleinfo_counter.bbrhpjr);
  if (teleinfo_counter.ejphn   != 0) str_json += ",\"" + String (TELEINFO_JSON_EJPHN)   + "\":" + String (teleinfo_counter.ejphn);
  if (teleinfo_counter.ejphpm  != 0) str_json += ",\"" + String (TELEINFO_JSON_EJPHPM)  + "\":" + String (teleinfo_counter.ejphpm);

  // end of TIC section
  str_json += "}";

  // generate MQTT message according to append mode
  if (append) ResponseAppend_P (PSTR(",%s"), str_json.c_str ());
  else Response_P (PSTR("{%s}"), str_json.c_str ());

  // publish it if not in append mode
  if (!append) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// display offload icons
void TeleinfoWebIconTic () { Webserver->send_P (200, PSTR ("image/png"), teleinfo_icon_tic_png, teleinfo_icon_tic_len); }

// get tic raw message
void TeleinfoWebTicUpdate () { Webserver->send_P (200, PSTR ("text/plain"), teleinfo_message.content.c_str (), teleinfo_message.content.length ()); }

// get status update
void TeleinfoWebGraphUpdate ()
{
  uint8_t  graph_period = TELEINFO_LIVE;
  int      index;
  long     power_diff;
  float    power_read;
  uint32_t time_now;
  String   str_text;

  // timestamp
  time_now = millis ();

  // check graph period to be displayed
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++) if (Webserver->hasArg(arr_period_cmnd[index])) graph_period = index;

  // chech for graph update
  str_text = teleinfo_graph[graph_period].updated;

  // check power and power difference for each phase
  for (index = 0; index < teleinfo_contract.phase; index++)
  {
    // calculate power difference
    power_diff = teleinfo_phase[index].papp - teleinfo_phase[index].last_papp;

    // if power has changed
    str_text += ";";
    if (power_diff != 0)
    {
      teleinfo_phase[index].last_papp = teleinfo_phase[index].papp;
      str_text += teleinfo_phase[index].papp;
    }

    // if more than 100 VA power difference
    str_text += ";";
    if (abs (power_diff) >= 100)
    {
      if (power_diff < 0) str_text += "- " + String (abs (power_diff));
      else str_text += "+ " + String (power_diff);
      teleinfo_phase[index].last_diff = millis ();
    }

    // else if diff has been published 10 sec
    else if ((teleinfo_phase[index].last_diff != UINT32_MAX) && (time_now - teleinfo_phase[index].last_diff > 10000))
    {
      str_text += "---";
      teleinfo_phase[index].last_diff = UINT32_MAX;
    }
  }

  // send result
  Webserver->send_P (200, PSTR ("text/plain"), str_text.c_str (), str_text.length ());

  // reset update flags
  teleinfo_graph[graph_period].updated = 0;
}

// append Teleinfo graph button to main page
void TeleinfoWebMainButton ()
{
  // Teleinfo message page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_TELEINFO_TIC, D_TELEINFO_MESSAGE);

  // Teleinfo graph page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_TELEINFO_GRAPH, D_TELEINFO_GRAPH);
}

// append Teleinfo configuration button to configuration page
void TeleinfoWebButton ()
{
  // heater and meter configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_TELEINFO_CONFIG, D_TELEINFO_CONFIG);
}

// append Teleinfo state to main page
bool TeleinfoWebSensor ()
{
  // display last TIC data received
  WSContentSend_PD (PSTR("{s}%s{m}%d{e}"), D_TELEINFO_TIC, teleinfo_message.count);

  // display TIC checksum errors
  WSContentSend_PD (PSTR("{s}%s{m}%d{e}"), D_TELEINFO_ERROR, teleinfo_message.error);
}

// Teleinfo web page
void TeleinfoWebPageConfig ()
{
  bool     need_restart = false;
  uint16_t value;
  char     argument[LOGSZ];
  String   str_text;

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get teleinfo mode according to MODE parameter
  if (Webserver->hasArg(D_CMND_TELEINFO_MODE))
  {
    // set teleinfo speed
    WebGetArg (D_CMND_TELEINFO_MODE, argument, LOGSZ);
    TeleinfoSetMode ((uint16_t) atoi (argument));

    // ask for reboot
    WebRestart(1);
  }

  // beginning of form
  WSContentStart_P (D_TELEINFO_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (TELEINFO_FORM_START, D_PAGE_TELEINFO_CONFIG);

  // speed selection form : start
  value = TeleinfoGetMode ();
  WSContentSend_P (TELEINFO_FIELD_START, D_TELEINFO_MODE);

  // speed selection form : available modes
  if (value == 0) str_text = "checked"; else str_text = "";
  WSContentSend_P (TELEINFO_INPUT_TEXT, D_CMND_TELEINFO_MODE, 0, str_text.c_str (), D_TELEINFO_DISABLED);
  if (value == 1200) str_text = "checked"; else str_text = "";      // 1200 baud
  WSContentSend_P (TELEINFO_INPUT_VALUE, D_CMND_TELEINFO_MODE, 1200, str_text.c_str (), 1200, D_TELEINFO_SPEED_HISTO);
  if (value == 2400) str_text = "checked"; else str_text = "";      // 2400 baud
  WSContentSend_P (TELEINFO_INPUT_VALUE, D_CMND_TELEINFO_MODE, 2400, str_text.c_str (), 2400, D_TELEINFO_SPEED_DEFAULT);
  if (value == 4800) str_text = "checked"; else str_text = "";      // 4800 baud
  WSContentSend_P (TELEINFO_INPUT_VALUE, D_CMND_TELEINFO_MODE, 4800, str_text.c_str (), 4800, D_TELEINFO_SPEED_DEFAULT);
  if (value == 9600) str_text = "checked"; else str_text = "";      // 9600 baud
  WSContentSend_P (TELEINFO_INPUT_VALUE, D_CMND_TELEINFO_MODE, 9600, str_text.c_str (), 9600, D_TELEINFO_SPEED_STD);
  if (value == 19200) str_text = "checked"; else str_text = "";     // 19200 baud
  WSContentSend_P (TELEINFO_INPUT_VALUE, D_CMND_TELEINFO_MODE, 19200, str_text.c_str (), 19200, D_TELEINFO_SPEED_DEFAULT);

  // speed selection form : end
  WSContentSend_P (TELEINFO_FIELD_STOP);

  // save button
  WSContentSend_P (PSTR ("<button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (TELEINFO_FORM_STOP);

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// JSON data page
void TeleinfoWebPageData ()
{
  bool     first_value;
  uint8_t  phase;
  uint16_t index, index_array, value;
  int      graph_period = TELEINFO_LIVE;  

  // check graph period to be displayed
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++) if (Webserver->hasArg(arr_period_cmnd[index])) graph_period = index;

  // start of data page
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR("{\"period\":\"%s\""), arr_period_label[graph_period]);
  WSContentSend_P (PSTR(",\"adco\":%s"), teleinfo_contract.adco.c_str ());
  WSContentSend_P (PSTR(",\"ssousc\":%d"), teleinfo_contract.ssousc);
  WSContentSend_P (PSTR(",\"phase\":%d"), teleinfo_contract.phase);

  // loop thru phasis
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // init value list
    first_value = true;

    // loop for the apparent power array
    WSContentSend_P (PSTR(",\"%s\":["), arr_color_phase[phase]);
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      // get target power array position and add value if defined
      index_array = (index + teleinfo_graph[graph_period].index) % TELEINFO_GRAPH_SAMPLE;
      if (teleinfo_graph[graph_period].arr_papp[phase][index_array] == UINT16_MAX) value = 0; 
        else value = teleinfo_graph[graph_period].arr_papp[phase][index_array];

      // add value to JSON array
      if (first_value) WSContentSend_P (PSTR("%d"), value); else WSContentSend_P (PSTR(",%d"), value);

      // first value is over
      first_value = false;
    }
    WSContentSend_P (PSTR("]"));
  }

  // end of page
  WSContentSend_P (PSTR("}"));
  WSContentEnd();
}

// TIC information page
void TeleinfoWebPageTic ()
{
  int index     = 0;
  int pos_eol   = 0;
  int pos_space = 0;
  String str_text, str_line, str_header, str_data, str_checksum;

  // beginning of form without authentification
  WSContentStart_P (D_TELEINFO_TIC, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function update() {\n"));
  WSContentSend_P (PSTR (" httpUpd=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpUpd.onreadystatechange=function() {\n"));
  WSContentSend_P (PSTR ("  if (httpUpd.responseText.length>0) {\n"));
  WSContentSend_P (PSTR ("   arr_param=httpUpd.responseText.split('\\n');\n"));
  WSContentSend_P (PSTR ("   num_param=arr_param.length;\n"));
  WSContentSend_P (PSTR ("   arr_value=arr_param[0].split(' ');\n"));
  WSContentSend_P (PSTR ("   document.getElementById('count').textContent=arr_value[1];\n"));
  WSContentSend_P (PSTR ("   for (i=1;i<num_param;i++) {\n"));
  WSContentSend_P (PSTR ("    arr_value=arr_param[i].split(' ');\n"));
  WSContentSend_P (PSTR ("    document.getElementById('h'+i).textContent=arr_value[0];\n"));
  WSContentSend_P (PSTR ("    document.getElementById('d'+i).textContent=arr_value[1];\n"));
  WSContentSend_P (PSTR ("    document.getElementById('c'+i).textContent=arr_value[2];\n"));
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR (" httpUpd.open('GET','tic.upd',true);\n"));
  WSContentSend_P (PSTR (" httpUpd.send();\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("setInterval(function() {update();},1000);\n"));
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%;margin:12px auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:5vw;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("span {font-size:4vw;padding:0.5rem 1rem;border-radius:1rem;background:#4d82bd;}\n"));
  WSContentSend_P (PSTR ("table {display:inline-block;border:none;background:none;padding:0.5rem;color:#fff;border-collapse:collapse;}\n"));
  WSContentSend_P (PSTR ("th,td {font-size:3vw;padding:0.5rem 1rem;border:1px #666 solid;}\n"));
  WSContentSend_P (PSTR ("th {font-style:bold;background:#555;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  // meter name and teleinfo icon
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div><img height=92 src='tic.png'></div>\n"));

  // display counter
  WSContentSend_P (PSTR ("<div><span id='count'>%d</span></div>\n"), teleinfo_message.count);

  // display table with header
  WSContentSend_P (PSTR ("<div><table>\n"));
  WSContentSend_P (PSTR ("<tr><th>Etiquette</th><th>Valeur</th><th>Checksum</th></tr>\n"));

  // display TIC message
  str_text = teleinfo_message.content;
  while (pos_eol != -1)
  {
    // reset strings
    str_header   = "";
    str_data     = "";
    str_checksum = "";
    
    // extract current line
    pos_eol = str_text.indexOf ('\n');
    if (pos_eol != -1)
    {
      str_line = str_text.substring (0, pos_eol);
      str_text = str_text.substring (pos_eol + 1);
    }
    else str_line = str_text;

    // extract data from line
    pos_space = str_line.indexOf (' ');
    if (pos_space != -1)
    {
      str_header = str_line.substring (0, pos_space);
      str_line   = str_line.substring (pos_space + 1);
      pos_space  = str_line.indexOf (' ');
    }
    if (pos_space != -1)
    {
      str_data     = str_line.substring (0, pos_space);
      str_checksum = str_line.substring (pos_space + 1);
    }

    // display line
    if ((index > 0) && (str_header.length () > 0)) WSContentSend_P (PSTR ("<tr><td id='h%d'>%s</td><td id='d%d'>%s</td><td id='c%d'>%s</td></tr>\n"), index, str_header.c_str (), index, str_data.c_str (), index, str_checksum.c_str ());

    index++;
  }

  // end of table and end of page
  WSContentSend_P (PSTR ("</table></div>\n"));
  WSContentSend_P (PSTR ("<div>\n"));
  WSContentStop ();
}

// Apparent power graph frame
void TeleinfoWebGraphFrame ()
{
  uint8_t  graph_period = TELEINFO_LIVE;  
  uint8_t  phase;
  uint16_t index, power_papp;
  int graph_left, graph_right, graph_width;  

  // check graph period to be displayed
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++) if (Webserver->hasArg(arr_period_cmnd[index])) graph_period = index;

  // loop thru phasis and power records to calculate max power
  teleinfo_graph[graph_period].pmax = teleinfo_contract.ssousc;
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      // update max power during the period
      power_papp = teleinfo_graph[graph_period].arr_papp[phase][index];
      if ((power_papp != UINT16_MAX) && (power_papp > teleinfo_graph[graph_period].pmax )) teleinfo_graph[graph_period].pmax = power_papp;
    }

  // boundaries of SVG graph
  graph_left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100;
  graph_right = TELEINFO_GRAPH_PERCENT_STOP * TELEINFO_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d' preserveAspectRatio='xMinYMinmeet'>\n"), 0, 0, TELEINFO_GRAPH_WIDTH, TELEINFO_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("text.power {font-size:20px;stroke:white;fill:white;}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:16px;fill:white;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // graph frame
  WSContentSend_P (PSTR ("<rect x='%d%%' y='%d%%' width='%d%%' height='%d%%' rx='10' />\n"), TELEINFO_GRAPH_PERCENT_START, 0, TELEINFO_GRAPH_PERCENT_STOP - TELEINFO_GRAPH_PERCENT_START, 100);

  // graph separation lines
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 25, TELEINFO_GRAPH_PERCENT_STOP, 25);
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 50, TELEINFO_GRAPH_PERCENT_STOP, 50);
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 75, TELEINFO_GRAPH_PERCENT_STOP, 75);

  // power units
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>VA</text>\n"), TELEINFO_GRAPH_PERCENT_STOP + 2, 4);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 4,  teleinfo_graph[graph_period].pmax );
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 26, teleinfo_graph[graph_period].pmax  * 3 / 4);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 51, teleinfo_graph[graph_period].pmax  / 2);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 76, teleinfo_graph[graph_period].pmax  / 4);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 99, 0);

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd();
}

// Apparent power graph curve
void TeleinfoWebGraphData ()
{
  uint8_t  graph_period = TELEINFO_LIVE;  
  uint8_t  phase;
  uint16_t index, index_array;
  long     graph_left, graph_right, graph_width;  
  long     graph_x, graph_y;  
  long     unit_width, shift_unit, shift_width;  
  uint16_t power_papp;
  TIME_T   current_dst;
  uint32_t current_time;
  String   str_text;

  // check graph period to be displayed
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++) if (Webserver->hasArg(arr_period_cmnd[index])) graph_period = index;

  // boundaries of SVG graph
  graph_left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100;
  graph_right = TELEINFO_GRAPH_PERCENT_STOP * TELEINFO_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d' preserveAspectRatio='xMinYMinmeet'>\n"), 0, 0, TELEINFO_GRAPH_WIDTH, TELEINFO_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("polyline {fill:none;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.ph1 {stroke:yellow;}\n"));
  WSContentSend_P (PSTR ("polyline.ph2 {stroke:orange;}\n"));
  WSContentSend_P (PSTR ("polyline.ph3 {stroke:red;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:16px;fill:grey;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // -----------------
  //   Power curves
  // -----------------

  // if a maximum power is defined, loop thru phasis
  if (teleinfo_graph[graph_period].pmax > 0) for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // loop for the apparent power graph
    WSContentSend_P (PSTR ("<polyline class='%s' points='"), arr_color_phase[phase]);
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      // get target power value
      index_array = (index + teleinfo_graph[graph_period].index) % TELEINFO_GRAPH_SAMPLE;
      power_papp = teleinfo_graph[graph_period].arr_papp[phase][index_array];

      // if power is defined
      if (power_papp != UINT16_MAX)
      {
        // calculate current position and add the point to the line
        graph_x = graph_left + (graph_width * index / TELEINFO_GRAPH_SAMPLE);
        graph_y = TELEINFO_GRAPH_HEIGHT - (power_papp * TELEINFO_GRAPH_HEIGHT / teleinfo_graph[graph_period].pmax);
        WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
      }
    }
    WSContentSend_P (PSTR("'/>\n"));
  }

  // ------------
  //     Time
  // ------------

  // get current time
  current_time = LocalTime();
  BreakTime (current_time, current_dst);

  // handle graph units according to period
  switch (graph_period) 
  {
    case TELEINFO_LIVE:
      // calculate horizontal shift
      unit_width  = graph_width / 6;
      shift_unit  = current_dst.minute % 5;
      shift_width = unit_width - (unit_width * shift_unit / 5) - (unit_width * current_dst.second / 300);

      // calculate first time displayed by substracting (5 * 5mn + shift) to current time
      current_time -= (5 * 300) + (shift_unit * 60); 

      // display 5 mn separation lines with time
      for (index = 0; index < 6; index++)
      {
        // convert back to date and increase time of 5mn
        BreakTime (current_time, current_dst);
        current_time += 300;

        // display separation line and time
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%02dh%02d</text>\n"), graph_x - 25, 55, current_dst.hour, current_dst.minute);
      }
      break;

    case TELEINFO_DAY:
      // calculate horizontal shift
      unit_width  = graph_width / 6;
      shift_unit  = current_dst.hour % 4;
      shift_width = unit_width - (unit_width * shift_unit / 4) - (unit_width * current_dst.minute / 240);

      // calculate first time displayed by substracting (5 * 4h + shift) to current time
      current_time -= (5 * 14400) + (shift_unit * 3600); 

      // display 4 hours separation lines with hour
      for (index = 0; index < 6; index++)
      {
        // convert back to date and increase time of 4h
        BreakTime (current_time, current_dst);
        current_time += 14400;

        // display separation line and time
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%02dh</text>\n"), graph_x - 15, 55, current_dst.hour);
      }
      break;

    case TELEINFO_WEEK:
      // calculate horizontal shift
      unit_width = graph_width / 7;
      shift_width = unit_width - (unit_width * current_dst.hour / 24) - (unit_width * current_dst.minute / 1440);

      // display day lines with day name
      current_dst.day_of_week --;
      for (index = 0; index < 7; index++)
      {
        // calculate next week day
        current_dst.day_of_week ++;
        current_dst.day_of_week = current_dst.day_of_week % 7;

        // display month separation line and week day (first days or current day after 6pm)
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        if ((index < 6) || (current_dst.hour >= 18)) WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%s</text>\n"), graph_x + 30, 53, arr_week_day[current_dst.day_of_week]);
      }
      break;

    case TELEINFO_MONTH:
      // calculate horizontal shift
      unit_width  = graph_width / 6;
      shift_unit  = current_dst.day_of_month % 5;
      shift_width = unit_width - (unit_width * shift_unit / 5) - (unit_width * current_dst.hour / 120);

      // calculate first time displayed by substracting (5 * 5j + shift en j) to current time
      current_time -= (5 * 432000) + (shift_unit * 86400); 

      // display 5 days separation lines with day number
      for (index = 0; index < 6; index++)
      {
        // convert back to date and increase time of 5 days
        BreakTime (current_time, current_dst);
        current_time += 432000;

        // display separation line and day of month
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%02d</text>\n"), graph_x - 10, 55, current_dst.day_of_month);
      }
      break;
      
    case TELEINFO_YEAR:
      // calculate horizontal shift
      unit_width = graph_width / 12;
      shift_width = unit_width - (unit_width * current_dst.day_of_month / 30);

      // display month separation lines with month name
      for (index = 0; index < 12; index++)
      {
        // calculate next month value
        current_dst.month = (current_dst.month % 12);
        current_dst.month++;

        // convert back to date to get month name
        current_time = MakeTime (current_dst);
        BreakTime (current_time, current_dst);

        // display month separation line and month name (if previous month or current month after 20th)
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        if ((index < 11) || (current_dst.day_of_month >= 24)) WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%s</text>\n"), graph_x + 12, 53, current_dst.name_of_month);
      }
      break;
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd();
}

// Graph public page
void TeleinfoWebPageGraph ()
{
  uint8_t graph_period = TELEINFO_LIVE;  
  int     index, idx_param;
  String  str_text;

  // check graph period to be displayed
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++) if (Webserver->hasArg(arr_period_cmnd[index])) graph_period = index;
  
  // beginning of form without authentification
  WSContentStart_P (D_TELEINFO_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function update() {\n"));
  WSContentSend_P (PSTR ("httpUpd=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR ("httpUpd.onreadystatechange=function() {\n"));
  WSContentSend_P (PSTR (" if (httpUpd.responseText.length>0) {\n"));
  WSContentSend_P (PSTR ("  arr_param=httpUpd.responseText.split(';');\n"));
  WSContentSend_P (PSTR ("  str_random=Math.floor(Math.random()*100000);\n"));
  WSContentSend_P (PSTR ("  if (arr_param[0]==1) {document.getElementById('data').data='data.svg?%s&rnd='+str_random;}\n"), arr_period_cmnd[graph_period]);
  for (index = 0; index < teleinfo_contract.phase; index++)
  {
    idx_param = 1 + index *2;
    WSContentSend_P (PSTR ("  if (arr_param[%d]!='') {document.getElementById('ph%d').innerHTML=arr_param[%d]+' VA';}\n"), idx_param, index + 1, idx_param);
    WSContentSend_P (PSTR ("  if (arr_param[%d]!='') {document.getElementById('dph%d').innerHTML=arr_param[%d];}\n"), idx_param + 1, index + 1, idx_param + 1);
  }
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("httpUpd.open('GET','graph.upd?%s',true);\n"), arr_period_cmnd[graph_period]);
  WSContentSend_P (PSTR ("httpUpd.send();\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("setInterval(function() {update();},1000);\n"));
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:12px auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR (".title {font-size:5vw;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.papp {display:inline-block;width:auto;padding:0.25rem 0.75rem;margin:0.5rem;border-radius:8px;}\n"));
  WSContentSend_P (PSTR ("div.papp span.power {font-size:4vw;}\n"));
  WSContentSend_P (PSTR ("div.papp span.diff {font-size:2vw;font-style:italic;}\n"));
  WSContentSend_P (PSTR ("div.ph1 {color:yellow;border:1px yellow solid;}\n"));
  WSContentSend_P (PSTR ("div.ph1 span {color:yellow;}\n"));
  WSContentSend_P (PSTR ("div.ph2 {color:orange;border:1px orange solid;}\n"));
  WSContentSend_P (PSTR ("div.ph2 span {color:orange;}\n"));
  WSContentSend_P (PSTR ("div.ph3 {color:red;border:1px red solid;}\n"));
  WSContentSend_P (PSTR ("div.ph3 span {color:red;}\n"));
  WSContentSend_P (PSTR (".button {font-size:2vw;padding:0.5rem 1rem;border:1px #666 solid;background:none;color:#fff;border-radius:8px;}\n"));
  WSContentSend_P (PSTR (".active {background:#666;}\n"));
  WSContentSend_P (PSTR (".svg-container {position:relative;vertical-align:middle;overflow:hidden;width:100%%;max-width:%dpx;padding-bottom:65%%;}\n"), TELEINFO_GRAPH_WIDTH);
  WSContentSend_P (PSTR (".svg-content {display:inline-block;position:absolute;top:0;left:0;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (TELEINFO_FORM_START, D_PAGE_TELEINFO_GRAPH);

  // room name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));

  // display icon
  WSContentSend_P (PSTR ("<div><img height=64 src='tic.png'></div>\n"));

  // display values
  WSContentSend_P (PSTR ("<div>\n"));
  for (index = 0; index < teleinfo_contract.phase; index++) WSContentSend_P (PSTR ("<div class='papp %s'><span class='power' id='%s'>%s VA</span><br><span class='diff' id='d%s'>---</span></div>\n"), arr_color_phase[index], arr_color_phase[index], String (Energy.apparent_power[index], 0).c_str (), arr_color_phase[index]);
  WSContentSend_P (PSTR ("</div>\n"));

  // display tabs
  WSContentSend_P (PSTR ("<div>\n"));
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++)
  {
    // if tab is the current graph period
    if (graph_period == index) str_text = "active";
    else str_text = "";

    // display button
    WSContentSend_P (PSTR ("<button name='%s' class='button %s'>%s</button>\n"), arr_period_cmnd[index], str_text.c_str (), arr_period_label[index]);
  }
  WSContentSend_P (PSTR ("</div>\n"));

  // display graph base and data
  WSContentSend_P (PSTR ("<div class='svg-container'>\n"));
  WSContentSend_P (PSTR ("<object class='svg-content' id='base' type='image/svg+xml' width='%d%%' height='%d%%' data='%s?%s'></object>\n"), 100, 100, D_PAGE_TELEINFO_BASE_SVG, arr_period_cmnd[graph_period]);
  WSContentSend_P (PSTR ("<object class='svg-content' id='data' type='image/svg+xml' width='%d%%' height='%d%%' data='%s?%s&ts=0'></object>\n"), 100, 100, D_PAGE_TELEINFO_DATA_SVG, arr_period_cmnd[graph_period]);
  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
  WSContentSend_P (TELEINFO_FORM_STOP);
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// energy driver
bool Xnrg15 (uint8_t function)
{
  // swtich according to context
  switch (function)
  {
    case FUNC_PRE_INIT:
      TeleinfoPreInit ();
      break;
    case FUNC_INIT:
      TeleinfoInit ();
      break;
  }
  return false;
}

// teleinfo sensor
bool Xsns15 (uint8_t function)
{
  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      TeleinfoGraphInit ();
      break;
    case FUNC_EVERY_50_MSECOND:
      if (teleinfo_enabled && (TasmotaGlobal.uptime > 4)) TeleinfoEvery50ms ();
      break;
    case FUNC_EVERY_SECOND:
      TeleinfoEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      if (teleinfo_enabled) TeleinfoShowJSON (true);
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      // pages
      Webserver->on ("/" D_PAGE_TELEINFO_CONFIG,   TeleinfoWebPageConfig);
      Webserver->on ("/" D_PAGE_TELEINFO_TIC,      TeleinfoWebPageTic);
      Webserver->on ("/" D_PAGE_TELEINFO_DATA,     TeleinfoWebPageData);

      // graph
      Webserver->on ("/" D_PAGE_TELEINFO_GRAPH,    TeleinfoWebPageGraph);
      Webserver->on ("/" D_PAGE_TELEINFO_BASE_SVG, TeleinfoWebGraphFrame);
      Webserver->on ("/" D_PAGE_TELEINFO_DATA_SVG, TeleinfoWebGraphData);
      
      // update status
      Webserver->on ("/graph.upd", TeleinfoWebGraphUpdate);
      Webserver->on ("/tic.upd",   TeleinfoWebTicUpdate);

      // icons
      Webserver->on ("/tic.png", TeleinfoWebIconTic);
      break;
    case FUNC_WEB_ADD_BUTTON:
      TeleinfoWebButton ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoWebMainButton ();
      break;
    case FUNC_WEB_SENSOR:
      TeleinfoWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }
  return false;
}

#endif   // USE_TELEINFO
#endif   // USE_ENERGY_SENSOR

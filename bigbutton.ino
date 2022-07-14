#include <stdio.h>
#include <HTTPClient.h>
#include <WebServer.h>
#define USE_MDNS
#ifdef USE_MDNS
#include <ESPmDNS.h>
#endif
#include <WiFiManager.h>
#define USE_NIMBLE
#include <BleKeyboard.h>
WebServer server(80);
#include <ArduinoJson.h>
WiFiManager wifiManager;
#include "time.h"

#include "SPIFFS.h"
#include <EEPROM.h>

#define TUYALOCAL
#ifdef TUYALOCAL
#include "TuyaLocal.hpp"
#endif
#define EEPROM_SIZE 100
TaskHandle_t Task1;
/*
   Login page
*/
const char* loginIndex =
  "<form name='loginForm'>"
  "<table width='20%' bgcolor='A09F9F' align='center'>"
  "<tr>"
  "<td colspan=2>"
  "<center><font size=4><b>Big Button Login Page</b></font></center>"
  "<br>"
  "</td>"
  "<br>"
  "<br>"
  "</tr>"
  "<td>Username:</td>"
  "<td><input type='text' size=25 name='userid'><br></td>"
  "</tr>"
  "<br>"
  "<br>"
  "<tr>"
  "<td>Password:</td>"
  "<td><input type='Password' size=25 name='pwd'><br></td>"
  "<br>"
  "<br>"
  "</tr>"
  "<tr>"
  "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
  "</tr>"
  "</table>"
  "</form>"
  "<script>"
  "function check(form)"
  "{"
  "if(form.userid.value=='admin' && form.pwd.value=='admin')"
  "{"
  "window.open('/serverIndex')"
  "}"
  "else"
  "{"
  " alert('Error Password or Username')/*displays error message*/"
  "}"
  "}"
  "</script>";

/*
   Server Index Page
*/

const char* serverIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<input type='file' name='update'>"
  "<input type='submit' value='Update'>"
  "</form>"
  "<div id='prg'>progress: 0%</div>"
  "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "</script>";



void(* resetFunc) (void) = 0; //declare reset function @ address 0

const char *settings_file = "/settings.txt";

const char *profiles_file = "/profiles.txt";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

const char *profiles_key = "profiles";
const char *SELECTED = "selected=\"selected\"";
const char *LED_ON_PRESSED = "led_on_press";


BleKeyboard bleKeyboard("BigButton");
#define TUYA_COMMAND_PORT 6668
#include "TuyaAuth.h"
TuyaAuth *tuya;
#ifdef TUYALOCAL

#endif
//#include <ezButton.h>
#define BUTTON_PIN 15
//ezButton button(BUTTON_PIN);  // create ezButton object that attach to pin 7;

#define LED_PIN 32

char device_name[20];


void writeId(const char *id) {
  for (int i = 0; i <= strlen(id); i++) {
    EEPROM.write(i, id[i]);
  }
  unsigned long calculatedCrc = eeprom_crc();
  EEPROM.put(EEPROM.length() - 4, calculatedCrc);

}


void readId(char *buffer) {
  for (int i = 0; i < 20; i++) {

    buffer[i] = EEPROM.read(i);
  }
}

void init_eeprom() {
  unsigned long calculatedCrc = eeprom_crc();

  // get stored crc
  unsigned long storedCrc;
  EEPROM.get(EEPROM.length() - 4, storedCrc);

  if (storedCrc != calculatedCrc) {
    writeId("bigbutton");
  }

}

unsigned long eeprom_crc(void)
{

  const unsigned long crc_table[16] =
  {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };

  unsigned long crc = ~0L;

  for (int index = 0 ; index < EEPROM.length() - 4  ; ++index)
  {
    byte b = EEPROM.read(index);
    crc = crc_table[(crc ^ b) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (b >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }
  return crc;
}



bool readFile(String name, String &buffer) {

  buffer = "";
  File file = SPIFFS.open(name, FILE_READ);
  if (file) {
    buffer.reserve(file.size());

    while (file.available()) {
      const char c = file.read();

      if (c != '\0') buffer += c;
    }
    Serial.println("Read file:"); Serial.println(name);
    Serial.println(buffer);
    return true;
  } else {

    Serial.print("error reading:"); Serial.println(name);
    return false;
  }
}



class CButtonProfile {

  public:
    String name;
    int power_off_time;

    int press_mode;
    int led_mode;
    int repeat_time;
    int pressrelease_time;
    int tuya_mode;
    String  press_string;
    String  tuya_device_id;
    String tuya_api_key;
    String tuya_api_client;
    String tuya_host;
    String tuya_ip;
    // switch command from cloud
    String tuya_switch;
    String tuya_localkey;
    String tuya_protocol;
    bool tuya_localkeyfound;
    bool tuya_have_status;
    bool tuya_on;
    bool led_on;
    bool button_on;
    long ms;

    bool tuya_error;
    String tuya_error_string;
    bool tuya_local;

};

#define MAX_PROFILES 10
CButtonProfile profile;
String profiles[MAX_PROFILES];

int profile_count = 0;

int haveProfile(String &name) {
  for (int i = 0; i < profile_count; i++) {
    if (profiles[i] == name) {
      return i;
    }
  }
  return -1;
}

void verifyProfile() {

}

void status( void * parameter) {


  for (;;) {
    
   
    if (tuya) {

      if (tuya->TGetOnStatus(profile.tuya_device_id.c_str(), profile.tuya_switch.c_str(), profile.tuya_on)) {
        //Serial.print("Switch is: "); Serial.println(profile.tuya_on);
        profile.tuya_error = false;
      } else {
        profile.tuya_error = true;
        profile.tuya_error_string = tuya->getError();
      }
    }
    
  
    delay(30000);
  }
  
 }

#ifdef TUYALOCAL
bool local_switch(bool on) {
  unsigned char message_buffer[1024];
  int numbytes;

  //String   ss_payload = String("{\"devId\":\"") + String(profile.tuya_device_id) + String("\",\"uid\":\"\",") +String("\"gwId\":\"") + String( profile.tuya_device_id) + String( "\",\"dps\":{\"1\":");
  long currenttime = time(NULL) ;
  
  String   ss_payload = String("{\"devId\":\"") + 
         String(profile.tuya_device_id) +String("\",\"gwId\":\"") + String( profile.tuya_device_id)+ 
         String("\",\"uid\":\"\",") + String("\"t\":") + String( currenttime) + String(",\"dps\":{\"1\":");
  if (on) {
    ss_payload += "true}}";
  } else {
    ss_payload += "false}}";
  }
  

  Serial.print("building switch payload: "); Serial.println( ss_payload);

   
  tuyaLocal *tuyaclient = new tuyaLocal(profile.tuya_ip,profile.tuya_device_id,profile.tuya_localkey,profile.tuya_protocol.c_str());

    
  int payload_len = tuyaclient->BuildTuyaMessage(message_buffer, TUYA_CONTROL, ss_payload,true);

  if (!tuyaclient->ConnectToDevice()) {
    Serial.println("No local connection");
    profile.tuya_local = false;
    return false;
  } else {
    Serial.println("Made local connection");
    profile.tuya_local = true;
  }
  numbytes = tuyaclient->send(message_buffer, payload_len);
  if (numbytes < 0) {
    Serial.print("ERROR writing to socket: ");
    Serial.println(numbytes);
  }

  numbytes = tuyaclient->receive(message_buffer, 1023,30);
  if (numbytes < 0) {
    Serial.print("ERROR reading from socket: ");
    Serial.println(numbytes);
  }

  String tuyaresponse = tuyaclient->DecodeTuyaMessage(message_buffer, numbytes ).c_str();
  Serial.print("TUYA SAYS: " );Serial.println(tuyaresponse);

    Serial.println(tuyaclient->getDps());
  
   delete tuyaclient;
  return true;
}

#endif
bool tuya_on() {

  if (tuya) {
#ifdef TUYALOCAL
    if (profile.tuya_localkey.length() > 0) {
      Serial.println("local switch on");
      if (local_switch(true)){
        profile.tuya_on = true;
        return true;
      }
    } else {
      
#endif
      String command = String( "{\"commands\":[{\"code\":\"") + profile.tuya_switch + String("\",\"value\":true}]}");
      if ( tuya->TCommand(profile.tuya_device_id.c_str(), command.c_str())) {
        profile.tuya_on = true;
        return true;
      }
#ifdef TUYALOCAL      
    }
#endif    
  }
  return false;
}

bool tuya_off() {

  if (tuya) {
 #ifdef TUYALOCAL
    if (profile.tuya_localkey.length() > 0) {
      Serial.println("local switch off");
      if (local_switch(false)){
         profile.tuya_on = false;
         return true;
      }
    } else {
      
#endif   
  
    String command = String( "{\"commands\":[{\"code\":\"") + profile.tuya_switch + String("\",\"value\":false}]}");
    if ( tuya->TCommand(profile.tuya_device_id.c_str(), command.c_str())) {
      profile.tuya_on = false;
    }  
#ifdef TUYALOCAL      
    }
#endif    
  }
  return false;
}

void writeCurrentProfile() {

  char buffer[1000];

  Serial.print("WriteCurrentProfile: "); Serial.println(profile.name);
  Serial.print("Profile index: "); Serial.println(haveProfile(profile.name));
  strcpy(buffer, " {\"t_device_id\":\""); strcat(buffer, profile.tuya_device_id.c_str());
  strcat(buffer,   "\",\"t_protocol\": \"" );  strcat(buffer, String(profile.tuya_protocol).c_str());
  strcat(buffer,   "\",\"l_mode\": \"" );  strcat(buffer, String(profile.led_mode).c_str());
  strcat(buffer,   "\",\"p_string\":\""); strcat(buffer, profile.press_string.c_str());
  strcat(buffer, "\",\"name\":\""); strcat(buffer, profile.name.c_str());
  strcat(buffer, "\",\"t_api_key\": \""); strcat(buffer, profile.tuya_api_key.c_str());
  strcat(buffer, "\",\"t_api_client\": \""); strcat(buffer, profile.tuya_api_client.c_str());
  strcat(buffer,   "\",\"t_host\": \"");  strcat(buffer, profile.tuya_host.c_str());
  strcat(buffer,   "\",\"t_localkey\": \"");  strcat(buffer, profile.tuya_localkey.c_str());
  strcat(buffer,   "\",\"t_ip\": \"");  strcat(buffer, profile.tuya_ip.c_str());
  strcat(buffer,   "\",\"t_mode\": \""); strcat(buffer, String(profile.tuya_mode).c_str());
  strcat(buffer,   "\",\"poff_time\" :\""); strcat(buffer, String(profile.power_off_time).c_str());
  strcat(buffer,   "\",\"repeat_time\" :\""); strcat(buffer, String(profile.repeat_time).c_str());
  strcat(buffer,   "\",\"pressrelease_time\" :\""); strcat(buffer, String(profile.pressrelease_time).c_str());
  strcat(buffer,  "\",\"p_mode\": \""); strcat(buffer, String(profile.press_mode).c_str());
  strcat(buffer, "\"}");

  String filename = String("/") + String(haveProfile(profile.name)) + ".js";
  SPIFFS.remove(filename);
  File file = SPIFFS.open(filename, FILE_WRITE);

  int l = strlen(buffer);
  Serial.print("Length: "); Serial.println(l);
  Serial.print(buffer);

  for (int i = 0; i <= l; i++) {
    file.write(buffer[i]);
  }
  file.close();
}

void writeSettings() {
  File file = SPIFFS.open(settings_file, FILE_WRITE);
  char buffer[3];
  sprintf(buffer, "%d", haveProfile(profile.name));
  Serial.print("writeSettings: "); Serial.println(buffer);
  for (int i = 0; i <= strlen(buffer); i++) {
    file.write(buffer[i]);
  }
  file.write('\0');
  file.close();
}

void saveSettings() {
  writeProfiles();
  writeSettings();
}

void init_default_profile() {
  Serial.println("init profile");
  profile.name = String("Default");
  profile_count = 1;
  profiles[0] = profile.name;
  saveSettings();
  writeCurrentProfile();
}



bool readSettings(String & buffer) {
  if (SPIFFS.exists(settings_file)) {

    if ( readFile(settings_file, buffer)) {

      Serial.print("Profile in settings: ");
      Serial.println(buffer);

      int profile_number = strtol(buffer.c_str(), NULL, 10);

      buffer =  String(profile_number);
      Serial.print("Read setting: "); Serial.println(buffer);
      return true;
    }

  }
  return false;
}

bool readProfile(int profilenumber, String & buffer) {

  Serial.print("read profile: "); Serial.print(buffer);
  String name = String("/") + String(profilenumber) + ".js";

  if (readFile(name, buffer)) {
    return true;
  }
  return false;
}

void installProfile(int profilenumber) {

  String buffer;
  readProfile(profilenumber, buffer);
  DynamicJsonDocument root(4000);
  Serial.println("install profile");
  DeserializationError err = deserializeJson(root, buffer);


  if (!err) {

    if (root.containsKey("name")) {
      profile.name = (const char *) root["name"];
    } else Serial.println("missing name");
    if (root.containsKey("l_mode")) {
      profile.led_mode = root["l_mode"];
    } else Serial.println("missing l_mode");
    if (root.containsKey("t_protocol")) {
      profile.tuya_protocol =  (const char *)root["t_protocol"];
    } else Serial.println("missing p_mode");
    if (root.containsKey("p_mode")) {
      profile.press_mode = root["p_mode"];
    } else Serial.println("missing p_mode");
    if (root.containsKey("t_mode")) {
      profile.tuya_mode = root["t_mode"];
    } else Serial.println("missing t_mode");
    if (root.containsKey("repeat_time")) {
      profile.repeat_time = root["repeat_time"];
    } else Serial.println("missing repeat_time");
    if (root.containsKey("pressrelease_time")) {
      profile.pressrelease_time = root["pressrelease_time"];
    } else Serial.println("missing pressrelease_time");
    if (root.containsKey("poff_time")) {
      profile.power_off_time = root["poff_time"];
    } else Serial.println("missing poff_time");
    if (root.containsKey("t_device_id")) {
      profile.tuya_device_id =   (const char *)root["t_device_id"];
    } else Serial.println("missing t_device_id");
    if (root.containsKey("t_api_client")) {
      profile.tuya_api_client =  (const char *)root["t_api_client"];
    } else Serial.println("missing t_api_client");
    if (root.containsKey("t_host")) {
      profile.tuya_host =   (const char *)root["t_host"];
    } else Serial.println("missing t_host");
    if (root.containsKey("t_localkey")) {
      profile.tuya_localkey =   (const char *)root["t_localkey"];
    } else Serial.println("missing t_localkey");
    if (root.containsKey("t_ip")) {
      profile.tuya_ip =   (const char *)root["t_ip"];
    } else Serial.println("missing t_ip");
    if (root.containsKey("t_api_key")) {
      profile.tuya_api_key =  (const char *)root["t_api_key"];
    } else Serial.println("missing t_api_key");

    if (root.containsKey("p_string")) {
      profile.press_string =  (const char *)root["p_string"];
    } else Serial.println("missing p_string");
    if (root.containsKey("t_device_id")) {
      profile.tuya_device_id =  (const char *)root["t_device_id"];
    } else Serial.println("missing t_device_id");

    if (profile.name != profiles[profilenumber]) {
      Serial.println("profile mismatch");
      Serial.print("index: "); Serial.println( profiles[profilenumber]);
      Serial.print("json: "); Serial.println( profile.name);
      profile.name =  profiles[profilenumber];
    }
  } else {
    Serial.println("json failed:");
    Serial.println(err.f_str());
    Serial.println(buffer);

    init_default_profile();

  }
}



bool writeProfiles() {
  auto file = SPIFFS.open(profiles_file, FILE_WRITE);
  Serial.println("Write Profiles");
  if (file) {
    for (int i = 0; i < profile_count; i++) {

      Serial.println(profiles[i]);
      for (int j = 0; j < profiles[i].length(); j++) {
        file.write(profiles[i][j]);
      }
      file.write('\n');

    }
    file.close();
    return true;
  }
  return false;
}


bool readProfiles() {
  char buffer[500];
  profile_count = 0;
  auto file = SPIFFS.open(profiles_file, FILE_READ);
  if (file) {
    int pos = 0;

    while (file.available() && profile_count < MAX_PROFILES) {
      const char c = file.read();
      if (c != '\n' ) {
        buffer[pos++] = c;
      } else {
        buffer[pos++] = '\0';
        profiles[profile_count++] = String(buffer);
        Serial.println(profiles[profile_count - 1]);
        pos = 0;
      }

    }
    Serial.print("Read profiles :"); Serial.println(profile_count);

  }
  if (profile_count == 0) {
    profile_count = 1;
    profiles[0] = "Default";
  }
  return true;

}

#define LED_ON_PRESS 0
#define LED_OFF_PRESS 1
#define LED_ON 2
#define LED_OFF 3
#define LED_TOGGLE_PRESS 4
#define LED_TUYA 5

#define PRESS_PRINT 0
#define PRESS_PRESSRELEASE 1
#define PRESS_NONE 2

#define TUYA_ON 3
#define TUYA_OFF 1
#define TUYA_TOGGLE 2
#define TUYA_NONE 0


const char style_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
<style>
.doton {
  height: 400px;
  width: 400px;
  background-color: #f00;
  border-radius: 50%;
  display: inline-block;
}

.dotoff {
  height: 400px;
  width: 400px;
  background-color: #000;
  border-radius: 50%;
  display: inline-block;
}
.form-style-2{
  max-width: 800px;
  padding: 20px 12px 10px 20px;
  font: 13px Arial, Helvetica, sans-serif;
}
.form-style-2-heading{
  font-weight: bold;
  font-style: italic;
  border-bottom: 2px solid #ddd;
  margin-bottom: 20px;
  font-size: 15px;
  padding-bottom: 3px;
}
.form-style-2 label{
  display: block;
  margin: 0px 0px 15px 0px;
}
.form-style-2 label > span{
  width: 170px;
  font-weight: bold;
  float: left;
  padding-top: 8px;
  padding-right: 5px;
}
.form-style-2 span.required{
  color:red;

.form-style-2 input.input-field, .form-style-2 .select-field{
  width: 48%; 
}
.form-style-2 input.input-field, 
 .form-style-2 .select-field{
  box-sizing: border-box;
  -webkit-box-sizing: border-box;
  -moz-box-sizing: border-box;
  border: 1px solid #C2C2C2;
  box-shadow: 1px 1px 4px #EBEBEB;
  -moz-box-shadow: 1px 1px 4px #EBEBEB;
  -webkit-box-shadow: 1px 1px 4px #EBEBEB;
  border-radius: 3px;
  -webkit-border-radius: 3px;
  -moz-border-radius: 3px;
  padding: 7px;
  outline: none;
}
.form-style-2 .input-field:focus, 
.form-style-2 .select-field:focus{
  border: 1px solid #0C0;
}

.form-style-2 input[type=submit],
.form-style-2 input[type=button]{
  border: none;
  padding: 8px 15px 8px 15px;
  background: #FF8500;
  color: #fff;
  box-shadow: 1px 1px 4px #DADADA;
  -moz-box-shadow: 1px 1px 4px #DADADA;
  -webkit-box-shadow: 1px 1px 4px #DADADA;
  border-radius: 3px;
  -webkit-border-radius: 3px;
  -moz-border-radius: 3px;
}
.form-style-2 input[type=submit]:hover,
.form-style-2 input[type=button]:hover{
  background: #EA7B00;
  color: #fff;
}

</style>
)rawliteral";

const char index_html[] PROGMEM = R"rawliteral(
  <title>Big Button Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  %REBOOT%<br />Status: %STATUS%
  <div class="form-style-2">
  <div class="form-style-2-heading">Provide your information</div>
  <form action="/setting" method="POST">
 <label for="profile_list"><span>Profile</span>%PROFILE_LIST%</label>
 <label for="profile"><span>Edit Profile Name</span><input name="profile" id="profile" value="%PROFILE%" ></label>
  <label for="p_string"><span>Bluetooth Press String</span><input name="p_string" id="p_string" value="%P_STRING%" ></label>
    <label for="p_mode"><span>Bluetooth Mode</span><select name="p_mode" id="p_mode" >
  <option value="print"  %PRESSPRINT_S%>Print</option>
  <option value="pressrelease" %PRESSRELEASE_S%>Press and Release</option>
  <option value="none" %PRESSNONE_S%>None</option>
</select></label>
      <label for="pressrelease_time"><span>Bluetooth Press Time (ms)</span><input id="pressrelease_time" type="number" name="pressrelease_time" value="%PRESSRELEASE_TIME%"></label>
        <label for="repeat_time"><span>Repeat Time (ms)</span><input id="repeat_time" type="number" name="repeat_time" value="%REPEAT_TIME%"></label>
    <label for="l_mode"><span>Led Mode</span><select name="l_mode" id="l_mode" >
  <option value="l_on"  %L_ON_S%>Always On
  <option value="l_off" %L_OFF_S%>Always Off
  <option value="l_on_press" %L_ON_PRESS_S%>On when pressed
  <option value="l_off_press" %L_OFF_PRESS_S%>Off when pressed
  <option value="l_toggle_press" %L_TOGGLE_PRESS_S%>Toggle when pressed
   <option value="l_tuya" %L_TUYA%>On when Tuya Device on
</select></label>
    <label for="l_time (ms)"><span>Led Time (ms)</span><input id="l_time" type="number" name="l_time" value="%L_TIME%"></label>
   <label for="t_mode"><span>Tuya Mode</span><select name="t_mode" id="t_mode" >
  <option value="t_on"  %T_ON_S%>On
  <option value="t_off" %T_OFF_S%>Off
  <option value="t_toggle" %T_TOGGLE_S%>Toggle
  <option value="t_none" %T_NONE_S%>None
  
</select></label>
    <label for="api_host"><span>Tuya API url</span><input  id="api_host" type="text" maxlength="100"  name="t_host" value="%T_HOST%"></label>
    <label for="api_id"><span>API Client Id</span><input  id="api_id" type="text" maxlength="20"  name="t_api_client" value="%T_CLIENT%"></label>
    <label for="api_key"><span>API Secret Key</span><input  id="api_key" type="text" maxlength="32" name="t_api_key" value="%T_APIKEY%"></label>
    <label for="device_id"><span>Device Id</span><input  id="device_id" type="text" maxlength="32" name="t_device_id" value="%T_DEVICE%"></label>
    <label for="localkey"><span>Local Key (system will find this)</span><input  id="localkey" type="text" maxlength="32" name="t_localkey" value="%T_LOCALKEY%"></label>
    <label for="protocol"><span>Protocol (3.3 or 3.1)</span><input  id="protocol" type="text" maxlength="32" name="t_protocol" value="%T_PROTOCOL%"></label>
    <label for="ip"><span>ip address</span><input  id="ip" type="text" maxlength="32" name="t_ip" value="%T_IP%"></label>
 <label for="poff"><span>Power Off (M) 0 for never</span><input  id="poff" type="text" maxlength="32" name="poff" value="%POWER_OFF%"></label>
  <input type='hidden' name='change' value='' />
    <input type="submit" name="action" value="Create Profile"><input type="submit" name="action" value="Delete Profile" onclick="return confirm('Are you sure you want to delete this profile?')"><input type="submit" name="action" value="Submit">
  </form>
 </div>
</body></html>)rawliteral";

const char root_html[] PROGMEM = R"rawliteral(
  <title>Big Button</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
   Status: %STATUS%
  <div class="form-style-2">
  <div class="form-style-2-heading">Use file following links</div>
  <p><a href="/setting">Settings - how do you want Big Button to behave?</a></p>
  <p><a href="/login">Login - Login to update Big Button with emailed new firmware?</a></p>

  <p><a href="http://%DEVICE%/on">on - turn on</a></p>
  <p><a href="http://%DEVICE%/off">off - turn off</a></p>
  <p><a href="http://%DEVICE%/status">status - see/set on/off</a></p>
    <p><a href="http://%DEVICE%/status">toggle - toggle device</a></p>
     <p><a href="/export">Export - Allows you to save settings to a file</a></p>
 </div></div>
</body></html>)rawliteral";



const char status_html[] PROGMEM = R"rawliteral(
  <title>Big Button On</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
   Status: %STATUS%
   <div style="text-align:center">
   <a href="http://%DEVICE%/toggle"><span class="%STATUS_CLASS%"></span></a>
   </div>
</body></html>)rawliteral";

String ip2Str(IPAddress ip) {
  String s = "";
  for (int i = 0; i < 4; i++) {
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  }
  return s;
}

String getStatus() {

  static String status;

  status = ip2Str(WiFi.localIP());
  time_t t = time(NULL);
  // time will be offset from 0 until we have synced
  if (t < 99999999) {
    status += " : Waiting on real time to sync before tuya can connect. This can occasionally take a couple of minutes : ";
  } else if (tuya && tuya->isConnected()) {
    status += "<span> : Tuya cloud connected : </span>";
    if (profile.tuya_error) {
      status += String("<span> : ") + profile.tuya_error_string + String(" : </span>");
    }

    if (profile.tuya_switch == "") {
      status += "<span> : Waiting to get switch information : </span>";
    }

  } else if (tuya) {
    status += "<span> : Tuya not connected : </span>";
  }

  if (tuya ) {
    if (profile.tuya_local) {
      status += "<span> : Tuya local connected : </span>";
    } else {
      status += "<span> : Tuya local not connected : </span>";
    }
  }


  if (profile.press_mode != PRESS_NONE) {
    if (bleKeyboard.isConnected()) {
      status += "<span> : Bluetooth connected : </span>";
    } else {
      status += "<span> : Bluetooth not connected : </span>";
    }
  }
  return status;
}



void handleRoot() {

  String new_index_html = String(style_html) + String(root_html);
  new_index_html.replace("%STATUS%", getStatus());
  new_index_html.replace("%DEVICE%", ip2Str(WiFi.localIP()));
  server.send(200, "text/html", new_index_html.c_str());
}

void handleStatus() {
  String new_index_html = String(style_html) + String(status_html);
  new_index_html.replace("</style>", "</style><meta http-equiv=\"refresh\" content=\"2;URL='/status'\">");
  new_index_html.replace("%STATUS%", getStatus());
  new_index_html.replace("%STATUS_CLASS%", profile.led_on ? "doton" : "dotoff");
  new_index_html.replace("%DEVICE%", ip2Str(WiFi.localIP()));
  server.send(200, "text/html", new_index_html.c_str());

}


void handleOn() {
  tuya_on();
  handleStatus();

}

void handleOff() {
  tuya_off();
  handleStatus();
}


void handleToggle() {
  if (profile.tuya_on) {
    tuya_off();
  } else {
    tuya_on();
  }
  handleStatus();
}

void handleExport() {
  String buffer;
  for (int i = 0; i < profile_count; i++) {
    String p;
    if (readProfile(i, p)) {
      Serial.println("export a profile");

      buffer = buffer + p + "\n" ;
    }
  }
  Serial.println(buffer);
  server.send(200, "text/plain", buffer.c_str());
}



void handleSetting() {

  Serial.println("__________________________________________\nHandle setting");
  bool reboot = false;
  bool changed = false;
  bool init = false;
  String error = "";
  Serial.println("read args");
  String ntuya_host = server.arg("t_host");
  String nname = server.arg("profile");
  String nprofile_list = server.arg("profile_list");
  int nrepeat_time = strtol(server.arg("repeat_time").c_str(), NULL, 10);
  int npressrelease_time = strtol(server.arg("pressrelease_time").c_str(), NULL, 10);
  int npower_off_time = strtol(server.arg("poff").c_str(), NULL, 10);
  String nled_mode_string = server.arg("l_mode");
  int nled_mode = profile.led_mode;
  String ntuya_mode_string = server.arg("t_mode");
  int ntuya_mode = profile.tuya_mode;
  String npress_mode_string = server.arg("p_mode");
  int npress_mode = profile.press_mode;
  String npress_string = server.arg("p_string");
  String ntuya_api_key = server.arg("t_api_key");
  String ntuya_device_id = server.arg("t_device_id");
  String ntuya_localkey = server.arg("t_localkey");
  String ntuya_ip = server.arg("t_ip");
  String ntuya_protocol = server.arg("t_protocol");
  String change = server.arg("change");

  String ntuya_api_client = server.arg("t_api_client");

  String action = server.arg("action");
  if (action == "Create Profile") {
    Serial.println("Create Profile");
    String nname = server.arg("profile_list") ;
    int i = 1;
    if (profile.name == nname) {
      nname += "_copy" + String(i);
    }
    while (haveProfile(nname) != -1) {
      nname.replace( String("_copy") + String(i), "");
      i++;
      nname += "_copy" + String(i);
    }
    Serial.println("New Profile");
    Serial.println(profile.name);
    if (profile_count == MAX_PROFILES) {
      error = "Sorry there is a limit of " + String(MAX_PROFILES) + " profiles";
      goto error_case;
    }
    // we will not free the old profile just reuse its fields, the form will have had all the values filled in
    profile.name = nname;
    profiles[profile_count++] = nname;
    writeSettings();
    writeProfiles();
    readProfiles();
    changed = true;
  }






  Serial.print("action field:");
  Serial.println(action);
  Serial.print("change field:");
  Serial.println(change);
  if (change == "change") {

    Serial.print("change selected profile");
    if (profile.name != nprofile_list) {
      profile.name = nprofile_list; // deals with there being a malformed profile file,
      installProfile(haveProfile(nprofile_list));
      writeSettings();
      error = "changed profile to " + profile.name;


      changed = true;
      reboot = true;
      goto error_case;
    }
  }
  if (action == "Delete Profile") {

    Serial.print("delete profile: ");
    Serial.println(nprofile_list);
    if (profile_count > 1) {
      int index = haveProfile(nprofile_list);
      String filename = String(index) + ".js";

      SPIFFS.remove(filename);
      for (int i = index + 1; i < profile_count; i++) {
        String next = String(i + 1) + ".js";;

        SPIFFS.rename(next, filename);
        profiles[index] = profiles[index + 1];
        filename = next;
      }
      installProfile(0);
      error = "Deleted profile " + nprofile_list;
      profile_count--;
      changed = true;
      writeProfiles();
      goto error_case;
    } else {
      error = "You must leave a single profile, so cannot delete " + nprofile_list;
      goto error_case;
    }
  }
  if (action == "Submit") {
    Serial.println("do submit");

    if (profile.name != nname) {
      Serial.println("change name");
      if (haveProfile(nname) != -1) {
        error = "Duplicate profile name " + nname + " in submission, current profile is " + profile.name;
        goto error_case;
      } else {
        int index = haveProfile(profile.name);
        profiles[index] = nname;
        profile.name = nname;
        writeProfiles();
        Serial.println("set profile to the renamed");
        Serial.println(nname);

        changed = true;
      }
      Serial.println("changed name");
    }


    if (profile.repeat_time != nrepeat_time) {
      profile.repeat_time = nrepeat_time;
      changed = true;
    }

    if (profile.pressrelease_time != npressrelease_time) {
      profile.pressrelease_time = npressrelease_time;
      changed = true;
    }

    if (profile.power_off_time != npower_off_time) {
      profile.power_off_time = npower_off_time;
      changed = true;
    }

    if (nled_mode_string == "l_on") {
      nled_mode = LED_ON;
    } else if (nled_mode_string == "l_off") {
      nled_mode = LED_OFF;
    } else if (nled_mode_string == "l_on_press") {
      nled_mode = LED_ON_PRESS;
    } else if (nled_mode_string == "l_off_press") {
      nled_mode = LED_OFF_PRESS;
    } else if (nled_mode_string == "l_toggle_press") {
      nled_mode = LED_TOGGLE_PRESS;
    } else if (nled_mode_string == "l_tuya") {
      nled_mode = LED_TUYA;
    }
    if (profile.led_mode != nled_mode) {
      profile.led_mode = nled_mode;
      changed = true;
    }


    if (ntuya_mode_string == "t_on") {
      ntuya_mode = TUYA_ON;
    } else if (ntuya_mode_string == "t_off") {
      ntuya_mode = TUYA_OFF;
    } else if (ntuya_mode_string == "t_toggle") {
      ntuya_mode = TUYA_TOGGLE;
    } else if (ntuya_mode_string == "t_none") {
      ntuya_mode = TUYA_NONE;
    }
    if (profile.tuya_mode != ntuya_mode) {
      profile.tuya_mode = ntuya_mode;
      changed = true;
      reboot = true;
    }


    if (npress_mode_string == "print") {
      npress_mode = PRESS_PRINT;
    } else if (npress_mode_string == "pressrelease") {
      npress_mode = PRESS_PRESSRELEASE;
    } else if (npress_mode_string == "none") {
      npress_mode = PRESS_NONE;
    }
    if (profile.press_mode != npress_mode) {
      profile.press_mode = npress_mode;
      changed = true;
    }

    if (profile.press_string != npress_string) {
      profile.press_string = npress_string;
      changed = true;
    }

    if (profile.tuya_device_id != ntuya_device_id) {
      profile.tuya_device_id = ntuya_device_id;
      changed = true;
      reboot = true;
    }
    if (profile.tuya_protocol != ntuya_protocol) {
      profile.tuya_protocol = ntuya_protocol;
      changed = true;
      
    }
    if (profile.tuya_api_key != ntuya_api_key) {
      profile.tuya_api_key = ntuya_api_key;
      changed = true;
      reboot = true;
    }

    if (profile.tuya_api_client != ntuya_api_client) {
      profile.tuya_api_client = ntuya_api_client;
      changed = true;
      reboot = true;
    }


    if (profile.tuya_host !=  ntuya_host)  {
      profile.tuya_host = ntuya_host;
      changed = true;
      reboot = true;
    }

    if (profile.tuya_ip !=  ntuya_ip)  {
      profile.tuya_ip = ntuya_ip;
      changed = true;
      reboot = true;
    }

    if (profile.tuya_localkey !=  ntuya_localkey)  {
      profile.tuya_localkey = ntuya_localkey;
      changed = true;
      reboot = true;
    }
  }
  Serial.println("Have read args");
  if (changed) {
    Serial.println("output changed file settings");
    writeCurrentProfile();
    error = "changes saved";
    Serial.println("changes saved");

  }

error_case:

  String new_index_html = String(style_html) + String(index_html);
  Serial.println("construct select");
  String profile_list = "<select onchange=\"this.form.change.value='change';this.form.submit()\" name=\"profile_list\" id=\"profile_list\"  >";
  for  (int i = 0; i < profile_count; i++) {
    String selected;
    if (profiles[i] == profile.name) {
      selected = String(SELECTED);
    }

    profile_list += "<option " + selected + " >" + profiles[i] +  "</option>";
  }
  profile_list += "</select>";
  ///Serial.println(profile_list);



  Serial.println("macro replacement");
  new_index_html.replace( "%STATUS%", getStatus());
  new_index_html.replace( "%PROFILE%", profile.name);
  new_index_html.replace( "%PROFILE_LIST%", profile_list);
  new_index_html.replace( "%P_MODE%", String(profile.press_mode));
  new_index_html.replace( "%REPEAT_TIME%", String(profile.repeat_time));
  new_index_html.replace( "%PRESSRELEASE_TIME%", String(profile.pressrelease_time));
  new_index_html.replace( "%L_MODE%", String(profile.led_mode));
  new_index_html.replace( "%T_DEVICE%", profile.tuya_device_id);
  new_index_html.replace( "%T_CLIENT%", profile.tuya_api_client);
  new_index_html.replace( "%T_APIKEY%", profile.tuya_api_key);
  new_index_html.replace( "%T_HOST%", profile.tuya_host);
  new_index_html.replace( "%T_LOCALKEY%", profile.tuya_localkey);
  new_index_html.replace( "%T_IP%", profile.tuya_ip);
  new_index_html.replace( "%P_STRING%", profile.press_string);
  new_index_html.replace( "%PRESSRELEASE_S%", profile.press_mode == PRESS_PRESSRELEASE ? SELECTED : "");
  new_index_html.replace( "%PRESSPRINT_S%", profile.press_mode == PRESS_PRINT ? SELECTED : "");
  new_index_html.replace( "%PRESSNONE_S%", profile.press_mode == PRESS_NONE ? SELECTED : "");
  new_index_html.replace( "%L_ON_S%", profile.led_mode == LED_ON ? SELECTED : "");
  new_index_html.replace( "%L_OFF_S%", profile.led_mode == LED_OFF ? SELECTED : "");
  new_index_html.replace( "%L_ON_PRESS_S%", profile.led_mode == LED_ON_PRESS ? SELECTED : "");
  new_index_html.replace( "%L_OFF_PRESS_S%", profile.led_mode == LED_OFF_PRESS ? SELECTED : "");
  new_index_html.replace( "%L_TOGGLE_PRESS_S%", profile.led_mode == LED_TOGGLE_PRESS ? SELECTED : "");
  new_index_html.replace( "%T_ON_S%", profile.tuya_mode == TUYA_ON ? SELECTED : "");
  new_index_html.replace( "%T_OFF_S%", profile.tuya_mode == TUYA_OFF ? SELECTED : "");
  new_index_html.replace( "%T_TOGGLE_S%", profile.tuya_mode == TUYA_TOGGLE ? SELECTED : "");
  new_index_html.replace( "%L_TUYA%", profile.led_mode == LED_TUYA ? SELECTED : "");
  new_index_html.replace( "%T_NONE_S%", profile.tuya_mode == TUYA_NONE ? SELECTED : "");
  new_index_html.replace( "%T_PROTOCOL%", profile.tuya_protocol);
  new_index_html.replace( "%POWER_OFF%", String(profile.power_off_time));

  Serial.println("macros replaced");

  if (reboot) {
    new_index_html.replace("%REBOOT%", String("<h1>Device rebooting, please connect again in a few seconds</h1>"));
  } else if (error != "") {
    new_index_html.replace("%REBOOT%", error);

  } else {
    new_index_html.replace( "%REBOOT%", String(""));
  }
  server.send(200, "text/html", new_index_html.c_str());
  Serial.println("html sent");
  if (reboot) {
    Serial.println("about to reboot");

    delay(4000);
    Serial.println("rebooting now");
    resetFunc();
  }
}

long last_activity;

void setup(void) {

  pinMode (LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  delay(10);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);

  while (!Serial) {

  }
  SPIFFS.begin(true);

  pinMode(BUTTON_PIN, INPUT_PULLUP); // config GIOP21 as input pin and enable the internal pull-up resistor

  EEPROM.begin(EEPROM_SIZE);
  init_eeprom();
  EEPROM.commit();
  readId(device_name);
  EEPROM.end();

  if (strcmp(device_name, "") == 0) {
    Serial.print("reset name");
    EEPROM.begin(EEPROM_SIZE);
    writeId("bigbutton");
    EEPROM.commit();
    EEPROM.end();
  }
  Serial.println(device_name);

  WiFiManagerParameter MDNSName("device_name", "Device Name", device_name, 19);
  wifiManager.addParameter(&MDNSName);

  wifiManager.setEnableConfigPortal(false);
  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    if ( digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("Button down so config");
      String PortalName = String("Configure") + String(device_name) + String("WiFi");
      wifiManager.startConfigPortal(PortalName.c_str());
      const char *new_name = MDNSName.getValue();
      if (strcmp(new_name, device_name) != 0) {
        EEPROM.begin(EEPROM_SIZE);
        writeId(new_name);
        EEPROM.commit();
        EEPROM.end();
        strcpy(device_name, new_name);
      }
    } else {

      Serial.println("reboot");
      resetFunc();
    }
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
#ifdef USE_MDNS
  if (MDNS.begin(device_name)) {
    Serial.println("MDNS responder");
  }
#endif
  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.on("/settings", handleSetting);
  server.on("/setting", handleSetting);
  server.on("/export", handleExport);
  server.on("/on", handleOn);
  server.on("/off", handleOff);
  server.on("/status", handleStatus);


  /*return index page which is stored in serverIndex */
  server.on("/login", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();
  Serial.println("HTTP server");
#if 1
  xTaskCreatePinnedToCore(
    status,   /* Task function. */
    "Task1",     /* name of task. */
    8000,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &Task1,      /* Task handle to keep track of created task */
    0);  
#endif

  String buffer;
  readProfiles();
  if (readSettings(buffer)) {


    Serial.print("Boot settings :"); Serial.println(profile_count);
    installProfile(atol(buffer.c_str()));


  } else {
    init_default_profile();
  }

  profile.tuya_error = false;
  profile.tuya_have_status = false;

  if (profile.tuya_api_client != "" && profile.tuya_api_key != "" && profile.tuya_host != ""  && profile.tuya_device_id != "" && profile.tuya_mode != TUYA_NONE) {

    Serial.print("Create Cloud Tuya Client : ");Serial.println(profile.tuya_device_id);
    tuya = new TuyaAuth(profile.tuya_host.c_str(), profile.tuya_api_client.c_str(), profile.tuya_api_key.c_str());
 #ifdef TUYALOCAL   
    Serial.println("Create Local Tuya Client");

 #endif   
  }
  switch (profile.led_mode) {
    case LED_ON:
      digitalWrite(LED_PIN, HIGH);
      profile.led_on = true;
      break;
    default:
      digitalWrite(LED_PIN, LOW);
      profile.led_on = false;
      break;

  }
  if (profile.press_mode != PRESS_NONE) {
    bleKeyboard.begin();
  }


  last_activity = millis();
  Serial.print("Power off: "); Serial.println(profile.power_off_time);
  Serial.println(millis());
}

void loop(void) {

  static long count = 1;
  if (profile.power_off_time != 0 && last_activity + (profile.power_off_time * 60000) < millis() ) {

    Serial.println("Going to sleep");
    delay(20);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, LOW);
    esp_deep_sleep_start();
  }
  //button.loop(); // MUST call the loop() function first
  server.handleClient();
  if (tuya && profile.tuya_switch == "") {
    if (tuya->isConnected()) {
      if (tuya->TGetSwitch(profile.tuya_device_id.c_str(), profile.tuya_switch)) {
        Serial.println(profile.tuya_switch);
        profile.tuya_error = false;
      } else {
        profile.tuya_error = true;
        profile.tuya_error_string = true;
      }
    }
  }
  if (tuya && !profile.tuya_localkeyfound) {
    if (tuya->isConnected()) {

#if 1
      if (tuya->TGetDeviceDetailString(profile.tuya_device_id.c_str(), "local_key", profile.tuya_localkey)) {
        Serial.print("localkey"); Serial.println(profile.tuya_localkey);
        profile.tuya_error = false;
        profile.tuya_localkeyfound = true;

      } else {
        profile.tuya_error = true;
        profile.tuya_error_string = true;
      }
#endif
    }
  }
  if (tuya && profile.tuya_have_status == false && profile.tuya_switch != "" ) {


    if (tuya->isConnected()) {

      if (tuya->TGetOnStatus(profile.tuya_device_id.c_str(), profile.tuya_switch.c_str(), profile.tuya_on)) {
        Serial.print("Switch is: "); Serial.println(profile.tuya_on);
      } else {
        profile.tuya_error = true;
      }
      profile.tuya_have_status = true;
      digitalWrite(LED_PIN, LOW);
      delay(500);
      digitalWrite(LED_PIN, HIGH);
      delay(500);
      digitalWrite(LED_PIN, LOW);
      delay(500);
      digitalWrite(LED_PIN, profile.led_on ? HIGH : LOW);
    }
  }

  //if (button.getState() == LOW) {
    
  //if (button.isPressed()) {
  if (digitalRead(BUTTON_PIN)== LOW){

    Serial.println("button pressed");
    //
    bool do_action = false;
    last_activity = millis();
    if (!profile.button_on) {
      profile.button_on = true;
      do_action = true;

      profile.ms = millis();
    } else if (profile.repeat_time > 0 && millis() > profile.ms + profile.repeat_time) {
      do_action = true;
      profile.ms = millis();
    }
    if (profile.press_mode != PRESS_NONE && do_action && bleKeyboard.isConnected()) {
      Serial.print(profile.press_string);

      switch (profile.press_mode) {


        case PRESS_PRINT:
          bleKeyboard.print(profile.press_string);
          break;
        case PRESS_PRESSRELEASE:
          for (int i = 0; i < profile.press_string.length(); i++) {
            bleKeyboard.press(profile.press_string[i]);

          }
          delay(profile.pressrelease_time);
      }

      bleKeyboard.releaseAll();
    }


    if (do_action) {
      switch (profile.led_mode) {
        case LED_ON_PRESS:
          digitalWrite(LED_PIN, HIGH);
          break;
        case LED_OFF_PRESS:
          digitalWrite(LED_PIN, LOW);
          break;
        case LED_TOGGLE_PRESS:
          digitalWrite(LED_PIN, profile.led_on ? LOW : HIGH);
          profile.led_on = !profile.led_on;
      }
    }
    if (tuya) {

      String command;


      if ( do_action) {
        switch (profile.tuya_mode) {
          case TUYA_ON:
            tuya_on();
            break;
          case TUYA_OFF:
            tuya_off();
            break;
          case TUYA_TOGGLE:
            if (profile.led_mode == LED_TUYA){
              if (profile.tuya_on) {
                digitalWrite(LED_PIN, LOW);
              } else {
                digitalWrite(LED_PIN, HIGH);
              }  
            
            }
          
            Serial.print("toggle"); Serial.println(profile.tuya_on);
            if (profile.tuya_on) {
              tuya_off();
            } else {
              tuya_on();
            }
        }
        if (profile.led_mode == LED_TUYA){
          profile.led_on =  profile.tuya_on;

         if (profile.tuya_on) {
           digitalWrite(LED_PIN, HIGH);
         } else {
            digitalWrite(LED_PIN, LOW);
          }
        }

      }
    }
  } else {

    profile.button_on = false;
    //Serial.println(profile.led_mode);
    //Serial.println(profile.tuya_on);
    switch (profile.led_mode) {
      case LED_ON_PRESS:
        digitalWrite(LED_PIN, LOW);
        break;
      case LED_OFF_PRESS:
        digitalWrite(LED_PIN, HIGH);
        break;
      case LED_ON:
        digitalWrite(LED_PIN, HIGH);
        profile.led_on = true;
        break;
      case LED_OFF  :
        digitalWrite(LED_PIN, LOW);
        profile.led_on = false;
        break;
      case LED_TUYA:

        profile.led_on =  profile.tuya_on;

        if (profile.tuya_on) {
          digitalWrite(LED_PIN, HIGH);
        } else {
          digitalWrite(LED_PIN, LOW);
        }
        break;
    }
  }

  //delay(1);//allow the cpu to switch to other tasks


}

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


#define EEPROM_SIZE 100

/*
   Login page
*/
const char* loginIndex =
  "<form name='loginForm'>"
  "<table width='20%' bgcolor='A09F9F' align='center'>"
  "<tr>"
  "<td colspan=2>"
  "<center><font size=4><b>ESP32 Login Page</b></font></center>"
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

#include "TuyaAuth.h"
TuyaAuth *tuya;


#include <ezButton.h>
#define BUTTON_PIN 15
ezButton button(BUTTON_PIN);  // create ezButton object that attach to pin 7;

#define LED_PIN 16

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

      buffer += c;
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
    int press_time;
    int press_mode;
    int led_mode;
    int led_time;
    int repeat_time;
    int tuya_mode;
    String  press_string;
    String  tuya_device;
    String tuya_api_key;
    String tuya_api_client;
    String tuya_host;
    String tuya_ip;
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


void writeCurrentProfile() {

  char buffer[3000];

  Serial.print("WriteCurrentProfile: ");Serial.println(profile.name);
  Serial.print("Profile index: ");Serial.println(haveProfile(profile.name));
  strcpy(buffer, " {\"t_device\":\""); strcat(buffer, profile.tuya_device.c_str());
   strcat(buffer,   "\",\"l_mode\": \"" );  strcat(buffer, String(profile.led_mode).c_str());
    Serial.println(258);Serial.flush();
     strcat(buffer,   "\",\"p_string\":\""); strcat(buffer, profile.press_string.c_str());
  strcat(buffer, "\",\"name\":\""); strcat(buffer, profile.name.c_str());
      Serial.println(260);Serial.flush();
  strcat(buffer, "\",\"t_ip\":\""); strcat(buffer, profile.tuya_ip.c_str());
  strcat(buffer, "\",\"t_api_key\": \""); strcat(buffer, profile.tuya_api_key.c_str());
  strcat(buffer, "\",\"t_api_client\": \""); strcat(buffer, profile.tuya_api_client.c_str());
  strcat(buffer,   "\",\"t_host\": \"");  strcat(buffer, profile.tuya_host.c_str());
      Serial.println(265);Serial.flush();
 
 
  Serial.println(268);Serial.flush();
  strcat(buffer,   "\",\"t_mode\": \""); strcat(buffer, String(profile.tuya_mode).c_str());
  strcat(buffer,    "\",\"l_time\": \""); strcat(buffer, String(profile.led_time).c_str());
  strcat(buffer,   "\",\"poff_time\" :\""); strcat(buffer, String(profile.power_off_time).c_str());
  strcat(buffer,   "\",\"repeat_time\" :\""); strcat(buffer, String(profile.repeat_time).c_str());
  strcat(buffer,  "\",\"p_mode\": \""); strcat(buffer, String(profile.press_mode).c_str());
  strcat(buffer,  "\",\"p_time\": \""); strcat(buffer, String(profile.press_time).c_str());
  strcat(buffer, "\"}");
  Serial.println(buffer);Serial.flush();
  String filename = String("/") + String(haveProfile(profile.name)) + ".js";
  Serial.println(filename);;Serial.flush();
  File file = SPIFFS.open(filename, FILE_WRITE);

  int l = strlen(buffer);
  Serial.print("Length: ");Serial.println(l);

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

bool readProfile(int profilenumber,String & buffer) {
  
 Serial.print("read profile: ");Serial.print(buffer);
  String name = String("/") + String(profilenumber) + ".js";
  
  if (readFile(name, buffer)){
    return true;
  }
  return false;
}

void installProfile(int profilenumber) {

  String buffer;
  readProfile(profilenumber,buffer);
  DynamicJsonDocument root(4000);

  DeserializationError err = deserializeJson(root, buffer);


  if (!err) {

    if (root.containsKey("name")) {profile.name = (const char *) root["name"];}else Serial.println("missing name");
    if (root.containsKey("l_mode")) {profile.led_mode = root["l_mode"];}else Serial.println("missing l_mode");
    if (root.containsKey("p_mode")) {profile.press_mode = root["p_mode"];}else Serial.println("missing p_mode");
    if (root.containsKey("t_mode")) {profile.tuya_mode = root["t_mode"];}else Serial.println("missing t_mode");
    if (root.containsKey("l_time")) {profile.led_time = root["l_time"];}else Serial.println("missing l_time");
    if (root.containsKey("p_time")) {profile.press_time = root["p_time"];}else Serial.println("missing p_time");
    if (root.containsKey("repeat_time")){profile.repeat_time = root["repeat_time"];}else Serial.println("missing repeat_time");
    if (root.containsKey("poff_time")){profile.power_off_time = root["poff_time"];}else Serial.println("missing poff_time");
    if (root.containsKey("t_ip")){profile.tuya_ip =   (const char *)root["t_ip"];}else Serial.println("missing t_ip");
    if (root.containsKey("t_device")){profile.tuya_device =   (const char *)root["t_device"];}else Serial.println("missing t_device");
    if (root.containsKey("t_api_client")){profile.tuya_api_client =  (const char *)root["t_api_client"];}else Serial.println("missing t_api_client");
     if (root.containsKey("t_api_key")){profile.tuya_api_key =  (const char *)root["t_api_key"];}else Serial.println("missing t_api_key");
    Serial.println(373);
    if (root.containsKey("p_string")){profile.press_string =  (const char *)root["p_string"];}else Serial.println("missing p_string");
    Serial.println(375);
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
  char buffer[100];
  profile_count = 0;
  auto file = SPIFFS.open(profiles_file, FILE_READ);
  if (file) {
    int pos = 0;

    while (file.available() && profile_count < MAX_PROFILES) {
      const char c = file.read();
      if (c != '\n') {
        buffer[pos++] = c;
      } else {
        buffer[pos++] = '\0';
        profiles[profile_count++] = String(buffer);
        Serial.println(profiles[profile_count-1]);
        pos=0;
      }
      
    }
    Serial.print("Read profiles :");Serial.println(profile_count);
    return true;
  }
  return false;

}

#define LED_ON_PRESS 0
#define LED_OFF_PRESS 1
#define LED_ON 2
#define LED_OFF 3

#define PRESS_PRINT 0
#define PRESS_PRESSRELEASE 1
#define PRESS_NONE 2

#define TUYA_ON 3
#define TUYA_OFF 1
#define TUYA_TOGGLE 2
#define TUYA_NONE 0


const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
<style type="text/css">
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

  <title>Big Button Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  %REBOOT%
  <div class="form-style-2">
  <div class="form-style-2-heading">Provide your information</div>
  <form action="/setting" method="POST">
 <label for="profile_list"><span>Profile</span>%PROFILE_LIST%</label>
 <label for="profile"><span>Edit Profile Name</span><input name="profile" id="profile" value="%PROFILE%" ></label>
  <label for="p_string"><span>Press String</span><input name="p_string" id="p_string" value="%P_STRING%" ></label>
    <label for="p_mode"><span>Bluetooth Mode</span><select name="p_mode" id="p_mode" >
  <option value="print"  %PRESSPRINT_S%>Print</option>
  <option value="pressrelease" %PRESSRELEASE_S%>Press and Release</option>
  <option value="none" %PRESSNONE_S%>None</option>
</select></label>
    <label for="p_time"><span>Press Time (ms)</span><input id="p_time" type="number" name="p_time" value="%P_TIME%"></label>
        <label for="repeat_time"><span>Repeat Time (ms)</span><input id="repeat_time" type="number" name="repeat_time" value="%REPEAT_TIME%"></label>
    <label for="l_mode"><span>Led Mode</span><select name="l_mode" id="l_mode" >
  <option value="l_on"  %L_ON_S%>Always On
  <option value="l_off" %L_OFF_S%>Always Off
  <option value="l_on_press" %L_ON_PRESS_S%>On when pressed
  <option value="l_off_press" %L_OFF_PRESS_S%>Off when pressed
  
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
 <label for="poff"><span>Power Off (M) 0 for never</span><input  id="poff" type="text" maxlength="32" name="poff" value="%POWER_OFF%"></label>
  <input type='hidden' name='change' value='' />
    <input type="submit" name="action" value="Create Profile"><input type="submit" name="action" value="Delete Profile" onclick="return confirm('Are you sure you want to delete this profile?')"><input type="submit" name="action" value="Submit">
  </form>
 </div>
</body></html>)rawliteral";




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
  int npress_time = strtol(server.arg("p_time").c_str(), NULL, 10);
  int nrepeat_time = strtol(server.arg("repeat_time").c_str(), NULL, 10);
  int nled_time = strtol(server.arg("l_time").c_str(), NULL, 10);
  int npower_off_time = strtol(server.arg("poff").c_str(), NULL, 10);
  String nled_mode_string = server.arg("l_mode");
  int nled_mode = profile.led_mode;
  String ntuya_mode_string = server.arg("t_mode");
  int ntuya_mode = profile.tuya_mode;
  String npress_mode_string = server.arg("p_mode");
  int npress_mode = profile.press_mode;
  String npress_string = server.arg("p_string");
  String ntuya_device = server.arg("t_device");
  String ntuya_ip = server.arg("t_ip");
  String ntuya_api_key = server.arg("t_api_key");
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
      profile.name.replace( String("_copy") + String(i), "");
      i++;
      profile.name += "_copy" + String(i);
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
        profiles[index]=nname;
        profile.name = nname;
        writeProfiles();
        Serial.println("set profile to the renamed");
        Serial.println(nname);
        
        changed = true;
        reboot = true;
      }
      Serial.println("changed name");
    }

    if (profile.press_time != npress_time) {
      profile.press_time = npress_time;
      changed = true;
    }

    if (profile.repeat_time != nrepeat_time) {
      profile.repeat_time = nrepeat_time;
      changed = true;
    }

    if (profile.led_time != nled_time) {
      profile.led_time = nled_time;
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
    } else if (nled_mode_string == LED_ON_PRESSED) {
      nled_mode = LED_ON_PRESS;
    } else if (nled_mode_string == "l_off_press") {
      nled_mode = LED_OFF_PRESS;
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
    }

    Serial.print("Bluetooth mode: " ); Serial.println(npress_mode_string);

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
    Serial.print("Bluetooth mode: " ); Serial.println(profile.press_mode);
    if (profile.press_string != npress_string) {
      profile.press_string = npress_string;
      changed = true;
    }
    if (profile.tuya_device != ntuya_device) {
      profile.tuya_device = ntuya_device;
      changed = true;
      reboot = true;
    }

    if (profile.tuya_ip != ntuya_ip) {
      profile.tuya_ip = ntuya_ip;
      changed = true;
      reboot = true;
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
  }
  Serial.println("Have read args");
  if (changed) {
    Serial.println("output changed file settings");
    writeCurrentProfile();
    error = "changes saved";
    Serial.println("changes saved");

  }

error_case:
  Serial.println("construct select");
  String profile_list = "<select onchange=\"this.form.change.value='change';this.form.submit()\" name=\"profile_list\" id=\"profile_list\"  >";
  for  (int i = 0; i != profile_count; i++) {
    String selected;
    if (profiles[i] == profile.name) {
      selected = String(SELECTED);
    }

    profile_list += "<option " + selected + " >" + profiles[i] +  "</option>";
  }
  profile_list += "</select>";
  ///Serial.println(profile_list);

  String new_index_html = index_html;

  Serial.println("macro replacement");

  new_index_html.replace( "%PROFILE%", profile.name);
  new_index_html.replace( "%PROFILE_LIST%", profile_list);
  new_index_html.replace( "%P_TIME%", String(profile.press_time));
  new_index_html.replace( "%P_MODE%", String(profile.press_mode));
  new_index_html.replace( "%L_TIME%", String(profile.led_time));
  new_index_html.replace( "%REPEAT_TIME%", String(profile.repeat_time));
  new_index_html.replace( "%L_MODE%", String(profile.led_mode));
  new_index_html.replace( "%T_DEVICE%", profile.tuya_device);
  new_index_html.replace( "%T_IP%", profile.tuya_ip);
  new_index_html.replace( "%T_CLIENT%", profile.tuya_api_client);
  new_index_html.replace( "%T_APIKEY%", profile.tuya_api_key);
  new_index_html.replace( "%T_HOST%", profile.tuya_host);
  new_index_html.replace( "%P_STRING%", profile.press_string);
  new_index_html.replace( "%PRESSRELEASE_S%", profile.press_mode == PRESS_PRESSRELEASE ? SELECTED : "");
  new_index_html.replace( "%PRESSPRINT_S%", profile.press_mode == PRESS_PRINT ? SELECTED : "");
  new_index_html.replace( "%PRESSNONE_S%", profile.press_mode == PRESS_NONE ? SELECTED : "");
  new_index_html.replace( "%L_ON_S%", profile.led_mode == LED_ON ? SELECTED : "");
  new_index_html.replace( "%L_OFF_S%", profile.led_mode == LED_OFF ? SELECTED : "");
  new_index_html.replace( "%L_ON_PRESS_S%", profile.led_mode == LED_ON_PRESS ? SELECTED : "");
  new_index_html.replace( "%L_OFF_PRESS_S%", profile.led_mode == LED_OFF_PRESS ? SELECTED : "");
  new_index_html.replace( "%T_ON_S%", profile.tuya_mode == TUYA_ON ? SELECTED : "");
  new_index_html.replace( "%T_OFF_S%", profile.tuya_mode == TUYA_OFF ? SELECTED : "");
  new_index_html.replace( "%T_TOGGLE_S%", profile.tuya_mode == TUYA_TOGGLE ? SELECTED : "");
  new_index_html.replace( "%T_NONE_S%", profile.tuya_mode == TUYA_NONE ? SELECTED : "");
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


void setup(void) {
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
    wifiManager.startConfigPortal("ConfigureWiFi");
    const char *new_name = MDNSName.getValue();
    if (strcmp(new_name, device_name) != 0) {
      EEPROM.begin(EEPROM_SIZE);
      writeId(new_name);
      EEPROM.commit();
      EEPROM.end();
      strcpy(device_name, new_name);
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

  server.on("/setting", handleSetting);

  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
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
  bleKeyboard.begin();


  String buffer;
  readProfiles();
  if (readSettings(buffer)) {


    Serial.print("Boot settings :");Serial.println(profile_count);
    installProfile(atol(buffer.c_str()));


  } else {
    init_default_profile();
  }

  if (profile.tuya_api_client != "" && profile.tuya_api_key != "" && profile.tuya_host != "" ) {
    tuya = new TuyaAuth(profile.tuya_host.c_str(), profile.tuya_api_client.c_str(), profile.tuya_api_key.c_str());
  }

}

void loop(void) {
  button.loop(); // MUST call the loop() function first
  server.handleClient();
  if (button.isPressed()) {
    if (bleKeyboard.isConnected()) {

      delay(50);
      bleKeyboard.releaseAll();
    } else {
      Serial.print(".");
    }
  }

  delay(20);//allow the cpu to switch to other tasks


}

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

bool readSettings(String &buffer) {
  if (SPIFFS.exists(settings_file)) {


    File file = SPIFFS.open(settings_file, FILE_READ);

    if (file) {
      buffer.reserve(file.size());
      int pos = 0;

      while (file.available()) {
        //Serial.print(pos);
        const char c = file.read();
        buffer += c;
        pos++;
      }


      file.close();
      Serial.println("Read buffer");
      Serial.println(buffer);
      return true;
    }

  }
  return false;
}

String profile;
class CButtonProfile {

  public:
    String name;
    int power_off_time;
    int press_time;
    int press_mode;
    int led_mode;
    int led_time;
    int repeat_time;
    int tuya_mode;
    String  press_string;
    String  tuya_device;
    String tuya_api_key;
    String tuya_api_client;
    String tuya_host;
    String tuya_ip;
};

SimpleMap<String, CButtonProfile *> *profiles;

#define LED_ON_PRESS 0
#define LED_OFF_PRESS 1
#define LED_ON 2
#define LED_OFF 3

#define PRESS_PRINT 0
#define PRESS_PRESSRELEASE 1

#define TUYA_ON 3
#define TUYA_OFF 1
#define TUYA_TOGGLE 2
#define TUYA_NONE 0


const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
<style type="text/css">
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
.form-style-2 .tel-number-field, 
.form-style-2 .textarea-field, 
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
.form-style-2 .tel-number-field:focus, 
.form-style-2 .textarea-field:focus,  
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

  <title>Big Button Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  %REBOOT%
  <div class="form-style-2">
  <div class="form-style-2-heading">Provide your information</div>
  <form action="/setting" method="POST">
 <label for="profile_list"><span>Profile</span>%PROFILE_LIST%</label>
  <label for="press_string"><span>Press String</span><input name="press_string" id="press_string" value="%PRESS_STRING%" ></label>
    <label for="press_mode"><span>Press Mode</span><select name="press_mode" id="press_mode" >
  <option value="print"  %PRINT_S%>Print</option>
  <option value="pressrelease" %PRESSRELEASE_S%>Press and Release</option>
</select></label>
    <label for="press_time"><span>Press Time (ms)</span><input id="press_time" type="number" name="press_time" value="%PRESS_TIME%"></label>
        <label for="repeat_time"><span>Repeat Time (ms)</span><input id="repeat_time" type="number" name="repeat_time" value="%REPEAT_TIME%"></label>
    <label for="led_mode"><span>Led Mode</span><select name="led_mode" id="led_mode" >
  <option value="led_on"  %LED_ON_S%>Always On</option>
  <option value="led_off" %LED_OFF_S%>Always Off</option>
  <option value="led_on_press" %LED_ON_PRESS_S%>On when pressed</option>
  <option value="led_off_press" %LED_OFF_PRESS_S%>Off when pressed</option>
  
</select></label>
    <label for="led_time (ms)"><span>Led Time (ms)</span><input id="led_time" type="number" name="led_time" value="%LED_TIME%"></label>
   <label for="tuya_mode"><span>Tuya Mode</span><select name="tuya_mode" id="tuya_mode" >
  <option value="tuya_on"  %T_ON_S%>On</option>
  <option value="tuya_off" %T_OFF_S%>Off</option>
  <option value="tuya_toggle" %T_TOGGLE_S%>Toggle</option>
  <option value="tuya_none" %T_NONE_S%>None</option>
  
</select></label>
    <label for="api_host"><span>Tuya API url</span><input  id="api_host" type="text" maxlength="100"  name="tuya_host" value="%T_HOST%"></label>
    <label for="api_id"><span>API Client Id</span><input  id="api_id" type="text" maxlength="20"  name="tuya_api_client" value="%T_CLIENT%"></label>
    <label for="api_key"><span>API Secret Key</span><input  id="api_key" type="text" maxlength="32" name="tuya_api_key" value="%T_APIKEY%"></label>
 <label for="power_off"><span>Power Off Minutes 0 for never</span><input  id="power_off" type="text" maxlength="32" name="power_off" value="%POWER_OFF%"></label>

    <input type="submit" name="action" value="Create Profile"><input type="submit" name="action" value="Delete Profile" onclick="return confirm('Are you sure you want to delete this profile?')"><input type="submit" name="action" value="Submit">
  </form>
 </div>
</body></html>)rawliteral";


void handleSetting() {
  bool reboot = false;
  bool changed = false;
  bool init = false;
  String error = "";

  String action = server.arg("action");
  if (action == "Create Profile") {

    int i = 1;
    if (profile == server.arg("profile_list")) {
      profile += "_copy" + String(i);
    }
    while (profiles->has(profile)) {
      profile.replace("_copy" + String(i), "");
      i++;
      profile += "_copy" + String(i);
    }
    profiles->put(profile, new CButtonProfile());
  }

  CButtonProfile *cp = profiles->get(profile);

  String ntuya_host = server.arg("tuya_host");
  String nname = server.arg("profile_list");
  int npress_time = strtol(server.arg("press_time").c_str(), NULL, 10);
  int nrepeat_time = strtol(server.arg("repeat_time").c_str(), NULL, 10);
  int nled_time = strtol(server.arg("led_time").c_str(), NULL, 10);
  int npower_off_time = strtol(server.arg("power_off").c_str(), NULL, 10);
  String nled_mode_string = server.arg("led_mode");
  int nled_mode = cp->led_mode;
  String ntuya_mode_string = server.arg("tuya_mode");
  int ntuya_mode = cp->tuya_mode;
  String npress_mode_string = server.arg("press_mode");
  int npress_mode = cp->press_mode;
  String npress_string = server.arg("press_string");
  String ntuya_device = server.arg("tuya_device");
  String ntuya_ip = server.arg("tuya_ip");
  String ntuya_api_key = server.arg("tuya_api_key");

  String ntuya_api_client = server.arg("tuya_api_client");

  if (cp->name != nname) {
    if (profiles->has(nname)) {
      error = "Duplicate profile name " + nname + " in submission, current profile is " + profile + "/" + cp->name;
      goto error_case;
    } else {
      profiles->remove(cp->name);
      cp->name = nname;
      profiles->put(nname, cp);
      profile = nname;
    }
  }

  if (cp->press_time != npress_time) {
    cp->press_time = npress_time;
    changed = true;
  }

  if (cp->repeat_time != nrepeat_time) {
    cp->repeat_time = nrepeat_time;
    changed = true;
  }

  if (cp->led_time != nled_time) {
    cp->led_time = nled_time;
    changed = true;
  }

  if (cp->power_off_time != npower_off_time) {
    cp->power_off_time = npower_off_time;
    changed = true;
  }



  if (nled_mode_string.length() < 20 ) {

    if (nled_mode_string == "led_on") {
      nled_mode = LED_ON;
    } else if (nled_mode_string == "led_off") {
      nled_mode = LED_OFF;
    } else if (nled_mode_string == LED_ON_PRESSED) {
      nled_mode = LED_ON_PRESS;
    } else if (nled_mode_string == "led_off_press") {
      nled_mode = LED_OFF_PRESS;
    }
    if (cp->led_mode != nled_mode) {
      cp->led_mode = nled_mode;
      changed = true;
    }
  }


  if (ntuya_mode_string.length() < 20 ) {

    if (ntuya_mode_string == "tuya_on") {
      ntuya_mode = TUYA_ON;
    } else if (ntuya_mode_string == "tuya_off") {
      ntuya_mode = TUYA_OFF;
    } else if (ntuya_mode_string == "tuya_toggle") {
      ntuya_mode = TUYA_TOGGLE;
    } else if (ntuya_mode_string == "tuya_none") {
      ntuya_mode = TUYA_NONE;
    }
    if (cp->tuya_mode != ntuya_mode) {
      cp->tuya_mode = ntuya_mode;
      changed = true;
    }
  }

  if (npress_mode_string == "print") {
    npress_mode = PRESS_PRINT;
  } else if (npress_mode_string == "pressrelease") {
    npress_mode = PRESS_PRESSRELEASE;
  }
  if (cp->press_mode != npress_mode) {
    cp->press_mode = npress_mode;
    changed = true;
  }
  if (cp->press_string != npress_string) {
    cp->press_string = npress_string;
    changed = true;
  }
  if (cp->tuya_device != ntuya_device) {
    cp->tuya_device = ntuya_device;
    changed = true;
    reboot = true;
  }

  if (cp->tuya_ip != ntuya_ip) {
    cp->tuya_ip = ntuya_ip;
    changed = true;
    reboot = true;
  }
  if (cp->tuya_api_key != ntuya_api_key) {
    cp->tuya_api_key = ntuya_api_key;
    changed = true;
    reboot = true;
  }

  if (cp->tuya_api_client != ntuya_api_client) {
    cp->tuya_api_client = ntuya_api_client;
    changed = true;
    reboot = true;
  }


  if (cp->tuya_host !=  ntuya_host)  {
    cp->tuya_host = ntuya_host;
    changed = true;
    reboot = true;
  }


  if (changed) {
    String buffer;
    buffer += "{ \"current\":\"" + profile + "\",";
    buffer += "\"profiles\":[";
    for (int i = 0; i < profiles->size(); i++) {
      CButtonProfile *p = profiles->getData(i);
      buffer += "{\"tuya_device\":\"" + p->tuya_device +
                "\",\"name\":\"" + p->name +
                "\",\"tuya_ip\":\"" + p->tuya_ip +
                "\",\"tuya_api_key\": \"" + p->tuya_api_key +
                "\",\"tuya_api_client\": \"" + p->tuya_api_client +
                "\",\"tuya_host\": \"" + p->tuya_host +
                "\",\"press_string\":\"" + p->press_string +
                "\",\"led_mode\": \"" + String(p->led_mode) +
                "\",\"tuya_mode\": \"" + String(p->tuya_mode) +
                "\",\"led_time\": \"" + String(p->led_time) +
                "\",\"power_off_time\" :\"" + String(p->power_off_time) +
                "\",\"repeat_time\" :\"" + String(p->repeat_time) +
                "\",\"press_mode\": \"" + String(p->press_mode) +
                "\",\"press_time\": \"" + String(p->press_time) + "\"}";
      if (i < profiles->size()    - 1) {
        buffer += ",";
      }
    }
    buffer += "]}";
    Serial.println(buffer);
    File file = SPIFFS.open(settings_file, FILE_WRITE);
    int l = buffer.length();

    for (int i = 0; i <= l; i++) {
      file.write(buffer[i]);
    }
    file.close();
  }

error_case:
  String profile_list = "<input name=\"profile_list\" id=\"profile_list\" type=\"text\" list=\"profiles\" value=\"" + profile + "\" /><datalist id=\"profile_list\" >";
  for  (int i = 0; i < profiles->size(); i++) {

    profile_list += "<option>" + profiles->getKey(i) + "</option>";
  }
  profile_list += "</datalist>";
  String new_index_html = index_html;

  new_index_html.replace( "%PROFILE_LIST%", profile_list);
  new_index_html.replace( "%PRESS_TIME%", String(npress_time));
  new_index_html.replace( "%PRESS_MODE%", String(npress_mode));
  new_index_html.replace( "%LED_TIME%", String(nled_time));
  new_index_html.replace( "%REPEAT_TIME%", String(nrepeat_time));
  new_index_html.replace( "%LED_MODE%", String(nled_mode));
  new_index_html.replace( "%T_DEVICE%", ntuya_device);
  new_index_html.replace( "%T_IP%", ntuya_ip);
  new_index_html.replace( "%T_CLIENT%", ntuya_api_client);
  new_index_html.replace( "%T_APIKEY%", ntuya_api_key);
  new_index_html.replace( "%T_HOST%", ntuya_host);
  new_index_html.replace( "%PRESS_STRING%", npress_string);
  new_index_html.replace( "%PRESSRELEASE_S%", npress_mode == PRESS_PRESSRELEASE ? SELECTED : "");
  new_index_html.replace( "%PRINT_S%", npress_mode == PRESS_PRINT ? SELECTED : "");
  new_index_html.replace( "%LED_ON_S%", nled_mode == LED_ON ? SELECTED : "");
  new_index_html.replace( "%LED_OFF_S%", nled_mode == LED_OFF ? SELECTED : "");
  new_index_html.replace( "%LED_ON_PRESS_S%", nled_mode == LED_ON_PRESS ? SELECTED : "");
  new_index_html.replace( "%LED_OFF_PRESS_S%", nled_mode == LED_OFF_PRESS ? SELECTED : "");
  new_index_html.replace( "%T_ON_S%", ntuya_mode == TUYA_ON ? SELECTED : "");
  new_index_html.replace( "%T_OFF_S%", ntuya_mode == TUYA_OFF ? SELECTED : "");
  new_index_html.replace( "%T_TOGGLE_S%", ntuya_mode == TUYA_TOGGLE ? SELECTED : "");
  new_index_html.replace( "%T_NONE_S%", ntuya_mode == TUYA_NONE ? SELECTED : "");
  new_index_html.replace( "%POWER_OFF%", String(npower_off_time));
  if (error != "") {
    new_index_html.replace("%REBOOT%", error);

  } else if (reboot) {
    new_index_html.replace("%REBOOT%", String("<h1>Device rebooting, please connect again in a few seconds</h1>"));
  } else {
    new_index_html.replace( "%REBOOT%", String(""));
  }
  server.send(200, "text/html", new_index_html.c_str());

  if (reboot) {
    delay(1000);
    resetFunc();
  }
}

void init_default_profile() {
  Serial.println("initialise profile");
  profile = "Default";
  CButtonProfile *p =  new CButtonProfile();
  p->name = profile;
  profiles->put(profile, p);
}

void setup(void) {
  Serial.begin(115200);

  while (!Serial) {
    delay(10);
  }
  if (!SPIFFS.begin(true)) {
    Serial.println("Error has occurred while mounting SPIFFS");
    return;
  }

  pinMode(BUTTON_PIN, INPUT_PULLUP); // config GIOP21 as input pin and enable the internal pull-up resistor


  EEPROM.begin(EEPROM_SIZE);
  init_eeprom();
  EEPROM.commit();
  readId(device_name);
  EEPROM.end();

  if (strcmp(device_name, "") == 0) {
    Serial.print("reset blank device name");
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
    wifiManager.startConfigPortal("ConfigureWiFi");
    const char *new_name = MDNSName.getValue();
    if (strcmp(new_name, device_name) != 0) {
      EEPROM.begin(EEPROM_SIZE);
      writeId(new_name);
      EEPROM.commit();
      EEPROM.end();
      strcpy(device_name, new_name);
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

  server.on("/setting", handleSetting);


  server.begin();
  Serial.println("HTTP server");
  bleKeyboard.begin();

  profiles =  new SimpleMap<String, CButtonProfile *>([](String & a, String & b) -> int {
    if (a == b) return 0;      // a and b are equal
    else if (a > b) return 1;  // a is bigger than b
    else return -1;            // a is smaller than b
  });
#if 1
  String buffer;

  if (readSettings(buffer)) {


    Serial.println("Readed settings");;

    DynamicJsonDocument root(4000);

    DeserializationError err = deserializeJson(root, buffer);


    if (!err) {
      Serial.println("parsed json");
      profile = (const char *)root["current"];
      Serial.println(profile);

      int arraySize = root[profiles_key].size();

      for (int i = 0; i < arraySize; i++) {

#if 1
        String profile_name = root["profiles"][i]["name"];
        CButtonProfile *p =  new CButtonProfile();
        profiles->put(profile_name, p);

        p->led_mode = root[profiles_key][i]["led_mode"];
        p->press_mode = root[profiles_key][i]["press_mode"];
        p->tuya_mode = root[profiles_key][i]["tuya_mode"];

        p->led_time = root[profiles_key][i]["led_time"];
        p->press_time = root[profiles_key][i]["press_time"];
        p->repeat_time = root[profiles_key][i]["repeat_time"];
        p->power_off_time = root[profiles_key][i]["power_off_time"];
        p->tuya_ip =   (const char *)root[profiles_key][i]["tuya_ip"];
        p->tuya_device =   (const char *)root[profiles_key][i]["tuya_device"];
        p->tuya_api_client =  (const char *)root[profiles_key][i]["tuya_api_client"];
        p->tuya_api_key =  (const char *)root[profiles_key][i]["tuya_api_key"];
        p->press_string =  (const char *)root[profiles_key][i]["press_string"];
#endif
      }

    } else {
      Serial.println("json parse failed:");
      //Serial.println(err.f_str());
      Serial.println(buffer);

      init_default_profile();

    }
  } else {
    init_default_profile();
  }
#else
  init_default_profile();
#endif

  CButtonProfile *p;
  if (profiles->has(profile)) {
    p = profiles->get(profile);
  } else {
    Serial.println("missing profile");
    Serial.println(profile);
    init_default_profile();
    p = profiles->get(profile);
  }
  if (p->tuya_api_client != "" && p->tuya_api_key != "" && p->tuya_host != "" ) {
    tuya = new TuyaAuth(p->tuya_host.c_str(), p->tuya_api_client.c_str(), p->tuya_api_key.c_str());
  }

}

void loop(void) {
  button.loop(); // MUST call the loop() function first
  server.handleClient();

  int btnState = digitalRead(BUTTON_PIN);
  if (button.isPressed()) {
    if (bleKeyboard.isConnected()) {

      delay(50);
      bleKeyboard.releaseAll();
    } else {
      Serial.print(".");
    }
  }

  delay(20);//allow the cpu to switch to other tasks


}



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

#include <SimpleMap.h>

#define EEPROM_SIZE 100

void(* resetFunc) (void) = 0; //declare reset function @ address 0

const char *settings_file = "settings.txt";
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



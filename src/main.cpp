#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include "LittleFS.h"
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <functional>

#define developerMode true

// Web server variables and setup:
AsyncWebServer server(80);

const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";
const char *PARAM_INPUT_3 = "ip";
const char *PARAM_INPUT_4 = "gateway";

String ssid;
String pass;
String ip;
String gateway;
String errWarn;

IPAddress localIP;
// IPAddress localIP(192, 168, 1, 166); // hardcoded
// Set your Gateway IP address
IPAddress localGateway;
// IPAddress localGateway(192, 168, 1, 1); //hardcoded
IPAddress subnet(255, 255, 0, 0);



// Atmega variables:
struct controller_settings{
  bool checked[8] = {
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false
    };
  String settings[10] = {"", "", "", "", "", "", "", "", "", ""};
};

struct controller_data{
  String KKK_Temp = "";
  String KVC_Temp = "";
  String GRN_Temp = "";
  String LAU_Temp = "";
  String relay[4] = {"N/A", "N/A", "N/A", "N/A"};
};

struct repeatedFunctionsOnTimer{
  unsigned long timeSinceLastCall;
  unsigned long timeDelay;
  bool (*func)();
};

controller_settings atmegaSet;
controller_data atmegaData;


// Other variables:
bool restart = false;

#define SPTR_SIZE 21
char *separatedData[SPTR_SIZE];

int i;
int gautas_int[53];       //kintamasis array MAX value 32767(16 bitų).
char gautas_char[53];
int zinutes_ilgis;
String temp[4] = {"N/A", "N/A", "N/A", "N/A"};
String relay[4] = {"N/A", "N/A", "N/A", "N/A"};
String KKK_Temp;
String KVC_Temp;
String GRN_Temp;
String LAU_Temp;

repeatedFunctionsOnTimer reFunctions[20];
int reFuncSize = 0;

bool dynamicIP = false;

// put function declarations here:
void initFS();
String processor(const String &var);
void startWebServer();
String responseAllDataJson();
void processMega();
int separate(String str, char **p, int size);
String read_temp(char data[], bool hasNegative = false);
void processSettingRecieve();
bool EEPROM_write_SSID(char ssid[]);
bool EEPROM_read_SSID(char val[], int arrSize);
bool is_set_SSID();
void EEPROM_delete_SSID();
bool EEPROM_write_password(char password[]);
void EEPROM_delete_password();
bool is_set_password();
bool EEPROM_read_passowrd(char val[], int arrSize);
void EEPROM_write_ip(IPAddress ip);
void EEPROM_delete_ip();
bool is_set_ip();
IPAddress EEPROM_read_ip();
void EEPROM_write_gateway(IPAddress gateway);
void EEPROM_delete_gateway();
bool is_set_gateway();
IPAddress EEPROM_read_gateway();
void EEPROM_deleta_all();
void APProtocol();
bool startWifiServer();
void log(String message);
void startAP();
void repeatFunction(int time, bool (*func)());
void runThroughRepeatFunctions();


//  ------------------------------------------------------------------------------------------------------------------------------------------------
//          SETUP 
//  ------------------------------------------------------------------------------------------------------------------------------------------------

void setup() {
  Serial.begin(9600);
  EEPROM.begin(256);  //Initialize EEPROM

  if(developerMode){
    log("\nDeveloper mode is enabled");
  }else{
    delay(3000);
  }
  log("\nESP Begin");

  initFS();


  log("checking for login");
  log(is_set_gateway());
  log(is_set_ip());
  log(is_set_SSID());

  if(is_set_gateway() && is_set_ip() && is_set_SSID())
  {
    log("setting wifi values");
    char tmp[33] = "";
    EEPROM_read_SSID(tmp, 33);
    ssid = String(tmp);
    
    pass = "";
    if(is_set_password()){
      char ptmp[64] = "";
      EEPROM_read_passowrd(ptmp, 64);
      pass = String(ptmp);
    }
    

    IPAddress IPtmp;
    IPAddress GWtmp;
    IPtmp = EEPROM_read_ip();
    
    ip = "";
    ip.concat((String)IPtmp[0]);
    ip.concat('.');
    ip.concat((String)IPtmp[1]);
    ip.concat('.');
    ip.concat((String)IPtmp[2]);
    ip.concat('.');
    ip.concat((String)IPtmp[3]);

    GWtmp = EEPROM_read_gateway();
    gateway = "";
    gateway.concat((String)GWtmp[0]);
    gateway.concat('.');
    gateway.concat((String)GWtmp[1]);
    gateway.concat('.');
    gateway.concat((String)GWtmp[2]);
    gateway.concat('.');
    gateway.concat((String)GWtmp[3]);

    // ssid = "HUAWEI-B525-CCA4";
    // pass = "8DBY2TFM80F";
    // ip = "192.168.8.166";
    // gateway = "192.168.8.1";
    log("ssid: " + ssid);
    log("pass: " + pass);
    log("ip: " + ip);
    log("gateway: " + gateway);
    if(ip == "0.0.0.0"){
      log("Using dynamic IP");
      dynamicIP = true;
    }

    if(startWifiServer()){
        return;
    }
    repeatFunction(300000, &startWifiServer);
  }

  startAP();
  
  
  log("Setup finished");
}

//  ------------------------------------------------------------------------------------------------------------------------------------------------
//          MAIN LOOP 
//  ------------------------------------------------------------------------------------------------------------------------------------------------

void loop() {
  if (restart)      //saugus kontrolerio restartavimas
  {
    log("restarting");
    EEPROM.commit();
    delay(100);
    ESP.restart();
  }

  if (Serial.available() > 0)
  {
    processMega();
  }

  runThroughRepeatFunctions();
}

void processMega()
{
  if (Serial.available()>0)         //Originaliai testms: if (RS232Serial.available())
  {
    delay(60);  //duok laiko atvykti visai serial zinutei
    for (i = 0; i < 53; i++)
      {
        gautas_char[i] = Serial.read();
        gautas_int[i] = gautas_char[i];
        //Serial.print(gautas_char[i]);     //čIA YRA GAUTAS TEKSTAS PO RAIDĘ
        if (gautas_int[i] == 13)  //kai gaunam carriage return, baigiam skaityti buferį
          {
            break;
          }
          else
          {
            zinutes_ilgis = i;
          }
      }
      Serial.println();                         //numetam eilutę              
      i = 0; 
      while (Serial.available())      //isvalom serial buferi
        {
          Serial.read();
        }
  } //If Serial available pabaiga

  // delay(30);
  // String dataRecieve = (String)Serial.readStringUntil(13);
  if (gautas_char[0] == 'R' && gautas_char[1] == ',')
  {
    separate(String(gautas_char), separatedData, SPTR_SIZE);
    int requestNum = atoi(separatedData[2]);
    switch (requestNum)
    {
    case 3:
    {
      atmegaData.GRN_Temp = read_temp((separatedData[3]));
      atmegaData.KKK_Temp = read_temp((separatedData[4]));
      atmegaData.KVC_Temp = read_temp((separatedData[5]));
      atmegaData.LAU_Temp = read_temp((separatedData[6]), true);      
    };
    break;
    case 8:
    {
      int switches = atoi(separatedData[3]);
      if (switches >= 128)
      {
        switches = switches - 128;
        atmegaData.relay[0] = "ON";
      }
      else
      {
        atmegaData.relay[0] = "OFF";
      }
      if (switches >= 64)
      {
        switches = switches - 64;
        atmegaData.relay[1] = "ON";
      }
      else
      {
        atmegaData.relay[1] = "OFF";
      }
      if (switches >= 32)
      {
        switches = switches - 32;
        atmegaData.relay[2] = "ON";
      }
      else
      {
        atmegaData.relay[2] = "OFF";
      }
      if (switches >= 16)
      {
        switches = switches - 16;
        atmegaData.relay[3] = "ON";
      }
      else
      {
        atmegaData.relay[3] = "OFF";
      }
    };
    break;
    case 40:
    {
      processSettingRecieve();  
    };
    break;
    default:
    break;
    }
  }
}

int separate(String str, char **p, int size)
{
  int n;
  char s[200];

  strcpy(s, str.c_str());

  *p++ = strtok(s, ",");
  for (n = 1; NULL != (*p++ = strtok(NULL, ",")); n++)
    if (size == n)
      break;

  return n;
}

void startWebServer(){
    log("Webserver gateway: " + WiFi.gatewayIP().toString());
    log("Starting Web Server on IP: " + WiFi.localIP().toString());
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", "text/html", false, processor); });

    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/settings.html", "text/html", false, processor); });

    // server.on("/getData", HTTP_GET, [](AsyncWebServerRequest *request)
    //           { request->send(200, "application/json", responseDataJson()); });

    server.on("/getAllData", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", responseAllDataJson()); });

    server.on("/loadDefParams", HTTP_GET, [](AsyncWebServerRequest *request){
      Serial.print("R,255,6");
      Serial.write(13);
      request->send(200);
    });

    server.on("/control1", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/control1.html", "text/html", false, processor); });

    server.on("/control2", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/control2.html", "text/html", false, processor); });

    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                         {
    if (request->url() == "/sendData") {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, (const char*)data);
      if((String)doc["checked"] != "null"){
        // 20,21,22,30,31,32,33,37
  

        switch ((int)doc["checked"][0]){
          case 1:
            Serial.print("R,255,20,");
            Serial.print((bool)doc["checked"][1]);
            Serial.write(13);
          break;
          case 2:
            Serial.print("R,255,21,");
            Serial.print((bool)doc["checked"][1]);
            Serial.write(13);
          break;
          case 3:
            Serial.print("R,255,22,");
            Serial.print((bool)doc["checked"][1]);
            Serial.write(13);
          break;
          case 4:
            Serial.print("R,255,30,");
            Serial.print((bool)doc["checked"][1]);
            Serial.write(13);
          break;
          case 5:
            Serial.print("R,255,31,");
            Serial.print((bool)doc["checked"][1]);
            Serial.write(13);
          break;
          case 6:
            Serial.print("R,255,32,");
            Serial.print((bool)doc["checked"][1]);
            Serial.write(13);
          break;
          case 7:
            Serial.print("R,255,33,");
            Serial.print((bool)doc["checked"][1]);
            Serial.write(13);
          break;
          case 8:
            Serial.print("R,255,37,");
            Serial.print((bool)doc["checked"][1]);
            Serial.write(13);
          break;
          default:
          break;
        }
        // checkedLocal[(int)doc["checked"][0] - 1] = (bool)doc["checked"][1];
      }

      //int Minimali_GRN_temp;                23  [5]
      //int GRN_KVC_valdymo_start_temp;       24  [6]
      //int GRN_KVC_valdymo_stop_temp;        25  [9]
      //int KVC_histereze;                    26  [4] 
      //int KVC_nustatyta_temp;               27  [8]
      //int GRN_nustatoma_temp;               28  [2] 
      //int Temp_pazemejimas;                 29  [7]
      //int GRN_pavaros_sukimo_periodas;      34  [1]
      //int Lauko_temp_KKK_startui;           35  [3] 
      //int KKK_valdymo_histereze;            36  [10]

      if((String)doc["settings"] != "null"){
        // 23,24,25,26,27,28,29,34,35,36
        switch ((int)doc["settings"][0])
        {
        case 1:
          Serial.print("R,255,23,");
          Serial.print((String)doc["settings"][1]);
          Serial.write(13);
        break;
        case 2:
          Serial.print("R,255,24,");
          Serial.print((String)doc["settings"][1]);
          Serial.write(13);
        break;
        case 3:
          Serial.print("R,255,25,");
          Serial.print((String)doc["settings"][1]);
          Serial.write(13);
        break;
        case 4:
          Serial.print("R,255,26,");
          Serial.print((String)doc["settings"][1]);
          Serial.write(13);
        break;
        case 5:
          Serial.print("R,255,27,");
          Serial.print((String)doc["settings"][1]);
          Serial.write(13);
        break;
        case 6:
          Serial.print("R,255,28,");
          Serial.print((String)doc["settings"][1]);
          Serial.write(13);
        break;
        case 7:
          Serial.print("R,255,29,");
          Serial.print((String)doc["settings"][1]);
          Serial.write(13);
        break;
        case 8:
          Serial.print("R,255,34,");
          Serial.print((String)doc["settings"][1]);
          Serial.write(13);
        break;
        case 9:
          Serial.print("R,255,35,");
          Serial.print((String)doc["settings"][1]);
          Serial.write(13);
        break;
        case 10:
          Serial.print("R,255,36,");
          Serial.print((String)doc["settings"][1]);
          Serial.write(13);
        break;
        default:
        break;
        }
        // settingsLocal[(int)doc["settings"][0] - 1] = (String)doc["settings"][1];
      }
      request->send(200);
    } });

    server.serveStatic("/", LittleFS, "/");

    server.on("/delConf", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      // currently not working
      EEPROM_deleta_all();
      // writeFile(LittleFS, ssidPath, "");
      // writeFile(LittleFS, passPath, "");
      // writeFile(LittleFS, ipPath, "");
      // writeFile(LittleFS, gatewayPath, "");

      request->send(200, "text/plain", "Done. ESP will restart.");

      // delay(5000);
      // ESP.restart();
      restart = true; 
    });

    // currently can't add new IP
    server.on("/newIp", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                int params = request->params();
                for (int i = 0; i < params; i++)
                {
                  AsyncWebParameter *p = request->getParam(i);
                  if (p->isPost())
                  {
                    // HTTP POST ssid value
                    if (p->name() == "ip")
                    {
                      ip = p->value().c_str();
                      IPAddress newIP;
                      newIP.fromString(ip);
                      log(ip);
                      EEPROM_write_ip(newIP);
                      // Write file to save value
                      // writeFile(LittleFS, ipPath, ip.c_str());
                    }
                  }
                }
                request->send(200);
                restart = true; });

    server.begin();

    log("Web server has started");
}

// process html file, load html elements 
String processor(const String &var)
{
  if (var == "CURRENTIP")
  {
    return WiFi.localIP().toString();
  }
  else if (var == "WARNING")
  {
    return errWarn;
  }
  else
  {
    return "";
  }
  return String();
}

// Could use JSON library but there could be a problem with preformance then
String responseAllDataJson()
{
  String json = "{";

  json.concat("\"temp1");
  json.concat("\": \"");
  json.concat(atmegaData.KKK_Temp);
  json.concat("\",");

  json.concat("\"temp2");
  json.concat("\": \"");
  json.concat(atmegaData.KVC_Temp);
  json.concat("\",");

  json.concat("\"temp3");
  json.concat("\": \"");
  json.concat(atmegaData.GRN_Temp);
  json.concat("\",");

  json.concat("\"temp4");
  json.concat("\": \"");
  json.concat(atmegaData.LAU_Temp);
  json.concat("\",");


  for (int i = 0; i < 4; i++)
  {
    json.concat("\"par");
    json.concat(String(i + 1));
    json.concat("\": \"");
    json.concat(atmegaData.relay[i]);
    json.concat("\",");
  }
  for (int i = 0; i < 8; i++)
  {
    json.concat("\"checked");
    json.concat(String(i + 1));
    json.concat("\": ");
    json.concat(atmegaSet.checked[i]);
    json.concat(",");
  }
    for (int i = 0; i < 9; i++)
  {
    json.concat("\"setting");
    json.concat(String(i + 1));
    json.concat("\": \"");
    json.concat(atmegaSet.settings[i]);
    json.concat("\",");
  }

    json.concat("\"setting");
    json.concat("10");
    json.concat("\": \"");
    json.concat(atmegaSet.settings[9]);
    json.concat("\"}");

  return json;
}

String read_temp(char data[], bool hasNegative){
  String store = "";
  if(data == "-12700"){
    store.concat("no data");
    return store;
  }
  if(hasNegative){
  store.concat(data[0]);
  store.concat(data[1]);
  store.concat(data[2]);
  store.concat(".");
  store.concat(data[3]);
  store.concat(data[4]);
  }else{
  store.concat(data[0]);
  store.concat(data[1]);
  store.concat(".");
  store.concat(data[2]);
  store.concat(data[3]);
  }
  return store;
}

void processSettingRecieve()
{
  for (int i = 0; i < 3; i++)
  {
    atmegaSet.checked[i] = atoi(separatedData[i + 3]);
  }

  for (int i = 3; i < 7; i++)
  {
    atmegaSet.checked[i] = atoi(separatedData[i + 10]);
  }

  atmegaSet.checked[7] = atoi(separatedData[20]);

// R,0,40,1,0,0,20,50,35,10,50,26,2,0,1,1,1,120,12,15,1

//bool GRN_valdymas_pagal_LAU_enable;   20  
//bool Manual_control_enable;           21  
//bool Lowered_regime_enable;           22  
//int Minimali_GRN_temp;                23  1  6  
//int GRN_KVC_valdymo_start_temp;       24  2  7  
//int GRN_KVC_valdymo_stop_temp;        25  3  8  
//int KVC_histereze;                    26  4  9  
//int KVC_nustatyta_temp;               27  5  10
//int GRN_nustatoma_temp;               28  6  11 
//int Temp_pazemejimas;                 29  7  12 
//bool Komforto_rezimas_enable;         30  
//bool KVC_valdymas_enable;             31  
//bool KKK_valdymas_enable;             32  
//bool GRN_valdymas_enable;             33  
//int GRN_pavaros_sukimo_periodas;      34  8  17 
//int Lauko_temp_KKK_startui;           35  9  18 
//int KKK_valdymo_histereze;            36  10 19 
//bool KVC_valdo_KKK_enable;            37  

  atmegaSet.settings[4] = String(separatedData[10]);
  atmegaSet.settings[5] = String(separatedData[11]);
  atmegaSet.settings[8] = String(separatedData[18]);
  atmegaSet.settings[3] = String(separatedData[9]);
  atmegaSet.settings[7] = String(separatedData[17]);
  atmegaSet.settings[1] = String(separatedData[7]);
  atmegaSet.settings[6] = String(separatedData[12]);
  atmegaSet.settings[0] = String(separatedData[6]);
  atmegaSet.settings[2] = String(separatedData[8]);
  atmegaSet.settings[9] = String(separatedData[19]);

  // atmegaSet.settings[0] = String(separatedData[6]);

  // for (int i = 1; i < 7; i++)
  // {
  //   atmegaSet.settings[i] = "";
  //   int stringLen = strlen(separatedData[i + 6]);
  //   if (stringLen > 2)
  //   {
  //     for (int y = 0; y < stringLen - 1; y++)
  //     {
  //       atmegaSet.settings[i].concat(String(separatedData[i + 6][y]));
  //     }
  //     atmegaSet.settings[i].concat(String(separatedData[i + 6][stringLen - 1]));
  //   }
  //   else
  //   {
  //     atmegaSet.settings[i] = separatedData[i + 6];
  //   }
  // }

  // for (int i = 7; i < 10; i++)
  // {
  //   atmegaSet.settings[i] = "";
  //   int stringLen = strlen(separatedData[i + 10]);
  //   if (stringLen > 2)
  //   {
  //     for (int y = 0; y < stringLen - 1; y++)
  //     {
  //       atmegaSet.settings[i] += String(separatedData[i + 10][y]);
  //     }
  //     atmegaSet.settings[i].concat(String(separatedData[i + 10][stringLen - 1]));
  //   }
  //   else
  //   {
  //     atmegaSet.settings[i] = separatedData[i + 10];
  //   }
  // }
}

void initFS()   //LittleFS bibliotekos inicijavimas (reikalingas duomenų flashinimui)
{
  log("Initializing LittleFS");

  if (!LittleFS.begin())
  {
    log("An error has occurred while mounting LittleFS");
  }
  else
  {
    log("LittleFS has been initiated");
  }
}


// R,0,40,1,0,0,20,50,35,10,50,26,2,0,1,1,1,120,12,15,1
// R,0,03,6840,5365,2820,+0230
// R,0,08,024gaunami iš Atmegos paeičiant single

// ssid reserved bytes 0-35 
// password reserved bytes 36-100
// ip reserved bytes 101-105
// gateway reserved bytes 106-110

bool EEPROM_write_SSID(const char ssid[]){
  int ssid_length = int(strlen(ssid));
  if(ssid_length > 32){
    EEPROM_delete_SSID();
    return false;
  }
  EEPROM.write(0, ssid_length);
  for (int i = 0; i < ssid_length; i++)
  {
    EEPROM.write(i+1, ssid[i]);
  }
  EEPROM.commit();
  return true;
}

void EEPROM_delete_SSID(){
  EEPROM.write(0, 0);
  
}

bool is_set_SSID(){
  return EEPROM.read(0);
}

bool EEPROM_read_SSID(char val[], int arrSize){
  int SSID_size = EEPROM.read(0);
  if(SSID_size+1 > arrSize){
    return false;
  }
  for (int i = 0; i < SSID_size; i++)
  {
    val[i] = EEPROM.read(i+1);
  }
  val[SSID_size] = '\0';
  return true;
}



bool EEPROM_write_password(const char EEpassword[]){
  int password_length = int(strlen(EEpassword));
  if(password_length > 63){
    EEPROM_delete_password();
    return false;
  }
  EEPROM.write(36, password_length);
  for (int i = 0; i < password_length; i++)
  {
    EEPROM.write(i+37, EEpassword[i]);
  }
  EEPROM.commit();
  return true;
}

void EEPROM_delete_password(){
  EEPROM.write(36, 0);
  

}

bool is_set_password(){
  return EEPROM.read(36);
}

bool EEPROM_read_passowrd(char val[], int arrSize){
  int password_size = EEPROM.read(36);
  if(password_size+1 > arrSize){
    return false;
  }
  for (int i = 0; i < password_size; i++)
  {
    val[i] = EEPROM.read(i+37);
  }
  val[password_size] = '\0';
  return true;
}



void EEPROM_write_ip(IPAddress EEip){
  EEPROM.write(101, 1);
  for (int i = 0; i < 4; i++)
  {
    EEPROM.write(i+102, EEip[i]);
  }
  EEPROM.commit();
  
}

void EEPROM_delete_ip(){
  EEPROM.write(101, 0);
  

}

bool is_set_ip(){
  return EEPROM.read(101);
}

IPAddress EEPROM_read_ip(){
  IPAddress ip; 
  for (int i = 0; i < 4; i++)
  {
    ip[i] = EEPROM.read(i+102);
  }
  return ip;
}



void EEPROM_write_gateway(IPAddress EEgateway){
  EEPROM.write(106, 1);
  for (int i = 0; i < 4; i++)
  {
    EEPROM.write(i+107, EEgateway[i]);
  }
  
  EEPROM.commit();
}

void EEPROM_delete_gateway(){
  EEPROM.write(106, 0);
  
}

bool is_set_gateway(){
  return EEPROM.read(106);
}

IPAddress EEPROM_read_gateway(){
  IPAddress ip; 
  for (int i = 0; i < 4; i++)
  {
    ip[i] = EEPROM.read(i+107);
  }
  return ip;
}

void EEPROM_deleta_all(){
  EEPROM_delete_ip();
  EEPROM_delete_gateway();
  EEPROM_delete_SSID();
  EEPROM_delete_password();

  EEPROM.commit();
}

void APProtocol()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/wifimanager.html", "text/html", false, processor); });

  server.on("/setupWifi", HTTP_POST, [](AsyncWebServerRequest *request){
    int params = request->params();
    for (int i = 0; i < params; i++) {
      AsyncWebParameter* p = request->getParam(i);
      String var = p->name();
      if(var == "ssid"){
        log("writing to ssid");
        log(p->value().c_str());

        EEPROM_write_SSID(p->value().c_str());
      }else if(var == "pass"){
        log("writing to pass");
        log(p->value().c_str());


        EEPROM_write_password(p->value().c_str());
      }else if(var == "ip"){
        log("writing to ip");
        log(p->value().c_str());

        IPAddress postIp;
        postIp.fromString(p->value().c_str());
        EEPROM_write_ip(postIp);
      }else if(var == "gateway"){
        log("writing to gateway");
        log(p->value().c_str());

        IPAddress postGw;
        postGw.fromString(p->value().c_str());
        EEPROM_write_gateway(postGw);

      }
      // Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    }
    request->send(200, "text/plain", "Done. Restarting");
    restart = true;
  });
}

bool startWifiServer(){
    log("initializing WiFi");

    WiFi.mode(WIFI_STA);
    localIP.fromString(ip.c_str());
    localGateway.fromString(gateway.c_str());

    WiFi.hostname("Superijus");

    if(!dynamicIP){
      if (!WiFi.config(localIP, localGateway, subnet))
      {
        log("STA Failed to configure");
        return false;
      }
    }

    WiFi.begin(ssid.c_str(), pass.c_str());

    delay(10000);
    if (WiFi.status() != WL_CONNECTED)
    {
      log("Failed to connect.");
      return false;
    }else{
      log("successfully connected to WiFi");
      WiFi.softAPdisconnect(true);
    }

    startWebServer();
    EEPROM_write_gateway(WiFi.gatewayIP());

    log("Setup finished");

    return true;
}

void log(String message){
  if(developerMode){
    Serial.println(message);
  }
}

void startAP(){
  log("initializing AP");

  IPAddress softAPIP(192,168,8,1);

  WiFi.softAP("Superijus Smart");

  //set-up the custom IP address
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(softAPIP, softAPIP, IPAddress(255, 255, 255, 0));   // subnet FF FF FF 00  

  // IPAddress IP = WiFi.softAPIP();
  log(WiFi.softAPIP().toString());

  APProtocol();

  server.serveStatic("/", LittleFS, "/");

  server.begin();

  // WiFi.softAPdisconnect (true);
}

void repeatFunction(int time, bool (*function)()){
  reFunctions[reFuncSize].func = function;
  reFunctions[reFuncSize].timeDelay = time;
  reFunctions[reFuncSize].timeSinceLastCall = millis();
  reFuncSize++;
}

void runThroughRepeatFunctions(){
  unsigned long currentMillis = millis();
  for (int i = 0; i < reFuncSize; i++)
  {
    if((currentMillis - reFunctions[i].timeSinceLastCall) > reFunctions[i].timeDelay){
      reFunctions[i].func();
      reFunctions[i].timeSinceLastCall = currentMillis;
    }
  }
}
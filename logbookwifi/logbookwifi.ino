#include "ESP8266WiFi.h"
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>

//TODO:
// - change password
// - change cache name


// power mode: 80mA
// sleep mode: 20uA

// flash size: 4MB, FS=2MB


#define ADMIN_PASSWORD                        "5151"

#define LOW_BATTERY_TRESHOLD                  2700
#define LOW_MEMORY_TRESHOLD                   1000
#define STOP_SYSLOG_BELOW_MEMORY              300000
#define HOSTNAME                              "logbook"
#define AP_WIFI_SSID                          "GeocacheLogbook"
#define AP_WIFI_PASS                          "geocaching"
#define LOGBOOK_FILE_NAME                     "/logbook.txt"
#define SYSLOG_FILE_NAME                      "/syslog.txt"
#define STATS_FILE_NAME                       "/stats.dat"
#define GRACE_PERIOD_SECONDS_AFTER_POWER_ON   120
#define SLEEP_AFTER_SECONDS                   60

#define LED_PIN 12
#define LED2_PIN 15


long cycles = 0;
char buffer[32];
char buffer2[64];
int ota_progress = 0;
unsigned long last_activity = 0;
bool low_battery = false;
int low_battery_check = 0;
char large_buffer[100];
long manual_off_ts = 0;
struct statsStruct {
    long power;
    long upseconds;
    long manualoffs;
} stats;
File fsUploadFile;


DNSServer dnsServer;
ESP8266WebServer server(80);
ADC_MODE(ADC_VCC);

void blink(int count, int ms)
{
  for (int i=0; i<count; i++)
  {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(LED2_PIN, HIGH);
    delay(ms);
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(LED2_PIN, LOW);
    delay(ms);
  }
}

void serialDebugP(const __FlashStringHelper* msg) {
  Serial.print(msg);
  serialDebugInfo();
}

void serialDebug(const char* msg) {
  Serial.print(msg);
  serialDebugInfo();
}

void serialDebugInfo() {
  Serial.print(F(" Vcc="));
  Serial.print(ESP.getVcc());
  Serial.print(F(" fs_space="));
  Serial.print(getFreeSpace());
  Serial.print(F(" free_heap="));
  Serial.println(ESP.getFreeHeap());  
}

void setupOTA()
{
  ArduinoOTA.onStart([]() {
    last_activity = millis();
    blink(5, 25);
    writeToSyslog("OTA-START");
  });

  ArduinoOTA.onEnd([]() {  
    last_activity = millis();
    blink(3, 200);
    writeToSyslog("OTA-DONE");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    last_activity = millis();
    writeToSyslog("OTA-ERR");
    blink(10, 25);
    ESP.restart();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    last_activity = millis();
    long percent = (progress * 100l) / total;
    sprintf(buffer, "OTA %d%%    ", percent);
    Serial.println(buffer);
    ota_progress++;
    digitalWrite(LED_PIN, ota_progress % 2 == 0 ? LOW : HIGH);
    digitalWrite(LED2_PIN, ota_progress % 2 == 0 ? LOW : HIGH);
  });  

  ArduinoOTA.setPassword(ADMIN_PASSWORD);
  ArduinoOTA.begin();
}

long getFreeSpace() {
  FSInfo fs_info;
  LittleFS.info(fs_info);
  return fs_info.totalBytes - fs_info.usedBytes;
}

void writeToSyslog(const char* msg)
{
  if (!LittleFS.exists(SYSLOG_FILE_NAME))
  {
    serialDebugP(F("creating syslog"));
    File create = LittleFS.open(SYSLOG_FILE_NAME, "w");
    create.close();
  } else {
    serialDebugP(F("syslog exists"));
  }

  long left = getFreeSpace();
  if (left >= STOP_SYSLOG_BELOW_MEMORY)
  {
    File file = LittleFS.open(SYSLOG_FILE_NAME, "a");

    //nr of power up
    ltoa(stats.power, buffer, 10);
    int l = strlen(buffer);
    buffer[l] = '\t';
    file.write(buffer, l+1);

    //write msg
    strcpy(buffer, msg);
    l = strlen(buffer);
    buffer[l] = '\t';
    file.write(buffer, l+1);

    //write free space
    ltoa(left,buffer,10);
    l = strlen(buffer);
    buffer[l] = '\t';
    file.write(buffer, l+1);

    //write vcc
    ltoa(ESP.getVcc(),buffer,10);
    l = strlen(buffer);
    buffer[l] = '\n';
    file.write(buffer, l+1);    

    file.close();
  }

  serialDebug(msg);
}

void readStats()
{
  memset((uint8_t*) &stats, 0, sizeof(stats));
  if (LittleFS.exists(STATS_FILE_NAME))
  { 
      serialDebugP(F("stats exists"));
      File file = LittleFS.open(STATS_FILE_NAME, "r");
      file.read((uint8_t*) &stats, sizeof(stats));
      file.close();
  }
  else
  {
    writeToSyslog("CREATESTATS");
    File file = LittleFS.open(STATS_FILE_NAME, "w");
    file.write((uint8_t*) &stats, sizeof(stats));
    file.close();
  }
}

void saveStats()
{
  File file = LittleFS.open(STATS_FILE_NAME, "w");
  file.write((uint8_t*) &stats, sizeof(stats));
  file.close();
}

void	wifi_handle_event_cb(System_Event_t	*evt) {
  if (evt->event == EVENT_SOFTAPMODE_STACONNECTED) {
    sprintf(buffer2, "CONNECTED "	MACSTR, MAC2STR(evt->event_info.sta_connected.mac));
    writeToSyslog(buffer2);
    last_activity = millis();
    blink(2, 30);
  }
}

void setup() {
  delay(50);
  pinMode(0, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);  
  pinMode(13, INPUT_PULLUP);
  pinMode(16, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  blink(2, 50);

  Serial.begin(9600);
  delay(10);
  Serial.println();
  Serial.println();
  serialDebugP(F("setup"));

  if (!LittleFS.begin()) {
    serialDebugP(F("could not mount the filesystem..."));
  }

  wifi_set_event_handler_cb(wifi_handle_event_cb);
  if (!WiFi.softAP(AP_WIFI_SSID, AP_WIFI_PASS)) {
    serialDebugP(F("could not start wifi..."));
  }

  IPAddress myIP = WiFi.softAPIP();
  Serial.println(myIP.toString());

  dnsServer.start(53, "*", myIP);
  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
  }

  WiFi.setHostname(HOSTNAME);
  setupWebServer();
  setupOTA();
  blink(3, 50);

  readStats();
  stats.power++;
  writeToSyslog("ON");
  last_activity = millis();
}

void setupWebServer()
{
  server.on("/",  HTTP_GET, [] {
    blink(1, 50);
    last_activity = millis();
    File file = LittleFS.open(getFreeSpace() < LOW_MEMORY_TRESHOLD ? "home-f.html" : "home.html" , "r");
    server.streamFile(file, "text/html");
    file.close(); 
  });

  server.on("/signin",  HTTP_GET, [] {
    blink(1, 50);
    last_activity = millis();
    File file = LittleFS.open(getFreeSpace() < LOW_MEMORY_TRESHOLD ? "signin-f.html" : "signin.html", "r");
    server.streamFile(file, "text/html"); 
    file.close();
  });

  server.on("/signin",  HTTP_POST, [] {
    blink(2, 50);
    last_activity = millis();
    if (getFreeSpace() < LOW_MEMORY_TRESHOLD) {
      File file = LittleFS.open("signin-f.html", "r");
      server.streamFile(file, "text/html"); 
      file.close();
    } else {

      if ( !server.hasArg("date") || !server.hasArg("nick") || !server.hasArg("info") ) {
        server.sendHeader("Location", "/signin?msg=datamissing", true);
        server.send(302);
      }

      String date = server.arg("date");
      String nick = server.arg("nick");
      String info = server.arg("info");  
      if (date.length() == 0 || nick.length() == 0 || info.length() == 0) {
        server.sendHeader("Location", "/signin?msg=datamissing", true);
        server.send(302);
      }

      saveToLogbook(date, nick, info);
      server.sendHeader("Location", "/?msg=ok", true);
      server.send(302);
    }
  });

  server.on("/browse",  HTTP_GET, [] {
    blink(1, 50);
    last_activity = millis();
    File file = LittleFS.open("browse.html", "r");
    server.streamFile(file, "text/html"); 
    file.close();
  });

  server.on("/finish",  HTTP_GET, [] {
    blink(1, 50);
    last_activity = millis();
    File file = LittleFS.open("finish.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  });   

  server.on("/finish",  HTTP_POST, [] {
    blink(1,50);
    server.sendHeader("Location", "/finish?msg=off", true);
    server.send(302);
    manual_off_ts = millis();
  });  

  server.on("/admin",  HTTP_GET, [] {
    blink(1, 50);
    last_activity = millis();
    File file = LittleFS.open("admin.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  });    

  server.on("/test",  HTTP_GET, [] {
    blink(1, 50);
    last_activity = millis();
    File file = LittleFS.open("test.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  });       

  server.on("/admin",  HTTP_POST, [] {
    blink(1, 50);
    last_activity = millis();
    if ( !server.hasArg("password") || !server.arg("password").equals(ADMIN_PASSWORD) ) {
      server.sendHeader("Location", "/admin?msg=err", true);
      server.send(302);
    } else {

      if (server.hasArg("logbook") && server.arg("logbook").equals("on")) {
        LittleFS.remove(LOGBOOK_FILE_NAME);
        writeToSyslog("CLEARLOGBOOK");
      }

      if (server.hasArg("syslog") && server.arg("syslog").equals("on")) {
        LittleFS.remove(SYSLOG_FILE_NAME);
      }

      if (server.hasArg("stats") && server.arg("stats").equals("on")) {
        LittleFS.remove(STATS_FILE_NAME);
        writeToSyslog("CLEARSTATS");
        readStats(); 
      }       

      server.sendHeader("Location", "/admin?msg=ok", true);
      server.send(302);
    }
  });     

  server.on("/upload",  HTTP_POST, [](){}, [] {
    blink(1, 50);
    last_activity = millis();
    if ( !server.hasArg("password") || !server.arg("password").equals(ADMIN_PASSWORD) ) {
      server.sendHeader("Location", "/admin?msg=err", true);
      server.send(302);
    } else {
      HTTPUpload& upload = server.upload();
      if(upload.status == UPLOAD_FILE_START){  
        String filename = upload.filename;
        if(!filename.startsWith("/")) filename = "/"+filename;
        fsUploadFile = LittleFS.open(filename, "w");
      } else if(upload.status == UPLOAD_FILE_WRITE){
        if(fsUploadFile)
          fsUploadFile.write(upload.buf, upload.currentSize); 
      } else if(upload.status == UPLOAD_FILE_END){
        if(fsUploadFile) {    
          writeToSyslog(fsUploadFile.fullName());                                 
          fsUploadFile.close();                    
          server.sendHeader("Location", "/admin?msg=ok", true);
          server.send(302);
        } else {
          server.sendHeader("Location", "/admin?msg=err", true);
          server.send(302);
        }
      } else {
        server.sendHeader("Location", "/admin?msg=err", true);
        server.send(302);
      }
    }
  });

  server.on(LOGBOOK_FILE_NAME,  HTTP_GET, [] {
    blink(1, 50);
    last_activity = millis();
    File file = LittleFS.open(LOGBOOK_FILE_NAME, "r");
    server.streamFile(file, "text/plain");
    file.close();
  });

  server.on(SYSLOG_FILE_NAME,  HTTP_GET, [] {
    blink(1, 50);
    last_activity = millis();
    File file = LittleFS.open(SYSLOG_FILE_NAME, "r");
    server.streamFile(file, "text/plain");
    file.close();
  });

  server.on("/status",  HTTP_GET, [] {
    blink(1, 50);
    last_activity = millis();
    int size = getLogbookSize();
    sprintf(large_buffer, "{\"size\":%d, \"low_bat\":%d}",size, low_battery ? 1 : 0);
    server.send(200, "application/json", large_buffer);
  }); 

  server.on("/internal",  HTTP_GET, [] {
    blink(1, 50);
    last_activity = millis();
    int size = getLogbookSize();
    sprintf(large_buffer, "{\"size\":%d, \"pwr\":%d, \"ups\":%d, \"man\":%d, \"bat\":%d, \"fs\":%d}",size, stats.power, stats.upseconds + millis()/1000, stats.manualoffs, ESP.getVcc(), getFreeSpace());
    server.send(200, "application/json", large_buffer);
  });     

  server.begin();
}

int getLogbookSize()
{
  if (LittleFS.exists(LOGBOOK_FILE_NAME))
  { 
      File file = LittleFS.open(LOGBOOK_FILE_NAME, "r");
      int size = 0;
      while (file.available()) {
        file.readStringUntil('\n');
        size++;
      }
      file.close();
      return size;
  }
  else
  {
    File file = LittleFS.open(LOGBOOK_FILE_NAME, "w");
    file.close();
    return 0;
  }
}

void saveToLogbook(String &date, String &nick, String &info)
{
  int logbook_size = getLogbookSize();
  File file = LittleFS.open(LOGBOOK_FILE_NAME, "a");
  safewWrite(file, date);
  file.write('\t');
  safewWrite(file, nick);
  file.write('\t');
  safewWrite(file, info);
  file.write('\n');
  file.close();
  writeToSyslog("LOG");
}

void safewWrite(File &file, String &text) {
  text.replace('\t', ' ');
  text.replace('\n', ' ');
  text.replace('\r', ' ');
  text.replace('<', ' ');
  text.replace('>', ' ');
  text.replace('"', ' ');
  text.replace('\'', ' ');
  file.write(text.c_str(), strlen(text.c_str()));
}

void goodbye() {
  stats.upseconds += (millis()/1000);
  saveStats();

  Serial.end();
  pinMode(0, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);  
  pinMode(13, INPUT_PULLUP);
  pinMode(16, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
  pinMode(LED_PIN, INPUT_PULLUP);
  delay(5);
  ESP.deepSleep(0, RF_DISABLED);
}

void loop() {
  ArduinoOTA.handle();
  dnsServer.processNextRequest();
  server.handleClient();
  delay(1);
  cycles++;

  // blink each 5s
  int bt = millis() % 3000;
  if (bt < 200) {
     digitalWrite(LED2_PIN, bt < 100 ? HIGH : LOW);
  }

  // auto off
  long inactivity_seconds = (millis() - last_activity) / 1000;   
  if (millis() > GRACE_PERIOD_SECONDS_AFTER_POWER_ON * 1000 && inactivity_seconds > SLEEP_AFTER_SECONDS)
  {
    writeToSyslog("AUTO-OFF");
    goodbye();
  }
  
  int turnOffInactivitySeconds = last_activity / 1000 + SLEEP_AFTER_SECONDS;
  int turnOffSeconds = turnOffInactivitySeconds <  GRACE_PERIOD_SECONDS_AFTER_POWER_ON ? GRACE_PERIOD_SECONDS_AFTER_POWER_ON : turnOffInactivitySeconds;
  if (turnOffSeconds - (millis() / 1000) < 15) {
    digitalWrite(LED_PIN, millis()%500 < 250 ? LOW : HIGH);
    digitalWrite(LED2_PIN, millis()%500 < 250 ? LOW : HIGH);
  }

  // manual off
  if (manual_off_ts > 0 && millis() - manual_off_ts > 3000)
  {
    stats.manualoffs++;
    blink(6, 250);
    writeToSyslog("MANUAL-OFF");
    goodbye();
  }

  if (cycles % 50 == 0 && low_battery_check != -1 && millis() < 10000)
  {
    if (ESP.getVcc() < LOW_BATTERY_TRESHOLD) 
    {
      low_battery_check++;
      if (low_battery_check > 30) {
        low_battery = true;
        low_battery_check = -1;
        writeToSyslog("BAT-LOW");
      }
    }
  }
}

/*
Program: Spek - Alpha 1.1
Author: Trey Breshears
Creation Date: 04/21/2023
Last modified: 05/03/2023     Please Update this!

This program connets via WiFi to the RTC and Google Calendar API Servers. It compiles and displayes these responses on a digital display indicating the current time, busy status,
and current events.
*/

#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <WiFiSSLClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTCZero.h>

//RTC Pre-initialization
RTCZero rtc;
int status = WL_IDLE_STATUS;
const int GMT = -5;
const int myClock = 12;
const int dateOrder = 1;
int myhours, mins, secs, myday, mymonth, myyear; //int values of current date
bool IsPM = false;

//Screen (SSD1306) Pre-initialization
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET 4
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi Setup - Replace with your own SSID (WiFi Name) and password
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// Google Calendar API endpoint and API key
String apiKey = "YOUR_API_KEY";
String userEmail = "YOUR_EMAIL";
/* Setting up the API key is a lengthy process. I asked ChatGPT to help */

// Replace with your own time zone offset
const char* timeZoneOffset = "-05:00";

//Event schedule manager pre-initialization
int schLen = 20; //how many events stored in the schedule
int daysMon[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

typedef struct{
  String title;
  time_t start;
  time_t end;
  String id;
  int lastCheck;
} event; //Event type struct

event sched[20]; //creat event schedule

//Function previews (other functions exist, but did not need previews)
//parseTime takes a unix timestring and converts it to a human-readable time object.
time_t parseTime(String timeStr);
//scheduleUpdate process an event object and updates the schedule list accordingly.
void scheduleUpdate(event evn);

/*int wifi, RTime, API, Event, Screen, ref, cycle = 0;
int wAvg, RAvg, AAvg, EAvg, SAvg = 0;
//These variables are for analyzing the timing of the various functions for their RTS applications.*/

void setup() {
  Serial.begin(9600); //start serial communication, make sure this matches the baud rate in the IDE dropdown

  //Schedule initialization
  event fut = {"placeholder", 5682697600, 5682697601, "dumb", 0}; //fill with dummy events that take place after our lifetime
  for(int i=0; i<schLen; i++)
    sched[i] = fut;

  //Display initialization
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); //loop forever if library fails, prevent screen damage. If nothing appears after 10 seconds, this is the issue.
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE); //White is the only color..not sure why this is needed but code breaks without it
  display.setCursor(0,0);
  display.setTextSize(1);

  //Initialize Wifi
  connectWifi();
  display.println("WiFi connected via: ");
  display.println();
  display.println(WiFi.localIP());
  display.println();
  display.println("Initializing....");
  display.display();

  delay(6000); //wait 6 seconds to ensure connection has time to establish itself

  //WiFi-dependent setup events
  rtc.begin();
  setRTC();
  fixTimeZone();
}

void loop() {
  // Get current time in ISO 8601 format
  //ref = millis();
  String currentTime = getCurrentTime();
  //RTime = millis()-ref;
  //ref=millis();

  bool skipDummy = false; //

  // Make HTTP request to Google Calendar API
  String apiURL = buildURL();
  //Serial.println(apiURL); //Uncomment to diagnose API URL issues
  WiFiSSLClient client;
  client.connect("www.googleapis.com", 443);
  client.print(String("GET ") + apiURL + " HTTP/1.1\r\n" +
               "Host: www.googleapis.com\r\n" +
               "User-Agent: ArduinoWiFi/1.1\r\n" +
               "Connection: close\r\n\r\n");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    
    if (line == "\r") {
      break;
    }
    Serial.println(line); //DEBUG
  }

  //API = millis()-ref;
  //ref=millis();
  // Parse JSON response
  DynamicJsonDocument doc(8192); //Setting this too high causes a buffer overflow.

  deserializeJson(doc, client);

  JsonArray items = doc["items"]; //split doc based on "items" deliminator

  if (!doc.containsKey("items")) {
    Serial.println("Error: Response does not contain items array");
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Error! Check API Key.");
    display.display();
    WiFi.end();
    connectWifi();
    return;
  }

  if (items.isNull()) {
    Serial.println("Error: Items array is empty");
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Error! Check JSON response.");
    display.display();
    return;
  }

  //iterate through events
  for (JsonVariant item : items) {
    /*//The following three if statements are for debugging. If everything looks like it should be working, but your event list is empty, start here.
    Serial.println("Attempting parse");
    if (!item.containsKey("summary")) {
    Serial.println("Error: Item does not contain summary field");
    continue;
    }
  
    if (!item.containsKey("start") || !item["start"].containsKey("dateTime")) {
      Serial.println("Error: Item does not contain start.dateTime field");
      continue;
    }
  
    if (!item.containsKey("end") || !item["end"].containsKey("dateTime")) {
      Serial.println("Error: Item does not contain end.dateTime field");
      continue;
    }*/

    //console print
    String summary = item["summary"].as<String>();
    String start = item["start"]["dateTime"].as<String>();
    String end = item["end"]["dateTime"].as<String>();
    Serial.print(summary);
    Serial.print(" (");
    Serial.print(start);
    Serial.print(" - ");
    Serial.print(end);
    Serial.println(")");

    event ev = {summary, parseTime(item["start"]["dateTime"].as<String>()), parseTime(item["end"]["dateTime"].as<String>()), item["id"].as<String>(), 0}; //create event
    scheduleUpdate(ev);
    skipDummy = true;
  }
  for(int i = 0; i<schLen; i++){ //tick up time since last checkin
    sched[i].lastCheck++;
  }
  if(!skipDummy){ //run scheduleupate with a dummy event if an actual event doesn't come through
    event dummy = {"placeholder", 5682697600, 5682697601, "dumb", 0};
    scheduleUpdate(dummy);
  }
  //Serial.println("Client stopped.");
  client.stop(); //disconnect from API

  //Event = millis()-ref;
  //ref=millis();

  //Update rtc time. Happens regularly enough to assume deviation is <1 minute
  secs = rtc.getSeconds();
  fixTimeZone();

  //RTime += millis()-ref;
  //ref = millis();
  
  //Update display
  display.clearDisplay();
  displayUpdate();

  /*Screen = millis()-ref; //The following are for RTS Analysis

  cycle++;
  wAvg+=wifi;
  RAvg+=RTime;
  AAvg+=API;
  EAvg+=Event;
  SAvg+=Screen;

  wifi = wAvg/cycle;
  RTime = RAvg/cycle;
  API = AAvg/cycle;
  Event = EAvg/cycle;
  Screen = SAvg/cycle;
  
  Serial.println(cycle);
  Serial.print("Wifi: ");
  Serial.println(wifi);
  Serial.print("RTC: ");
  Serial.println(RTime);
  Serial.print("API: ");
  Serial.println(API);
  Serial.print("Event: ");
  Serial.println(Event);
  Serial.print("Screen: ");
  Serial.println(Screen);*/

  //delay(10000); //Enforces 10 second refresh cycle so you have time to actually read the console
}

//buildURL returns the APU URL accunting for the user's unique key and the current date
String buildURL(){
  String apiURL = "/calendar/v3/calendars/";
  apiURL += userEmail;
  apiURL += "/events?key="; //2023-04-30T00:00:00Z&timeMax=2023-05-01T23:59:59Z";
  apiURL += apiKey;
  apiURL += "&timeMin=";
  apiURL += "20";
  apiURL += String(myyear);
  apiURL += "-";
  if(mymonth<10){
    apiURL += "0";
  }
  apiURL += String(mymonth);
  apiURL += "-";
  //apiURL += "2023-04-30T00:00:00Z";
  if(myday<10){
    apiURL += "0";
  }
  apiURL += String(myday);
  apiURL += "T00:00:00Z";
  apiURL += "&timeMax=";
  apiURL += "20";
  if(mymonth==12&&myday==31){
    apiURL += String((myyear+1)%100);
  }
  else{
    apiURL += String(myyear);
  }
  apiURL += "-";
  bool monthFlag = false;
  if(myday>=30&&mymonth>=9&&mymonth!=12){
    if(mymonth<9){
      apiURL += "0";
    }
    apiURL += String(mymonth+1);
    monthFlag = true;
  }
  else if(myday==31&&mymonth==12){
    apiURL += "01";
    monthFlag = true;
  }
  else if(myday>=28){
    if (mymonth<9){
      apiURL += "0";
    }
    apiURL += String(mymonth+1);
    monthFlag = true;
  }
  else{
    if(mymonth<10){
      apiURL += "0";
    }
    apiURL += String(mymonth);
  }
  apiURL += "-";
  if(monthFlag){
    apiURL += "00";
  }
  else if(myday<9){
    apiURL += "0";
    apiURL += String(myday+1);
  }
  else{
    apiURL += String(myday+1);
  }
  apiURL += "T01:00:00Z";

  return apiURL;
}

//getCurrentTime contacts the RTS server stores the data locally
String getCurrentTime() {
  // Get current time in seconds
  unsigned long currentTimeSeconds = millis() / 1000;

  // Convert to tmElements_t struct
  tmElements_t currentTime;
  breakTime(currentTimeSeconds, currentTime);

  // Convert to ISO 8601 format
  char currentTimeISO[25];
  sprintf(currentTimeISO, "%04d-%02d-%02dT%02d:%02d:%02dZ",
          currentTime.Year + 1970, currentTime.Month, currentTime.Day,
          currentTime.Hour, currentTime.Minute, currentTime.Second);

  return String(currentTimeISO);
}

// helper function to convert time string to Unix time format
time_t parseTime(String timeStr) {
  int year = timeStr.substring(0, 4).toInt();
  int month = timeStr.substring(5, 7).toInt();
  int day = timeStr.substring(8, 10).toInt();
  int hour = timeStr.substring(11, 13).toInt();
  int minute = timeStr.substring(14, 16).toInt();
  int second = timeStr.substring(17, 19).toInt(); //3023-02-02T18:00:00-06:00
  int hrOffset = timeStr.substring(20, 22).toInt();

  if(timeStr.substring(19, 20) == "+"){
    hrOffset *= -1;
  }
  
  tmElements_t tm;
  tm.Year = year - 1970;
  tm.Month = month;
  tm.Day = day;
  tm.Hour = hour += hrOffset;
  tm.Minute = minute;
  tm.Second = second;
  
  return makeTime(tm);
}

//scheduleUpdate process a new event and updates the schedule list accordingly
void scheduleUpdate(event evn){
  Serial.println("Schedule Updater Started");

  bool valid = true; //bool to check if event should be added to list
  bool sort = false; //bool to check if scchedule needs to be sorted.
  for(int i = 0; i<schLen; i++){
    if(sched[i].end<rtc.getEpoch()||(sched[i].lastCheck>1&&sched[i].id!="dumb")){ //replaces events that have ended, kicks them off the front of the list
      sched[i] = {"placeholder", 5682697600, 5682697601, "dumb",0};
      sort = true;
      Serial.println("List 0 check failed, removing.");
      break;
    }
    if(evn.id==sched[i].id){ //checks for identical event
      if((evn.title==sched[i].title&&evn.start==sched[i].start&&evn.end==sched[i].end)||evn.start>sched[schLen-1].start){
        valid = false;
        sched[i].lastCheck=0;
        Serial.println("Identical Event in schedule!");
      }
      else{
        sched[i] = {"placeholder", 5682697600, 5682697601, "dumb",0};
        Serial.println("Details changed, event removed");
      }
      break;
    }
  }
  /*Serial.print("Event ends: ");
  Serial.println(evn.end);
  Serial.print("Current time is: ");
  Serial.println(rtc.getEpoch());*/ //Debugs for event validation
  if(evn.end>=rtc.getEpoch() && valid && evn.id!="dumb"){ //checks if event has already ended or is a placeholder
    //add to the end of the list
    sched[schLen-1] = evn;
    sort = true;
  }
  else{
    Serial.print("Invalid Event thrown out: ");
    Serial.println(evn.title);
  }

  if(sort == true||sched[0].id=="dumb"){ //sorts and prints the new schedule
   // perform bubble sort
    Serial.println("Schedule sorted.");
    for (int i = 0; i < schLen-1; i++) {
      for (int j = 0; j < schLen-i-1; j++) {
        if (sched[j].start > sched[j+1].start) {
          event temp = sched[j];
          sched[j] = sched[j+1];
          sched[j+1] = temp;
        }
      }
    }

    // print the updated list
    for (int i = 0; i < schLen; i++) {
      Serial.print(sched[i].title);
      Serial.print(", ");
    }
    Serial.println();
  }
  Serial.println("Schedule Updater finished");
}

//setRTC pulls the current unix code from the time server and updates the local time variables accordingly
void setRTC() {
  unsigned long epoch;
  int numberOfTries = 0, maxTries = 8;
  do {
    epoch = WiFi.getTime(); // The RTC is set to GMT or 0 Time Zone and stays at GMT.
    numberOfTries++;
  }
  while ((epoch == 0) && (numberOfTries < maxTries)); //Holds program 

  if (numberOfTries == maxTries) {
    Serial.print("NTP unreachable!!");
    display.setCursor(0,0);
    display.println("Time Server unreachable!");
    display.println();
    display.println("Please press the reset button.");
    display.display();
    while (1);  // hang
  }
  else {
    Serial.print("Epoch Time = ");
    Serial.println(epoch);
    rtc.setEpoch(epoch);
    Serial.println();
  }
}

/* There is more to adjusting for time zone than just changing the hour.
   Sometimes it changes the day, which sometimes chnages the month, which
   requires knowing how many days are in each month, which is different
   in leap years, and on Near Year's Eve, it can even change the year! */
void fixTimeZone() {
  if (myyear % 4 == 0) daysMon[2] = 29; // fix for leap year
  myhours = rtc.getHours();
  mins = rtc.getMinutes();
  myday = rtc.getDay();
  mymonth = rtc.getMonth();
  myyear = rtc.getYear();
  myhours +=  GMT; // initial time zone change is here
  if (myhours < 0) {  // if hours rolls negative
    myhours += 24; // keep in range of 0-23
    myday--;  // fix the day
    if (myday < 1) {  // fix the month if necessary
      mymonth--;
      if (mymonth == 0) mymonth = 12;
      myday = daysMon[mymonth];
      if (mymonth == 12) myyear--; // fix the year if necessary
    }
  }
  if (myhours > 23) {  // if hours rolls over 23
    myhours -= 24; // keep in range of 0-23
    myday++; // fix the day
    if (myday > daysMon[mymonth]) {  // fix the month if necessary
      mymonth++;
      if (mymonth > 12) mymonth = 1;
      myday = 1;
      if (mymonth == 1)myyear++; // fix the year if necessary
    }
  }
  if (myClock == 12) {  // this is for 12 hour clock
    IsPM = false;
    if (myhours > 11)IsPM = true;
    myhours = myhours % 12; // convert to 12 hour clock
    if (myhours == 0) myhours = 12;  // show noon or midnight as 12
  }
}

//displayUpdate updates the display (duh) based on the current event status (less duh)
void displayUpdate(){
  //event happening
  if(sched[0].title == "placeholder" && sched[0].start == 5682697600){
    display.setTextSize(3);
    display.setCursor(0,0);
    if(myhours<10){
      display.print("0");
    }
    display.print(myhours);
    display.print(":");
    if(mins<10){
      display.print("0");
    }
    display.print(mins);
    if(IsPM){
      display.println("PM");
    }
    else{
      display.println("AM");
    }
    display.setTextSize(1);
    display.println();
    display.println("No events on the");
    display.println("schedule!");
  }
  else if(sched[0].start < rtc.getEpoch() && rtc.getEpoch() < sched[0].end){
    display.setTextSize(1);
    display.setCursor(0,0);
    if(myhours<10){
      display.print("0");
    }
    display.print(myhours);
    display.print(":");
    if(mins<10){
      display.print("0");
    }
    display.print(mins);
    if(IsPM){
      display.println("PM");
    }
    else{
      display.println("AM");
    }
    display.println();
    display.setTextSize(3);
    display.println("BUSY!");
    display.setTextSize(2);
    display.println(sched[0].title);
    display.setTextSize(1);
    display.print("Ends in ");
    int endtime = (sched[0].end - rtc.getEpoch())/60;
    if(endtime == 0){
      display.println(" <1 minute!");
    }
    else if(endtime == 1){
      display.println(" 1 minute.");
    }
    else if(endtime<60){
      display.print(endtime);
      display.println(" minutes.");
    }
    else{
      endtime /= 60;
      if(endtime == 1){
        display.println("1 hour.");
      }
      else{
        display.print(endtime);
        display.println(" hours.");
      }
    }
  }
  //event not happening
  else{
    display.setTextSize(3);
    display.setCursor(0,0);
    if(myhours<10){
      display.print("0");
    }
    display.print(myhours);
    display.print(":");
    if(mins<10){
      display.print("0");
    }
    display.print(mins);
    if(IsPM){
      display.println("PM");
    }
    else{
      display.println("AM");
    }
    display.setTextSize(1);
    display.println();
    int nexttime = (sched[0].start - rtc.getEpoch())/60;
    if(nexttime==0){
      display.print("< 1 minute");
    }
    else if(nexttime==1){
      display.print(nexttime);
      display.print(" minute ");
    }
    else if(nexttime<60){
      display.print(nexttime);
      display.print(" minutes ");
    }
    else{
      nexttime /= 60;
      if(nexttime == 1){
        display.print(nexttime);
        display.print(" hour ");
      }
      else{
        display.print(nexttime);
        display.print(" hours ");
      }
    }
    display.println("until:");
    display.setTextSize(2);
    display.println(sched[0].title);
  }

  display.display();
}

void connectWifi(){
  // Connect to WiFi
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    tries++;
    if(tries>100){
      display.println("WiFi connection issues!");
      display.println();
      display.println("Please press the reset button.");
      display.println();
      display.println("If the issue persists, check SSID & Password.");
      display.display();
      while(1){
        //hang
      }
    }
    delay(1000); //allow time to connect
  }

  // Print WiFi connection details
  Serial.print("WiFi connected via: ");
  Serial.println(WiFi.localIP());
}

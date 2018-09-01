// TODO:
// Local Storage to log data(and dump with laptop)

#include <EEPROM.h>

///// TIMERS /////
//#include <DS1302RTC.h>
#include <TimeLib.h>
#include <TimeAlarms.h>

bool setit = false;
//int toSetseconds, minutes, hours, dayofweek, dayofmonth, month, year;
int toSetseconds    = 0;
int toSetminutes    = 20;
int toSethours      = 23;
int toSetdayofweek  = 6;  // Day of week, any day can be first, counts 1...7
int toSetdayofmonth = 10; // Day of month, 1...31
int toSetmonth      = 3;  // month 1...12
int toSetyear       = 2018;

// Init the DS1302
// Set pins:  CE, IO,CLK
//DS1302RTC RTC(11, 13, 12);
#define bcd2bin(h,l)    (((h)*10) + (l))
typedef struct ds1302_struct
{
  uint8_t Seconds:4;      // low decimal digit 0-9
  uint8_t Seconds10:3;    // high decimal digit 0-5
  uint8_t CH:1;           // CH = Clock Halt
  uint8_t Minutes:4;
  uint8_t Minutes10:3;
  uint8_t reserved1:1;
  union
  {
    struct
    {
      uint8_t Hour:4;
      uint8_t Hour10:2;
      uint8_t reserved2:1;
      uint8_t hour_12_24:1; // 0 for 24 hour format
    } h24;
    struct
    {
      uint8_t Hour:4;
      uint8_t Hour10:1;
      uint8_t AM_PM:1;      // 0 for AM, 1 for PM
      uint8_t reserved2:1;
      uint8_t hour_12_24:1; // 1 for 12 hour format
    } h12;
  };
  uint8_t Date:4;           // Day of month, 1 = first day
  uint8_t Date10:2;
  uint8_t reserved3:2;
  uint8_t Month:4;          // Month, 1 = January
  uint8_t Month10:1;
  uint8_t reserved4:3;
  uint8_t Day:3;            // Day of week, 1 = first day (any day)
  uint8_t reserved5:5;
  uint8_t Year:4;           // Year, 0 = year 2000
  uint8_t Year10:4;
  uint8_t reserved6:7;
  uint8_t WP:1;             // WP = Write Protect
};

/////

///// DHT /////
#include "DHT.h"
int DHTPin = 10;
#define DHTTYPE DHT11
DHT dht(DHTPin, DHTTYPE);

float h;
float t;
float f;
float hi;
float hiDegC;
float dewP;

float humidityThreshholdMin = 58.00;
float humidityThreshholdMax = 62.00;

float tempThreshholdMin = 23.00;
float tempThreshholdMax = 25.00;
/////

///// MOISTURE /////
int moisturePin = A0;
int moisture = 0;
int moistureThreshhold = 2000; //int moistureThreshhold = 600;
/////

///// WATER LEVEL /////
int REED_PIN = 2;
bool waterLow = false;
/////

///// LIGHTS /////
int hourOn = 5; // At the start of this hour
int hourOff = 22; // At the end of this hour
bool lightState = false;

int light = 9;
/////

///// PUMP /////
int pump = 8;
bool pumpState = false;
/////
///// FAN /////
int fan = 7;
bool fanState = false;
// TODO:
volatile byte half_revolutions = 0;
unsigned int rpm = 0;
unsigned long timeold = 0;
/////

///// CONSOLE /////
String cmd;
bool serialTrigger = true;
/////

///// DATA /////
// Example settings structure

void readData() {
  for (unsigned int thehour=0; thehour<24; thehour++) {
    int dataStart = 32;
    
    struct StoreStruct {
      int thehour; // Hour (But can't use hour/time)
      int thedate;
      int themonth;
      int theyear;
      int moisture;
      double temperature, humidity, dewp;
    } storage = {
    };


    if (thehour != 0) {
      dataStart = dataStart + sizeof(storage) * thehour;
    }
    
    //Serial.print("Reading from: ");
    //Serial.println(dataStart);
      
    for (unsigned int counter=0; counter<sizeof(storage); counter++) {
      *((char*)&storage + counter) = EEPROM.read(dataStart + counter);
    }
  
    int toRead = EEPROM.read(dataStart + 1);
  
    if (24 > toRead && toRead <= 0) {
      Serial.print(storage.thehour);
      Serial.print(",");
      Serial.print(storage.thedate);
      Serial.print(",");
      Serial.print(storage.themonth);
      Serial.print(",");
      Serial.print(storage.theyear);
      Serial.print(",");
      Serial.print(storage.temperature);
      Serial.print(",");
      Serial.print(storage.humidity);
      Serial.print(",");
      Serial.print(storage.dewp);
      Serial.print(",");
      Serial.println(storage.moisture);
    }
    else {
      Serial.print("Invalid data for hour: ");
      Serial.println(thehour);
    }
  }
}

void saveData() {
  int dataStart = 32;
  
  struct StoreStruct {
    int thehour; // Hour (But can't use hour/time)
    int thedate;
    int themonth;
    int theyear;
    int moisture;
    double temperature, humidity, dewp;
  } storage = {
  };

  time_t systemNow = now();
  
  storage.thehour = hour(systemNow);
  storage.thedate = day(systemNow);
  storage.themonth = month(systemNow);
  storage.theyear = year(systemNow);

  fetchTemperature();
  
  storage.temperature = t;
  storage.humidity = h;
  storage.dewp = dewP;
  storage.moisture = moistureRead();

  if (storage.thehour != 0) {
    dataStart = dataStart + sizeof(storage) * storage.thehour;
  }

  Serial.print("Saving data, Hour: ");
  Serial.println(storage.thehour);
  
  Serial.print("Position: ");
  Serial.println(dataStart);
  
  for (unsigned int t=0; t<sizeof(storage); t++) {
    EEPROM.write(dataStart + t, *((char*)&storage + t));
  }
  
  //Alarm.timerOnce(60, saveData);
}

void clearData() {
  int dataStart = 32;
  
  struct StoreStruct {
    int thehour; // Hour (But can't use hour/time)
    int thedate;
    int themonth;
    int theyear;
    int moisture;
    double temperature, humidity, dewp;
  } storage = {
    0, 0, 0, 0, 0, 0.00, 0.00, 0.00
  };

  for (unsigned int thehour=0; thehour<24; thehour++) {
    if (thehour != 0) {
      dataStart = dataStart + sizeof(storage) * thehour;
    }
  
    for (unsigned int t=0; t<sizeof(storage); t++) {
      EEPROM.write(dataStart + t, *((char*)&storage + t));
    }
    Serial.print("Cleared env data at: ");
    Serial.println(dataStart);
  }
}

void turnOn(int pin) {
  digitalWrite(pin, LOW);
}

void turnOff(int pin) {
  digitalWrite(pin, HIGH);
}

time_t getTime() {
  time_t systemNow = now();
}

void printTime(int hh, int mm, int ss, int dd, int MM, int yyyy) {
  char buffer[80];
  Serial.println("System Clock time:");
  sprintf(buffer, "%02d:%02d:%02d ", hh, mm, ss);
  Serial.print(buffer);
  
  sprintf(buffer, "%d/%d/%d", dd, MM, yyyy);
  Serial.println(buffer);
  
}

time_t updateSystemTime() {
  // Read Clock
  ds1302_struct rtc;
  char buffer[80];
  DS1302_clock_burst_read( (uint8_t *) &rtc);
  int dd = bcd2bin( rtc.Date10, rtc.Date);
  int MM = bcd2bin( rtc.Month10, rtc.Month);
  int yyyy = 2000 + bcd2bin( rtc.Year10, rtc.Year);
  //int dow = rtc.Day; // Day of the week

  int hh = bcd2bin( rtc.h24.Hour10, rtc.h24.Hour);
  int mm = bcd2bin( rtc.Minutes10, rtc.Minutes);
  int ss = bcd2bin( rtc.Seconds10, rtc.Seconds);

  // Print date and time
  //Serial.println("Updated system time");
  //sprintf(buffer, "%02d:%02d:%02d, ", hh, mm, ss);
  //Serial.println(buffer);
  
  //sprintf(buffer, "%d/%d/%d", dd, MM, ss);
  //Serial.println(buffer);
  printTime(hh,mm,ss,dd,MM,yyyy);
  setTime(hh,mm,ss,dd,MM,yyyy);
}

void lightsOn() {
  turnOn(light);
  if (!lightState) {
    lightState = true;
    Serial.print("ENV:4:");
    Serial.println(1);
  }
}

void lightsOff() {
  turnOff(light);
  if (lightState) {
    lightState = false;
    Serial.print("ENV:4:");
    Serial.println(0);
  }
}

double dewPoint(double celsius, double humidity) {
  // John Main added dewpoint code from : http://playground.arduino.cc/main/DHT11Lib
  // Also added DegC output for Heat Index.
  // dewPoint function NOAA
  // reference (1) : http://wahiduddin.net/calc/density_algorithms.htm
  // reference (2) : http://www.colorado.edu/geography/weather_station/Geog_site/about.htm
  //
  // (1) Saturation Vapor Pressure = ESGG(T)
  double RATIO = 373.15 / (273.15 + celsius);
  double RHS = -7.90298 * (RATIO - 1);
  RHS += 5.02808 * log10(RATIO);
  RHS += -1.3816e-7 * (pow(10, (11.344 * (1 - 1 / RATIO ))) - 1) ;
  RHS += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
  RHS += log10(1013.246);

  // factor -3 is to adjust units - Vapor Pressure SVP * humidity
  double VP = pow(10, RHS - 3) * humidity;

  // (2) DEWPOINT = F(Vapor Pressure)
  double T = log(VP / 0.61078); // temp var
  return (241.88 * T) / (17.558 - T);
}

void fetchTemperature() {
  h = dht.readHumidity();
  t = dht.readTemperature();     // Read temperature as Celsius
  f = dht.readTemperature(true); // Read temperature as Fahrenheit

  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    //fetchTemperature();
  } else {
    // Compute heat index - Must send in temp in Fahrenheit!
    hi = dht.computeHeatIndex(f, h);
    hiDegC = dht.convertFtoC(hi);
    dewP = dewPoint(t, h);
  }
}

void fanOn() {
  turnOn(fan);
  if (!fanState) {
    fanState = true;
    Serial.print("ENV:6:");
    Serial.println(1);
  }
}

void fanOff() {
  turnOff(fan);
  if (fanState) {
    fanState = false;
    Serial.print("ENV:6:");
    Serial.println(0);
  }
}

void temperatureCheck() {
  fetchTemperature();
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.println(" °C");
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.println(" %");

  bool hSetToOn = false;
  if (humidityThreshholdMin <= h && h >= humidityThreshholdMax) {
    hSetToOn = true;
  } else {
    hSetToOn = false;
  }

  bool tSetToOn = false;
  if (tempThreshholdMin <= t && t >= tempThreshholdMax) {
    tSetToOn = true;
  } else {
    tSetToOn = false;
  }

  if (hSetToOn || tSetToOn) {
    fanOn();
  } else {
    fanOff();
  }
  
  //submit(String(hiDegC), String(h), String(t), String(dewP));
}

void pumpOn() {
  turnOn(pump);
  if (!pumpState) {
    pumpState = true;
    Serial.print("ENV:5:");
    Serial.println(1);
  }
}

void pumpOff() {
  turnOff(pump);
  if (pumpState) {
    pumpState = false;
    Serial.print("ENV:5:");
    Serial.println(0);
  }
}

int moistureRead() {
  moisture = analogRead(A0);
  return moisture;
}

void moistureCheck() {
  Serial.print("Moisture: ");
  Serial.println(moistureRead());
  if (moisture > moistureThreshhold) {
    pumpOn();
    Alarm.timerOnce(3, pumpOff);
  }
}

void pumpCheck() {
  if (pumpState) {
    pumpOn();
    //Serial.println("Pump: On");
  } else {
    pumpOff();
    //Serial.println("Pump: Off");
  }
}

void lightCheck() {
  // TODO: Add light sensor to monitor britghtness
  
  // Should the lights be on ?
  int hourNow = hour(now());
  if (hourOn <= hourNow && hourNow <= hourOff) {
    lightState = true;
    lightsOn();
    //Serial.println("Light: On");
  } else {
    lightState = false;
    lightsOff();
    //Serial.println("Light: Lights Off");
  }
}

void fanCheck() {
  //Alarm.timerOnce(8, fanOff);
  if (fanState) {
    fanOn();
    //Serial.println("Fan: On");
  } else {
    fanOff();
    //Serial.println("Fan: Off");
  }
}

void waterCheck() {
  // TODO: Alert on low water
  // Read the state of the switch
  int proximity = digitalRead(REED_PIN);
  if (proximity == LOW) {
    // If the pin is pulled LOW, the water level is high
    waterLow = false;
  } else {
    waterLow = true;
  }
}

void showDate() {
  time_t sys = now();
  printTime(hour(sys), minute(sys), second(sys), day(sys), month(sys), year(sys));
}

void cmdEnvStats() {
  fetchTemperature();

  Serial.println("------ for upload ------");
  
  Serial.print("ENV:1:");
  Serial.println(h);
  Serial.print("ENV:2:");
  Serial.println(t);
  
  Serial.print("ENV:3:");
  Serial.println(moistureRead());
  Serial.print("ENV:4:");
  Serial.println(lightState);
  Serial.print("ENV:5:");
  Serial.println(pumpState);
  Serial.print("ENV:6:");
  Serial.println(fanState);
  
  //Serial.print("ENV:7:");
  //Serial.println(hiDegC);
  //Serial.print("ENV:8:");
  //Serial.println(dewP);
  
  Serial.println("------ ------");
}

void cmdShowEnv() {
  fetchTemperature();

  Serial.println("------ Env ------");
  
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.println(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.println(" °C ");
  Serial.print("Heat index: ");
  Serial.print(hiDegC);
  Serial.println(" °C ");
  Serial.print("Dew Point (°C): ");
  Serial.println(dewP);
  
  Serial.print("Moisture: ");
  Serial.println(moistureRead());
  Serial.print("Light State: ");
  Serial.println(lightState);
  Serial.print("Pump State: ");
  Serial.println(pumpState);
  Serial.print("Fan State: ");
  Serial.println(fanState);
  Serial.print("Low Water?: ");
  Serial.println(waterLow);
  Serial.println("------  ------");
}

void setup () {
  Serial.begin(57600);
  Serial.println("Starting..."); 

  RTCsetup();
  updateSystemTime();
  
  if (pumpState) { pumpOn(); } else { pumpOff(); }
  if (lightState) { lightsOn(); } else { lightsOff(); }
  if (fanState) { fanOn(); } else { fanOff; }
  pinMode(pump, OUTPUT);
  pinMode(fan, OUTPUT);
  pinMode(light, OUTPUT);
  
  pinMode(REED_PIN, INPUT_PULLUP);
  
  //Alarm.alarmOnce(hour(now()),minute(now())+1,0, saveData);
  
  Alarm.alarmRepeat(05,00,0, lightsOn);       // 05h00 - Lights on
  Alarm.alarmRepeat(23,00,0, lightsOff);      // 23h00 - Lights off
  
  Alarm.timerRepeat(600, updateSystemTime);   // 10min - Update System Time
  Alarm.timerRepeat(60, cmdEnvStats);         //   60s - Print envrironment for upload
  Alarm.timerRepeat(60, moistureCheck);       //   60s - Check soil moisture
  Alarm.timerRepeat(60, waterCheck);          //   60s - Check water level
  Alarm.timerRepeat(10, temperatureCheck);    //   10s - Check temperature and humidity
  Alarm.timerRepeat(10, lightCheck);          //   10s - Check light levels
  Alarm.timerRepeat(5, fanCheck);             //   2s -  Check fan status, only enable when fan is triggered(Helps us turn the fan off)
  Alarm.timerRepeat(2, pumpCheck);            //   2s  - Check pump status, only enable when pump is triggered(Helps us turn the pump off)
  
  
//  attachInterrupt(0, rpm_fun, RISING); // Measure RPM
  delay(4000);
  Serial.println("System started.");
  }

void cmdShowHelp() {
  Serial.println("--- Help ---");
  Serial.println(" - Commands -");
  Serial.println("");
  Serial.println("   General");
  Serial.println("help       - Show help menu");
  Serial.println("env        - Print current state");
  Serial.println("date       - Show current system time");
  Serial.println("");
  Serial.println(" Data Management");
  Serial.println("save data  - Save current env in memory");
  Serial.println("read data  - Print env data as csv");
  Serial.println("clear data - Clear all env data");
  Serial.println("");
  Serial.println("  Relay Control");
  Serial.println("light on   - Grow lights on");
  Serial.println("light off  - Grow lights off");
  Serial.println("pump on    - Water pump on");
  Serial.println("pump off   - Water pump off");
  Serial.println("fan on     - Humidity extraction fan on");
  Serial.println("fan off    - Humidity extraction fan off");
}

void doCommand(String cmd) {
  if (cmd == "help") {
    cmdShowHelp();
  } else if (cmd == "env") {
    cmdShowEnv();
  } else if (cmd == "upload env") {
    cmdEnvStats();
  } else if (cmd == "date") {
    showDate();
  } else if (cmd == "save data") {
    saveData();
  } else if (cmd == "read data") {
    readData();
  } else if (cmd == "clear data") {
    clearData();
  } else if (cmd == "light on") {
    lightsOn();
  } else if (cmd == "light off") {
    lightsOff();
  } else if (cmd == "pump on") {
    pumpOn();
  } else if (cmd == "pump off") {
    pumpOff();
  } else if (cmd == "fan on") {
    fanOn();
  } else if (cmd == "fan off") {
    fanOff();
  }
}



// TODO: Use Struct to store an hourly datastructure of the data

void loop () {

  if (Serial) {
    if (serialTrigger) {
      serialTrigger = false;
      cmdShowHelp();
      cmdShowEnv();
    }
    cmd = Serial.readString();// read the incoming data as string
    cmd.trim();
    if (cmd.length() > 0) {
      Serial.println(cmd);
      doCommand(cmd);
    }
  } else {
    serialTrigger = true;
  }
  
//  if (half_revolutions >= 20) { 
    //Update RPM every 20 counts, increase this for better RPM resolution,
    //decrease for faster update
//    rpm = 30*1000/(millis() - timeold)*half_revolutions;
//    timeold = millis();
//    half_revolutions = 0;
//    Serial.println(rpm,DEC);
//   }
  Alarm.delay(70);
}

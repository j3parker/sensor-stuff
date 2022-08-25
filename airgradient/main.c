#include <AirGradient.h>
#include <jacobtest.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
//#include <WiFiClient.h>
#include <EEPROM.h>

#include "SGP30.h"
#include <U8g2lib.h>

AirGradient ag = AirGradient();
SGP30 SGP;
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

String sensorId = "testing3";
String APIROOT = "http://solidangle.ca:8080/metrics/airquality";
boolean connectWIFI = true;

// Stores/averages a single measurement
class Measurement {
  public:
    Measurement() {
      _latest = -1;
      _sum = 0;
      _count = 0;
    }

    // Add another reading to the measurement
    void Add(double value) {
      _latest = value;
      _sum += value;
      _count++;
    }

    // Return the average and reset the measurement
    double Aggregate() {
      double avg = _count == 0 ? -1 : _sum/(double)_count;
      _sum = 0;
      _count = 0;
      return avg;
    }

    // Read the latest value
    double Latest() {
      return _latest;
    }

  private:
    double _latest;
    double _sum;
    unsigned _count;
};

// A clock that keeps track of elapsed time on Tick(), "triggering" after a periodic interval.
class Clock {
  public:
    Clock(unsigned long alertInterval, unsigned long firstAlertDelay = 0) {
      _alertInterval = alertInterval;
      _prev = millis();
      _dt = alertInterval - firstAlertDelay;
    }

    // Returns true ("triggers") every _alertInterval
    bool Tick() {
      unsigned long cur = millis();
      _dt += cur - _prev;
      _prev = cur;
 
      if(_dt < _alertInterval) {
        return false;
      }

      _dt = _dt % _alertInterval;
      
      return true;
    }

    // Clock will trigger on Tick() after the next _alertInterval milliseconds
    void Reset() {
      _prev = millis();
      _dt = 0;
    }

    // Set the clock to trigger after dt milliseconds
    void SetNextAlertDelay(unsigned long dt) {
      _prev = millis();
      _dt = (long)_alertInterval - dt;
    }

  private:
    unsigned long _alertInterval;
    unsigned long _prev;
    long _dt;
};

void setup() {
  Serial.begin(115200);
  Serial.println("Hello.");

  // Draw the initial screen
  u8g2.begin();
  updateOLED();

  SGP.begin();
  //SGP.GenericReset();

  ag.CO2_Init();
  ag.PMS_Init();
  ag.TMP_RH_Init(0x44);

  if(connectWIFI) {
    connectToWifi();
  }

  initVOC();
  initTransmitter();
  
  Serial.println("ready");
}


// VOC needs to be measured every second for internal calibration,
// but for simplicity just measure everything at that rate.
Clock measurementClock = Clock(1000);
Clock transmitClock = Clock(30*1000);
Clock oledClock = Clock(10*1000);

Measurement temp = Measurement();
Measurement rh = Measurement();
Measurement co2 = Measurement();
Measurement pm1 = Measurement();
Measurement pm2_5 = Measurement();
Measurement pm10 = Measurement();
Measurement tvoc = Measurement();
Measurement eco2 = Measurement();

void loop() {
  if(measurementClock.Tick()) {
    measureTemp();
    measureCO2();
    measurePM();
    measureVOC();    
  }

  if(oledClock.Tick()) {
    updateOLED();
  }

  if(transmitClock.Tick()) {
    transmit();
  }
}

void measureTemp() {
  TMP_RH result = ag.periodicFetchData();
  temp.Add(result.t);
  rh.Add(result.rh);
}

void measureCO2() {
  int value = ag.getCO2_Raw();

  // Filter out errors
  if(value == -1) {
    return;
  }

  co2.Add(value);
}

void measurePM() {
  AirGradient::DATA PM = ag.getPM_Raw();
  pm1.Add(PM.PM_SP_UG_1_0);
  pm2_5.Add(PM.PM_SP_UG_2_5);
  pm10.Add(PM.PM_SP_UG_10_0);
}

// store baseline calibration in flash once per day for reboots
// it needs to be a minimum of 12 hours after "initial operation", and the docs suggest once per hour but the calibrations are "good for 7 days"
// for simplicity and reducing EEPROM I'm just doing once per day: I don't expect frequent power losses, and 1 day < 7 days, and 1 day > 12 hours.
Clock vocBaselineClock = Clock(1000*60*60*24);

// periodically update the voc chip on the temperature/humidity for its internal
// calibration algorithm
Clock vocTempRHClock = Clock(10*1000);

void initVOC() {
  uint16_t eCO2Baseline, tvocBaseline;
  readEEPROM(tvocBaseline, eCO2Baseline);

  if(eCO2Baseline != 0 || tvocBaseline != 0) {
    Serial.println("Initializing SGP baselines");
    SGP.setBaseline(eCO2Baseline, tvocBaseline);
  }

  vocBaselineClock.Reset();
}

const uint32_t ExpectedMagic = 0xDEADBEEF;
const int EEPROMSize = sizeof(ExpectedMagic) + 2*sizeof(uint16_t);

void writeEEPROM(uint16_t tvocBaseline, uint16_t eco2Baseline, bool alreadyUsingEEPROM = false) {
  printSGPCalibration("Writing SGP calibration from EEPROM", tvocBaseline, eco2Baseline);

  if(!alreadyUsingEEPROM) {
    EEPROM.begin(EEPROMSize);
  }

  // write 4 bytes of magic to signal that we've initialized
  EEPROM.put(0, ExpectedMagic);

  // store zeros for SGP calibration
  EEPROM.put(sizeof(ExpectedMagic), tvocBaseline);
  EEPROM.put(sizeof(ExpectedMagic) + sizeof(tvocBaseline), eco2Baseline);

  if(!alreadyUsingEEPROM) {
    EEPROM.end();  
  }
}

void readEEPROM(uint16_t& tvocBaseline, uint16_t& eco2Baseline) {
  Serial.println("Reading EEPROM.");

  EEPROM.begin(EEPROMSize);

  int magic;
  EEPROM.get(0, magic);

  if(magic != ExpectedMagic) {
    Serial.println("Insufficient magic. Re-initializing.");
    writeEEPROM(0, 0, true);
  }

  EEPROM.get(sizeof(ExpectedMagic), tvocBaseline);
  EEPROM.get(sizeof(ExpectedMagic) + sizeof(tvocBaseline), eco2Baseline);

  printSGPCalibration("Reading SGP calibration from EEPROM", tvocBaseline, eco2Baseline);
  
  EEPROM.end();
}

void printSGPCalibration(const char* msg, uint16_t tvocBaseline, uint16_t eco2Baseline) {
  Serial.println(msg);
  
  Serial.print("  TVOC baseline: ");
  Serial.println(String(tvocBaseline).c_str());

  Serial.print("  eCO2 baseline: ");
  Serial.println(String(eco2Baseline).c_str());
}

void measureVOC() {
  if(vocTempRHClock.Tick() ) {
    SGP.setRelHumidity(temp.Latest(), rh.Latest());  
  }

  if( false && vocBaselineClock.Tick() ) {
    Serial.println("Reading and storing VOC baselines.");
    uint16_t eco2Baseline, tvocBaseline;
    SGP.getBaseline(&eco2Baseline, &tvocBaseline);
    writeEEPROM(tvocBaseline, eco2Baseline);
  }

  SGP.measure();
  tvoc.Add(SGP.getTVOC());
  eco2.Add(SGP.getCO2());
}

int oledPage = -2;

void updateOLED() {
  oledPage++;
  
  u8g2.firstPage();
  do {
    switch(oledPage) {
      case -1: drawHello(); break;
      case 0: drawMain(); break;
      case 1: drawVOC(); break;
      case 2: drawPM(); break;
      case 3: /* blank screen */ break;
    }
  } while ( u8g2.nextPage() );

  if(oledPage == 3) {
    oledPage = -1;
    // blank the screen briefly to mitigate OLED burn-in
    oledClock.SetNextAlertDelay(250);
  }
}

void drawHello() {
  u8g2.setFont(u8g2_font_luRS10_tr);
  u8g2_uint_t width = u8g2.drawStr(1, 35, "Hello, ") + 1;
  width += u8g2.drawStr(width, 35, sensorId.c_str());
  u8g2.drawStr(width, 35, "!");
}

void drawMain() {
  const int xoffset = 58;
  u8g2.setFont(u8g2_font_luRS10_tr);
  u8g2.drawStr(1, 20, "Temp");
  u8g2.drawStr(1, 35, "RH");
  u8g2_uint_t co2offset = u8g2.drawStr(1, 50, "CO") + 1;

  u8g2_uint_t tempoffset = u8g2.drawStr(xoffset, 20, String(temp.Latest(), 1).c_str()) + xoffset;
  u8g2_uint_t rhoffset = u8g2.drawStr(xoffset, 35, String(rh.Latest(), 0).c_str()) + xoffset;
  u8g2.drawStr(rhoffset + 2, 35, "%");
  u8g2_uint_t co2measureoffset = u8g2.drawStr(xoffset, 50, String(co2.Latest(), 0).c_str()) + xoffset;
  u8g2.drawStr(co2measureoffset, 50, " ppm");

  u8g2.setFont(u8g2_font_t0_11_me);
  tempoffset += u8g2.drawStr(tempoffset + 3, 15, "o");
  tempoffset += u8g2.drawStr(tempoffset + 3, 17, "C");
  u8g2.drawStr(co2offset, 53, "2");
}

void drawVOC() {
  u8g2.setFont(u8g2_font_luRS10_tr);
  u8g2.drawStr(1, 10, "TVOC");
  u8g2_uint_t width = u8g2.drawStr(1, 27, "eCO");

  const int xoffset = 58;
  u8g2_uint_t width3 = u8g2.drawStr(xoffset, 10, String(tvoc.Latest(),1).c_str()) + xoffset;
  u8g2.drawStr(width3, 10, " ppb");
  width3 = u8g2.drawStr(xoffset, 27, String(eco2.Latest(), 0).c_str()) + xoffset;
  u8g2.drawStr(width3, 27, " ppm");
  
  u8g2.setFont(u8g2_font_t0_11_me);
  u8g2.drawStr(width + 1, 29, "2");
}

void drawPM() {
  u8g2.setFont(u8g2_font_luRS10_tr);
  u8g2_uint_t width = u8g2.drawStr(1, 11, "PM");
  u8g2.drawStr(1, 26, "PM");
  u8g2.drawStr(1, 41, "PM");
  u8g2.drawStr(1, 60, "AQHI");

  const int xoffset = 58;
  u8g2_uint_t offset = u8g2.drawStr(xoffset, 11, String(pm1.Latest(),1).c_str()) + xoffset;
  u8g2_uint_t offsetA = u8g2.drawStr(offset, 11, " ug/m") + offset;
  offset = u8g2.drawStr(xoffset, 26, String(pm2_5.Latest()-pm1.Latest(),1).c_str()) + xoffset;
  u8g2_uint_t offsetB = u8g2.drawStr(offset, 26, " ug/m") + offset;
  offset = u8g2.drawStr(xoffset, 41, String(pm10.Latest()-pm2_5.Latest(),1).c_str()) + xoffset;
  u8g2_uint_t offsetC = u8g2.drawStr(offset, 41, " ug/m") + offset;
  u8g2.drawStr(xoffset, 60, "6?");

  u8g2.setFont(u8g2_font_t0_11_me);
  u8g2.drawStr(width + 2, 14, "1");
  u8g2.drawStr(width + 2, 29, "2.5");
  u8g2.drawStr(width + 2, 44, "10");
  u8g2.drawStr(offsetA, 8, "3");
  u8g2.drawStr(offsetB, 23, "3");
  u8g2.drawStr(offsetC, 38, "3");
}

char payload[32*1024];

void initTransmitter() {
  transmitClock.Reset();
}

void transmit() {
  if( WiFi.status() != WL_CONNECTED ) {
    return;
  }

  snprintf(
    payload, sizeof(payload),
    "airquality sensor=%s temp=%f rh=%f co2=%f pm1=%f pm2=%f pm10=%f tvoc=%f eco2=%f",
    sensorId.c_str(),
    temp.Aggregate(),
    rh.Aggregate(),
    co2.Aggregate(),
    pm1.Aggregate(),
    pm2_5.Aggregate(),
    pm10.Aggregate(),
    tvoc.Aggregate(),
    eco2.Aggregate());
          
  Serial.println(payload);
  String POSTURL = APIROOT;
  Serial.println(POSTURL);
  WiFiClient client;
  HTTPClient http;
  http.begin(client, POSTURL);
  http.addHeader("content-type", "application/json");
  int httpCode = http.PUT(payload);
  String response = http.getString();
  Serial.println(httpCode);
  Serial.println(response);
  http.end();
}

void connectToWifi() {
  WiFiManager wifiManager;
  //WiFi.disconnect(); //to delete previous saved hotspot
  String HOTSPOT = "AIRGRADIENT-" + String(ESP.getChipId(), HEX);
  wifiManager.setTimeout(120);
  if (!wifiManager.autoConnect((const char * ) HOTSPOT.c_str())) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
    delay(5000);
  }
}

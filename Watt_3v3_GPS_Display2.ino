#include <avr/pgmspace.h>
// Workaround for http://gcc.gnu.org/bugzilla/show_bug.cgi?id=34734
#ifdef PROGMEM
#undef PROGMEM
#define PROGMEM __attribute__((section(".progmem.data")))
#endif

// for voltage-measurement
static const int voltage_pin = A7;
static const float voltage_r1 = 103800.0;
static const float voltage_r2 = 9930.0;
static const float resistorFactor = 1023.0 * (voltage_r2/ (voltage_r1 + voltage_r2));
float vbat;

#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306_I2C.h>
Adafruit_SSD1306_I2C display2 = Adafruit_SSD1306_I2C(4);

#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>

Adafruit_GPS GPS(&Serial);

char gps_date[13];
char gps_time[9];
char gps_deg[10];
char printbuffer[23];

#include <SdFat.h>
SdFat sd;
bool cardPresent = false;
PROGMEM char filename[] = "gpslog.csv";

// call back for file timestamps
void dateTime(uint16_t* date, uint16_t* time) {
  // return date using FAT_DATE macro to format fields
  *date = FAT_DATE(GPS.year, GPS.month, GPS.day);

  // return time using FAT_TIME macro to format fields
  *time = FAT_TIME(GPS.hour, GPS.minute, GPS.seconds);
}

void initSD(void) {
  cardPresent = sd.begin(9, SPI_HALF_SPEED);
}

void setupFile(void) {
  if (cardPresent) {
    if (!sd.exists(filename)) {
      ofstream sdout(filename, ios::out | ios::app);
      sdout << "date/time, humidity %, temperature C, pressure pa, battery voltage\n";
      sdout.close();
    }
  }
}

boolean usingInterrupt = false;
//void useInterrupt(boolean); // Func prototype keeps Arduino 0023 happy

// Interrupt is called once a millisecond, looks for any new GPS data, and stores it
SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();
}

void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    usingInterrupt = true;
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt = false;
  }
}

void setup() {
  Serial.begin(9600);
  
  // for voltage measurement via a voltage-divider
  analogReference(INTERNAL);
  pinMode(voltage_pin, INPUT);

  display2.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display2.setTextWrap(false);
  display2.setTextColor(WHITE);
  display2.clearDisplay();

  GPS.begin(9600);

//  GPS.sendCommand(PMTK_SET_BAUD_4800);
//  GPS.sendCommand(PMTK_SET_BAUD_9600);
//  GPS.sendCommand(PMTK_SET_BAUD_38400);
//  GPS.sendCommand(PMTK_SET_BAUD_57600);
  Serial.end();

  Serial.begin(4800);  
  GPS.begin(4800);
  
//  GPS.sendCommand(PMTK_SET_BAUD_9600);
//  GPS.sendCommand(PMTK_SET_BAUD_38400);
//  GPS.sendCommand(PMTK_SET_BAUD_57600);

//  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
//  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_ALLDATA);
  
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
//  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_5HZ);

//  GPS.sendCommand(PGCMD_ANTENNA);
   GPS.sendCommand(PGCMD_NOANTENNA);

  useInterrupt(true);

  // see if the card is present and can be initialized:
//  initSD();
}

uint32_t timer = millis();

void loop() {
  display2.clearDisplay();
  display2.setTextSize(1);

//  if (! usingInterrupt) {
//    // read data from the GPS in the 'main loop'
//    char c = GPS.read();
//    // if you want to debug, this is a good time to do it!
//  }

  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences! 
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    //Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
  
    if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
      return;  // we can fail to parse a sentence in which case we should just wait for another
  }

  if (timer > millis())  timer = millis();
  
  if (millis() - timer > 1000) {
    timer = millis();

    sprintf(gps_date, "20%02d-%02d-%02d", GPS.year, GPS.month, GPS.day);

    display2.setCursor(0, 0);
    display2.setTextSize(1);
    display2.print(gps_date);  

    sprintf(gps_time, "%02d:%02d:%02d", GPS.hour, GPS.minute, GPS.seconds);

    display2.setCursor(80, 0);
    display2.print(gps_time);

    vbat = (analogRead(voltage_pin) / resistorFactor) * 1.1;
    display2.setCursor(98, 24);
    display2.print(vbat); display2.print(F("V"));

    if (GPS.fix) {
      display2.setCursor(66, 0);
      display2.print(GPS.satellites, DEC);

      display2.setCursor(0, 8);
      sprintf(printbuffer, "Lt:%9s%c", ftoa(gps_deg, GPS.latitude, 4), GPS.lat);
      display2.print(printbuffer);
      
      display2.setCursor(0, 16);
      sprintf(printbuffer, "Ln:%9s%c", ftoa(gps_deg, GPS.longitude, 4), GPS.lon);
      display2.print(printbuffer);

      display2.setCursor(86, 16);
      display2.print(F("DOP:")); display2.print(GPS.HDOP, 1);
      
      display2.setCursor(0, 24);
      display2.print(F("Alt:")); display2.print(GPS.altitude, 2); display2.print(F("m"));
    } else {
      display2.setTextSize(2);
      display2.setCursor(0, 16);
      display2.print(F("No fix"));
    }

    display2.display();
  }
  
}

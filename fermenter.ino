#define VERSION 2.0
#define TYPE "FERMENTER"

#include <string.h>
#include <EEPROM.h>
#include <DallasTemperature.h>
#include <OneWire.h>

// device
struct device {
  char sn[17] = "";
  float version = VERSION;
} myDevice;

// sensors
#define RESOLUTION 12
#define WAIT_FOR_CONVERSION false
#define SENSOR_REQUEST_DELAY 1000
const int ONE_WIRE[] = { 10, 13 };
OneWire oneWireBus[] = { OneWire(ONE_WIRE[0]), OneWire(ONE_WIRE[1]) };
DallasTemperature sensors[] = { &oneWireBus[0], &oneWireBus[1] };
unsigned long sensorsRequestEnd = 0;

// MODES
#define OFF 0
#define ON 1

// CHILL pins
const int PIN_CHILL[] = { 2, 4 };

// fermenter config
struct config {
  int mode = OFF;
  float setpoint = 64.0;
  float hysteresis = 0.2;
  unsigned long antiCycle = 300000;
} myConfig[2];

// fermenter state
struct fermenter {
  float temperature = 0.0;
  unsigned long antiCycle = 0;
  unsigned long endChill = 0;
} myFermenter[2];

// buffer for serial communication
#define BUFFER_SIZE 64

struct buffer {
  char buffer[BUFFER_SIZE+1];
  int index = 0;
  boolean overflow = false;
} myBuffer;

///////////////////////////////////
// FUNCTIONS
///////////////////////////////////

void setup() {
  setupSerial();
  setupSensors();
  setupPins();
  setupConfig();
}

void loop() {
  loopSensors();
  loopFermenter();
}

void setupSerial() {
  Serial.begin(9600);
}

void setupSensors() {
  for (int i=0 ; i<2 ; i++) {
    sensors[i].begin();
    sensors[i].setWaitForConversion(WAIT_FOR_CONVERSION);
    sensors[i].setResolution(RESOLUTION);
  }
  requestTemperatures();
}

void setupPins() {
  for (int i=0 ; i<2 ; i++) {
    pinMode(PIN_CHILL[i], OUTPUT);
    offPin(PIN_CHILL[i]);
  }
}

void setupConfig() {
  struct device memDevice;
  EEPROM.get(0, memDevice);
  if (memDevice.version == VERSION) {
    myDevice = memDevice;
    EEPROM.get(sizeof(struct device), myConfig);
  } else {
    randomSeed(analogRead(0));
    for (int i=0 ; i<16 ; i++) {
      myDevice.sn[i] = '0'+random(10);
    }
    saveConfig();
  }
}

void saveConfig() {
  int i;
  EEPROM.put(0, myDevice);
  EEPROM.put(sizeof(struct device), myConfig);
}

void serialEvent() {
  char c;
  c = Serial.read();
  if (c == '\n') {
    if (!myBuffer.overflow) {
      parseBuffer();
    } else {
      Serial.println("buffer overflow");
    }
    resetBuffer();
  } else {
    if (myBuffer.index < BUFFER_SIZE) {
      myBuffer.buffer[myBuffer.index++] = c;
    } else {
      myBuffer.overflow = true;
    }
  }
}

void parseBuffer() {
  char * b = myBuffer.buffer;
  char * word;
  char * cmd = NULL;
  char * fermenter = NULL;
  char * param = NULL;
  int count = 0;
  while ((word = strtok_r(b,",",&b)) != NULL) {
    switch(count) {
      case 0:
        cmd = word;
        break;
      case 1:
        fermenter = word;
        break;
      case 2:
        param = word;
        break;
      default:
        Serial.println("invalid command");
        return;
    }
    count++;
  }
  runCommand(cmd, fermenter, param);
}

void resetBuffer() {
  int i;
  int count;
  for ( i=0 ; i<BUFFER_SIZE+1 ; i++ ) {
    myBuffer.buffer[i] = '\0';
  }
  myBuffer.index = 0;
  myBuffer.overflow = false;
}

void runCommand(char * cmd, char * fermenter, char * param) {
  if (strcmp(cmd, "getVersion") == 0) {
    Serial.println(myDevice.version);
  } else if (strcmp(cmd, "getType") == 0) {
    Serial.println(TYPE);
  } else if (strcmp(cmd, "getSN") == 0) {
    Serial.println(myDevice.sn);
  } else if (strcmp(cmd, "getMode") == 0) {
    Serial.println(myConfig[atoi(fermenter)].mode);
  } else if (strcmp(cmd, "getSetpoint") == 0) {
    Serial.println(myConfig[atoi(fermenter)].setpoint);
  } else if (strcmp(cmd, "getHysteresis") == 0) {
    Serial.println(myConfig[atoi(fermenter)].hysteresis);
  } else if (strcmp(cmd, "getAntiCycle") == 0) {
    Serial.println(myConfig[atoi(fermenter)].antiCycle);
  } else if (strcmp(cmd, "getTemperature") == 0) {
    Serial.println(myFermenter[atoi(fermenter)].temperature,4);
  } else if (strcmp(cmd, "setMode") == 0) {
    setMode(atoi(fermenter), atoi(param));
  } else if (strcmp(cmd, "setSetpoint") == 0) {
    setSetpoint(atoi(fermenter), atof(param));
  } else if (strcmp(cmd, "setHysteresis") == 0) {
    setHysteresis(atoi(fermenter), atof(param));
  } else if (strcmp(cmd, "setAntiCycle") == 0) {
    setAntiCycle(atoi(fermenter), atol(param));
  } else {
    Serial.println("unknown command");
  }
}

void setMode(int fermenter, int mode) {
  if (mode == OFF) {
    myConfig[fermenter].mode = OFF;
    saveConfig();
    resetFermenter(fermenter);
    Serial.println("set");
  } else if (mode == ON) {
    myConfig[fermenter].mode = ON;
    saveConfig();
    resetFermenter(fermenter);
    Serial.println("set");
  } else {
    Serial.println("unknown mode");
  }
}

void setSetpoint(int fermenter, float setpoint) {
  if (setpoint >= 32.0 && setpoint <= 212.0) {
    myConfig[fermenter].setpoint = setpoint;
    saveConfig();
    resetFermenter(fermenter);
    Serial.println("set");
  } else {
    Serial.println("out of range (32 to 212)");
  }
}

void setHysteresis(int fermenter, float hysteresis) {
  if (hysteresis >= 0 && hysteresis <= 5) {
    myConfig[fermenter].hysteresis = hysteresis;
    saveConfig();
    resetFermenter(fermenter);
    Serial.println("set");
  } else {
    Serial.println("out of range (0 to 5)");
  }
}

void setAntiCycle(int fermenter, unsigned long antiCycle) {
  if (antiCycle >= 0 && antiCycle <= 3600000) {
    myConfig[fermenter].antiCycle = antiCycle;
    saveConfig();
    resetFermenter(fermenter);
    Serial.println("set");
  } else {
    Serial.println("out of range (0 to 3600000)");
  }
}

void loopSensors() {
  if (millis() >= sensorsRequestEnd) {
    getTemperatures();
    requestTemperatures();
  }
}

void requestTemperatures() {
  for (int i=0; i<2 ; i++) {
    sensors[i].requestTemperatures();
  }
  sensorsRequestEnd = millis() + SENSOR_REQUEST_DELAY;
}

void getTemperatures() {
  for (int i=0 ; i<2 ; i++) {
    myFermenter[i].temperature = sensors[i].getTempFByIndex(0);
  }
}

// are the sensors reporting reasonable data?
boolean isSensorData() {
  for (int i=0 ; i<2 ; i++) {
    if (myFermenter[i].temperature < 32) {
      return false;
    }
  }
  return true;
}

// is the mode ON?
boolean isModeOn(int fermenter) {
  return myConfig[fermenter].mode == ON;
}

// is it chilling?
boolean isChilling(int fermenter) {
  return myFermenter[fermenter].endChill != 0;
}

// is chilling required?
boolean needsChill(int fermenter) {
  return myFermenter[fermenter].temperature - myConfig[fermenter].setpoint > myConfig[fermenter].hysteresis;
}

// is chilling delayed?
boolean isChillingDelayed(int fermenter) {
  return millis() <= myFermenter[fermenter].antiCycle;
}

// is chill cycle complete?
boolean isChillingComplete(int fermenter) {
  return myFermenter[fermenter].temperature - myConfig[fermenter].setpoint < 1 && millis() >= myFermenter[fermenter].endChill;
}

// reset fermenter state and turn off pin
void resetFermenter(int fermenter) {
  if (myFermenter[fermenter].endChill != 0) {
    offPin(PIN_CHILL[fermenter]);
  }
  myFermenter[fermenter].endChill = 0;
  myFermenter[fermenter].antiCycle = millis() + 1000; //allow 1 second for other changes
}

// fermenter loop
void loopFermenter() {
  for (int i=0 ; i<2 ; i++) {
    if (!isSensorData()) {
      resetFermenter(i);
      return;
    }
    if (isModeOn(i)) {
      if (isChilling(i)) {
        if (!needsChill(i) || isChillingComplete(i)) {
          offPin(PIN_CHILL[i]);
          myFermenter[i].endChill = 0;
          myFermenter[i].antiCycle = millis() + myConfig[i].antiCycle;
        }
      } else if (needsChill(i) && !isChillingDelayed(i)) {
        onPin(PIN_CHILL[i]);
        myFermenter[i].endChill = millis() + ((myFermenter[i].temperature - myConfig[i].setpoint) / 0.1 ) * 5000;
      }
    }
  }
}

void offPin(int pin) {
  digitalWrite(pin, HIGH);
}

void onPin(int pin) {
  digitalWrite(pin, LOW);
}

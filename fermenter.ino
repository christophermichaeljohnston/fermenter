#define VERSION 1.0
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

// MODES
#define OFF 0
#define ON 1

// CHILL and HEAT pins
//const int PIN_CHILL[] = { 2, 4 };
//const int PIN_HEAT[] = { 3, 5 };
const int PIN_CHILL = 2;
const int PIN_HEAT =  3;

// config with defaults
struct config {
  int mode         = OFF;
  float setpoint   = 64.0;
  float hysteresis = 0.1;
  unsigned long antiCycle    = 300000; // 5 minutes
  unsigned long antiFight    = 1800000; // 30 minutes
} myConfig;

// temperatures
struct temperatures {
  float internal = 0.0;
  float external = 0.0;
} myTemperatures;

// state
struct state {
  unsigned long sensorsRequestEnd = 0;
  int activePin = 0;
  int lastPin = 0;
  unsigned long activeEnd = 0;
  unsigned long lastEnd = 0;
} myState;

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
  sensors[0].begin();
  sensors[0].setWaitForConversion(WAIT_FOR_CONVERSION);
  sensors[0].setResolution(RESOLUTION);
  sensors[1].begin();
  sensors[1].setWaitForConversion(WAIT_FOR_CONVERSION);
  sensors[1].setResolution(RESOLUTION);
  requestTemperatures();
}

void setupPins() {
  pinMode(PIN_CHILL, OUTPUT);
  offPin(PIN_CHILL);
  pinMode(PIN_HEAT, OUTPUT);
  offPin(PIN_HEAT);
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
  char * param = NULL;
  int count = 0;
  while ((word = strtok_r(b,",",&b)) != NULL) {
    switch(count) {
      case 0:
        cmd = word;
        break;
      case 1:
        param = word;
        break;
      default:
        Serial.println("invalid command");
        return;
    }
    count++;
  }
  runCommand(cmd, param);
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

void runCommand(char * cmd, char * param) {
  if (strcmp(cmd, "getVersion") == 0) {
    Serial.println(myDevice.version);
  } else if (strcmp(cmd, "getType") == 0) {
    Serial.println(TYPE);
  } else if (strcmp(cmd, "getSN") == 0) {
    Serial.println(myDevice.sn);
  } else if (strcmp(cmd, "getMode") == 0) {
    Serial.println(myConfig.mode);
  } else if (strcmp(cmd, "getSetpoint") == 0) {
    Serial.println(myConfig.setpoint);
  } else if (strcmp(cmd, "getHysteresis") == 0) {
    Serial.println(myConfig.hysteresis);
  } else if (strcmp(cmd, "getAntiCycle") == 0) {
    Serial.println(myConfig.antiCycle);
  } else if (strcmp(cmd, "getAntiFight") == 0) {
    Serial.println(myConfig.antiFight);
  } else if (strcmp(cmd, "getInternalTemperature") == 0) {
    Serial.println(myTemperatures.internal,4);
  } else if (strcmp(cmd, "getExternalTemperature") == 0) {
    Serial.println(myTemperatures.external,4);
  } else if (strcmp(cmd, "setMode") == 0) {
    setMode(atoi(param));
  } else if (strcmp(cmd, "setSetpoint") == 0) {
    setSetpoint(atof(param));
  } else if (strcmp(cmd, "setHysteresis") == 0) {
    setHysteresis(atof(param));
  } else if (strcmp(cmd, "setAntiCycle") == 0) {
    setAntiCycle(atol(param));
  } else if (strcmp(cmd, "setAntiFight") == 0) {
    setAntiFight(atol(param));
  } else {
    Serial.println("unknown command");
  }
}

void setMode(int mode) {
  if (mode == OFF) {
    myConfig.mode = OFF;
    resetPins();
    Serial.println("set");
  } else if (mode == ON) {
    myConfig.mode = ON;
    Serial.println("set");
  } else {
    Serial.println("unknown mode");
  }
  saveConfig();
}

void setSetpoint(float setpoint) {
  if (setpoint >= 32.0 && setpoint <= 212.0) {
    myConfig.setpoint = setpoint;
    saveConfig();
    resetPins();
    Serial.println("set");
  } else {
    Serial.println("out of range (32 to 212)");
  }
}

void setHysteresis(float hysteresis) {
  if (hysteresis >= 0 && hysteresis <= 5) {
    myConfig.hysteresis = hysteresis;
    saveConfig();
    resetPins();
    Serial.println("set");
  } else {
    Serial.println("out of range (0 to 5)");
  }
}

void setAntiCycle(unsigned long antiCycle) {
  if (antiCycle >= 0 && antiCycle <= 3600000) {
    myConfig.antiCycle = antiCycle;
    saveConfig();
    resetPins();
    Serial.println("set");
  } else {
    Serial.println("out of range (0 to 3600000)");
  }
}

void setAntiFight(unsigned long antiFight) {
  if (antiFight >= 0 && antiFight <= 3600000) {
    myConfig.antiFight = antiFight;
    saveConfig();
    resetPins();
    Serial.println("set");
  } else {
    Serial.println("out of range (0 to 3600000)");
  }
}

void resetPins() {
  if (isActivePin()) {
    offPin(myState.activePin);
  }
  myState.activePin = 0;
  myState.activeEnd = 0;
  myState.lastPin = 0;
  myState.lastEnd = 0;
}


void loopSensors() {
  if (millis() >= myState.sensorsRequestEnd) {
    getTemperatures();
    requestTemperatures();
  }
}

void requestTemperatures() {
  sensors[0].requestTemperatures();
  sensors[1].requestTemperatures();
  myState.sensorsRequestEnd = millis() + SENSOR_REQUEST_DELAY;
}

void getTemperatures() {
  myTemperatures.external = sensors[0].getTempFByIndex(0);
  myTemperatures.internal = sensors[1].getTempFByIndex(0);
}

// is the mode ON?
boolean isModeOn() {
  return myConfig.mode == ON;
}

// is there an active pin?
boolean isActivePin() {
  return myState.activePin != 0;
}

// is it chilling?
boolean isChilling() {
  return myState.activePin == PIN_CHILL;
}

// is it heating?
boolean isHeating() {
  return myState.activePin == PIN_HEAT;
}

// are the sensors reporting reasonable data?
boolean isSensorData() {
  return myTemperatures.internal > 32 && myTemperatures.external > 32;
}

// is chilling required?
boolean needsChill() {
  return myTemperatures.internal - myConfig.setpoint > myConfig.hysteresis;
}

// is heating required?
boolean needsHeat() {
  return myConfig.setpoint - myTemperatures.internal > myConfig.hysteresis;
}

// is heat/chill fighting?
boolean isFighting(int pin) {
  return myState.lastPin != 0 && myState.lastPin != pin && millis() < myState.lastEnd + myConfig.antiFight;
}

// is chilling delayed?
boolean isChillingDelayed() {
  return myState.lastPin != 0 && millis() <= myState.lastEnd + myConfig.antiCycle;
}

// is chill cycle complete?
boolean isChillingComplete() {
  return myTemperatures.internal - myConfig.setpoint < 1 && millis() >= myState.activeEnd;
}

// is heat cycle hot?
boolean isHeatingHot() {
  return myTemperatures.external - myConfig.setpoint > 0.5;
}

// is heat cycle cool?
boolean isHeatingCool() {
  return myTemperatures.external - myConfig.setpoint < 0;
}

void loopFermenter() {
  if (!isSensorData()) {
    resetPins();
    return;
  }
  if (isModeOn()) {
    if (isChilling()) {
      if (!needsChill() || isChillingComplete()) {
        offPin(PIN_CHILL);
      }
    } else if (isHeating()) {
      if (!needsHeat() || isHeatingHot()) {
        offPin(PIN_HEAT);
      }
    } else if (needsChill() && !isFighting(PIN_CHILL) && !isChillingDelayed()) {
      onPin(PIN_CHILL);
      // chill 5s for every 0.1s off setpoint
      myState.activeEnd = millis() + (((myTemperatures.internal - myConfig.setpoint) / 0.1 ) * 5000);
    } else if (needsHeat() && isHeatingCool() && !isFighting(PIN_HEAT)) {
      onPin(PIN_HEAT);
    }
  }
}

void offPin(int pin) {
  digitalWrite(pin, HIGH);
  myState.activePin = 0;
  myState.activeEnd = 0;
  myState.lastEnd = millis();
}

void onPin(int pin) {
  digitalWrite(pin, LOW);
  myState.activePin = pin;
  myState.lastPin = pin;
}

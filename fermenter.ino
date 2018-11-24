#define VERSION 1.1
#define TYPE "FERMENTER"
#define FERMENTERS 2

#include <string.h>
#include <EEPROM.h>
#include <DallasTemperature.h>
#include <OneWire.h>

//
// device
//
struct device {
  char sn[17]      = "";
  float version    = VERSION;
} myDevice;

//
// sensors
//
#define ONE_WIRE 8
#define RESOLUTION 12
#define WAIT_FOR_CONVERSION false
#define SENSOR_DELAY 1000
OneWire oneWire(ONE_WIRE);
DallasTemperature sensors(&oneWire);

struct sensorStateMachine {
  unsigned long sensorDelay = 0;  
} mySensorStateMachine;

//
// MODES
//
#define OFF 0
#define ON 1

//
// Operations
//
#define OP_NONE 0
#define OP_CHILL 1
#define OP_HEAT 2

//
// CHILL and HEAT pins
//
const int PIN_CHILL[] = { 4, 6 };
const int PIN_HEAT[] = { 5, 7 };

// 
// basic cycle control
//
const float hysteresis = 0.2;
const float t1   = 75.0;
const float t2   = 50.0;
const unsigned long c1 = 5000;  // 5 seconds
const unsigned long c2 = 10000; // 10 seconds
const unsigned long c3 = 60000; // 60 seconds
const unsigned long h1 = (5*60000); // 5 minute
const unsigned long h2 = (2.5*60000); // 2.5 minute
const unsigned long h3 = 60000; // 1 minute
const unsigned long antiCycle  = 300000; // 5 minutes
const unsigned long antiFight  = 1800000; // 15 minutes


//
// fermenters
//
struct fermenterConfig {
  int mode         = OFF;
  float setpoint   = 64.0;
};

struct fermenterStateMachine {
  int fight_pin = 0;
  unsigned long fight_end = 0;
  int cycle_pin = 0;
  unsigned long cycle_end = 0;
};

struct fermenter {
  DeviceAddress deviceAddress;
  struct fermenterConfig config;
  struct fermenterStateMachine stateMachine;
  float temperature = 0.0;
} myFermenter[FERMENTERS];

//
// buffer for serial communication
//
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
  int i;
  loopSensors();
  for ( i=0 ; i<FERMENTERS ; i++ ) {
    loopFermenter(i);
  }
}

void setupSerial() {
  Serial.begin(9600);
}

void setupSensors() {
  sensors.begin();
  sensors.setWaitForConversion(WAIT_FOR_CONVERSION);
  sensors.setResolution(RESOLUTION);
}

void setupPins() {
  int i;
  for ( i=0 ; i<FERMENTERS ; i++ ) {
    pinMode(PIN_CHILL[i], OUTPUT);
    offPin(PIN_CHILL[i]);
    pinMode(PIN_HEAT[i], OUTPUT);
    offPin(PIN_HEAT[i]);
  }
}

void setupConfig() {
  int i;
  struct device memDevice;
  EEPROM.get(0, memDevice);
  if (memDevice.version == VERSION) {
    myDevice = memDevice;
    for ( i=0 ; i<FERMENTERS ; i++ ) {
      EEPROM.get(sizeof(struct device)+(sizeof(struct fermenter)*i),myFermenter[i]);
    }
  } else {
    randomSeed(analogRead(0));
    for (int i=0 ; i<16 ; i++) {
      myDevice.sn[i] = '0'+random(10);
    }
    for ( i=0 ; i<FERMENTERS ; i++ ) {
      sensors.getAddress(myFermenter[i].deviceAddress, i);
    }
    saveConfig();
  }
}

void saveConfig() {
  int i;
  EEPROM.put(0, myDevice);
  for ( i=0 ; i<FERMENTERS ; i++ ) {
    EEPROM.put(sizeof(struct device)+(sizeof(struct fermenter)*i), myFermenter[i]);
  }
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

void runCommand(char * cmd, char * fermenter, char * param) {
  if (strcmp(cmd,"getVersion") == 0) {
    Serial.println(myDevice.version);
  } else if (strcmp(cmd,"getType") == 0) {
    Serial.println(TYPE);
  } else if (strcmp(cmd,"getFermenters") == 0) {
    Serial.println(FERMENTERS);
  } else if (strcmp(cmd,"getSN") == 0) {
    Serial.println(myDevice.sn);
  } else if (strcmp(cmd, "getMode") == 0) {
    Serial.println(myFermenter[atoi(fermenter)].config.mode);
  } else if (strcmp(cmd,"getSetpoint") == 0) {
    Serial.println(myFermenter[atoi(fermenter)].config.setpoint);
  } else if (strcmp(cmd,"getTemperature") == 0) {
    Serial.println(myFermenter[atoi(fermenter)].temperature,4);
  } else if (strcmp(cmd,"setMode") == 0) {
    setMode(atoi(fermenter),atoi(param));
  } else if (strcmp(cmd,"setSetpoint") == 0) {
    setSetpoint(atoi(fermenter),atof(param));
  } else {
    Serial.println("unknown command");
  }
}

void setMode (int fermenter, int mode) {
  if (mode == OFF) {
    myFermenter[fermenter].config.mode = OFF;
    Serial.println("set");
  } else if (mode == ON) {
    myFermenter[fermenter].config.mode = ON;
    Serial.println("set");
  } else {
    Serial.println("unknown mode");
  }
  saveConfig();
}

void setSetpoint(int fermenter, float setpoint) {
  if (setpoint >= 32.0 && setpoint <= 212.0) {
    myFermenter[fermenter].config.setpoint = setpoint;
    saveConfig();
    if (myFermenter[fermenter].stateMachine.cycle_pin != 0) {
      offPin(myFermenter[fermenter].stateMachine.cycle_pin);
    }
    myFermenter[fermenter].stateMachine.cycle_pin = 0;
    myFermenter[fermenter].stateMachine.cycle_end = 0;
    myFermenter[fermenter].stateMachine.fight_pin = 0;
    myFermenter[fermenter].stateMachine.fight_end = 0;
    Serial.println("set");
  } else {
    Serial.println("out of range (32 to 212)");
  }
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


void loopSensors() {
  if (mySensorStateMachine.sensorDelay == 0) {
    requestTemperatures();
  } else if ((millis() - mySensorStateMachine.sensorDelay) >= SENSOR_DELAY) {
    getTemperatures();
  }
}

void requestTemperatures() {
  sensors.requestTemperatures();
  mySensorStateMachine.sensorDelay = millis();
}

void getTemperatures() {
  int i;
  for ( i=0 ; i<FERMENTERS ; i++ ) {
    myFermenter[i].temperature = sensors.getTempF(myFermenter[i].deviceAddress);
  }
  mySensorStateMachine.sensorDelay = 0;
}

void loopFermenter(int fermenter) {
  if (myFermenter[fermenter].config.mode == OFF) {
    if (myFermenter[fermenter].stateMachine.cycle_pin != 0) {
      offPin(myFermenter[fermenter].stateMachine.cycle_pin);
    }
    myFermenter[fermenter].stateMachine.cycle_pin = 0;
    myFermenter[fermenter].stateMachine.cycle_end = 0;
    myFermenter[fermenter].stateMachine.fight_pin = 0;
    myFermenter[fermenter].stateMachine.fight_end = 0;
  } else if (myFermenter[fermenter].stateMachine.cycle_end == 0 && myFermenter[fermenter].temperature > 32) {
    if ((myFermenter[fermenter].temperature - myFermenter[fermenter].config.setpoint) > hysteresis) {
      if (myFermenter[fermenter].stateMachine.fight_pin == PIN_CHILL[fermenter] || millis() > myFermenter[fermenter].stateMachine.fight_end) {
        onPin(PIN_CHILL[fermenter]);
        myFermenter[fermenter].stateMachine.cycle_pin = PIN_CHILL[fermenter];
        if (myFermenter[fermenter].temperature > t1) {
          myFermenter[fermenter].stateMachine.cycle_end = millis() + c1;
        } else if (myFermenter[fermenter].temperature > t2) {
          myFermenter[fermenter].stateMachine.cycle_end = millis() + c2;
        } else {
          myFermenter[fermenter].stateMachine.cycle_end = millis() + c3;
        }
      }
    } else if ((myFermenter[fermenter].config.setpoint - myFermenter[fermenter].temperature) > hysteresis) {
      if (myFermenter[fermenter].stateMachine.fight_pin == PIN_HEAT[fermenter] || millis() > myFermenter[fermenter].stateMachine.fight_end) {
        onPin(PIN_HEAT[fermenter]);
        myFermenter[fermenter].stateMachine.cycle_pin = PIN_HEAT[fermenter];
        if (myFermenter[fermenter].temperature > t1) {
          myFermenter[fermenter].stateMachine.cycle_end = millis() + h1;
        } else if (myFermenter[fermenter].temperature > t2) {
          myFermenter[fermenter].stateMachine.cycle_end = millis() + h2;
        } else {
          myFermenter[fermenter].stateMachine.cycle_end = millis() + h3;
        }
      }
    }
  } else if (millis() >= myFermenter[fermenter].stateMachine.cycle_end) {
    if (myFermenter[fermenter].stateMachine.cycle_pin != 0) {
      if ((myFermenter[fermenter].stateMachine.cycle_pin == PIN_CHILL[fermenter] && ((myFermenter[fermenter].temperature - myFermenter[fermenter].config.setpoint) < 1.0)) || 
          (myFermenter[fermenter].stateMachine.cycle_pin == PIN_HEAT[fermenter]  && ((myFermenter[fermenter].config.setpoint - myFermenter[fermenter].temperature) < 1.0))) {
        offPin(myFermenter[fermenter].stateMachine.cycle_pin);
        myFermenter[fermenter].stateMachine.fight_pin = myFermenter[fermenter].stateMachine.cycle_pin;
        myFermenter[fermenter].stateMachine.fight_end = millis() + antiFight;
        myFermenter[fermenter].stateMachine.cycle_pin = 0;
        myFermenter[fermenter].stateMachine.cycle_end = millis() + antiCycle;
      }
    } else {
      myFermenter[fermenter].stateMachine.cycle_end = 0;
    }
  }
}

void offPin(int pin) {
  digitalWrite(pin, HIGH);
}

void onPin(int pin) {
  digitalWrite(pin, LOW);
}

#define VERSION 0.81
#define TYPE 'F'

#include <string.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//
// configuration with defaults
//
struct device {
  char sn[17]      = "";
  float version    = VERSION;
} myDevice;

//
// sensors
//
#define ONE_WIRE 2
#define RESOLUTION 12
#define WAIT_FOR_CONVERSION false
#define SENSOR_DELAY 1000
OneWire oneWire(ONE_WIRE);
DallasTemperature sensors(&oneWire);
struct sensorStateMachine {
  unsigned long sensors   = 0;  
} mySensorStateMachine;

//
// pumps
//
const int PUMP[] = {4,5};

//
// fermenters
//
#define OFF  0
#define CHILL 1
#define HEAT  2
struct fermenterConfig {
  char name[17]     = "";
  int mode         = OFF;
  float setpoint   = 64.0;
  float hysteresis = 0.1;
  float rampStep   = 0.0;
  float rampPoint  = 68.0;
  unsigned long rampDelay = 3600000;
  unsigned long pumpRun   = 5000;
  unsigned long pumpDelay = 300000;
};
struct fermenterStateMachine {
  unsigned long rampDelay = 0;
  unsigned long pump      = 0;
  unsigned long pumpDelay = 0;
};
struct fermenter {
  struct fermenterConfig config;
  struct fermenterStateMachine stateMachine;
  DeviceAddress deviceAddress;
  float temperature = 0.0;
} myFermenter[2];

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
  setupPumps();
  setupConfig();
  Serial.println("Ready...");
}

void loop() {
  int i;
  loopSensors();
  for ( i=0 ; i<=1 ; i++ ) {
    loopFermenter(i);
  }
}

void setupConfig() {
  struct device memDevice;
  EEPROM.get(0, memDevice);
  if (memDevice.version == VERSION) {
    myDevice = memDevice;
    EEPROM.get(sizeof(struct device),myFermenter[0]);
    EEPROM.get(sizeof(struct device)+sizeof(struct fermenter),myFermenter[1]);
  } else {
    randomSeed(analogRead(0));
    for (int i=0 ; i<16 ; i++) {
      myDevice.sn[i] = '0'+random(10);
    }
    strcpy(myFermenter[0].config.name,"0");
    strcpy(myFermenter[1].config.name,"1");
    sensors.getAddress(myFermenter[0].deviceAddress, 0);
    sensors.getAddress(myFermenter[1].deviceAddress, 1);
    saveConfig();
  }
}

void saveConfig() {
  EEPROM.put(0, myDevice);
  EEPROM.put(sizeof(struct device), myFermenter[0]);
  EEPROM.put(sizeof(struct device)+sizeof(struct fermenter), myFermenter[1]);
}

void setupSerial() {
  Serial.begin(9600);
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
  } else if (strcmp(cmd,"getSN") == 0) {
    Serial.println(myDevice.sn);
  } else if (strcmp(cmd,"getName") == 0) {
    Serial.println(myFermenter[atoi(fermenter)].config.name);
  } else if (strcmp(cmd, "getMode") == 0) {
    Serial.println(myFermenter[atoi(fermenter)].config.mode);
  } else if (strcmp(cmd,"getSetpoint") == 0) {
    Serial.println(myFermenter[atoi(fermenter)].config.setpoint);
  } else if (strcmp(cmd,"getHysteresis") == 0) {
    Serial.println(myFermenter[atoi(fermenter)].config.hysteresis);
  } else if (strcmp(cmd, "getRampStep") == 0) {
    Serial.println(myFermenter[atoi(fermenter)].config.rampStep);
  } else if (strcmp(cmd, "getRampPoint") == 0) {
    Serial.println(myFermenter[atoi(fermenter)].config.rampPoint);
  } else if (strcmp(cmd, "getRampDelay") == 0) {
    Serial.println(myFermenter[atoi(fermenter)].config.rampDelay);
  } else if (strcmp(cmd,"getPumpRun") == 0) {
    Serial.println(myFermenter[atoi(fermenter)].config.pumpRun);
  } else if (strcmp(cmd,"getPumpDelay") == 0) {
    Serial.println(myFermenter[atoi(fermenter)].config.pumpDelay);
  } else if (strcmp(cmd,"getTemperature") == 0) {
    Serial.println(myFermenter[atoi(fermenter)].temperature);
  } else if (strcmp(cmd,"getDeviceAddress") == 0) {
    printAddress(myFermenter[atoi(fermenter)].deviceAddress);
    Serial.println();
  } else if (strcmp(cmd,"setName") == 0) {
    setName(atoi(fermenter),param);
  } else if (strcmp(cmd,"setMode") == 0) {
    setMode(atoi(fermenter),atoi(param));
  } else if (strcmp(cmd,"setSetpoint") == 0) {
    setSetpoint(atoi(fermenter),atof(param));
  } else if (strcmp(cmd,"setHysteresis") == 0) {
    setHysteresis(atoi(fermenter),atof(param));
  } else if (strcmp(cmd,"setRampStep") == 0) {
    setRampStep(atoi(fermenter),atof(param));
  } else if (strcmp(cmd,"setRampPoint") == 0) {
    setRampPoint(atoi(fermenter),atof(param));
  } else if (strcmp(cmd,"setRampDelay") == 0) {
    setRampDelay(atoi(fermenter),strtoul(param, NULL, 10));
  } else if (strcmp(cmd, "setPumpRun") == 0) {
    setPumpRun(atoi(fermenter),strtoul(param, NULL, 10));
  } else if (strcmp(cmd, "setPumpDelay") == 0) {
    setPumpDelay(atoi(fermenter),strtoul(param, NULL, 10));
  } else {
    Serial.println("unknown command");
  }
}

void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void setName (int fermenter, char * name) {
  if (strlen(name) < 16) {
    strcpy(myFermenter[fermenter].config.name, name);
    saveConfig();
    Serial.println("set");
  } else {
    Serial.println("string too long (16 max)");
  }
}

void setMode (int fermenter, int mode) {
  if (mode == OFF) {
    myFermenter[fermenter].config.mode = OFF;
    Serial.println("set");
  } else if (mode == CHILL) {
    myFermenter[fermenter].config.mode = CHILL;
    Serial.println("set");
  } else if (mode == HEAT) {
    myFermenter[fermenter].config.mode = HEAT;
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
    Serial.println("set");
  } else {
    Serial.println("out of range (32 to 212)");
  }
}

void setHysteresis(int fermenter, float hysteresis) {
  if (hysteresis >= 0.1 && hysteresis <= 1.0) {
    myFermenter[fermenter].config.hysteresis = hysteresis;
    saveConfig();
    Serial.println("set");
  } else {
    Serial.println("out of range (0.1 to 1.0)");
  }
}

void setRampStep(int fermenter, float rampStep) {
  if (rampStep >= -1.0 && rampStep <= 1.0) {
    myFermenter[fermenter].config.rampStep = rampStep;
    saveConfig();
    Serial.println("set");
  } else {
    Serial.println("out or range (-1.0 to 1.0)");
  }
}

void setRampPoint(int fermenter, float rampPoint) {
  if (rampPoint >= 32.0 && rampPoint <= 212.0) {
    myFermenter[fermenter].config.rampPoint = rampPoint;
    saveConfig();
    Serial.println("set");
  } else {
    Serial.println("out or range (0.1 to 1.0)");
  }
}

void setRampDelay(int fermenter, unsigned long rampDelay) {
  if (rampDelay >= 3600000 && rampDelay <= 86400000) {
    myFermenter[fermenter].config.rampDelay = rampDelay;
    saveConfig();
    Serial.println("set");
  } else {
    Serial.println("out or range (3600000 to 86400000)");
  }
}

void setPumpRun(int fermenter, unsigned long pumpRun) {
  if (pumpRun >= 1000 && pumpRun <= 60000) {
    myFermenter[fermenter].config.pumpRun = pumpRun;
    saveConfig();
    Serial.println("set");
  } else {
    Serial.println("out of range (1000 to 60000)");
  }
}

void setPumpDelay(int fermenter, unsigned long pumpDelay) {
  if (pumpDelay >= 10000 && pumpDelay <= 600000) {
    myFermenter[fermenter].config.pumpDelay = pumpDelay;
    saveConfig();
    Serial.println("set");
  } else {
    Serial.println("out of range (60000 to 600000)");
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

void setupSensors() {
  sensors.begin();
  sensors.setWaitForConversion(WAIT_FOR_CONVERSION);
  sensors.setResolution(RESOLUTION);
}

void setupPumps() {
  int i;
  for ( i=0 ; i<=1 ; i++) {
    pinMode(PUMP[i], OUTPUT);
    offPump(i);
  }
}

void loopSensors() {
  if (mySensorStateMachine.sensors == 0) {
    requestTemperatures();
  } else if ((millis() - mySensorStateMachine.sensors) >= SENSOR_DELAY) {
    getTemperatures();
  }
}

void requestTemperatures() {
  sensors.requestTemperatures();
  mySensorStateMachine.sensors = millis();
}

void getTemperatures() {
  int i;
  for ( i=0 ; i<=1 ; i++ ) {
    myFermenter[i].temperature = sensors.getTempF(myFermenter[i].deviceAddress);
  }
  mySensorStateMachine.sensors = 0;
}

void loopFermenter(int fermenter) {
  if (myFermenter[fermenter].stateMachine.pumpDelay == 0) {
    if (myFermenter[fermenter].stateMachine.pump == 0) {
      if ((myFermenter[fermenter].config.mode == CHILL && ((myFermenter[fermenter].temperature-myFermenter[fermenter].config.setpoint) >= myFermenter[fermenter].config.hysteresis)) ||
          (myFermenter[fermenter].config.mode == HEAT  && ((myFermenter[fermenter].temperature-myFermenter[fermenter].config.setpoint) <= (-myFermenter[fermenter].config.hysteresis)))) {
        onPump(fermenter);
      }
    } else {
      if ((millis() - myFermenter[fermenter].stateMachine.pump) >= myFermenter[fermenter].config.pumpRun) {
        offPump(fermenter);
      }
    }
  } else if ((millis() - myFermenter[fermenter].stateMachine.pumpDelay) >= myFermenter[fermenter].config.pumpDelay) {
    myFermenter[fermenter].stateMachine.pumpDelay = 0;
  }
  if (myFermenter[fermenter].config.rampStep != 0.0) {
    if (myFermenter[fermenter].stateMachine.rampDelay == 0) {
      myFermenter[fermenter].config.setpoint += myFermenter[fermenter].config.rampStep;
      if (abs(myFermenter[fermenter].config.rampPoint-myFermenter[fermenter].config.setpoint) < abs(myFermenter[fermenter].config.rampStep)){
        myFermenter[fermenter].config.setpoint = myFermenter[fermenter].config.rampPoint;
        myFermenter[fermenter].config.rampStep = 0;
      }
      saveConfig();
      myFermenter[fermenter].stateMachine.rampDelay = millis();
    } else if ((millis() - myFermenter[fermenter].stateMachine.rampDelay) >= myFermenter[fermenter].config.rampDelay) {
      myFermenter[fermenter].stateMachine.rampDelay = 0;
    }
  }
}

void offPump(int fermenter) {
  myFermenter[fermenter].stateMachine.pump = 0;
  myFermenter[fermenter].stateMachine.pumpDelay = millis();
  digitalWrite(PUMP[fermenter], HIGH);
}

void onPump(int fermenter) {
  myFermenter[fermenter].stateMachine.pump = millis();
  myFermenter[fermenter].stateMachine.pumpDelay = 0; 
  digitalWrite(PUMP[fermenter], LOW);
}


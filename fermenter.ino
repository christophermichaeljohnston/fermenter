#define VERSION 0.4
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
#define CHILL 0
#define HEAT  1
#define SLOW  0
#define FAST  1
struct fermenterConfig {
  char name[17]     = "";
  int mode        = CHILL;
  int speed       = SLOW;
  float setpoint   = 64.0;
  float hysteresis = 0.1;
  long pumpRun     = 5000;
  long pumpDelay   = 60000;
};
struct fermenterStateMachine {
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
  setupConfig();
  setupSensors();
  setupPump();
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
  //runCommand(cmd, param);
}

/*
void runCommand(char * cmd, char * param) {
  if (strcmp(cmd,"getVersion") == 0) {
    Serial.println(myConfig.version);
  } else if (strcmp(cmd,"getType") == 0) {
    Serial.println(TYPE);
  } else if (strcmp(cmd,"getSN") == 0) {
    Serial.println(myConfig.sn);
  } else if (strcmp(cmd,"getTag") == 0) {
    Serial.println(myConfig.tag);
  } else if (strcmp(cmd, "getMode") == 0) {
    Serial.println(myConfig.mode);
  } else if (strcmp(cmd,"getSetpoint") == 0) {
    Serial.println(myConfig.setpoint);
  } else if (strcmp(cmd,"getHysteresis") == 0) {
    Serial.println(myConfig.hysteresis);
  } else if (strcmp(cmd,"getPumpRun") == 0) {
    Serial.println(myConfig.pumpRun);
  } else if (strcmp(cmd,"getPumpDelay") == 0) {
    Serial.println(myConfig.pumpDelay);
  } else if (strcmp(cmd,"getTemperature") == 0) {
    Serial.println(myFermenter.temperature);
  } else if (strcmp(cmd,"getFailsafe") == 0) {
    Serial.println(myFermenter.failsafe);
  } else if (strcmp(cmd,"getDeviceAddress") == 0) {
    printAddress(myFermenter.deviceAddress);
    Serial.println();
  } else if (strcmp(cmd,"setTag") == 0) {
    setTag(param);
  } else if (strcmp(cmd,"setMode") == 0) {
    setMode(param);
  } else if (strcmp(cmd,"setSetpoint") == 0) {
    setSetpoint(atof(param));
  } else if (strcmp(cmd,"setHysteresis") == 0) {
    setHysteresis(atof(param));
  } else if (strcmp(cmd, "setPumpRun") == 0) {
    setPumpRun(atol(param));
  } else if (strcmp(cmd, "setPumpDelay") == 0) {
    setPumpDelay(atol(param));
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

void setTag (char * tag) {
  if (strlen(tag) < 16) {
    strcpy(myConfig.tag, tag);
    saveConfig();
    Serial.println("set");
  } else {
    Serial.println("string too long (16 max)");
  }
}

void setMode (char * mode) {
  if (strcmp(mode,"C") == 0) {
    myConfig.mode = CHILL;
    Serial.println("set");
  } else if (strcmp(mode,"H") == 0) {
    myConfig.mode = HEAT;
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
    Serial.println("set");
  } else {
    Serial.println("out of range (32-212)");
  }
}

void setHysteresis(float hysteresis) {
  if (hysteresis >= 0.1 && hysteresis <= 1.0) {
    myConfig.hysteresis = hysteresis;
    saveConfig();
    Serial.println("set");
  } else {
    Serial.println("out of range (0.1-1.0)");
  }
}

void setPumpRun(long pumpRun) {
  if (pumpRun >= 1000 && pumpRun <= 60000) {
    myConfig.pumpRun = pumpRun;
    saveConfig();
    Serial.println("set");
  } else {
    Serial.println("out of range (1000-60000)");
  }
}

void setPumpDelay(long pumpDelay) {
  if (pumpDelay >= 60000 && pumpDelay <= 600000) {
    myConfig.pumpDelay = pumpDelay;
    saveConfig();
    Serial.println("set");
  } else {
    Serial.println("out of range (60000-600000)");
  }
}
*/

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
  sensors.getAddress(myFermenter[0].deviceAddress, 0);
  sensors.getAddress(myFermenter[1].deviceAddress, 1);
}

void setupPump() {
  int i;
  for ( i=0 ; i<=1 ; i++) {
    pinMode(i, OUTPUT);
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


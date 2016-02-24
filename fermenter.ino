#define VERSION 0.1
#define TYPE "FERMENTER"

#include <string.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//
// OneWire Definitions and Globals
//
#define ONE_WIRE 2
#define RESOLUTION 12
#define WAIT_FOR_CONVERSION false
#define SENSOR_DELAY 1000
OneWire oneWire(ONE_WIRE);
DallasTemperature sensors(&oneWire);

//
// Pump Definitions
//
#define PUMP 4

//
// state machine tracking
//
struct stateMachine {
  unsigned long sensors   = 0;
  unsigned long pump      = 0;
  unsigned long pumpDelay = 0;
} myStateMachine;

//
// configuration with defaults
//
#define CHILL 0
#define HEAT 1
struct config {
  char id[16]      = "";
  int mode         = CHILL;
  float setpoint   = 64.0;
  float hysteresis = 0.1;
  long pumpRun     = 5000;
  long pumpDelay   = 60000;
  float version    = VERSION;
} myConfig;

//
// ferementer
//
struct fermenter {
  DeviceAddress deviceAddress;
  float temperature = 0.0;
  boolean failsafe = false;
} myFermenter;

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
  setupConfig();
  setupSerial();
  setupSensors();
  setupPump();
  Serial.println("Ready...");
}

void loop() {
  loopSensors();
  loopPump();
}

void setupConfig() {
  config mem;
  EEPROM.get(0, mem);
  if (mem.version == VERSION) {
    myConfig = mem;
  } else {
    for (int i=0 ; i<16 ; i++) {
      randomSeed(analogRead(0));
      myConfig.id[i] = 'a'+random(26);
    }
    saveConfig();
  }
}

void saveConfig() {
  EEPROM.put(0, myConfig);
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
  runCommand(cmd, param);
}

void runCommand(char * cmd, char * param) {
  if (strcmp(cmd,"getVersion") == 0) {
    Serial.println(myConfig.version);
  } else if (strcmp(cmd,"getType") == 0) {
    Serial.println(TYPE);
  } else if (strcmp(cmd,"getID") == 0) {
    Serial.println(myConfig.id);
  } else if (strcmp(cmd, "getMode") == 0) {
    if (myConfig.mode == CHILL) {
      Serial.println("CHILL");
    } else {
      Serial.println("HEAT");
    }
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
  } else if (strcmp(cmd,"setID") == 0) {
    setID(param);
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

void setID (char * id) {
  if (strlen(id) < 16) {
    strcpy(myConfig.id, id);
    saveConfig();
  }
}

void setMode (char * mode) {
  if (strcmp(mode,"CHILL") == 0) {
    myConfig.mode = CHILL;
  } else if (strcmp(mode,"HEAT") == 0) {
    myConfig.mode = HEAT;
  }
  saveConfig();
}

void setSetpoint(float setpoint) {
  if (setpoint >= 32.0 && setpoint <= 212.0) {
    myConfig.setpoint = setpoint;
    saveConfig();
  }
}

void setHysteresis(float hysteresis) {
  if (hysteresis >= 0.1 && hysteresis <= 1.0) {
    myConfig.hysteresis = hysteresis;
    saveConfig();
  }
}

void setPumpRun(long pumpRun) {
  if (pumpRun >= 1000 && pumpRun <= 60000) {
    myConfig.pumpRun = pumpRun;
    saveConfig();
  }
}

void setPumpDelay(long pumpDelay) {
  if (pumpDelay >= 60000 && pumpDelay <= 600000) {
    myConfig.pumpDelay = pumpDelay;
    saveConfig();
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
  sensors.getAddress(myFermenter.deviceAddress, 0);
}

void setupPump() {
  pinMode(PUMP, OUTPUT);
  offPump();
}

void loopSensors() {
  if (myStateMachine.sensors == 0) {
    requestTemperatures();
  } else if ((millis() - myStateMachine.sensors) >= SENSOR_DELAY) {
    getTemperatures();
  }
}

void requestTemperatures() {
  sensors.requestTemperatures();
  myStateMachine.sensors = millis();
}

void getTemperatures() {
  myFermenter.temperature = sensors.getTempF(myFermenter.deviceAddress);
  myStateMachine.sensors = 0;
}

void loopPump() {
  //Serial.println((myConfig.mode == HEAT  && ((myFermenter.temperature - myConfig.setpoint) <= (myConfig.hysteresis*2))));
  if (myStateMachine.pumpDelay == 0) {
    if ((myConfig.mode == CHILL && ((myFermenter.temperature - myConfig.setpoint) >= (myConfig.hysteresis*2))) || 
        (myConfig.mode == HEAT  && ((myFermenter.temperature - myConfig.setpoint) <= (myConfig.hysteresis*2)))) {
        myFermenter.failsafe = true;
        onPump();
    }
    if (myFermenter.failsafe) {
      if ((myConfig.mode == CHILL && (myFermenter.temperature - myConfig.setpoint) <= 0.0) ||
          (myConfig.mode == HEAT  && (myFermenter.temperature - myConfig.setpoint) >= 0.0)) {
        myFermenter.failsafe = false;
        offPump();
      }
    } else {
      if (myStateMachine.pump == 0) {
        if ((myConfig.mode == CHILL && ((myFermenter.temperature - myConfig.setpoint) >= myConfig.hysteresis)) ||
            (myConfig.mode == HEAT  && ((myFermenter.temperature - myConfig.setpoint) <= myConfig.hysteresis))) {
          onPump();
        }
      } else {
        if ((millis() - myStateMachine.pump) >= myConfig.pumpRun) {
          offPump();
        }
      }
    }
  } else if ((millis() - myStateMachine.pumpDelay) >= myConfig.pumpDelay) {
    myStateMachine.pumpDelay = 0;
  }
}

void offPump() {
  myStateMachine.pump = 0;
  myStateMachine.pumpDelay = millis();
  digitalWrite(PUMP, HIGH);
}

void onPump() {
  myStateMachine.pump = millis();
  myStateMachine.pumpDelay = 0; 
  digitalWrite(PUMP, LOW);
}

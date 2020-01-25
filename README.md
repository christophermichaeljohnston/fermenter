# fermenter

Requires an Arduino, 2x DS18B20 temperature sensors, and something the ardunio can trigger for chill and heat cycles. For an SSBrewtech fermenter (brew bucket, unitank) that would be a pump, heating pad, power supply, and a 2 channel relay (or SSR).

One temperature sensor is for measuring internal temperature (thermowell) and one is for external temperature (heat pad or ambient).

Controllable either through the Arduino IDE serial port, USB connected to a Raspberry PI that is running the brewery frontend, or your own serial implementation.

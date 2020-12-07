# Zirkulationspumpen-Steuerung
This is an Arduino project using an ESP32 board to switch a crculation pump depeneding on the 
warm water temperature, so that it is not needed to run the pump all the time.
As most of these kind of pumps are not designed to be shut off for a longer 
time (which causes the motor to hang. e.g. Wilo Star Z Nova) a "keep alive" function is also included
to avoid this. 

# Arduino environment monitor
Arduino based themperature monitor with sms notifications.
## Hardware
- Arduino Leonardo
- SIM 900A GSM module
- 4 x Dallas OneWire thermal Sensors
- DS3232 RTC clock

## Logic
Sketch periodicaly checking for themperature, if current measurings exeeds defined thesholds sms notification is composed and sended to speceifed arrays of phone numbers. 
After sending message GSM module make a call to pass trough "Do not distrub" mode on modern smartphones.

In SMS program trying to calculate some statistical information about measurements, like an:

- average temperatures for 3 time periods (5 mins, 10 mins, 60 mins)
- temperature changing speed
- forecast of elasped time to critical temperature

### Failover
- SMS message is sending once a hour;
- Making call once a hour;
- GMS network checking;
- Thermo sensors working checking;

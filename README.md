# Arduino environment monitor
Arduino based themperature monitor with sms notifications.
## Hardware
- Arduino Leonardo ![Arduino image] (https://www.arduino.cc/en/uploads/Main/LeonardoNoHeadersFront_2_450px.jpg)
- SIM 900A GSM module ![SIM 900A GSM image ](/images/SIM900A.jpg)
- 4 x Dallas OneWire thermal Sensors ![Dallas image](/images/SV003332-G.jpg)
- DS3232 RTC clock	![DS3232 image](/images/DS3231.jpg)

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

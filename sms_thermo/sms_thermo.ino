#include <DS3232RTC.h>
#include <Time.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include <math.h>

// TODO: Запилить журнал ошибок, учитывать время которое показатели были ниже нормы. Ежедневный отчет по ошибкам, или еженедельный. СМС меню.

SoftwareSerial SIM900 (10, 11);                   //SIM900 is connected to 10 & 11 PINs RX, TX
#define ONE_WIRE_BUS 13                           //Dallas OneWire sensors is connected to 13 PIN

String phone_number[]   = {"+79000000000"};       // Phone numbers for SMS notifications
int HighAlarmTemp[]     = {26, 26, 26, 26};       // Highest temperature when user is notificated
int DestroyAlarmTemp[]  = {30, 30, 30, 30};       // Temperature used to make forecast
int TargetTemp[]        = {24, 24, 24, 24};       // Temperature used to make forecast
int LowAlarmTemp[]      = { -28, -28, -28, -28};  // Lowest thmperature when user is notificated
int cycleDelay          = 3000;
// Set total connected sensors

String textForSMS;
String last_alarm_time;

int last_call_hour;
int last_message_hour;

int monPeriodLen[3] = {600, 3600, 21600};                              // 10 minutes // 60 minutes // 12 hours


float gsm_signal;
float tempSumm[3][4];
float tempTrend[3][4];
int   tempAvgCur[3][4];
int   tempAvgPrev[3][4];
int   measuresCount[3];
int   measuresPerPeriod[3];
long  startOfPeriod[3];


boolean call_done;
boolean message_done;
boolean armed;
tmElements_t tm;


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress thermalSensor[1];                   //Set number of Thermal Sensors Connected


void setup()
{
  Serial.begin(9600);                             //Serial debug console initialize at 9k
  Wire.begin();

  SIM900.begin(57600);                            //SIM900 console initialize at 56k
  sensors.begin();                                //sensors initialize;

  // initial value setting
  for (int i; i < 3; i++)
  {
    startOfPeriod[i] = now ();
    measuresPerPeriod[i] = monPeriodLen[i] / (cycleDelay / 1000);
    tempAvgPrev[0][i] = sensors.getTempC(thermalSensor[i]);
  }


  call_done = false;
  // mode select
  armed = false;                                  //GSM activity


  SIM900.println("AT");                           //Sending "AT" to modem for auto baud rate sensing;

  Serial.print("[boot] SIM900 ----- ");
  if (getSIM900status(false))
  {
    Serial.println("connected");
  }
  else
  {
    Serial.println("ERROR!");
  }

  // SIM900 serial console initialize and hanging off call (just for shure);
  delay(1000);

  Serial.print("[boot] SIM card ----- ");
  if (getSimCardStatus(false))
  {
    Serial.println("OK");
  }
  else
  {
    Serial.println("ERROR!");
  }

  Serial.print("[boot] Registering in Network  ----- ");
  for (int i = 0; i <= 10; i++)
  {
    if (getNetworkStatus(false))
    {
      i = 100;
      Serial.println(toProgressBar(2, 30, gsm_signal));
    }
    Serial.print("-");
    delay(1000);
  }
  if (getNetworkStatus(false))
  {
    Serial.print("ERROR!");
  }
  Serial.println();
  delay(1000);
  SIM900.println("ATH");        // hanging off call (just for shure);
  delay(1000);
  SIM900.println("AT+CMGD=1,4"); // delete all SMS
  delay(1000);

  Serial.print("[boot] Syncing with RTC clock  ----- ");
  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  if (timeStatus() != timeSet)
    Serial.println("ERROR!");
  else
    Serial.println("OK");

  // locate termal sensors on the bus

  Serial.print("[boot] Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  // search for devices on the bus and assign based on an index.
  for (int sensor = 0; sensor < sensors.getDeviceCount(); sensor++)
  {
    if (!sensors.getAddress(thermalSensor[sensor], sensor))
    {
      Serial.print("[boot] Unable to find address for Device:"); Serial.print(sensor);
    }

    Serial.print("[boot] Sensor #"); Serial.print(sensor);
    Serial.print(": "); Serial.println(getAddress(thermalSensor[sensor]));
    sensors.setHighAlarmTemp (thermalSensor[sensor], HighAlarmTemp[sensor]);          // alarm when temp is higher than HighAlarmTemp[i]
    sensors.setLowAlarmTemp(thermalSensor[sensor], LowAlarmTemp[sensor]);             // alarm when temp is lower than LowAlarmTemp[i]
    Serial.print("[boot] Alarms:");  Serial.println(getAlarms(thermalSensor[sensor]));
    for (int period; period < 3; period++)
    {
      tempSumm[period][sensor] = sensors.getTempC(thermalSensor[sensor]);
    }
  }
}
// get sim900 module status
boolean getSIM900status(boolean debug)
{
  boolean sim900status = false;
  String readString;
  SIM900.println("AT"); // check SIM900 status
  while (SIM900.available() > 0)
  { char inchar = SIM900.read();
    readString += inchar;
    delay(10);
  }
  if (debug) {
    Serial.print("[DBG] getSIM900status:");
    Serial.print(readString);
  }
  for (int i = 0; i < 200; i++)
  {
    if (readString.substring(i, i + 2) == "OK")
    {
      sim900status = true;
      return sim900status;
      break;
    }
    if (debug) {
      Serial.print("FOUND:");
      Serial.println(readString.substring(i, i + 5));
    }
  }
  return sim900status;
}

// get SIM card in SIM900 module status
boolean getSimCardStatus(boolean debug)
{
  boolean sim900status = false;
  String readString;
  SIM900.println("AT+CPIN?"); // check SIM card status
  while (SIM900.available() > 0)
  { char inchar = SIM900.read();
    readString += inchar;
    delay(10);
  }

  if (debug) {
    Serial.print("[DBG] getSimCardStatus:");
    Serial.print(readString);
  }

  for (int i = 0; i < 200; i++)
  {
    if (readString.substring(i, i + 5) == "READY")
    {
      sim900status = true;

      if (debug) {
        Serial.print("FOUND:");
        Serial.println(readString.substring(i, i + 5));
      }
      break;
    }
  }
  return sim900status;
}

// get and update network db rate from SIM900 module
boolean getNetworkStatus(boolean debug)
{
  boolean sim900status = false;
  String readString;

  SIM900.println("AT+CSQ"); // check SIM card status
  while (SIM900.available() > 0)
  { char inchar = SIM900.read();
    readString += inchar;
    delay(10);
  }
  if (debug) {
    Serial.print("[DBG] getNetworkStatus:");
    Serial.print(readString);
  }
  for (int i = 0; i < 200; i++)
  {
    if (readString.substring(i, i + 5) == "+CSQ:")
    {
      sim900status = true;
      if (debug) {
        Serial.print("FOUND:"); Serial.println(readString.substring(i, i + 10));
      }
      gsm_signal = readString.substring(i + 5, i + 10).toFloat();
      break;
    }
  }
  return sim900status;
}

//return fancy progress bar in string
String toProgressBar(int minV, int maxV, int currV)
{
  String result;
  int len = 10;
  int absV = maxV - minV;

  float mPos;
  mPos = ((float)currV - (float)minV) / (float)absV;
  int nPos = round(len * mPos);
  result = String("[");
  for (int i = 1; i <= len; i++)
  {
    if (i <= nPos)
    {
      result = String(result) + String("#");
    }
    else
    {
      result = String(result) + String("-");
    }
  }
  result = String(result) + String("]");
  return String(result);
}

// get a termal-sensors device address
String getAddress(DeviceAddress deviceAddress)
{
  String datastring;
  for (uint8_t i = 0; i < 4; i++)
  {
    if (deviceAddress[i] < 16) datastring = String("0");
    datastring = String(deviceAddress[i], HEX);
  }
  return datastring;
}

// get a assigned alarms on devices
String getAlarms(uint8_t deviceAddress[])
{

  String datastring = String ("<");
  datastring = datastring + String(sensors.getLowAlarmTemp(deviceAddress), DEC);
  datastring = datastring + String(", >");
  datastring = datastring + String(sensors.getHighAlarmTemp(deviceAddress), DEC);
  return datastring;
}

// get time in short format
String getShortTimeString ()
{
  String shtime;
  shtime = String(shtime + hour());
  shtime = String(shtime + ":");
  shtime = String(shtime + minute());
  shtime = String(shtime + ":");
  shtime = String(shtime + second());
  return shtime;
}

// update statistical information
void updateStats(boolean debug)
{

  for (int period = 0; period < 3; period++)
  {
    for (int sensor = 0; sensor < sizeof(thermalSensor) / sizeof(DeviceAddress); sensor++)
    {
      if (now() - startOfPeriod[period] <= monPeriodLen[period])
      {
        measuresCount[period]++;
        tempSumm[period][sensor] = tempSumm[period][sensor] + sensors.getTempC(thermalSensor[sensor]); // Calcluating summ of temperature vaules for average calculation
      }
      else
      {
        startOfPeriod[period] = now();
        tempAvgPrev[period][sensor] = tempAvgCur[period][sensor];

        tempAvgCur[period][sensor] = round(tempSumm[period][sensor] / measuresCount[period]); // Calculating average value for period 1;
        tempTrend[period][sensor] = (tempAvgCur[period][sensor] - tempAvgPrev[period][sensor]) / measuresCount[period];

        tempSumm[period][sensor] = 0;
        measuresCount[period] = 0;
        if (debug)
        {
          Serial.print("[DBG] TREND "); Serial.print(sensor); Serial.print(":"); Serial.println(tempTrend[period][sensor]);
          makeForecast(sensor, true);
        }
      }
      measuresCount[period] = 1;


    }
  }
  if (debug) {

    for (int sensor = 0; sensor < sizeof(thermalSensor) / sizeof(DeviceAddress); sensor++)
    {
      Serial.print("TEMP"); Serial.print(sensor); Serial.print(":");
      Serial.print(sensors.getTempC(thermalSensor[sensor]));
      for (int period = 0; period < 3; period++)
      {
        Serial.print("/"); Serial.print(tempAvgCur[period][sensor]);
      }
      Serial.println();
    }
  }
}

// get information about a sensors
String getData(DeviceAddress deviceAddress)
{
  String datastring;
  datastring = String(datastring + "addr: ");
  datastring = String(datastring + getAddress(deviceAddress));
  datastring = String(datastring + " t:");
  datastring = String(datastring + sensors.getTempC(deviceAddress));
  return datastring;
}

// compose alarm message for SMS sending
String makeMessage(int sensor)
{
  int timeToDeath =  makeForecast(sensor, false);
  int minToDeath = timeToDeath / 60;
  int secToDeath = timeToDeath - minToDeath * 60;

  String datastring;
  datastring = getShortTimeString ();
  datastring = datastring + String(" ");
  datastring = datastring + String("Overheat");
  datastring = datastring + String("(") + getData(thermalSensor[sensor]) + String(")");
  datastring = datastring + String("[");
  for (int period; period < 3; period++)
  {
    datastring = datastring + String(tempAvgCur[period][sensor]) + String("/");
  }
  datastring = datastring + String("Death after: ") + minToDeath + String(":") + secToDeath;
  Serial.println(datastring);
  return datastring;

}

// check for active alarms on device
boolean checkAlarm(int deviceid)
{

  if (sensors.hasAlarm(thermalSensor[deviceid]))
  {
    last_alarm_time = getShortTimeString();
    Serial.print(last_alarm_time);
    Serial.print(" ALARM: ");
    Serial.println(getData(thermalSensor[deviceid]));
    if (armed)
    {
      for (int i = 0; i < sizeof(phone_number) / sizeof (String); i++)
      {
        if (!call_done)
        {
          Serial.print("Sending SMS:");
          {
            message_done =  sendSMS(phone_number[i], makeMessage(deviceid));
          }
          last_message_hour =  hour();
        } else
        {
          if (last_message_hour != hour())
          {
            Serial.println("Send delay expired ");
            message_done = false;
          }
        }

        if (!call_done)
        {

          Serial.print("Calling: ");
          call_done = makecall(phone_number[i]);

          last_call_hour =  hour();
        } else
        {
          if (last_call_hour != hour())
          {
            Serial.println(" Callhome delay expired ");
            call_done = false;
          }
        }


      }
    }

  } else {
    //    Serial.print(".");
  }
}


// forecast in seconds elasped time to DestroyAlarmTemp[]
int makeForecast(int sensor, bool debug)
{
  float forecastSeconds ;
  float forecastMinutes ;
  float delta_temp;
  float cur_temp = sensors.getTempC(thermalSensor[sensor]);

  // Serial.print("[mF] Cur_temp:"); Serial.println(cur_temp);

  if (tempTrend[0][sensor] < 0 )
  { //recovery time calculation
    delta_temp = cur_temp - TargetTemp[sensor];
    //Serial.print("[mF] Trend < 0. delta_temp:"); Serial.println(delta_temp);

  }

  if (tempTrend[0][sensor] > 0 )
  { 
     delta_temp = DestroyAlarmTemp[sensor] - cur_temp;
    // Serial.print("[mF] Trend > 0. delta_temp:"); Serial.println(delta_temp);
  }

  forecastSeconds = delta_temp / (tempTrend[0][sensor] / monPeriodLen[0]);
  forecastMinutes = forecastSeconds / 60;
  //Serial.print(delta_temp); Serial.print("/"); Serial.print(tt1mTrend[deviceid]); Serial.print("="); Serial.println(forecastSeconds);
  if (debug)
  {
    Serial.print("[DBG] "); Serial.print(timeToHuman(forecastSeconds)); Serial.print(" till ");
    if (tempTrend[0][sensor] < 0 )
    {
      Serial.println(TargetTemp[sensor]);
    }
    else {
    if (tempTrend[0][sensor] > 0 )
    {
      Serial.println(DestroyAlarmTemp[sensor]);
    }
    else
    {
      Serial.println("NAN");
    }}
  }
  return int(round(forecastSeconds));


}

String timeToHuman (unsigned long inttime)
{

   unsigned long hours, mins, secs;
   String timeString;


   // Now, inttime is the remainder after subtracting the number of seconds
   // in the number of days
   hours    = inttime / 3600;
   inttime  = inttime % 3600;

   // Now, inttime is the remainder after subtracting the number of seconds
   // in the number of days and hours
   mins     = inttime / 60;
   inttime  = inttime % 60;

   // Now inttime is the number of seconds left after subtracting the number
   // in the number of days, hours and minutes. In other words, it is the
   // number of seconds.
   secs = inttime;

   // Don't bother to print days

   timeString =  String(hours);
   timeString = timeString + String(":");
   timeString = timeString + String(mins);
   timeString = timeString + String("."); 
   timeString = timeString + String(secs);      
   return timeString;
  
}


// proceed a call just for make a loud sound in the middle of the "do not distrub" mode on smartphone
boolean makecall(String number)
{
  SIM900.print("ATD");
  SIM900.print(number);
  Serial.println(number);
  SIM900.println(";");
  delay(100);
  SIM900.println();
  delay(60000);            // wait for 30 seconds...
  SIM900.println("ATH");   // hang up
  Serial.print(" done");
  return true;
}

// proceed a sending message to number
boolean sendSMS(String number, String message)
{
  SIM900.print("AT+CMGF=1\r");                              // AT command to send SMS message
  delay(100);
  SIM900.print("AT + CMGS = \"");
  SIM900.print(number);
  Serial.println(number);
  SIM900.println("\"");                                     // recipient's mobile number, in international format
  delay(100);
  SIM900.println(message);                                  // message to send
  Serial.println(message);
  delay(100);
  SIM900.println((char)26);                                 // End AT command with a ^Z, ASCII code 26
  delay(100);
  SIM900.println();
  delay(30000);                                             // give module time to send SMS
  return true;
}


String readSMS(int smsID, bool debug)
{
  bool isMsgExists;
  String readString;
  String resultString;

  SIM900.print("AT+CMGR=2AT+CMGR="); SIM900.println(smsID);
  while (SIM900.available() > 0)
  { char inchar = SIM900.read();
    readString += inchar;
    delay(10);
  }
  if (debug) {
    Serial.print("[DBG] readSMSreadSMS AT+CMGR=2AT+CMGR=AT+CMGR=2AT+CMGR= return:");
    Serial.print(readString);
    readString = "";
  }

  for (int i = 0; i < 200; i++)
  {
    for (int j = 0; j < sizeof(phone_number); j++)
    {
      if (readString.substring(i, i + 12) == phone_number[j])
      {
        isMsgExists = true;
        if (debug) {
          Serial.print("FOUND:");
          Serial.println(readString.substring(i, i + 12));
        }
      }
      if (isMsgExists)
      {
        for (int k = i; k < 200 - i; k++)
        {
          if (readString.substring(k, k + 4) == "CMD:")
          {
            resultString = String(readString.substring(k + 4));
            if (debug) {
              Serial.print("COMMAND:");
              Serial.println(readString.substring(k + 4));
            }
          }
        }
      }
    }
  }
  return readString;
}

void execRemoteCommand ()
{
  String cmdLine;
  for (int i = 1; i <= 15; i++)
  {
    cmdLine = String(readSMS(i, true));
    Serial.println(cmdLine);
  }
}

// main loop
void loop()
{

  sensors.requestTemperatures(); // Send the command to get temperatures
  updateStats(false);

  for (int i = 0; i < sizeof(thermalSensor) / sizeof(DeviceAddress); i++)
  {
    Serial.println(getData(thermalSensor[i]));
    Serial.print("TREND "); Serial.print(i); Serial.print(":"); Serial.println(tempTrend[0][i]);
    makeForecast(i, true);
  }

  //execRemoteCommand();
  //printAlarms(t0); // Вывод пороговых значений на экран
  delay(cycleDelay);  // задержка проверки
  //getNetworkStatus(false); Serial.println(toProgressBar(2, 30, gsm_signal)); // Network signal quality print
}

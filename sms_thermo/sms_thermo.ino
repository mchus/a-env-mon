#include <DS3232RTC.h>
#include <Time.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include <math.h>

// TODO: Запилить журнал ошибок, учитывать время которое показатели были ниже нормы. Ежедневный отчет по ошибкам, или еженедельный. СМС меню.

SoftwareSerial SIM900 (10, 11); //SIM900 is connected to 10 & 11 PINs RX, TX
#define ONE_WIRE_BUS 13         //Dallas OneWire sensors is connected to 13 PIN

String phone_number[] = {"+79000000000"}; // Phone number for SMS notification
int HighAlarmTemp[] = {28, 28, 28, 28};     // Highest temperature when user is notificated
int DestroyAlarmTemp[] = {60, 60, 60, 60};  // Temperature used to make forecast
int LowAlarmTemp[] = { -28, -28, -28, -28}; // Lowest thmperature when user is notificated

String textForSMS;
String last_alarm_time;

int last_call_hour;
int last_message_hour;

int monPeriod1 = 300; // 5 minutes
int monPeriod2 = 600; // 10 minutes
int monPeriod3 = 1800;// 60 minutes

long count_t1, count_t2, count_t3;
long startOfPeriod1, startOfPeriod2, startOfPeriod3;


float gsm_signal = 0;
float tt1m[4], tt2m[4], tt3m[4];
float tt1mLast[4], tt2mLast[4], tt3mLast[4];
float tt1mTrend[4], tt2mTrend[4], tt3mTrend[4];
int tt1m_avg[4], tt2m_avg[4], tt3m_avg[4];

boolean call_done;
boolean message_done;
boolean armed;
tmElements_t tm;


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress thermalSensor[2];


void setup()
{
  Serial.begin(9600);   //Serial debug console initialize at 9k
  Wire.begin();

  SIM900.begin(57600);  //SIM900 console initialize at 56k
  sensors.begin();      //sensors initialize;

  startOfPeriod1 = now();
  startOfPeriod2 = now();
  startOfPeriod3 = now();

  call_done = false;
  armed = false;
  SIM900.println("AT"); //Sending "AT" to modem for auto baud rate sensing;

  if (getSIM900status(false))
  {
    Serial.println("[boot] SIM900 connected");
  }
  else
  {
    Serial.println("[boot] SIM900 missing");
  }

  // SIM900 serial console initialize and hanging off call (just for shure);
  delay(1000);


  if (getSimCardStatus(false))
  {
    Serial.println("[boot] SIM card: OK");
  }
  else
  {
    Serial.println("[boot]  SIM card: missing");
  }

  Serial.print("[boot] Registering in Network");
  for (int i = 0; i <= 100; i++)
  {
    if (getNetworkStatus(false))
    {
      i = 100;
      Serial.print(": DONE");
      Serial.print(" Signal: ");
      Serial.println(toProgressBar(2,30,gsm_signal));
    }
    Serial.print(".");
    delay(100);
  }
if (getNetworkStatus(false))
{
  Serial.println("ERROR!");
}

delay(1000);
SIM900.println("ATH");        // hanging off call (just for shure);
delay(1000);
SIM900.println("AT+CMGD=1,4"); // delete all SMS
delay(1000);

setSyncProvider(RTC.get);   // the function to get the time from the RTC
if (timeStatus() != timeSet)
  Serial.println("[boot] Unable to sync with the RTC");
else
  Serial.println("[boot] RTC has set the system time");

// locate termal sensors on the bus

Serial.print("[boot] Found ");
Serial.print(sensors.getDeviceCount(), DEC);
Serial.println(" devices.");

// search for devices on the bus and assign based on an index.
for (int i = 0; i < sensors.getDeviceCount(); i++)
{
  if (!sensors.getAddress(thermalSensor[i], i))
  {
    Serial.print("[boot] Unable to find address for Device:"); Serial.print(i);
  }

  Serial.print("[boot] Sensor #"); Serial.print(i);
  Serial.print(": "); Serial.println(getAddress(thermalSensor[i]));
  sensors.setHighAlarmTemp (thermalSensor[i], HighAlarmTemp[i]);          // alarm when temp is higher than HighAlarmTemp[i]
  sensors.setLowAlarmTemp(thermalSensor[i], LowAlarmTemp[i]);             // alarm when temp is lower than LowAlarmTemp[i]
  Serial.print("[boot] Alarms:");  Serial.println(getAlarms(thermalSensor[i]));
  tt1m[i] = sensors.getTempC(thermalSensor[i]);
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
      gsm_signal = readString.substring(i+5, i + 10).toFloat();
      break;
    }
  }
  return sim900status;
}

String toProgressBar(int minV, int maxV, int currV)
{
  String result;
  int len = 10;
  int absV = maxV - minV;

  float mPos;
  mPos = ((float)currV - (float)minV) / (float)absV;
  int nPos = round(len * mPos);
  result = String("[");
  for (int i=1;i<=len;i++)
  {
    if (i <= nPos)
    { 
      result = String(result) + String("#");
      }
    else
    {
      result=String(result) + String("-");
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

  if (now() - startOfPeriod1 <= monPeriod1)
  {
    count_t1++;
  }
  else
  { startOfPeriod1 = now();

    for (int i = 0; i < sizeof(thermalSensor) / sizeof(DeviceAddress); i++)
    {
      tt1mTrend[i] = (tt1m[i] - tt1mLast[i]) / count_t1;
      tt1mLast[i] = tt1m[i];
      tt1m[i] = 0;
      //          Serial.print("TREND "); Serial.print(i); Serial.print(":"); Serial.println(tt1mTrend[i]);
      //          Serial.print("Time to death: ");Serial.println( makeForecast(i) );
    }
    count_t1 = 1;
  }

  if (now() - startOfPeriod2 <= monPeriod2)
  {
    count_t2++;
  }
  else
  {
    startOfPeriod2 = now();
    count_t2 = 1;
    for (int i = 0; i < sizeof(thermalSensor) / sizeof(DeviceAddress); i++) {
      tt2m[i] = 0;
    }
  }


  if (now() - startOfPeriod3 <= monPeriod3)
  {
    count_t3++;
  }
  else
  {
    startOfPeriod3 = now();
    count_t3 = 1;
    for (int i = 0; i < sizeof(thermalSensor) / sizeof(DeviceAddress); i++) {
      tt3m[i] = 0;
    }
  }
  for (int i = 0; i < sizeof(thermalSensor) / sizeof(DeviceAddress); i++)
  {

    tt1m[i] = tt1m[i] + sensors.getTempC(thermalSensor[i]);
    tt1m_avg[i] = round(tt1m[i] / count_t1);

    tt2m[i] = tt2m[i] + sensors.getTempC(thermalSensor[i]);
    tt2m_avg[i] = round(tt2m[i] / count_t2);

    tt3m[i] = tt3m[i] + sensors.getTempC(thermalSensor[i]);
    tt3m_avg[i] = round(tt3m[i] / count_t3);

    if (debug) {
      Serial.print("TEMP"); Serial.print(i); Serial.print(":");
      Serial.print(sensors.getTempC(thermalSensor[i]));
      Serial.print("/"); Serial.print(tt1m_avg[i]);
      Serial.print("/"); Serial.print(tt2m_avg[i]);
      Serial.print("/"); Serial.print(tt3m_avg[i]);
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
String makeMessage(int deviceid)
{
  int timeToDeath =  makeForecast(deviceid);
  int minToDeath = timeToDeath / 60;
  int secToDeath = timeToDeath - minToDeath * 60;

  String datastring;
  datastring = getShortTimeString ();
  datastring = datastring + String(" ");
  datastring = datastring + String("Overheat");
  datastring = datastring + String("(") + getData(thermalSensor[deviceid]) + String(")");
  datastring = datastring + String("[") + String(tt1m[deviceid]) + String("/");
  datastring = datastring + String(tt2m[deviceid]) + String("/");
  datastring = datastring + String(tt3m[deviceid]) + String("] ");
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
int makeForecast(int deviceid)
{
  float forecastSeconds ;
  float delta_temp;
  float cur_temp = sensors.getTempC(thermalSensor[deviceid]);
  // Serial.print("[mF] Cur_temp:"); Serial.println(cur_temp);

  if (tt1mTrend[deviceid] < 0 )
  { //recovery time calculation
    delta_temp = cur_temp - HighAlarmTemp[deviceid];
    //Serial.print("[mF] Trend < 0. delta_temp:"); Serial.println(delta_temp);

  }

  if (tt1mTrend[deviceid] > 0 )
  { //Destroy time calculation
    delta_temp = DestroyAlarmTemp[deviceid] - cur_temp;
    // Serial.print("[mF] Trend > 0. delta_temp:"); Serial.println(delta_temp);
  }

  forecastSeconds = delta_temp / tt1mTrend[deviceid];
  //Serial.print(delta_temp); Serial.print("/"); Serial.print(tt1mTrend[deviceid]); Serial.print("="); Serial.println(forecastSeconds);
  return int(round(forecastSeconds));
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

// main loop
void loop()
{

  sensors.requestTemperatures(); // Send the command to get temperatures
  updateStats(false);

  for (int i = 0; i < sizeof(thermalSensor) / sizeof(DeviceAddress); i++)
  {
    checkAlarm(i);
  }

  //printAlarms(t0); // Вывод пороговых значений на экран
  delay(3000);  // задержка проверки
  getNetworkStatus(false);
  Serial.println(toProgressBar(2,30,gsm_signal));

}

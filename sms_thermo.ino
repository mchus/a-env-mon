#include <DS3232RTC.h>
#include <Time.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include <math.h>

SoftwareSerial SIM900 (10, 11); // RX, TX

String phone_number[] = {"+7905XXXXXXX"};
int HighAlarmTemp[] = {28, 28, 28, 28};
int DestroyAlarmTemp[] = {60, 60, 60, 60};
int LowAlarmTemp[] = { -28, -28, -28, -28};

String textForSMS;
String last_alarm_time;

int last_call_hour;
int last_message_hour;

int monPeriod1 = 300; // 5 минут
int monPeriod2 = 600; // 10 минут
int monPeriod3 = 1800;// 60 минут


long count_t1, count_t2, count_t3;
long startOfPeriod1, startOfPeriod2, startOfPeriod3;

float tt1m[4], tt2m[4], tt3m[4];
float tt1mLast[4], tt2mLast[4], tt3mLast[4];
float tt1mTrend[4], tt2mTrend[4], tt3mTrend[4];
int tt1m_avg[4], tt2m_avg[4], tt3m_avg[4];


boolean call_done;
boolean message_done;
boolean armed;
tmElements_t tm;


#define ONE_WIRE_BUS 13

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress thermalSensor[2];


void setup()
{
  Serial.begin(9600);
  Wire.begin();
  SIM900.begin(57600);
  sensors.begin();

  startOfPeriod1 = now();
  startOfPeriod2 = now();
  startOfPeriod3 = now();
  call_done = false;
  armed = true;


  delay(1000);
  SIM900.println("AT");
  delay(1000);
  SIM900.println("ATH");
  delay(1000);

  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  if (timeStatus() != timeSet)
    Serial.println("Unable to sync with the RTC");
  else
    Serial.println("RTC has set the system time");


  // locate devices on the bus
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  // search for devices on the bus and assign based on an index.
  for (int i = 0; i < sensors.getDeviceCount(); i++)
  {
    if (!sensors.getAddress(thermalSensor[i], i))
    {
      Serial.print("Unable to find address for Device:"); Serial.print(i);
    }

    Serial.print("Sensor #"); Serial.print(i);
    Serial.print(": "); Serial.println(getAddress(thermalSensor[i]));
    sensors.setHighAlarmTemp (thermalSensor[i], HighAlarmTemp[i]);     // alarm when temp is higher than
    sensors.setLowAlarmTemp(thermalSensor[i], LowAlarmTemp[i]);    // alarm when temp is lower than
    Serial.print("Alarms:");  Serial.println(getAlarms(thermalSensor[i]));
    tt1m[i] = sensors.getTempC(thermalSensor[i]);
  }
}


// function to print a thermo-device address
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


String getAlarms(uint8_t deviceAddress[])
{

  String datastring = String ("<");
  datastring = datastring + String(sensors.getLowAlarmTemp(deviceAddress), DEC);
  datastring = datastring + String(", >");
  datastring = datastring + String(sensors.getHighAlarmTemp(deviceAddress), DEC);
  return datastring;
}

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
    for (int i = 0; i < sizeof(thermalSensor) / sizeof(DeviceAddress); i++) {tt2m[i] = 0;}
  }


  if (now() - startOfPeriod3 <= monPeriod3)
  {
    count_t3++;
  }
  else
  {
    startOfPeriod3 = now();
    count_t3 = 1;
    for (int i = 0; i < sizeof(thermalSensor) / sizeof(DeviceAddress); i++) {tt3m[i] = 0;}
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
// main function to print information about a device


String getData(DeviceAddress deviceAddress)
{
  String datastring;
  datastring = String(datastring + "addr: ");
  datastring = String(datastring + getAddress(deviceAddress));
  datastring = String(datastring + " t:");
  datastring = String(datastring + sensors.getTempC(deviceAddress));
  return datastring;
}

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
  datastring = datastring + String("Death after: ") + minToDeath + String(":")+ secToDeath;
  Serial.println(datastring);
  return datastring;

}


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


// phone
int makeForecast(int deviceid) // расчет в секундах времени до достижения точки разрушения на основе тренда
{
  float forecastSeconds ;
  float delta_temp;
  float cur_temp = sensors.getTempC(thermalSensor[deviceid]);
  // Serial.print("[mF] Cur_temp:"); Serial.println(cur_temp);
 
  if (tt1mTrend[deviceid] < 0 ) 
  {  //recovery time calculation
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

boolean sendSMS(String number, String message)
//boolean sendSMS()
{
  SIM900.print("AT+CMGF=1\r");                                                        // AT command to send SMS message
  delay(100);
  SIM900.print("AT + CMGS = \"");
  SIM900.print(number);
  Serial.println(number);
  SIM900.println("\"");                                     // recipient's mobile number, in international format
  delay(100);
  SIM900.println(message);        // message to send
  Serial.println(message);
  delay(100);
  SIM900.println((char)26);                       // End AT command with a ^Z, ASCII code 26
  delay(100);
  SIM900.println();
  delay(30000);                                     // give module time to send SMS
  return true;
}




void loop()
{

  sensors.requestTemperatures(); // Send the command to get temperatures
  updateStats(false);
  
  for (int i = 0; i < sizeof(thermalSensor) / sizeof(DeviceAddress); i++)
  {
       checkAlarm(i);
  }

  //printAlarms(t0); // Вывод пороговых значений на экран
  //delay(10000); // задержка проверки
 // Serial.println();
}

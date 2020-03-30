//#########################################################################################################
#include "WSconfig.h" //set user configurations such as TOKEN and WIFI in this file
//#########################################################################################################
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//#include <NTPClient.h>
#include <time.h>
#include <TaskScheduler.h>
#include <Ticker.h>
#include <BlynkSimpleEsp8266.h>

//#########################################################################################################
//LED related defines
//#########################################################################################################
#define LED_ON      LOW
#define LED_OFF     HIGH
#define LED1        16  //on board
#define LED2        2   //on chip

//#########################################################################################################
//stepper motor related defines
//#########################################################################################################
#define IN1  D5 //connect Motor IN1 with D5 
#define IN2  D6 //connect Motor IN2 with D6
#define IN3  D7 //connect Motor IN3 with D7
#define IN4  D8 //connect Motor IN4 with D8

#define CLOSE 0       // fully closed door !!DO NOT USE VALUES < 0!!

#define fSpeed 1.5 //defines motor basic speed in milliseconds e.g. 1.5 --> 1.5ms per step
int iSpeedDiv = 1;

//#########################################################################################################
//BLYNK app related defines
//#########################################################################################################
#define BLYNK_PRINT Serial
char auth[] = TOKEN;

//#########################################################################################################
//timer related defines
//#########################################################################################################
#define iTstep  1000  //time in milliseconds to call t1

//#########################################################################################################
//network settings
//#########################################################################################################
char ssid[] = WIFI_SSD;        //network ssid
char pass[] = WIFI_PASSWD;     //password

//#########################################################################################################
//function prototypes
//#########################################################################################################
void t1Callback();
void stepper(long lPosition);
int sunh(int doy, int nowh, int nowmin, int dst, float lat, float len);

char buffer [80];
Task t1(iTstep, TASK_FOREVER, &t1Callback);
Scheduler runner;
Ticker tickStepper;
WiFiUDP ntpUDP;

//#########################################################################################################
//automatic open close concertning variables
//#########################################################################################################
float fSunHeigth = 0;                     //Heigth of sun in degrees
bool bFirstRun = true;                    //indicates first run after reboot
const float geo[] = {50.8, 12.9};         //geographic coordinates Chemnitz

enum opMode {
  AUTO,
  MANUALLY
};
enum todo {
  toCLOSE,
  toOPEN
};

int iDayOpen = 86;  // 90 --> ~March,30th
int iDayClose = 300; // 300 --> ~October,26th
float fSunhOpen = 5;// sun more than 5 deg above horizon
float fSunhClose = -5;// sun less than -5 deg below horizon
int iOpMode = AUTO;
int iCommand = toCLOSE;
long lCurrentPos = 0; // initial Position = 0
long lTargetPos = 0;


int yday;
int mon;

// Use WiFiClient class
WiFiClient client;

//#########################################################################################################
//callback on tickStepper independently from loop
//#########################################################################################################
void runStepper()
{
  static int i = 0;
  if (i == iSpeedDiv)
  {
    if (lCurrentPos < lTargetPos)
    {
      stepper(lCurrentPos);
      lCurrentPos++;  //UP
    }
    else if (lCurrentPos > lTargetPos)
    {
      stepper(lCurrentPos);
      lCurrentPos--;  //DOWN
    }
    else
    {
      stepper(-1); //OFF
    }
    i = 0;
  }
  else
  {
    i++;
  }
}
//#########################################################################################################

//#########################################################################################################
//callback on t1Callback if time for loop < than t1Callback
//#########################################################################################################
void t1Callback()
{
  ////Serial.println("Hello from Timer!");

  time_t now = time(nullptr);
  struct tm * timeinfo;
  timeinfo = localtime(&now);
  //strftime (buffer, 80, "Now it's %c, the %jth day of the year", timeinfo);
  //Serial.println (buffer);
  ////Serial.println (rawtime);
  Blynk.virtualWrite(V0, lCurrentPos);//write current position to BLYNK
  //fSunHeigth = sunh(ptm->tm_yday, ptm->tm_hour, ptm->tm_min, ptm->tm_isdst, geo[0], geo[1]);
  fSunHeigth = sunh(timeinfo->tm_yday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_isdst, geo[0], geo[1]);
  //Serial.print("sunheigth[deg]: ");
  //Serial.println(fSunHeigth);
  if (iOpMode == AUTO)    // AUTOMATIC mode
  {
    if (timeinfo->tm_yday > iDayOpen && timeinfo->tm_yday < iDayClose) // curren day of year must be within "iDayOpen" and "iDayClose"
    {
      if (fSunHeigth > fSunhOpen) // sun rised above "fSunhOpen"
      {
        if (lTargetPos == CLOSE)
        {
          lTargetPos = OPEN;
          Blynk.notify(String("Die Tuer wurde um ") + ctime(&now) + " geoeffnet");
        }
        //Blynk.virtualWrite(V3, 1);
        if (bFirstRun)
        {
          lTargetPos = OPEN;
          lCurrentPos = CLOSE;
          bFirstRun = false;
        }
      }
      else if (fSunHeigth < fSunhClose) // sun set below"fSunhClose"
      {
        if (lTargetPos == OPEN)
        {
          lTargetPos = CLOSE;
          iCommand  = toCLOSE;
          Blynk.notify(String("Die Tuer wurde um ") + ctime(&now) + " geschlossen");
        }
        //Blynk.virtualWrite(V3, 0);
        if (bFirstRun)
        {
          lTargetPos = CLOSE;
          lCurrentPos = OPEN;
          bFirstRun = false;
        }
      }
    }
    else // curren day of year is not within "iDayOpen" and "iDayClose"
    {
      lTargetPos = CLOSE;
      //Blynk.virtualWrite(V3, 0);
      if (bFirstRun)
      {
        lCurrentPos = OPEN;
        bFirstRun = false;
      }
    }
  }
  else  // MANUAL mode
  {
    if (iCommand == toCLOSE)
    {
      if (lTargetPos == OPEN)
      {
        lTargetPos = CLOSE;
        iCommand  = toCLOSE;
        Blynk.notify(String("Die Tuer wurde um ") + ctime(&now) + " geschlossen");
      }
      if (bFirstRun)
      {
        lCurrentPos = OPEN;
        bFirstRun = false;
      }
    }
    else
    {
      if (lTargetPos == CLOSE)
      {
        lTargetPos = OPEN;
        iCommand  = toOPEN;
        Blynk.notify(String("Die Tuer wurde um ") + ctime(&now) + " geoeffnet");
      }
      lTargetPos = OPEN;
    }
    if (bFirstRun)
    {
      lCurrentPos = CLOSE;
      bFirstRun = false;
    }
  }

  //Serial.print("bFirstRun: ");
  //Serial.println(bFirstRun);
  //Serial.print("iOpMode: ");
  //Serial.println(iOpMode);
  //Serial.print("iCommand: ");
  //Serial.println(iCommand);
  //Serial.print("Target position: ");
  //Serial.println(lTargetPos);
  //Serial.print("Door position: ");
  //Serial.println(lCurrentPos);
}
//#########################################################################################################

//#########################################################################################################
//setup routine (initialization)
//#########################################################################################################
void setup() {
  //*************************************************LEDs****************************************************
  pinMode(LED1, OUTPUT);  //init LED1
  pinMode(LED2, OUTPUT);  //init LED2
  digitalWrite(LED1, LED_ON);  //switch LED1 on
  digitalWrite(LED2, LED_ON);  //switch LED2 on
  //************************************************Serial***************************************************
  //Serial.begin(115200);
  //Serial.println("Booting");
  //************************************************Serial***************************************************
  Blynk.begin(auth, ssid, pass);//Do BLYNK settings
  //WiFi.mode(WIFI_STA);
  //WiFi.begin(ssid, password);
  //**************************************************WiFi***************************************************
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    //Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  //Serial.println("Ready");
  //Serial.print("IP address: ");
  //Serial.println(WiFi.localIP());
  //**************************************************NTP****************************************************
  // Start Time service.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 0);          // Zeitzone MEZ setzen

  //Serial.println("\nWaiting for time");

  while (!time(nullptr)) // vorsichtshalber auf die Initialisierund der Lib warten
  {
    //Serial.print(".");
    delay(500);
  }
  //Serial.println("OK");
  delay (1000);
  time_t now = time(nullptr);
  //Serial.println(ctime(&now));
  //*********************************************STEPPER_MOTOR***********************************************
  pinMode(IN1, OUTPUT);   //connect motor
  pinMode(IN2, OUTPUT);   //connect motor
  pinMode(IN3, OUTPUT);   //connect motor
  pinMode(IN4, OUTPUT);   //connect motor
  //*************************************************Timer***************************************************
  runner.init();
  runner.addTask(t1);
  t1.enable();
  //Serial.println("Enabled Timer t1");
  //*************************************************Ticker**************************************************
  tickStepper.attach_ms(fSpeed, runStepper); //Use <strong>attach_ms</strong> if you need time
  //Serial.println("Enabled tickStepper");
  digitalWrite(LED1, LED_OFF);  //switch LED1 off
  digitalWrite(LED2, LED_OFF);  //switch LED2 off
  Blynk.notify("Schildis Torsteuerung wird gestartet!");

  //***************************************************OTA***************************************************
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    //Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    //Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    //Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      //Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      //Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      //Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      //Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      //Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}
//#########################################################################################################

//#########################################################################################################
// BLYNK stuff handler
//#########################################################################################################
BLYNK_CONNECTED()
{
  Blynk.syncAll();  // Synchronize hardware with App widgets when connected
}

BLYNK_WRITE(V0)     //current state
{

}

BLYNK_WRITE(V1)     //Speed
{
  iSpeedDiv = param.asInt();
}

BLYNK_WRITE(V2)     //Mode
{
  iOpMode = param.asInt();
}

BLYNK_WRITE(V3)     //manuell öffnen/schließen
{
  iCommand = param.asInt();
}
//#########################################################################################################

//#########################################################################################################
// main loop
//#########################################################################################################
void loop() {
  ArduinoOTA.handle();
  Blynk.run();
  runner.execute();
  //Serial.println("Hello from Loop!");
  //delay(100);
}
//#########################################################################################################

//#########################################################################################################
// calculate sunheigth in degree
//#########################################################################################################
int sunh(int doy, int nowh, int nowmin, int dst, float lat, float len)
{
  //sun heigth in Ã‚Â° is degre(asin())
  float deklin = 23.45 * sin(2 * M_PI * (284 + (float) doy) / 365);
  float omeg = (12 - ((float) nowh + (float) nowmin / 60) - (1 - len / 15)
                + (float) dst) / 24 * 360;
  lat = lat * 2 * M_PI / 360;       //deg to rad latitude
  len = len * 2 * M_PI / 360;       //deg to rad length
  omeg = omeg * 2 * M_PI / 360;     //deg to rad omega
  deklin = deklin * 2 * M_PI / 360; //deg to rad declination
  return asin(sin(deklin) * sin(lat) + cos(deklin) * cos(lat) * cos(omeg))
         * 360 / (2 * M_PI);
}

//#########################################################################################################

//#########################################################################################################
// function stepper with the argument long position.
// valid values: 0 < lPosition < 2.147.483.647
// for negative values: stepper ist in open mode
//#########################################################################################################
void stepper(long lPosition)
{
  int Steps = lPosition % 8;
  switch (Steps) {
    case 0:
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, HIGH);
      break;
    case 1:
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, HIGH);
      break;
    case 2:
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);
      break;
    case 3:
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);
      break;
    case 4:
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, LOW);
      break;
    case 5:
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, HIGH);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, LOW);
      break;
    case 6:
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, LOW);
      break;
    case 7:
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, HIGH);
      break;
    default:
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, LOW);
      break;
  }
}

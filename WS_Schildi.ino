//#########################################################################################################
#include "WSconfig.h" //set user configurations such as TOKEN and WIFI in this file
//#########################################################################################################
#include <NTPClient.h>
#include "time.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
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

#define OPEN 15000; // set steps to fully open door
#define CLOSE 0;    // fully closed door !!DO NOT USE VALUES < 0!!

#define fSpeed 1 //defines motor basic speed in milliseconds e.g. 1.5 --> 1.5ms per step
int iSpeedDiv = 1;

//#########################################################################################################
//BLYNK app related defines
//#########################################################################################################
#define BLYNK_PRINT Serial
char auth[] = TOKEN;

//#########################################################################################################
//Internet Time (NTP) related defines
//#########################################################################################################
#define NTP_OFFSET  1  * 60 * 60      // in seconds 1hour for MEZ (time zone setting)
#define NTP_INTERVAL 60 * 1000        // in miliseconds
#define NTP_ADDRESS  "0.pool.ntp.org" // ntp server address

//#########################################################################################################
//timer related defines
//#########################################################################################################
#define iTstep  1000  //time in milliseconds to call t1

//#########################################################################################################
//network settings
//#########################################################################################################
char ssid[] = WIFI_SSD;         //network ssid
char pass[] = WIFI_PASSWD;     //password

//#########################################################################################################
//function prototypes
//#########################################################################################################
void t1Callback();
void stepper(long lPosition);
int sunh(int doy, int nowh, int nowmin, int dst, float lat, float len);

// Time Structure
struct tm timeinfo;

Task t1(iTstep, TASK_FOREVER, &t1Callback);
Scheduler runner;
Ticker tickStepper;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

float fSunHeigth = 0;                  //Heigth of sun in degrees
String command;

const float geo[] = {50.7695, 12.9713};   //geographic coordinates Einsiedel

enum opMode {
  AUTO,
  MANUALLY
};

int iOpMode = AUTO;
int iDayOpen = 90;  // 90 --> ~March,30th
int iDayClose = 300; // 300 --> ~October,26th
float fSunhOpen = 2.5; // sun more than 2.5 deg above horizon
float fSunhClose = -2.5; // sun less than -2.5 deg below horizon

long lCurrentPos = 0; // initial Position = 0
long lTargetPos = 0;

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
      //Blynk.notify("Zielposition erreicht!");
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
  Serial.println("Hello from Timer!");
  Blynk.virtualWrite(V0, lCurrentPos);//write current position to BLYNK
  fSunHeigth = sunh(timeinfo.tm_yday + 1, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_isdst, geo[0], geo[1]);
  Serial.print("sunheigth[deg]: ");
  Serial.println(fSunHeigth);
  if (iOpMode == AUTO)
  {
    if (fSunHeigth > fSunhOpen)
    {
      lTargetPos = OPEN;
    }
    else if (fSunHeigth < fSunhClose)
    {
      lTargetPos = CLOSE;
    }
  }
  Serial.print("Door position: ");
  Serial.println(lCurrentPos);
}
//#########################################################################################################

//#########################################################################################################
//setup routine (initialization)
//#########################################################################################################
void setup() {
  pinMode(LED1, OUTPUT);  //init LED1
  pinMode(LED2, OUTPUT);  //init LED2
  digitalWrite(LED1, LED_ON);  //switch LED1 on
  digitalWrite(LED2, LED_ON);  //switch LED2 on
  Serial.begin(9600); // Initialize serial communications with the PC
  while (!Serial);      // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  Blynk.begin(auth, ssid, pass);//Do BLYNK settings
  // You can also specify server:
  //Blynk.begin(auth, ssid, pass, "blynk-cloud.com", 80);
  //Blynk.begin(auth, ssid, pass, IPAddress(192,168,1,100), 8080);

  //connecting to wifi and wait until connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  timeClient.begin(); //start real time client
  timeClient.update();//update internet time

  pinMode(IN1, OUTPUT);   //connect motor
  pinMode(IN2, OUTPUT);   //connect motor
  pinMode(IN3, OUTPUT);   //connect motor
  pinMode(IN4, OUTPUT);   //connect motor
  //init timer
  runner.init();
  runner.addTask(t1);
  t1.enable();
  Serial.println("Enabled Timer t1");
  //init tickStepper
  tickStepper.attach_ms(fSpeed, runStepper); //Use <strong>attach_ms</strong> if you need time
  Serial.println("Enabled tickStepper");
  digitalWrite(LED1, LED_OFF);  //switch LED1 off
  digitalWrite(LED2, LED_OFF);  //switch LED2 off
  Blynk.notify("Schildis Torsteuerung wird gestartet!");
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
  if (iOpMode == MANUALLY)
  {
    if (param.asInt() == 1)
    { // button set
      lTargetPos = OPEN;
    }
    else if (param.asInt() == 0)
    {
      lTargetPos = CLOSE;
    }
  }
  else
  {
    Blynk.notify("Torsteuerung im AUTO-Modus, vorher auf Manuell umstellen!");
  }
}

//#########################################################################################################

//#########################################################################################################
// main loop
//#########################################################################################################
void loop() {
  runner.execute();
  //Serial.println("Hello from Loop!");
  //delay(100);
  Blynk.run();
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

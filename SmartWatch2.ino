#include "Time.h"
#include "U8glib.h"
#include "Wire.h"  // I2C
#include "I2Cdev.h"
#include "MPU6050.h"
#include "SoftwareSerial.h"
#include "bitmap.h"

#define NUMCOUNTB1 7

#define KEY_NONE 0
#define KEY_PREV 1
#define KEY_NEXT 2
#define KEY_SELECT 3
#define KEY_BACK 4

#define FACE_ITEMS 6
#define CLOCK_ITEMS 2

#define speaker 16

#define SWalk 0.7
#define SRun 0.8
#define KcalWalk 0.0392 // == (0,7 m/step) * (56kcal/km) / (1000 m)
#define KcalRun 0.0544 // == (0,8 m/step) * (68kcal/km) / (1000 m)

U8GLIB_SH1106_128X64 u8g(4, 5, 6, 7, 8);  // D0=13, D1=11, CS=10, DC=9, Reset=8
SoftwareSerial mySerial(2, 3); // RX, TX

//////////////////////////////////////
//BT
/////////////////////////////////////
char command;
String string, c;
String daytime[10];
uint8_t count;



/////////////////////////////////////////////////////////////////
//                          Button                             //
/////////////////////////////////////////////////////////////////
uint8_t uiKeyPrev = 12;
uint8_t uiKeyNext = 11;
uint8_t uiKeySelect = 9;
uint8_t uiKeyBack = 10;

uint8_t uiKeyCodeFirst = KEY_NONE;
uint8_t uiKeyCodeSecond = KEY_NONE;
uint8_t uiKeyCode = KEY_NONE;
uint8_t last_key_code = KEY_NONE;

void uiSetup(void) {
  // configure input keys
  pinMode(uiKeyPrev, INPUT_PULLUP);           // set pin to input with pullup
  pinMode(uiKeyNext, INPUT_PULLUP);           // set pin to input with pullup
  pinMode(uiKeySelect, INPUT_PULLUP);           // set pin to input with pullup
  pinMode(uiKeyBack, INPUT_PULLUP);           // set pin to input with pullup
}
void uiStep(void) {
  uiKeyCodeSecond = uiKeyCodeFirst;
  if ( digitalRead(uiKeyPrev) == LOW )
    uiKeyCodeFirst = KEY_PREV;
  else if ( digitalRead(uiKeyNext) == LOW )
    uiKeyCodeFirst = KEY_NEXT;
  else if ( digitalRead(uiKeySelect) == LOW )
    uiKeyCodeFirst = KEY_SELECT;
  else if ( digitalRead(uiKeyBack) == LOW )
    uiKeyCodeFirst = KEY_BACK;
  else
    uiKeyCodeFirst = KEY_NONE;

  if ( uiKeyCodeSecond == uiKeyCodeFirst )
    uiKeyCode = uiKeyCodeFirst;
  else
    uiKeyCode = KEY_NONE;
}

/////////////////////////////////////////////////////////////////
//                          Clock                              //
/////////////////////////////////////////////////////////////////
uint8_t face_current = 0;
uint8_t clock_current = 0;

String str1;  //declaring string
String str2;  //declaring string
String str3;  //declaring string
String str4;  //declaring string
String str5;  //declaring string

/////////////////////////////////////////////////////////////////
//                          MPU6050                            //
/////////////////////////////////////////////////////////////////
MPU6050 accelgyro;

int16_t ax, ay, az;
int16_t gx, gy, gz;
float threshhold = 15000.0;
float xavg;
float yavg;
float zavg;
int steps, flag = 0;
boolean sleepon;
float awake, light, deep;
float totave = 0;
/////////////////////////////////////////////////////////////////
//                      Hearth rate                            //
/////////////////////////////////////////////////////////////////
uint8_t pulsePin = 0;                 // Pulse Sensor purple wire connected to analog pin 0


uint8_t BPM_last = 0;
int timeEnd;
int timeStart = 0;
int countdown = 0;

volatile short BPM = 0;                 // int that holds raw Analog in 0. updated every 2mS
volatile short Signal;                // holds the incoming raw data
volatile short IBI = 600;             // int that holds the time interval between beats! Must be seeded!
volatile boolean Pulse = false;     // "True" when User's live heartbeat is detected. "False" when not a "live beat".
volatile boolean QS = false;        // becomes true when Arduoino finds a beat.
boolean checkHeart = false;


/////////////////////////////////////////////////////////////////
//                                                             //
/////////////////////////////////////////////////////////////////
void draw(void);
void updateFace(void);
void calibrate(void);
void Step(void);

void setup() {
  // put your setup code here, to run once:
  mySerial.begin(9600);
  uiSetup();                      // setup key detection and debounce algorithm
  setTime(18, 10, 0, 17, 04, 2016);
  accelgyro.initialize();
  calibrate();
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  interruptSetup();                 // sets up to read Pulse Sensor signal every 2mS
  sleepon = false;
  awake = 0;
  light = 0;
  deep = 0;
}

void loop() {
  uiStep();                                // check for key press
  u8g.firstPage();  // Put information on OLED
  do {
    u8g.setFont(u8g_font_fur11);
    draw();
  } while ( u8g.nextPage());
  updateFace();
  Step();
  if (mySerial.available() > 0)
  {
    string = "";
    c = "";
    count = 0;
    memset(daytime, 0, sizeof(daytime));
  }
  while (mySerial.available() > 0)
  { command = ((byte)mySerial.read());
    if (command == ':')
    {
      break;
    }
    else
    {
      if ( c == "T") {
        if (command == '-') {
          count++;
        } else {
          daytime[count] += command;
        }
      }
      if (command == 'T') {
        c = 'T';
      } else {
        string += command;
      }

    }
    delay(1);
  }
  if ( c == "T") {
    setTime(daytime[1].toInt(), daytime[0].toInt(), second(), daytime[2].toInt() , daytime[3].toInt() + 1, daytime[4].toInt());

    c = "";
  }
  //  if (string == "TO")
  //  {
  //    ledOn();
  //    ledon = true;
  //  }
  //  if (string == "TF")
  //  {
  //    ledOff();
  //    ledon = false;
  //    mySerial.println(string); //debug
  //  }
  //  if ((string.toInt() >= 0) && (string.toInt() <= 255))
  //  {
  //    if (ledon == true)
  //    {
  //      analogWrite(led, string.toInt());
  //      mySerial.println(string); //debug
  //    }
  //  }
}
//void ledOn()
//{
//  analogWrite(led, 255);
//}
//void ledOff()
//{
//  analogWrite(led, 0);
//}
void updateFace(void) {
  if ( uiKeyCode != KEY_NONE && last_key_code == uiKeyCode ) {
    return;
  }
  last_key_code = uiKeyCode;

  switch ( uiKeyCode ) {
    case KEY_NEXT:
      tone(speaker, 5000, 200);
      if (face_current == 2) {
        digitalWrite(15, HIGH);
        checkHeart = false;
      }
      face_current++;
      if ( face_current >= FACE_ITEMS ) {
        face_current = 0;
      }

      break;
    case KEY_PREV:
      tone(speaker, 5000, 200);
      if (face_current == 2) {
        digitalWrite(15, HIGH);
        checkHeart = false;
      }
      if ( face_current <= 0 ) {
        face_current = FACE_ITEMS;
      }
      face_current--;

      break;
    case KEY_SELECT:
      tone(speaker, 5000, 200);
      if (face_current == 0) {
        clock_current = clock_current + 1;
        if (clock_current >= CLOCK_ITEMS) {
          clock_current = 0;
        }
      }
      if (face_current == 2) {
        digitalWrite(15, LOW);
        timeEnd = hour() * 60 + minute() * 60 + second() + 15;
        checkHeart = true;
        countdown = 15;
        timeStart = hour() * 60 + minute() * 60 + second();
      }
      if (face_current == 3) {
        if(sleepon){
          sleepon = false;
        }else{
          sleepon = true;
        }
      }
      break;
    case KEY_BACK:
      tone(speaker, 5000, 200);
      if (face_current == 2) {
        digitalWrite(15, HIGH);
        checkHeart = false;
      }
      break;
  }
}
void calibrate(void)
{
  float sum = 0;
  float sum1 = 0;
  float sum2 = 0;
  for (uint8_t i = 0; i < 100; i++)
  {
    accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    sum = abs(ax) + sum;
    sum1 = abs(ay) + sum1;
    sum2 = abs(az) + sum2;
  }
  xavg = sum / 100.0;
  yavg = sum1 / 100.0;
  zavg = sum2 / 100.0;
}
void Step(void) {
  float totvect = 0;  
  float zacclave = 0;
  calibrate();
  for (uint8_t i = 0; i < 100; i++)
  {
    accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    ax = abs(ax);
    ay = abs(ay);
    az = abs(az);
    totvect = sqrt(((ax - xavg) * (ax - xavg)) + ((ay - yavg) * (ay - yavg)) + ((az - zavg) * (az - zavg)));
    if (i == 0) {
      totave = totvect;
      zacclave = az;
    } else {
      totave = (totave + totvect) / 2 ;
      zacclave = (zacclave + az) / 2;
    }

    //cal steps
    if (totave > threshhold && flag == 0)
    {
      if (zacclave > -4000 and zacclave < 4000)
      {
        steps = steps + 1;
        flag = 1;
      }
    }
    else if (totave > threshhold && flag == 1)
    {
      //do nothing
    }
    if (totave < threshhold  && flag == 1)
    {
      flag = 0;
    }
  }
}

void draw(void) {
  char b[20];

  switch (face_current) {
    case 0:
      switch (clock_current) {
        case 0:
          u8g.drawLine(0, 0, 127, 0);
          u8g.drawLine(0, 23, 127, 23);
          u8g.drawLine(0, 63, 127, 63);
          str1 = String(hour()); //converting integer into a string
          str2 = String(minute()); //converting integer into a string
          str3 = String(second()); //converting integer into a string
          if (second() < 10) str3 = "0" + str3;
          if (minute() < 10) str2 = "0" + str2;
          if (hour() < 10) str1 = "0" + str1;
          str4 = str1 + ":" + str2 + ":" + str3;

          str1 = String(day());
          str2 = String(month());
          str3 = String(year());
          if (day() < 10) str1 = "0" + str1;
          if (month() < 10) str2 = "0" + str2;
          str5 = str1 + "/" + str2 + "/" + str3;

          str5.toCharArray(b, 11);
          u8g.drawStr( 20, 20, b);
          str4.toCharArray(b, 11);
          u8g.drawStr( 34, 50, b);
          break;
        case 1:
          str1 = String(hour()); //converting integer into a string
          str2 = String(minute()); //converting integer into a string
          str3 = String(second()); //converting integer into a string
          if (second() < 10) str3 = "0" + str3;
          if (minute() < 10) str2 = "0" + str2;
          if (hour() < 10) str1 = "0" + str1;
          str4 = str1 + ":" + str2 + ":" + str3;
          str4.toCharArray(b, 11);
          u8g.drawStr( 30, 20, b);
          break;
      }
      break;
    case 1:
      u8g.drawStr(0, 20, "Step");
      u8g.setPrintPos(0, 40); u8g.print(steps);
      u8g.drawBitmapP( 78, 7, 6, 50, img_footstep);
      break;
    case 2:
      u8g.drawStr(0, 20, "Heart Rate");
      if (checkHeart) {
        if (timeEnd != hour() * 60 + minute() * 60 + second()) {
          if (QS) {
            str1 = String(BPM) + " BPM";
            str1.toCharArray(b, 11);
            u8g.drawStr( 0, 40, b);

            BPM_last = BPM;
          }
          u8g.drawBox(0, 54, 128 * (15 - (timeEnd - (hour() * 60 + minute() * 60 + second()))) / 15 , 10);
          str1 = String(BPM) + " BPM";
          str1.toCharArray(b, 11);
          u8g.drawStr( 0, 40, b);
        } else {
          checkHeart = false;
          digitalWrite(15, HIGH);

          str1 = String(hour()); //converting integer into a string
          str2 = String(minute()); //converting integer into a string
          //          str3 = String(second()); //converting integer into a string
          //          if (second() < 10) str3 = "0" + str3;
          if (minute() < 10) str2 = "0" + str2;
          if (hour() < 10) str1 = "0" + str1;
          str4 = str1 + "/" + str2;

          str1 = String(day());
          str2 = String(month());
          str3 = String(year());
          if (day() < 10) str1 = "0" + str1;
          if (month() < 10) str2 = "0" + str2;
          str5 = str3 + "/" + str2 + "/" + str1 + "/" + str4 + String(BPM_last);

          str5.toCharArray(b, 11);
          mySerial.print(str5);
        }
      }  else {
        str1 = String(BPM_last) + " BPM";
        str1.toCharArray(b, 11);
        u8g.drawStr( 0, 40, b);
        if (BPM_last) {
          //          u8g.setPrintPos(0, 63);
          //          u8g.print(((hour() * 60 + minute() * 60 + second() - timeEnd)) / 60);
          //          u8g.drawStr(19, 63, "Minute(s) before");
          str1 = String(((hour() * 60 + minute() * 60 + second() - timeEnd)) / 60) + " Minute before";
          str1.toCharArray(b, 20);
          u8g.drawStr(0, 63, b);
        }
      }
      if (Pulse) u8g.drawBitmapP( 78, 0, 6, 50, img_heartrate);
      else u8g.drawBitmapP( 78, 0, 6, 50,  img_heart);
      break;
    case 3:
      if (sleepon) {
        u8g.drawStr(0, 20, "Sleep On");
        u8g.drawStr(0, 35, "Awake:");
        u8g.setPrintPos(50, 35); u8g.print(awake/30000);
        u8g.drawStr(0, 50, "Light:");
        u8g.setPrintPos(40, 50); u8g.print(light/30000);
        u8g.drawStr(0, 64, "Deep:");
        u8g.setPrintPos(40, 64); u8g.print(deep/30000);
      } else {
        u8g.drawStr(0, 20, "Sleep Off");
      }
      //      u8g.setPrintPos(0, 40); u8g.print(steps);
      u8g.drawBitmapP( 78, 7, 6, 50, img_sleep);
      break;
    case 4:
      u8g.drawStr(0, 20, "Distance");

      u8g.setPrintPos(0, 40); u8g.print(steps * SWalk);
      u8g.drawBitmapP( 78, 7, 6, 50, img_Dis);
      break;
    case 5:
      u8g.drawStr(0, 20, "Calorie burn");
      u8g.setPrintPos(0, 40); u8g.print(steps * KcalWalk);
      u8g.drawBitmapP( 78, 7, 6, 50, img_Kcal);
      break;
  }
}


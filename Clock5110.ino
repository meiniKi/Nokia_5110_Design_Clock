//
//              Created by Meinhard Kissich 2017
//              www.meinhard-kissich.at
//
//    This sketch is work in progress (!). It's not finished
//    and there might be some faults. Further information on
//    the hardware and casing are comming soon.
//
//  You may do whatever you want with this code, as long as:
//    a. share it and keep it FREE. 
//    b. keep the original credits.


// Set time: hold down the button at pin 9 for 2 seconds to 
// change the set time. Using the button on pin 10 you can 
// increase the blinking value. With a single press on pin 9 button
// you jump to the next one.
//
// Set alarm: hold down the button at pin 10 for 2 seconds to
// set the alarm time. Press the button once to enable and disable
// the alarm
//
// Set calender: press the button at pin 10 twice to set the calender
// increase with the other button (pin 9) and press button (pin 10) once
// to jump to the next value


// KNOW ERROS:
// 1. day is not displayed correcetly!
// 2. alarm function is not included yet!
// 3. backlight not included yet

#include <MD_DS3231.h>
#include <U8glib.h>
#include <stdio.h>


#define ALARM_PIN 3
#define BACKLIGHT_PIN 5
#define BUTTON_BLUE_PIN 10
#define BUTTON_GREEN_PIN 9

#define ALARM_ON_TYPE   DS3231_ALM_HM     // type of alarm set to turn on
#define ALARM_OFF_TYPE  DS3231_ALM_DDHM   // type of alarm set to turn off

/*************************************/
/*** .     TIMES / DEBOUNCES .     ***/
/*************************************/
#define UPDATE_RATE                   300     //[ms] update rate of screen
#define BUTTON_DEBOUNCE_TIME          30      //[ms] to debounce switch
#define BUTTON_LONG_TIME              2000    //[ms] time which needs to be hold down to activate a long press
#define BUTTON_DOUBLE_ACTION_TIME_MAX 800     //[ms] time within 2 presses must happen to be a "double-pressed"
#define BLINK_TIME_ON_SETTING         350     //[no real world unit] the higher the slower

U8GLIB_PCD8544 u8g(13, 11, 6, 2, 8);

uint8_t alarmPic[] = {0x0c,0x00,0x1e,0x00,0x21,0x00,0x21,0x00,0x21,0x00,0x21,0x00,0x21,0x00,0x40,0x80,0x7f,0x80,0x0c,0x00};

/*************************************/
/***    ENUMS / STRUCTS / DEFS     ***/
/*************************************/

enum clockState {normal, setTimeHours, setTimeMinutes, setTimeSeconds, setAlarm1Minutes, setAlarm1Hours, setCalDay, setCalMonth, setCalYear}; 

char* dictDayNames[] = {"---", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
char* dictMonthNames[] = {"---", "Jan", "Feb", "Mar", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

struct clockSettings {
  boolean alarm1Enable = false;
  boolean alarm1EnableOld = false;   //for aniamtion
  enum clockState state = normal;
};

enum buttonState {pressed, longPressed, doublePressed, notPressed, inEvaluation};

struct buttonData {
  enum buttonState state = notPressed;
  boolean pressedBefore = false;
  unsigned long pressedTime = 0;
  boolean isActiveLow = true;
  uint8_t pin = 0;
};

struct simpleTime {
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t seconds = 0;
};

/*************************************/
/***          VARIABLES            ***/
/*************************************/

int lastUpdate = 0;
struct buttonData buttonBlue;
struct buttonData buttonGreen;
struct clockSettings settings;
unsigned long tempTime;

/*************************************/
/***          FUNCTIONS            ***/
/*************************************/

void alarmHorn() {
  digitalWrite(ALARM_PIN,HIGH);
}

void alarmHornOFF() {
  digitalWrite(ALARM_PIN,LOW);
}

void backlightON() {
  digitalWrite(BACKLIGHT_PIN,LOW);
}

void backlightOFF() {
  digitalWrite(BACKLIGHT_PIN,HIGH);
}

boolean buttonPressed(uint8_t pin, uint8_t lowActive) {
  if (lowActive)
    return !digitalRead(pin);
  else
    return digitalRead(pin); 
}

struct buttonData getButtonData(struct buttonData btnData)
{ 
  if(buttonPressed(btnData.pin, btnData.isActiveLow) && millis() - btnData.pressedTime >= BUTTON_DEBOUNCE_TIME)  {
    //was not pressed before 
    if (!btnData.pressedBefore) {
      if(btnData.state == notPressed) {
        btnData.state = inEvaluation;
        btnData.pressedTime = millis();
      } else if (btnData.state == inEvaluation && millis()) {
        Serial.println("button state = DOUBLE_PRESSED");
        btnData.state = doublePressed;
        btnData.pressedTime = 0;
        btnData.pressedBefore = false;
      }
    }

    //was pressed before
    if(btnData.state == inEvaluation && btnData.pressedBefore && millis() - btnData.pressedTime >= BUTTON_LONG_TIME) {
      Serial.println(millis() - btnData.pressedTime);
      Serial.println("button state = LONG_PRESSED");
      btnData.state = longPressed;
      btnData.pressedTime = 0;
      btnData.pressedBefore = false;
    }
  }

  if (!buttonPressed(btnData.pin, btnData.isActiveLow) && btnData.state == inEvaluation && millis() - btnData.pressedTime >= BUTTON_DOUBLE_ACTION_TIME_MAX) {
     Serial.println("button state = SINGLE_PRESSED");
     btnData.state = pressed;
     btnData.pressedTime = 0;
     btnData.pressedBefore = false;
  }
  btnData.pressedBefore = buttonPressed(btnData.pin, btnData.isActiveLow);

  return btnData;
}



/*************************************/
/***            SETUP             ***/
/*************************************/

void setup() {
  //I/0
  pinMode(BACKLIGHT_PIN,OUTPUT);
  pinMode(ALARM_PIN,OUTPUT);
  pinMode(BUTTON_BLUE_PIN, INPUT_PULLUP);
  pinMode(BUTTON_GREEN_PIN, INPUT_PULLUP);
  
  //Serial
  Serial.begin(9600);
  
  //RTC setup
  RTC.control(DS3231_12H, DS3231_OFF);  // 24 hour clock

  //display setup
  u8g.setDefaultForegroundColor();

  //button setup
  buttonBlue.isActiveLow = true;
  buttonGreen.isActiveLow = true;
  buttonBlue.pin = BUTTON_BLUE_PIN;
  buttonGreen.pin = BUTTON_GREEN_PIN;

  //alarm default OFF
  RTC.writeAlarm1(ALARM_OFF_TYPE);
}

void loop() {
  //evaluate buttons
  buttonBlue = getButtonData(buttonBlue);
  buttonGreen = getButtonData(buttonGreen);

  //edit time (!!! order !!!)
  if (buttonGreen.state != notPressed) {
    if (buttonGreen.state == pressed && settings.state == setTimeSeconds)
      settings.state = normal;
      
    if (buttonGreen.state == pressed && settings.state == setTimeMinutes)
      settings.state = setTimeSeconds;

    if (buttonGreen.state == pressed && settings.state == setTimeHours)
      settings.state = setTimeMinutes;
    
    if (buttonGreen.state == longPressed && settings.state == normal) 
      settings.state = setTimeHours;
  }
  
  //set calender && alarm
  if (buttonBlue.state != notPressed)
  {
    //cal
    if (buttonBlue.state == pressed && settings.state == setCalYear)
      settings.state = normal;

    if (buttonBlue.state == pressed && settings.state == setCalMonth)
      settings.state = setCalYear;
      
    if (buttonBlue.state == pressed && settings.state == setCalDay)
      settings.state = setCalMonth;
    
    if (buttonBlue.state == doublePressed && settings.state == normal)
      settings.state = setCalDay;

    //alarm 1
    if (buttonBlue.state == pressed && settings.state == setAlarm1Minutes)
      settings.state = normal;

    if (buttonBlue.state == pressed && settings.state == setAlarm1Hours)
      settings.state = setAlarm1Minutes;

    if (buttonBlue.state == longPressed && settings.state == normal && settings.alarm1Enable)
      settings.state = setAlarm1Hours; 
  }
  

  //enable / disable alarm
  if (buttonBlue.state == pressed && settings.state == normal)
  {
    settings.alarm1Enable = !settings.alarm1Enable;
    if (settings.alarm1Enable)
      RTC.writeAlarm1(ALARM_ON_TYPE);
    else
      RTC.writeAlarm1(ALARM_OFF_TYPE);
  }
  
  //write cal
  if (!settings.state == normal && buttonPressed(BUTTON_GREEN_PIN,true) && millis() - 100 >= tempTime) {
    tempTime = millis();
    if(settings.state == setCalDay)
      RTC.dd = RTC.dd >= 31? 1 : RTC.dd + 1;
    if(settings.state == setCalMonth)
      RTC.mm = RTC.mm >= 12? 1 : RTC.mm + 1;
    if(settings.state == setCalYear)
      RTC.yyyy = RTC.yyyy >= 2050? 2017 : RTC.yyyy + 1;

    RTC.writeTime();
    
  }
  
  //write time
  if (!settings.state == normal && buttonPressed(BUTTON_BLUE_PIN,true) && millis() - 100 >= tempTime) {
    tempTime = millis();
    if(settings.state == setTimeHours)
      RTC.h = RTC.h >= 23? 0 : RTC.h + 1;
    if(settings.state == setTimeMinutes)
      RTC.m = RTC.m >= 59? 0 : RTC.m + 1;
    if(settings.state == setTimeSeconds)
      RTC.s = RTC.s >= 59? 0 : RTC.s + 1;

    RTC.writeTime();
  }

  //write Alarm 1
  if (!settings.state == normal && buttonPressed(BUTTON_GREEN_PIN,true) && millis() - 100 >= tempTime) {
    tempTime = millis();
    if(settings.state == setAlarm1Hours)
      RTC.h = RTC.h >= 23? 0 : RTC.h + 1;
    if(settings.state == setAlarm1Minutes)
      RTC.m = RTC.m >= 59? 0 : RTC.m + 1;

    RTC.s = RTC.yyyy = RTC.dow = RTC.dd = RTC.mm = 0;
    RTC.writeAlarm1(ALARM_ON_TYPE);
  }  
  
  //update screen
  if (millis() - lastUpdate >= UPDATE_RATE)
  {
    updateHomeScreen();
    lastUpdate = millis();

    //'Old' values update
    settings.alarm1EnableOld = settings.alarm1Enable;
  }

  //reset buttons
  if (buttonGreen.state != inEvaluation)
    buttonGreen.state = notPressed;
  if (buttonBlue.state != inEvaluation)
    buttonBlue.state = notPressed;


}



void updateHomeScreen() {
  char year[6];
  char monthName[4];
  char day[4];
  char dayName[4];
  char hh[3];
  char mm[3];
  char sec[3];

  char hhAlarm1[3];
  char mmAlarm1[3];

  RTC.readAlarm1();
  sprintf(hhAlarm1,"%2.d",RTC.h);
  sprintf(mmAlarm1,"%.2d",RTC.m);  

  RTC.readTime();
  sprintf(year,"'%d",RTC.yyyy);
  strcpy(monthName, dictMonthNames[RTC.mm]);
  sprintf(day,"%2.d.",RTC.dd);
  Serial.println(RTC.dow);
  strcpy(dayName, dictDayNames[RTC.dow]);
  
  sprintf(hh,"%2.d",RTC.h);
  sprintf(mm,"%.2d",RTC.m);
  sprintf(sec,"%2.d",RTC.s);

  uint8_t xOffsetAlarm = 0;
  boolean animationFinished = true;

  //picture loop
  do
  {
    u8g.firstPage();
    do
    {
        u8g.setFont(u8g_font_courB14);
  
        if (!(settings.state == setTimeHours && (millis() / BLINK_TIME_ON_SETTING) % 2 == 0))
          u8g.drawStr(15,15,hh);
  
          u8g.drawStr(37,15,":");
          
        if (!(settings.state == setTimeMinutes && (millis() / BLINK_TIME_ON_SETTING) % 2 == 0))
          u8g.drawStr(47,15,mm);
  
        //seconds bar blank
        u8g.drawVLine(0,20,3);  //0
        u8g.drawVLine(14,20,3); //15
        u8g.drawVLine(29,20,3); //30
        u8g.drawVLine(44,20,3); //45
        u8g.drawVLine(59,20,3); //60
        //seconds bar fill
        u8g.drawHLine(0,21,RTC.s);
  
        //seconds numeric
        u8g.setFont(u8g_font_04b_03r);
        if (!(settings.state == setTimeSeconds && (millis() / BLINK_TIME_ON_SETTING) % 2 == 0))
          u8g.drawStr(75,24,sec);
        
        //alarm - static alarm enabled
        if (settings.alarm1Enable && settings.alarm1EnableOld)
        {
          u8g.drawBitmap(12,31,2,10,alarmPic);
          if (!(settings.state == setAlarm1Hours && (millis() / BLINK_TIME_ON_SETTING) % 2 == 0))
            u8g.drawStr(4,48,hhAlarm1);
            
          u8g.drawStr(10,48," : ");
          if (!(settings.state == setAlarm1Minutes && (millis() / BLINK_TIME_ON_SETTING) % 2 == 0))
            u8g.drawStr(17,48,mmAlarm1);
        }
        
        //calender - static alarm enabled
        if (settings.alarm1Enable && settings.alarm1EnableOld)
          printCalenderFrame(42,30, dayName, day, monthName, year);

        //calender - static alarm disabled
        if (!settings.alarm1Enable && !settings.alarm1EnableOld)
          printCalenderFrame(20,30, dayName, day, monthName, year);
        
  
        //calender - ANIMATION -> enable
        if (settings.alarm1Enable && !settings.alarm1EnableOld)
        {
          printCalenderFrame(20 + xOffsetAlarm,30, dayName, day, monthName, year);
          animationFinished = ++xOffsetAlarm < 12 ? false : true;
          delay(20);
        }

        //calender - ANIMATION -> disable
        if (!settings.alarm1Enable && settings.alarm1EnableOld)
        {
          printCalenderFrame(20 + 12 - xOffsetAlarm,30, dayName, day, monthName, year);
          animationFinished = ++xOffsetAlarm < 12 ? false : true;
          delay(20);
        }
    }
    while(u8g.nextPage());
  }
  while(!animationFinished);

  //seconds bar
  
}

//in picture frame!
void printCalenderFrame(uint8_t x, uint8_t y, char dayNameStr[], char dayStr[], char monthNameStr[], char yearStr[])
{
  char yearShort[4];
  sprintf(yearShort,"'%c%c",yearStr[3],yearStr[4]);
  
  u8g.drawFrame(x,y+3,40,15);
  u8g.drawVLine(x+7,y,3);
  u8g.drawVLine(x+8,y,3);
  u8g.drawVLine(x+15,y,3);
  u8g.drawVLine(x+16,y,3);
  u8g.drawVLine(x+23,y,3);
  u8g.drawVLine(x+24,y,3);
  u8g.drawVLine(x+31,y,3);
  u8g.drawVLine(x+32,y,3);

  if (!(settings.state == setCalDay && (millis() / BLINK_TIME_ON_SETTING) % 2 == 0))
  {
    u8g.drawStr(x+7, y+10, dayNameStr);
    u8g.drawStr(x+20, y+10, ",");
    u8g.drawStr(x+25, y+10, dayStr);
  }

  if (!(settings.state == setCalMonth && (millis() / BLINK_TIME_ON_SETTING) % 2 == 0))
    u8g.drawStr(x+6, y+16, monthNameStr);

  if (!(settings.state == setCalYear && (millis() / BLINK_TIME_ON_SETTING) % 2 == 0))
    u8g.drawStr(x+23, y+16, yearShort);
}




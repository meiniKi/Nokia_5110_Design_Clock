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





#include <MD_DS3231.h>
#include <U8glib.h>
#include <stdio.h>



#define ALARM_PIN 3
#define BACKLIGHT_PIN 5
#define BUTTON_BLUE_PIN 10
#define BUTTON_GREEN_PIN 9

/*************************************/
/*** .     TIMES / DEBOUNCES .     ***/
/*************************************/
#define UPDATE_RATE                   300     //[ms] update rate of screen
#define BUTTON_DEBOUNCE_TIME          30      //[ms] to debounce switch
#define BUTTON_LONG_TIME              2000    //[ms] time which needs to be hold down to activate a long press
#define BUTTON_DOUBLE_ACTION_TIME_MAX 800     //[ms] time within 2 presses must happen to be a "double-pressed"
#define BLINK_TIME_ON_SETTING         350     //[no real world unit] the higher the slower

U8GLIB_PCD8544 u8g(13, 11, 6, 2, 8);

uint8_t alarmPic[] = {0x01,0x80
,0x03,0xc0
,0x07,0xe0
,0x0c,0x30
,0x08,0x10
,0x08,0x10
,0x18,0x18
,0x18,0x18
,0x18,0x18
,0x10,0x08
,0x30,0x0c
,0x20,0x04
,0x60,0x06
,0x7f,0xfe
,0x03,0xc0
,0x01,0x80};

/*************************************/
/***    ENUMS / STRUCTS / DEFS     ***/
/*************************************/

enum clockState {normal, setTimeHours, setTimeMinutes, setTimeSeconds}; 


struct clockSettings {
  boolean alarm1Enable = false;
  boolean alarm2Enable = false;
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
  

  
  //update screen
  if (millis() - lastUpdate >= UPDATE_RATE)
  {
    updateHomeScreen();
    lastUpdate = millis();
  }

  //reset buttons
  if (buttonGreen.state != inEvaluation)
    buttonGreen.state = notPressed;
  if (buttonBlue.state != inEvaluation)
    buttonBlue.state = notPressed;
}

void updateHomeScreen() {
  char hh[3];
  char mm[3];
  char sec[3];

  RTC.readTime();

  sprintf(hh,"%2.d",RTC.h);
  sprintf(mm,"%.2d",RTC.m);
  sprintf(sec,"%2.d",RTC.s);

  //picture loop
  u8g.firstPage();
  do
  {
      u8g.setFont(u8g_font_courB14);

      if (!(settings.state == setTimeHours && (millis() / BLINK_TIME_ON_SETTING) % 2 == 0))
        u8g.drawStr(10,15,hh);

        u8g.drawStr(32,15,":");
        
      if (!(settings.state == setTimeMinutes && (millis() / BLINK_TIME_ON_SETTING) % 2 == 0))
        u8g.drawStr(42,15,mm);

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
      
      //alarm
      u8g.drawBitmap(10,30,2,16,alarmPic);
  }
  while(u8g.nextPage());

  //seconds bar
  
}




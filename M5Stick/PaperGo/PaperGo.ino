// INCLUDE: COMMON
#include <M5Stack.h>

// INCLUDE: SCREEN
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>

// DEFINE: PIN
#define PIN_LED                      19
#define PIN_BUZZER                   26
#define PIN_BTN                      35
#define PIN_REMOTE                   25

// DEFINE: BTN
#define BTN_LONGPRESS_SEC            3

// DEFINE: SCREEN
#define ICON_FONT                    u8g2_font_open_iconic_all_1x_t
#define BIG_FONT                     u8g2_font_helvB18_tr
#define CHINESE_FONT                 u8g2_font_wqy16_t_gb2312
#define STATUS_FONT                  u8g2_font_profont12_tr

#define SCREEN_BRIGHT                254
#define SCREEN_TIMEOUT               300

#define SCREEN_LINE_LEFT             17
#define SCREEN_LINE_TOP              5

#define TIME_PAGE_BASE_MS            1000
#define TIME_RANDOM_MS               9000

#define TOTAL_BASE_PERCENT           1.1
#define TOTAL_MAX_PERCENT            1.4

#define TOTAL_BASE_MIN               30
#define TOTAL_BASE_PAGE              300

// VAR: PAGE
bool isSwitchMode = false;
bool isSwitchDone = false;
int totalPage = 1;
uint32_t activePageTime = 0;
uint32_t totalPageTime = 0;

uint32_t targetTotalPage = 0;
uint32_t targetTotalTime = 0;

// VAR: TIME
uint32_t targetUpdateTime = 0;
uint32_t targetScreenOffTime = 0;
uint32_t targetNextPageTime = 0;
uint32_t targetLongPressTime = 0;

// VAR: SCREEN
int screen_PowerSave = false;

U8G2_SH1107_64X128_F_4W_HW_SPI u8g2(U8G2_R3, /* cs=*/ 14, /* dc=*/ 27, /* reset=*/ 33);

void setup() 
{
  delay(1000);

  //begin(LCDEnable=true, SDEnable=true, SerialEnable=true, I2CEnable=false);
  M5.begin(false, false, true, false);
  
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFlipMode(0);
  u8g2.clearDisplay();

  u8g2.setContrast(SCREEN_BRIGHT);
  u8g2.setFont(BIG_FONT);
  u8g2.setFontMode(0);   // enable transparent mode, which is faster
  
  u8g2.setFontRefHeightExtendedText();
  u8g2.setFontPosTop();

  Serial.println();

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_REMOTE, OUTPUT);

  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_REMOTE, LOW);
  
  SwitchPowerSave(false);
  ShowLoadingScreen();
} 

void loop() 
{
  // Update Screen
  if (targetUpdateTime < millis()) {
    u8g2.firstPage();
    do {
      PrintUI();
      
      targetUpdateTime = millis() + 500;
    } while ( u8g2.nextPage() );
  }
  
  // Page Time
  if (isSwitchMode){ 
    uint32_t totalMinute = GetTotalMinutes();
    if (totalPage >= targetTotalPage && totalMinute >= targetTotalTime) {
      if (!isSwitchDone && targetNextPageTime > 0) {
        isSwitchDone = true;
        targetNextPageTime = 0;
        
        totalPageTime += (millis() - activePageTime);
        activePageTime = 0;
        
        ShowDialog("翻页完成", 5000, true);
      }
    } else {
      SwitchPowerSave(false);
      
      if (targetNextPageTime > 0 && targetNextPageTime < millis()) {
        targetNextPageTime = millis() + SwitchPage();
      }
    }
  }

  // Screen Powersave
  if (targetScreenOffTime < millis()) {
    SwitchPowerSave(true);
  }

  // Btn
  if (digitalRead(PIN_BTN) == 0) {
    SwitchPowerSave(false);

    if (targetLongPressTime == 0) {
      targetLongPressTime = millis() + 1000 * BTN_LONGPRESS_SEC;
    }
    
    if (targetLongPressTime < millis()) {
      ShowDialog("重置数据", 2000, false);
      
      isSwitchMode = false;
      isSwitchDone = false;
      
      totalPage = 1;
      activePageTime = 0;
      totalPageTime = 0;
      
      targetTotalTime = 0;
      targetTotalPage = 0;
      
      targetNextPageTime = 0;
      targetLongPressTime = 0;
    }
  }
  else
  {
    if (targetLongPressTime > 0) {
    
      if (targetNextPageTime > 0) {
        targetNextPageTime = 0;
        
        totalPageTime += (millis() - activePageTime);
        activePageTime = 0;
        
        ShowDialog("暂停", 2000, false);
      } else {
        randomSeed(analogRead(0));
        isSwitchMode = true;

        if (targetTotalTime == 0 || targetTotalPage == 0) {
          totalPage = 1;
          targetTotalTime = random(TOTAL_BASE_MIN * TOTAL_BASE_PERCENT, TOTAL_BASE_MIN * TOTAL_MAX_PERCENT);
          targetTotalPage = random(TOTAL_BASE_PAGE * TOTAL_BASE_PERCENT, TOTAL_BASE_PAGE * TOTAL_MAX_PERCENT);

          Serial.print("targetTotalTime: ");
          Serial.print(targetTotalTime);
          Serial.print(" | targetTotalPage: ");
          Serial.println(targetTotalPage);
        }

        String statusText = "";
        
        if (totalPage > 1) {
          statusText = "继续";
        } else {
          statusText = "开始";
        }
        
        if (isSwitchDone){
          statusText += ": " + String(GetTotalMinutes()) + "分/" + String(totalPage) + "页";
        } else {
          statusText += ": " + String(targetTotalTime - GetTotalMinutes()) + "分/" + String(targetTotalPage - totalPage) + "页";
        }
        
        ShowDialog(statusText, 2000, false);
        
        activePageTime = millis();
        targetNextPageTime = millis() + 1000 * 2;
      }
    }
    
    targetLongPressTime = 0;
  }  
}

uint32_t GetTotalMinutes(){
  return (millis() - activePageTime + totalPageTime) / 1000 / 60;
}

int SwitchPage() {
   totalPage++;

  int rndPageDelayMs = random(TIME_PAGE_BASE_MS, TIME_PAGE_BASE_MS + TIME_RANDOM_MS);
  int rndBtnDelayMs = random(100, 150);
  
  digitalWrite(PIN_REMOTE, HIGH);
  digitalWrite(PIN_LED, HIGH);
  delay(rndBtnDelayMs);
  digitalWrite(PIN_REMOTE, LOW);
  digitalWrite(PIN_LED, LOW);

  return rndPageDelayMs;
}

void ShowCenterText(int inputX_From, int inputX_To, int inputY, bool inputIsUtf8, String inputText) {
  int textWidth = 0;
  int textOffset = 0;
  
  if (inputIsUtf8){
    textWidth = u8g2.getUTF8Width(inputText.c_str());
    textOffset = (inputX_To - inputX_From - textWidth) / 2;
    u8g2.drawUTF8(inputX_From + textOffset, inputY, inputText.c_str());
  } else {
    textWidth = u8g2.getStrWidth(inputText.c_str());
    textOffset = (inputX_To - inputX_From - textWidth) / 2;
    u8g2.drawStr(inputX_From + textOffset, inputY, inputText.c_str());
  }  
}

void ShowDialog(String inputText, int delayTime, bool isBeep){
  u8g2_uint_t textWidth = 0;

  SwitchPowerSave(false);
  u8g2.firstPage();
  do {
    u8g2.drawFrame(0, 0, u8g2.getDisplayWidth(), u8g2.getDisplayHeight() );
    u8g2.setFont(CHINESE_FONT);
    textWidth = u8g2.getUTF8Width(inputText.c_str());
    ShowCenterText(0, u8g2.getDisplayWidth(), u8g2.getDisplayHeight() / 10 * 4, true, inputText.c_str());
    //u8g2.drawUTF8(25, 25, inputText.c_str());
  } while ( u8g2.nextPage() );

  if (isBeep)
  {
    for (int i = 0; i < 100; i++) {
      digitalWrite(PIN_BUZZER, HIGH);
      delay(1);
      digitalWrite(PIN_BUZZER, LOW);
      delay(1);
    }
    delay(5);
    for (int i = 0; i < 100; i++) {
      digitalWrite(PIN_BUZZER, HIGH);
      delay(2);
      digitalWrite(PIN_BUZZER, LOW);
      delay(2);
    }
  }

  Serial.print("ShowDialog: ");
  Serial.print(inputText);
  Serial.print(" | textWidth: ");
  Serial.println(textWidth);

  delay(delayTime);
  u8g2.clearDisplay();
}

void ShowLoadingScreen(){
  String strVer = "Ver 1.1";

  u8g2.clearBuffer();

  u8g2.setFont(BIG_FONT);
  u8g2.drawStr(12, 10, "PaperGo");

  u8g2.setFont(STATUS_FONT);
  u8g2.drawStr(2, 50, strVer.c_str());

  u8g2.sendBuffer();
  
  delay(2000);
}

void PrintUI() {
  u8g2.setFont(ICON_FONT);
  u8g2.drawLine(SCREEN_LINE_LEFT, SCREEN_LINE_TOP, SCREEN_LINE_LEFT, u8g2.getDisplayHeight() - SCREEN_LINE_TOP - 1);

  if (isSwitchMode){
    uint32_t countSec = 0;
    if (targetNextPageTime > 0) {
      u8g2.drawGlyph(4, u8g2.getDisplayHeight() / 15 * 7, 0x00d3);
      countSec = (millis() - activePageTime + totalPageTime) / 1000;
    } else {
      u8g2.drawGlyph(4, u8g2.getDisplayHeight() / 15 * 7, 0x00d2);
      countSec = totalPageTime / 1000;
    }
      
    int intHour = countSec / 3600;
    int intMinute = countSec / 60 % 60;
    int intSec = countSec % 60;

    String displayHour = intHour >= 10 ? String(intHour) :  "0" + String(intHour);
    String displayMinute = intMinute >= 10 ? String(intMinute) :  "0" + String(intMinute);
    String displaySec = intSec >= 10 ? String(intSec) : "0" + String(intSec); 
    
    String displayTime = "时间: " + displayHour + ":" + displayMinute + ":" + displaySec;
    String displayPage = "页数: " + String(totalPage);
    
    u8g2.setFont(CHINESE_FONT);
    u8g2.drawUTF8(SCREEN_LINE_LEFT + 5, u8g2.getDisplayHeight() / 20 * 4, displayTime.c_str());
    u8g2.drawUTF8(SCREEN_LINE_LEFT + 5, u8g2.getDisplayHeight() / 20 * 13, displayPage.c_str());
  } else {
    u8g2.drawGlyph(4, u8g2.getDisplayHeight() / 15 * 7, 0x00d9);
    u8g2.setFont(BIG_FONT);
    ShowCenterText(SCREEN_LINE_LEFT, u8g2.getDisplayWidth(), u8g2.getDisplayHeight() / 20 * 7, false, "READY");
  }
}

void SwitchPowerSave(bool inputTo)
{
  if (inputTo != screen_PowerSave)
  {
    screen_PowerSave = inputTo;
    u8g2.setPowerSave(screen_PowerSave);
  }
  
  if (!inputTo)
  {
    targetScreenOffTime = millis() + 1000 * SCREEN_TIMEOUT;
  }
}

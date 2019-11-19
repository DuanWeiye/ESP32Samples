// INCLUDE: COMMON
#include <M5StickC.h>

// DEFINE: PIN
#define PIN_REMOTE                          32

// DEFINE: BATTERY
#define BATTERY_MAX_V                       410
#define BATTERY_MIN_V                       320
#define BATTERY_STATUS_ONBATTERY            1
#define BATTERY_STATUS_CHARGING             2
#define BATTERY_STATUS_FULL                 3
#define BATTERY_STATUS_FAILED               0
#define BATTERY_UPDATE_INTERVAL_MSEC        3000

// DEFINE: SCREEN
#define SCREEN_UPDATE_INTERVAL_MSEC         500
#define SCREEN_BRIGHT                       9
#define SCREEN_WIDTH                        80
#define SCREEN_HEIGHT                       160
#define SCREEN_TITLE_TEXT_GAP               6
#define SCREEN_TITLE_TEXT_WIDTH             (SCREEN_WIDTH - 2 * SCREEN_TITLE_TEXT_GAP)
#define SCREEN_TITLE_TEXT_HEIGHT            32
#define SCREEN_INFO_TEXT_WIDTH              SCREEN_WIDTH
#define SCREEN_INFO_TEXT_HEIGHT             48
#define SCREEN_BATTERY_TEXT_WIDTH           SCREEN_WIDTH
#define SCREEN_BATTERY_TEXT_HEIGHT          28

#define TIME_PAGE_BASE_MS                   1500
#define TIME_RANDOM_MS                      8500

#define TOTAL_BASE_PERCENT                  1.1
#define TOTAL_MAX_PERCENT                   1.4

#define TOTAL_BASE_MIN                      30
#define TOTAL_BASE_PAGE                     300

// VAR: BASE
int battery_Status = BATTERY_STATUS_FAILED;
int battery_Status_last = BATTERY_STATUS_FAILED;
int battery_Percent = 0;
int battery_Percent_last = 0;
String battery_Title = String();
String battery_Text_V = String();
String battery_Text_A = String();

// VAR: PAGE
bool isSwitchMode = false;
bool isSwitchDone = false;
int totalPage = 1;
uint32_t activePageTime = 0;
uint32_t totalPageTime = 0;

uint32_t targetTotalPage = 0;
uint32_t targetTotalTime = 0;

// VAR: TIME
uint32_t lastUpdateStatusTime = -SCREEN_UPDATE_INTERVAL_MSEC;
uint32_t lastUpdateBatteryTime = -BATTERY_UPDATE_INTERVAL_MSEC;

uint32_t targetNextPageTime = 0;
uint32_t targetLongPressTime = 0;

// VAR: SCREEN
TFT_eSprite titleArea = TFT_eSprite(&M5.Lcd);
TFT_eSprite infoArea = TFT_eSprite(&M5.Lcd);
TFT_eSprite batteryArea = TFT_eSprite(&M5.Lcd);

void setup() 
{
  M5.begin();
 
  // LED init
  pinMode(M5_LED, OUTPUT);
  digitalWrite(M5_LED, HIGH);
  pinMode(PIN_REMOTE, OUTPUT);
  digitalWrite(PIN_REMOTE, LOW);

  // Screen Init
  M5.Axp.ScreenBreath(SCREEN_BRIGHT);
  M5.Lcd.setRotation(0);
  M5.Lcd.fillScreen(TFT_BLACK);

  titleArea.createSprite(SCREEN_TITLE_TEXT_WIDTH, SCREEN_TITLE_TEXT_HEIGHT);
  batteryArea.createSprite(SCREEN_BATTERY_TEXT_WIDTH, SCREEN_BATTERY_TEXT_HEIGHT);
  infoArea.createSprite(SCREEN_INFO_TEXT_WIDTH, SCREEN_INFO_TEXT_HEIGHT);
  
  DrawStatus();
  Serial.println("Done");
  Serial.flush();
} 

void loop() 
{
  M5.update();
  UpdateBatteryStatus();
  
  // Update Screen
  DrawStatus();
  
  // Page Time
  if (isSwitchMode){ 
    uint32_t totalMinute = GetTotalMinutes();
    if (totalPage >= targetTotalPage && totalMinute >= targetTotalTime) {
      if (!isSwitchDone && targetNextPageTime > 0) {
        isSwitchDone = true;
        targetNextPageTime = 0;
        
        totalPageTime += (millis() - activePageTime);
        activePageTime = 0;
      }
    } else {
      if (targetNextPageTime > 0 && targetNextPageTime < millis()) {
        targetNextPageTime = millis() + SwitchPage();
      }
    }
  }

  // Btn
  if (M5.BtnB.wasPressed()) {    
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
  
  if (M5.BtnA.wasPressed()) {
    if (targetNextPageTime > 0) {
      targetNextPageTime = 0;
      
      totalPageTime += (millis() - activePageTime);
      activePageTime = 0;
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

      activePageTime = millis();
      targetNextPageTime = millis() + 1000 * 2;
    }
  }  
}

uint32_t GetTotalMinutes(){
  return (millis() - activePageTime + totalPageTime) / 1000 / 60;
}

void DrawStatus() {
  if ((uint32_t)(millis() - lastUpdateStatusTime) < SCREEN_UPDATE_INTERVAL_MSEC){
    return;
  }
  lastUpdateStatusTime = millis();
  
  // Title
  titleArea.fillSprite(TFT_BLUE);
  titleArea.setTextColor(TFT_WHITE);

  titleArea.drawCentreString("PAPER", SCREEN_TITLE_TEXT_WIDTH / 2, 0, 2);
  titleArea.drawCentreString("GO", SCREEN_TITLE_TEXT_WIDTH / 2, 16, 2);
  titleArea.pushSprite(SCREEN_TITLE_TEXT_GAP, SCREEN_TITLE_TEXT_GAP);

  // Info
  infoArea.fillSprite(TFT_BLACK);
  if (isSwitchMode) {
    if (isSwitchDone){
      infoArea.drawCentreString("DONE", SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_TITLE_TEXT_HEIGHT / 2, 4);
    } else {
      if (targetNextPageTime == 0) {
        infoArea.drawCentreString("PAUSE", SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_TITLE_TEXT_HEIGHT / 2, 4);
      } else {
        if (isSwitchMode){
          uint32_t countSec = 0;
          if (targetNextPageTime > 0) {
            countSec = (millis() - activePageTime + totalPageTime) / 1000;
          } else {
            countSec = totalPageTime / 1000;
          }
          
          int intHour = countSec / 3600;
          int intMinute = countSec / 60 % 60;
          int intSec = countSec % 60;
      
          String displayHour = intHour >= 10 ? String(intHour) :  "0" + String(intHour);
          String displayMinute = intMinute >= 10 ? String(intMinute) :  "0" + String(intMinute);
          String displaySec = intSec >= 10 ? String(intSec) : "0" + String(intSec); 
          
          String displayTime = displayHour + ":" + displayMinute + ":" + displaySec;
          String displayPage = String(totalPage) + " p";

          infoArea.drawCentreString(displayTime, SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_TITLE_TEXT_HEIGHT / 4, 2);
          infoArea.drawCentreString(displayPage, SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_TITLE_TEXT_HEIGHT / 4 * 3, 2);
        }
      }
    }
  } else {
    infoArea.drawCentreString("IDLE", SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_TITLE_TEXT_HEIGHT / 2, 4);
  }
  infoArea.pushSprite(0, (SCREEN_HEIGHT - SCREEN_INFO_TEXT_HEIGHT) / 2);
  
  // Battery
  batteryArea.fillSprite(TFT_BLACK);
  batteryArea.setTextDatum(MC_DATUM);
  if (battery_Status == BATTERY_STATUS_CHARGING || 
      battery_Status == BATTERY_STATUS_FULL ) {
    batteryArea.setTextColor(TFT_GREEN);
  } else if (battery_Status == BATTERY_STATUS_ONBATTERY) {
    if (M5.Axp.GetWarningLeve() == 0){
      batteryArea.setTextColor(TFT_YELLOW);
    } else {
      batteryArea.setTextColor(TFT_RED);
    }
  } else {
    batteryArea.setTextColor(TFT_RED);
  }
  batteryArea.drawCentreString(battery_Title.c_str(), SCREEN_BATTERY_TEXT_WIDTH / 2, 0, 1);
  batteryArea.drawCentreString(battery_Text_V.c_str(), SCREEN_BATTERY_TEXT_WIDTH / 2, 10, 1);
  batteryArea.drawCentreString(battery_Text_A.c_str(), SCREEN_BATTERY_TEXT_WIDTH / 2, 20, 1);
  batteryArea.pushSprite(0, SCREEN_HEIGHT - (SCREEN_BATTERY_TEXT_HEIGHT * 1.2));
}

int SwitchPage() {
  totalPage++;

  int rndPageDelayMs = random(TIME_PAGE_BASE_MS, TIME_PAGE_BASE_MS + TIME_RANDOM_MS);
  int rndBtnDelayMs = random(100, 150);
  
  digitalWrite(PIN_REMOTE, HIGH);
  digitalWrite(M5_LED, LOW);
  delay(rndBtnDelayMs);
  digitalWrite(PIN_REMOTE, LOW);
  digitalWrite(M5_LED, HIGH);

  return rndPageDelayMs;
}

void UpdateBatteryStatus() {
  if ((uint32_t)(millis() - lastUpdateBatteryTime) < BATTERY_UPDATE_INTERVAL_MSEC){
    return;
  }
  lastUpdateBatteryTime = millis();
  
  double battV = M5.Axp.GetVbatData() * 1.1 / 1000;
  double chargeI = M5.Axp.GetIchargeData() / 2;
  double dischargeI = M5.Axp.GetIdischargeData() / 2;

  int battV_formatted = battV * 100;
  if (battV_formatted > BATTERY_MAX_V) {
    battV_formatted = BATTERY_MAX_V;
  } else if (battV_formatted < BATTERY_MIN_V) {
    battV_formatted = BATTERY_MIN_V;
  }
  
  battery_Percent = map(battV_formatted, BATTERY_MIN_V, BATTERY_MAX_V, 0, 100);

  battery_Title = String(battery_Percent) + "%";
  if (chargeI > 0 && dischargeI == 0){
    battery_Status = BATTERY_STATUS_CHARGING;
    battery_Text_V = String(battV, 2) + " V";
    battery_Text_A = "+ " + String(chargeI, 0) + " mA";
  } else if (chargeI == 0 && dischargeI == 0) {
    battery_Status = BATTERY_STATUS_FULL;
    battery_Text_V = String(battV, 2) + " V";
    battery_Text_A = "FULL";
  } else if (chargeI == 0 && dischargeI > 0) {
    battery_Status = BATTERY_STATUS_ONBATTERY;
    battery_Text_V = String(battV, 2) + " V";
    battery_Text_A = "- " + String(dischargeI, 0) + " mA";
  } else {
    battery_Status = BATTERY_STATUS_FAILED;
    battery_Title = String();
    battery_Text_V = "NO";
    battery_Text_A = "BATTERY";
  }

  /*
  Serial.print("UpdateBatteryStatus(): bStatus = ");
  Serial.print(battery_Status);
  Serial.print(", bPercent = ");
  Serial.print(battery_Percent);
  Serial.print(", battV = ");
  Serial.print(battV);
  Serial.print(", chargeI = ");
  Serial.print(chargeI);
  Serial.print(", dischargeI = ");
  Serial.print(dischargeI);
  Serial.print(", WarningLevel = ");
  Serial.println(M5.Axp.GetWarningLeve());
  */
  
  // Send Battery NOTIFY
  if (battery_Status != battery_Status_last || battery_Percent != battery_Percent_last)
  {
    battery_Status_last = battery_Status;
    battery_Percent_last = battery_Percent;
  }
}

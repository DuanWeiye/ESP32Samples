/*
 * Require library:
 * 
 * TinyGPSPlus
 * https://github.com/mikalhart/TinyGPSPlus
 * 
 */

#include <M5StickC.h>
#include <TinyGPS++.h>

// DEFINE: BATTERY
#define BATTERY_MAX_V                 410
#define BATTERY_MIN_V                 320
#define BATTERY_STATUS_ONBATTERY      1
#define BATTERY_STATUS_CHARGING       2
#define BATTERY_STATUS_FULL           3
#define BATTERY_STATUS_FAILED         0
#define BATTERY_UPDATE_INTERVAL_MSEC  3000

// DEFINE: SCREEN
#define STATUS_UPDATE_INTERVAL_MSEC   200
#define SCREEN_WIDTH                  160
#define SCREEN_HEIGHT                 80
#define SCREEN_TITLE_TEXT_H_GAP       6
#define SCREEN_TITLE_TEXT_W_GAP       12
#define SCREEN_TITLE_TEXT_WIDTH       (SCREEN_WIDTH - 2 * SCREEN_TITLE_TEXT_W_GAP)
#define SCREEN_TITLE_TEXT_HEIGHT      16
#define SCREEN_INFO_TEXT_WIDTH        SCREEN_WIDTH
#define SCREEN_INFO_TEXT_HEIGHT       40
#define SCREEN_INFO_TEXT_1_OF_2       12
#define SCREEN_INFO_TEXT_2_OF_2       28
#define SCREEN_INFO_TEXT_ONLY_1       20
#define SCREEN_BATTERY_TEXT_WIDTH     SCREEN_WIDTH
#define SCREEN_BATTERY_TEXT_HEIGHT    8

#define SCREEN_SHOW_MODE_LOCATION     0
#define SCREEN_SHOW_MODE_SATELITES    1
#define SCREEN_SHOW_MODE_ALT_N_SPD    2
#define SCREEN_SHOW_MODE_DATE_N_TIME  3

// VAR: SCREEN
int screen_Bright = 9;
uint32_t targetUpdateStatusTime = 0;
uint32_t targetUpdateBatteryTime = 0;

TFT_eSprite titleArea = TFT_eSprite(&M5.Lcd);
TFT_eSprite infoArea = TFT_eSprite(&M5.Lcd);
TFT_eSprite batteryArea = TFT_eSprite(&M5.Lcd);

// VAR: BASE
int battery_Status = BATTERY_STATUS_FAILED;
int battery_Status_last = BATTERY_STATUS_FAILED;
int battery_Percent = 0;
int battery_Percent_last = 0;
String battery_Text = String();

// VAR: GPS
HardwareSerial gpsSerial(2);
TinyGPSPlus gpsObject;
int showMode = SCREEN_SHOW_MODE_LOCATION;

void setup() {
  M5.begin();

  // init GPS
  gpsSerial.begin(9600, SERIAL_8N1, 33, 32);
  
  // init screen
  M5.Axp.ScreenBreath(screen_Bright);
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(TFT_BLACK);
  
  titleArea.createSprite(SCREEN_TITLE_TEXT_WIDTH, SCREEN_TITLE_TEXT_HEIGHT);
  batteryArea.createSprite(SCREEN_BATTERY_TEXT_WIDTH, SCREEN_BATTERY_TEXT_HEIGHT);
  infoArea.createSprite(SCREEN_INFO_TEXT_WIDTH, SCREEN_INFO_TEXT_HEIGHT);

  DrawStatus();

  Serial.println("Init done.");
  Serial.flush();
} 

void loop() {
  M5.update();
  
  UpdateBatteryStatus();
  UpdateGPSStatus();
  
  DrawStatus();
    
  if(M5.Axp.GetBtnPress() == 0x02) {
    Serial.println("BtnPWR.wasPressed");
  }

  if (M5.BtnA.wasPressed()) {
    Serial.println("BtnA.wasPressed");

    showMode += 1;
    if (showMode > SCREEN_SHOW_MODE_DATE_N_TIME) {
      showMode = SCREEN_SHOW_MODE_LOCATION;
    }
  }

  if (M5.BtnB.wasPressed()) {
    Serial.println("BtnB.wasPressed");
  }
  
  if (M5.BtnA.pressedFor(1000)) {
    Serial.println("BtnA.pressedFor(1000)");
  }
  
  if (M5.BtnB.pressedFor(1000)) {
    Serial.println("BtnB.pressedFor(1000)");
  }
}

void UpdateGPSStatus(){
  if(Serial.available()) {
    int ch = Serial.read();
    gpsSerial.write(ch);  
  }

  if(gpsSerial.available()) {
    char c = gpsSerial.read();
    gpsObject.encode(c);
    Serial.write(c);
  }
}

void DrawStatus() {
  if (targetUpdateStatusTime >= millis()){
    return;
  }
  
  // Title
  if (gpsObject.satellites.isValid() && gpsObject.satellites.value() > 0) {
    titleArea.fillSprite(TFT_GREEN);
    titleArea.setTextColor(TFT_BLACK);
  } else {
    titleArea.fillSprite(TFT_BLUE);
    titleArea.setTextColor(TFT_WHITE);
  }
  titleArea.drawCentreString("GPS INFO", SCREEN_TITLE_TEXT_WIDTH / 2, 0, 2);
  titleArea.pushSprite(SCREEN_TITLE_TEXT_W_GAP, SCREEN_TITLE_TEXT_H_GAP);

  // Info
  infoArea.fillSprite(TFT_BLACK);
  infoArea.setTextColor(TFT_WHITE);
  String textToShow = String();
  if (showMode == SCREEN_SHOW_MODE_LOCATION){
    if (gpsObject.location.isValid()){
      infoArea.drawCentreString("Latitude: " + String(gpsObject.location.lat(), 6), SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_INFO_TEXT_1_OF_2, 1);
      infoArea.drawCentreString("Longitude: " + String(gpsObject.location.lng(), 6), SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_INFO_TEXT_2_OF_2, 1);
    } else {
      infoArea.drawCentreString("Latitude: N/A", SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_INFO_TEXT_1_OF_2, 1);
      infoArea.drawCentreString("Longitude: N/A", SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_INFO_TEXT_2_OF_2, 1);
    }
  } else if (showMode == SCREEN_SHOW_MODE_SATELITES){
    if (gpsObject.satellites.isValid()){
      infoArea.drawCentreString("Satellites: " + String(gpsObject.satellites.value()), SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_INFO_TEXT_ONLY_1, 1);
    } else {
      infoArea.drawCentreString("Satellites: N/A", SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_INFO_TEXT_ONLY_1, 1);
    }
  } else if (showMode == SCREEN_SHOW_MODE_ALT_N_SPD){
    if (gpsObject.altitude.isValid()){
      infoArea.drawCentreString("Altitude: " + String(gpsObject.altitude.meters()), SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_INFO_TEXT_1_OF_2, 1);      
    } else {
      infoArea.drawCentreString("Aatitude: N/A", SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_INFO_TEXT_1_OF_2, 1);
    }

    if (gpsObject.speed.isValid()){
      infoArea.drawCentreString("Speed: " + String(gpsObject.speed.kmph()), SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_INFO_TEXT_2_OF_2, 1);      
    } else {
      infoArea.drawCentreString("Speed: N/A", SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_INFO_TEXT_2_OF_2, 1);
    }
  } else if (showMode == SCREEN_SHOW_MODE_DATE_N_TIME){
    if (gpsObject.date.isValid()){
      char gpsDate[32];
      sprintf(gpsDate, "%04d/%02d/%02d", gpsObject.date.year(), gpsObject.date.month(), gpsObject.date.day());
      infoArea.drawCentreString("Date: " + String(gpsDate), SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_INFO_TEXT_1_OF_2, 1);      
    } else {
      infoArea.drawCentreString("Date: N/A", SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_INFO_TEXT_1_OF_2, 1);
    }

    if (gpsObject.time.isValid()){
      char gpsTime[32];
      sprintf(gpsTime, "%02d:%02d:%02d", gpsObject.time.hour(), gpsObject.time.minute(), gpsObject.time.second());
      infoArea.drawCentreString("Time: " + String(gpsTime), SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_INFO_TEXT_2_OF_2, 1);      
    } else {
      infoArea.drawCentreString("Time: N/A", SCREEN_INFO_TEXT_WIDTH / 2, SCREEN_INFO_TEXT_2_OF_2, 1);
    }
  } else {
    //
  }
  infoArea.pushSprite(0, SCREEN_TITLE_TEXT_H_GAP + SCREEN_TITLE_TEXT_HEIGHT);
  
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
  batteryArea.drawCentreString(battery_Text.c_str(), SCREEN_BATTERY_TEXT_WIDTH / 2, 0, 1);
  batteryArea.pushSprite(0, SCREEN_HEIGHT - SCREEN_BATTERY_TEXT_HEIGHT);

  targetUpdateStatusTime = millis() + STATUS_UPDATE_INTERVAL_MSEC;
}

void UpdateBatteryStatus() {
  if (targetUpdateBatteryTime >= millis()){
    return;
  }

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

  battery_Text = String(battery_Percent) + "%, ";
  if (chargeI > 0 && dischargeI == 0){
    battery_Status = BATTERY_STATUS_CHARGING;
    battery_Text += String(battV, 2) + " V, +" + String(chargeI, 0) + " mA"; 
  } else if (chargeI == 0 && dischargeI == 0) {
    battery_Status = BATTERY_STATUS_FULL;
    battery_Text += String(battV, 2) + " V, 0 mA";
  } else if (chargeI == 0 && dischargeI > 0) {
    battery_Status = BATTERY_STATUS_ONBATTERY;
    battery_Text += String(battV, 2) + " V, -" + String(dischargeI, 0) + " mA";
  } else {
    battery_Status = BATTERY_STATUS_FAILED;
    battery_Text = "NO BATTERY";
  }

  // Send Battery NOTIFY
  if (battery_Status != battery_Status_last || battery_Percent != battery_Percent_last)
  {
    battery_Status_last = battery_Status;
    battery_Percent_last = battery_Percent;
  }

  targetUpdateBatteryTime = millis() + BATTERY_UPDATE_INTERVAL_MSEC;
}

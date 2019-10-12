/*
 * Require library:
 * 
 * ESP32 BLE Mouse library
 * https://github.com/T-vK/ESP32-BLE-Mouse
 * 
 */

#include <M5StickC.h>
#include <BleMouse.h>
#include "Wire.h"

// DEFINE: PIN
#define PIN_SCL                       26
#define PIN_SDA                       0

// DEFINE: MOUSE
#define MOUSE_MOVE_SPEED_MIN          10
#define MOUSE_MOVE_SPEED_MAX          30

// DEFINE: BATTERY
#define BATTERY_MAX_V                 410
#define BATTERY_MIN_V                 320
#define BATTERY_STATUS_ONBATTERY      1
#define BATTERY_STATUS_CHARGING       2
#define BATTERY_STATUS_FULL           3
#define BATTERY_STATUS_FAILED         0
#define BATTERY_UPDATE_INTERVAL_MSEC  3000

// DEFINE: SCREEN
#define STATUS_UPDATE_INTERVAL_MSEC   500
#define SCREEN_WIDTH                  80
#define SCREEN_HEIGHT                 160
#define SCREEN_TITLE_TEXT_GAP         6
#define SCREEN_TITLE_TEXT_WIDTH       (SCREEN_WIDTH - 2 * SCREEN_TITLE_TEXT_GAP)
#define SCREEN_TITLE_TEXT_HEIGHT      32
#define SCREEN_INFO_TEXT_WIDTH        SCREEN_WIDTH
#define SCREEN_INFO_TEXT_HEIGHT       32
#define SCREEN_BATTERY_TEXT_WIDTH     SCREEN_WIDTH
#define SCREEN_BATTERY_TEXT_HEIGHT    28

// DEFINE: JOYSTICK
#define JOYSTICK_ADDRESS              0x38
#define JOYSTICK_UPDATE_INTERVAL_MSEC 50
#define JOYSTICK_X_ACC_TRIGGER_MIN    10
#define JOYSTICK_X_ACC_TRIGGER_MAX    110
#define JOYSTICK_Y_ACC_TRIGGER_MIN    10
#define JOYSTICK_Y_ACC_TRIGGER_MAX    110

// VAR: SCREEN
int screen_Bright = 9;
uint32_t targetUpdateStatusTime = 0;
uint32_t targetUpdateBatteryTime = 0;
uint32_t targetUpdateJoystickTime = 0;

TFT_eSprite titleArea = TFT_eSprite(&M5.Lcd);
TFT_eSprite infoArea = TFT_eSprite(&M5.Lcd);
TFT_eSprite batteryArea = TFT_eSprite(&M5.Lcd);

// VAR: MOUSE
BleMouse bleMouse("ESP32-BLEMouse", "-", 100);
bool isMouseEnabled = false;
bool isMouseMoving = false;
bool bleMouseLastState = false;
String cmdLine = String();

// VAR: BASE
int battery_Status = BATTERY_STATUS_FAILED;
int battery_Status_last = BATTERY_STATUS_FAILED;
int battery_Percent = 0;
int battery_Percent_last = 0;
String battery_Title = String();
String battery_Text_V = String();
String battery_Text_A = String();

// VAR: JOYSTICK
bool isWireReady = false;
int8_t accX = 0;
int8_t accY = 0;
int8_t accZ = 0;

void setup() {

  // M5 init
  //bool LCDEnable=true, bool PowerEnable=true, bool SerialEnable=true
  M5.begin();
  Wire.begin(PIN_SDA, PIN_SCL, 100000);
  
  // LED init
  pinMode(M5_LED, OUTPUT);
  digitalWrite(M5_LED, HIGH);
  
  M5.Axp.ScreenBreath(screen_Bright);
  M5.Lcd.setRotation(0);
  M5.Lcd.fillScreen(TFT_BLACK);
  
  titleArea.createSprite(SCREEN_TITLE_TEXT_WIDTH, SCREEN_TITLE_TEXT_HEIGHT);
  batteryArea.createSprite(SCREEN_BATTERY_TEXT_WIDTH, SCREEN_BATTERY_TEXT_HEIGHT);
  infoArea.createSprite(SCREEN_INFO_TEXT_WIDTH, SCREEN_INFO_TEXT_HEIGHT);
  
  DrawStatus();
    
  Serial.print("Starting BLE...");

  isMouseEnabled = false;
  bleMouseLastState = bleMouse.isConnected();
  bleMouse.begin();
  
  Serial.println("Done");
  Serial.flush();
}

void loop() {
  M5.update();
  
  UpdateBatteryStatus();
  //UpdateJoystickStatus();
  
  HandleJoyStickData();
  HandleMouseMove();
  
  DrawStatus();
    
  if(M5.Axp.GetBtnPress() == 0x02) {
    Serial.println("BtnPWR.wasPressed");
  }
  
  if (M5.BtnA.wasPressed()) {
    //JoystickCalibrationMax();
    //JoystickCalibrationSave();
    Serial.println("BtnA.wasPressed");
    
    if (bleMouse.isConnected()) {
      isMouseEnabled = !isMouseEnabled;
      digitalWrite(M5_LED, LOW);
      delay(20);
      digitalWrite(M5_LED, HIGH);
    }
  }

  if (M5.BtnB.wasPressed()) {
    Serial.println("BtnB.wasPressed");
        
    //JoystickCalibrationCenter();
    //JoystickCalibrationSave();
    
    if (bleMouse.isConnected() && isMouseEnabled){
      bleMouse.click(MOUSE_RIGHT);
    }
  }
  
  if (M5.BtnA.pressedFor(1000)) {
    Serial.println("BtnA.pressedFor(1000)");
  }
  
  if (M5.BtnB.pressedFor(1000)) {
    Serial.println("BtnB.pressedFor(1000)");
  }
}

void HandleMouseMove() {
  if (Serial.available()) {
    cmdLine = String();
    while (Serial.available()) {
      char buf = Serial.read();
      if (buf != '\n') {
        cmdLine += String(buf);
      }
    }
  }

  if (bleMouseLastState != bleMouse.isConnected()) {
    cmdLine = String();
    bleMouseLastState = bleMouse.isConnected();

    if (bleMouse.isConnected()) {
      Serial.println("HandleMouseMove(): Device connected.");
    } else {
      Serial.println("HandleMouseMove(): Device disconnected.");
    }
  }
  
  if (bleMouse.isConnected() && cmdLine.length() > 0) {
    int moveX = 0;
    int moveY = 0;
    int clickBtn = 0;
    int wheelRoll = 0;
    long cmdNumbers = SplitRawCmdNumbers(cmdLine);

    Serial.print("HandleMouseMove(): Get cmd[");
    Serial.print(cmdLine.length());
    Serial.print("]: ");
    Serial.println(cmdLine);
    
    if (cmdLine.startsWith("X")) {
      moveX = cmdNumbers;
    } else if (cmdLine.startsWith("Y")) {
      moveY = cmdNumbers;
    } else if (cmdLine.startsWith("B")) {
      //#define MOUSE_LEFT 1
      //#define MOUSE_RIGHT 2
      //#define MOUSE_MIDDLE 4
      clickBtn = cmdNumbers;
    } else if (cmdLine.startsWith("W")) {
      wheelRoll = cmdNumbers;
    } else {
      // nothing todo
    }

    if (moveX != 0 || moveY != 0 || wheelRoll != 0) {
      Serial.print("HandleMouseMove(): Move: X = ");
      Serial.print(moveX);
      Serial.print(", Y = ");
      Serial.print(moveY);
      Serial.print(", Wheel = ");
      Serial.println(wheelRoll);
      
      bleMouse.move(moveX, moveY, wheelRoll);
    }
    
    if (clickBtn != 0) {
      Serial.print("HandleMouseMove(): Btn = ");
      Serial.println(clickBtn);
      
      bleMouse.click(clickBtn);
    }

    cmdLine = String();
  }
}

void DrawStatus() {
  if (targetUpdateStatusTime >= millis()){
    return;
  }
  
  // Title
  if (bleMouse.isConnected()) {
    titleArea.fillSprite(TFT_GREEN);
    titleArea.setTextColor(TFT_BLACK);
  } else {
    titleArea.fillSprite(TFT_BLUE);
    titleArea.setTextColor(TFT_WHITE);
  }
  titleArea.drawCentreString("BLE", SCREEN_TITLE_TEXT_WIDTH / 2, 0, 2);
  titleArea.drawCentreString("MOUSE", SCREEN_TITLE_TEXT_WIDTH / 2, 16, 2);
  titleArea.pushSprite(SCREEN_TITLE_TEXT_GAP, SCREEN_TITLE_TEXT_GAP);

  // Info
  infoArea.fillSprite(TFT_BLACK);
  if (bleMouse.isConnected() && isMouseEnabled){
    infoArea.setTextColor(TFT_WHITE);
    if (isMouseMoving) {
      infoArea.drawCentreString("MOVE", SCREEN_INFO_TEXT_WIDTH / 2, 0, 4);  
    } else {
      infoArea.drawCentreString("IDLE", SCREEN_INFO_TEXT_WIDTH / 2, 0, 4);  
    }
  } else {
    infoArea.drawCentreString("-", SCREEN_INFO_TEXT_WIDTH / 2, 0, 4);  
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

  targetUpdateStatusTime = millis() + STATUS_UPDATE_INTERVAL_MSEC;
}


void JoystickCalibrationCenter(){
  Wire.beginTransmission(JOYSTICK_ADDRESS);
  Wire.write(0x03);
  Wire.write(0x01);
  Wire.endTransmission();

  Serial.print("JoystickCalibrationCenter(): Done");
}

void JoystickCalibrationMax(){
  Wire.beginTransmission(JOYSTICK_ADDRESS);
  Wire.write(0x03);
  Wire.write(0x02);
  Wire.endTransmission();

  Serial.print("JoystickCalibrationMax(): Done");
}

void JoystickCalibrationSave(){
  Wire.beginTransmission(JOYSTICK_ADDRESS);
  Wire.write(0x03);
  Wire.write(0x03);
  Wire.endTransmission();
  
  Serial.print("JoystickCalibrationSave(): Done");
}


void UpdateJoystickStatus(){
  Wire.beginTransmission(JOYSTICK_ADDRESS);
  Wire.write(0x02);
  Wire.endTransmission();
  Wire.requestFrom(JOYSTICK_ADDRESS, 3);
  
  if (Wire.available()) {
    isWireReady = true;
    accX = Wire.read();
    accY = Wire.read();
    accZ = Wire.read();
    
    Serial.print("UpdateJoystickStatus(): accX = ");
    Serial.print(accX);
    Serial.print(", accY = ");
    Serial.print(accY);
    Serial.print(", accZ = ");
    Serial.println(accZ);
  } else {
    isWireReady = false;
    
    accX = 0;
    accY = 0;
    accZ = 0;
  }
}

void HandleJoyStickData(){
  if (targetUpdateJoystickTime >= millis()){
    return;
  }  
  
  UpdateJoystickStatus();
  if (bleMouse.isConnected() && isMouseEnabled && isWireReady) {   
    int moveX = 0;
    int moveY = 0;
    
    // X
    if (abs(accX) < JOYSTICK_X_ACC_TRIGGER_MIN){
      moveX = 0;
    } else if (abs(accX) > JOYSTICK_X_ACC_TRIGGER_MAX) {
      moveX = MOUSE_MOVE_SPEED_MAX * (accX / abs(accX));
    } else {
      moveX = map(abs(accX), JOYSTICK_X_ACC_TRIGGER_MIN, JOYSTICK_X_ACC_TRIGGER_MAX, MOUSE_MOVE_SPEED_MIN, MOUSE_MOVE_SPEED_MAX) * (accX / abs(accX));
    }

    // Y
    if (abs(accY) < JOYSTICK_Y_ACC_TRIGGER_MIN){
      moveY = 0;
    } else if (abs(accY) > JOYSTICK_Y_ACC_TRIGGER_MAX) {
      moveY = -MOUSE_MOVE_SPEED_MAX * (accY / abs(accY));
    } else {
      moveY = -map(abs(accY), JOYSTICK_Y_ACC_TRIGGER_MIN, JOYSTICK_Y_ACC_TRIGGER_MAX, MOUSE_MOVE_SPEED_MIN, MOUSE_MOVE_SPEED_MAX) * (accY / abs(accY));
    }

    if (accZ == 0){
      bleMouse.click(MOUSE_LEFT);
    }
    
    if (moveX != 0 || moveY != 0) {
      isMouseMoving = true;
      
      Serial.print("HandleJoyStickData(): Move: X = ");
      Serial.print(moveX);
      Serial.print(", Y = ");
      Serial.println(moveY);
      
      bleMouse.move(moveX, moveY);
    } else {
      isMouseMoving = false;
    }
  } else {
    isMouseMoving = false;
  }
  
  targetUpdateJoystickTime = millis() + JOYSTICK_UPDATE_INTERVAL_MSEC;
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

  // Send Battery NOTIFY
  if (battery_Status != battery_Status_last || battery_Percent != battery_Percent_last)
  {
    battery_Status_last = battery_Status;
    battery_Percent_last = battery_Percent;

    bleMouse.setBatteryLevel(battery_Percent);
  }

  targetUpdateBatteryTime = millis() + BATTERY_UPDATE_INTERVAL_MSEC;
}

long SplitRawCmdNumbers(String inputRawCmd) {
  String tmpStr = inputRawCmd;
  tmpStr.remove(0, 1);
  return tmpStr.toInt();
}
  

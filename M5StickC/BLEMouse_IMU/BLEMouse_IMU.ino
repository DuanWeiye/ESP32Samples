/*
 * Require library:
 * 
 * ESP32 BLE Mouse library
 * https://github.com/T-vK/ESP32-BLE-Mouse
 * 
 */

#include <M5StickC.h>
#include <BleMouse.h>

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

// DEFINE: IMU
#define IMU_UPDATE_INTERVAL_MSEC      20
#define IMU_X_ACC_TRIGGER_MIN         2000
#define IMU_X_ACC_TRIGGER_MAX         4000
#define IMU_Y_ACC_TRIGGER_MIN         1000
#define IMU_Y_ACC_TRIGGER_MAX         3000

// VAR: SCREEN
int screen_Bright = 9;
uint32_t targetUpdateStatusTime = 0;
uint32_t targetUpdateBatteryTime = 0;
uint32_t targetUpdateIMUTime = 0;

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

// VAR: IMU
int16_t gyroX = 0;
int16_t gyroY = 0;
int16_t gyroZ = 0;

int16_t accX = 0;
int16_t accY = 0;
int16_t accZ = 0;

int16_t accX_offset = -1200; // MIN 0
int16_t accY_offset = -450;  // MIN 0
int16_t accZ_offset = -400;  // MAX 4096

int16_t accX_final = 0; // MIN 0
int16_t accY_final = 0; // MIN 0
int16_t accZ_final = 0; // MAX 4096

void setup() {

  // M5 init
  //bool LCDEnable=true, bool PowerEnable=true, bool SerialEnable=true
  M5.begin();

  // LED init
  pinMode(M5_LED, OUTPUT);
  digitalWrite(M5_LED, HIGH);

  // IMU init
  M5.IMU.Init();

  UpdateIMUStatus();
  
  M5.Axp.ScreenBreath(screen_Bright);
  M5.Lcd.setRotation(2);
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

  HandleIMUData();
  HandleMouseMove();
  
  DrawStatus();

  if(M5.Axp.GetBtnPress() == 0x02) {
    Serial.println("BtnPWR.wasPressed");

    if (bleMouse.isConnected() && isMouseEnabled){
      bleMouse.click(MOUSE_RIGHT);
    }
  }
  
  if (M5.BtnA.wasPressed()) {
    Serial.println("BtnA.wasPressed");

    if (bleMouse.isConnected() && isMouseEnabled){
      bleMouse.click(MOUSE_LEFT);
    }
  }

  if (M5.BtnB.wasPressed()) {
    Serial.println("BtnB.wasPressed");

    if (bleMouse.isConnected()) {
      isMouseEnabled = !isMouseEnabled;
      digitalWrite(M5_LED, LOW);
      delay(20);
      digitalWrite(M5_LED, HIGH);
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

void UpdateIMUStatus(){
  M5.IMU.getGyroAdc(&gyroX, &gyroY, &gyroZ);
  M5.IMU.getAccelAdc(&accX, &accY, &accZ);

  accX_final = accX + accX_offset;
  accY_final = accY + accY_offset;
  accZ_final = accZ + accZ_offset;
}

void HandleIMUData(){
  if (targetUpdateIMUTime >= millis()){
    return;
  }

  if (bleMouse.isConnected() && isMouseEnabled) {
    UpdateIMUStatus();
    
    int moveX = 0;
    int moveY = 0;
    
    // X
    if (abs(accX_final) < IMU_X_ACC_TRIGGER_MIN){
      moveX = 0;
    } else if (abs(accX_final) > IMU_X_ACC_TRIGGER_MAX) {
      moveX = MOUSE_MOVE_SPEED_MAX * (accX_final / abs(accX_final));
    } else {
      moveX = map(abs(accX_final), IMU_X_ACC_TRIGGER_MIN, IMU_X_ACC_TRIGGER_MAX, MOUSE_MOVE_SPEED_MIN, MOUSE_MOVE_SPEED_MAX) * (accX_final / abs(accX_final));
    }

    // Y
    if (abs(accY_final) < IMU_Y_ACC_TRIGGER_MIN){
      moveY = 0;
    } else if (abs(accY_final) > IMU_Y_ACC_TRIGGER_MAX) {
      moveY = -MOUSE_MOVE_SPEED_MAX * (accY_final / abs(accY_final));
    } else {
      moveY = -map(abs(accY_final), IMU_Y_ACC_TRIGGER_MIN, IMU_Y_ACC_TRIGGER_MAX, MOUSE_MOVE_SPEED_MIN, MOUSE_MOVE_SPEED_MAX) * (accY_final / abs(accY_final));
    }
    
    if (moveX != 0 || moveY != 0) {
      isMouseMoving = true;
      
      Serial.print("HandleIMUData(): Move: X = ");
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
  
  /*
  Serial.print("DrawStatus(): gyroX = ");
  Serial.print(gyroX);
  Serial.print(", gyroY = ");
  Serial.print(gyroY);
  Serial.print(", gyroZ = ");
  Serial.print(gyroZ);
  Serial.print(", gRes(x1000) = ");
  Serial.println(M5.IMU.gRes * 1000);
    
  Serial.print("DrawStatus(): accX_final = ");
  Serial.print(accX_final);
  Serial.print(", accY_final = ");
  Serial.print(accY_final);
  Serial.print(", accZ_final = ");
  Serial.print(accZ_final);
  Serial.print(", aRes(x1000) = ");
  Serial.println(M5.IMU.aRes * 1000);
   */ 
  targetUpdateIMUTime = millis() + IMU_UPDATE_INTERVAL_MSEC;
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

    bleMouse.setBatteryLevel(battery_Percent);
  }

  targetUpdateBatteryTime = millis() + BATTERY_UPDATE_INTERVAL_MSEC;
}

long SplitRawCmdNumbers(String inputRawCmd) {
  String tmpStr = inputRawCmd;
  tmpStr.remove(0, 1);
  return tmpStr.toInt();
}
  

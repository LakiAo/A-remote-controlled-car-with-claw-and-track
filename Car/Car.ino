#include "Motor.h"
#include <PS2X_lib.h>//按键名PSAB_PAD_RIGHT、PSAB_PAD_UP、PSAB_PAD_DOWN、PSAB_PAD_LEFT、PSAB_L2、PSAB_R2、PSAB_L1、PSAB_R1、PSAB_GREEN、PSAB_RED、PSAB_BLUE、PSAB_PINK、PSAB_TRIANGLE、PSAB_CIRCLE、PSAB_CROSS、PSAB_SQUARE
#include <Wire.h>
#include "FastLED.h"            
#include <Adafruit_PWMServoDriver.h>
#define PS2_DAT 13
#define PS2_CMD 11
#define PS2_SEL 10
#define PS2_CLK 12
#define pressures true
#define rumble true
#define track1 35  //循迹引脚
#define track2 37
#define track3 33
#define track4 31

#define LED_DT  6           // 定义控制LED的引脚
#define NUM_LEDS  19          // 定义LED数量
#define LED_TYPE WS2812         // LED灯带型号
#define COLOR_ORDER GRB         // RGB灯珠中红色、绿色、蓝色LED的排列顺序
CRGB leds[NUM_LEDS];            // 创建LED数组
CRGB myRGBcolor(50,50,50);   //myRGBcolor（红色数值，绿色数值，蓝色数值）

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
Motor motor(22, 24, 28, 26, 30, 32, 36, 34, 2, 3, 4, 5);

int speedPinA = 8;
int speedPinB = 9;

PS2X ps2x;

int line = 0;//黑线1 白线0
int underground = 1;//黑1 白0
int error = 0;
byte type = 0;
byte vibrate = 0;
int servoMin = 10;
int servoMax = 500;
int begin1 = 330;  //爪子 320-450（后续只设置到400是因为已经够大了）
int begin2 = 90;  //几把 80~335(80上 335下)
int begin3 = 80;   //begin3和4不懂有什么用 先留着吧
int begin4 = 280;
int Sensor[4] = { 0, 0, 0, 0 };  //初始化循迹的值
static int lastTurnDirection = 0;

void setup() {
  motor.speed(100);
  Serial.begin(9600);
  error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, pressures, rumble);
  pwm.begin();
  pwm.setPWMFreq(50);
  servos_begin();
  Track_Init();  //循迹模块初始化
  LEDS.addLeds<LED_TYPE, LED_DT, COLOR_ORDER>(leds, NUM_LEDS);  // 初始化光带
  FastLED.setBrightness(255);                            // 设置光带亮度
  set_max_power_in_volts_and_milliamps(5, 500);                 // 光带电源管理（设置光带5伏特，500mA）
}

void loop() {
  //aprint();  //打印循迹的高低电平
  fill_solid(leds, NUM_LEDS, myRGBcolor); 
  myRGBcolor.r+=3;
  myRGBcolor.g+=5;
  myRGBcolor.b+=7;
  FastLED.show();
  if (error == 1)
    return;

  if (type == 2) {
    return;
  } else {
    ps2x.read_gamepad(false, vibrate);

    byte LY = ps2x.Analog(PSS_LY);
    byte LX = ps2x.Analog(PSS_LX);
    byte RY = ps2x.Analog(PSS_RY);
    byte RX = ps2x.Analog(PSS_RX);

    if (LY < 110) {
      motor.speed(250 - LY);
      motor.forward();
      delay(50);
    }

    if (LY > 140) {
      motor.speed(LY - 10);
      motor.backward();
      delay(50);
    }

    if (LX < 110) {
      motor.speed(200);
      motor.left();
      delay(50);
    }

    if (RX > 230) {
      motor.speed(200);
      motor.horizontal_L();
      delay(50);
    }

    if (RX < 20) {
      motor.speed(200);
      motor.horizontal_R();
      delay(50);
    }

    if (LX > 140) {
      motor.speed(200);
      motor.right();
      delay(50);
    }

    if (RY > 133) {
      begin2 = begin2 - 8;
      pwm.setPWM(1, 0, begin2);
      if (begin2 < 85) {
        begin2 = 84;
      }
    }

    if (RY < 123) {
      begin2 = begin2 + 8;
      pwm.setPWM(1, 0, begin2);
      if (begin2 > 335) {
        begin2 = 334;
      }
    }

    if (LY >= 110 && LY <= 140 && LX >= 110 && LX <= 140) {
      motor.stop();
      delay(50);
    }

    if (ps2x.Button(PSB_L1)) {
      begin1 = begin1 + 8;
      pwm.setPWM(0, 0, begin1);
      if (begin1 > 400) {
        begin1 = 399;
      }
    }

    if (ps2x.Button(PSB_L2)) {
      while (1) {
        ps2x.read_gamepad(false, vibrate);
        Sensor_Read();  //不断地读取循迹模块的高低电平
        delay(50);
        follow_line();
        if (ps2x.Button(PSB_R2)) {
          break;
        }
      }
    }

    if (ps2x.Button(PSB_R1)) {
      begin1 = begin1 - 8;
      pwm.setPWM(0, 0, begin1);
      Serial.print(begin1);
      if (begin1 < 325) {
        begin1 = 324;
      }
    }
    if (ps2x.Button(UP_STRUM)) {//一键抬起
      Serial.print("抬起");
      if(begin2 > 130){
        begin2 = begin2 - 50;
        pwm.setPWM(1, 0, begin2);
      }else{
        pwm.setPWM(1, 0, 80);
      }
    }
  }
}

void servos_begin() {
  pwm.setPWM(0, 0, begin1);
  pwm.setPWM(1, 0, begin2);
  pwm.setPWM(2, 0, begin3);
  pwm.setPWM(3, 0, begin4);
}

void Track_Init() {
  //循迹模块D0引脚初始化，设置为输入模式
  pinMode(track1, INPUT);
  pinMode(track2, INPUT);
  pinMode(track3, INPUT);
  pinMode(track4, INPUT);
}

void Sensor_Read() {
  Sensor[0] = digitalRead(track1);  //检测到黑线为高电平（1），白线为低电平（0)
  Sensor[1] = digitalRead(track2);
  Sensor[2] = digitalRead(track3);
  Sensor[3] = digitalRead(track4);
}

void follow_line() {

  pwm.setPWM(1, 0, 105);
  if (Sensor[0] == underground && Sensor[1] == line && Sensor[2] == line && Sensor[3] == underground) {
    motor.speed(110);
    motor.backward();
    Serial.print("直行\n");
    lastTurnDirection = 0;
  } 
  // 小左转
  else if (Sensor[0] == underground && Sensor[1] == line && Sensor[2] == underground && Sensor[3] == underground) {
    motor.speed(110);
    motor.right();
    Serial.print("小左转\n");
    lastTurnDirection = -1;
  }
  // 小右转
  else if (Sensor[0] == underground && Sensor[1] == underground && Sensor[2] == line && Sensor[3] == underground) {
    motor.speed(110);
    motor.left();
    Serial.print("小右转\n");
    lastTurnDirection = 1;
  }
  // 大左转
  
  else if (Sensor[0] == line && Sensor[1] == underground && Sensor[2] == underground && Sensor[3] == underground) {
    motor.speed(110);
    motor.right();
    Serial.print("大左转\n");
    lastTurnDirection = -1;
  }
  // 大右转
   else if (Sensor[0] == underground && Sensor[1] == line && Sensor[2] == line && Sensor[3] == line) {
    motor.speed(110);
    motor.left();
    Serial.print("急急急右转\n");
    lastTurnDirection = 1;
  }
  // 急急急右转
  
  else if (Sensor[0] == line && Sensor[1] == line && Sensor[2] == line && Sensor[3] == underground) {
    motor.speed(110);
    motor.right();
    Serial.print("急急急左转\n");
    lastTurnDirection = -1;
  }
  // 急急急左转
  else if (Sensor[0] == underground && Sensor[1] == underground && Sensor[2] == underground && Sensor[3] == line) {
    motor.speed(110);
    motor.left();
    Serial.print("大右转\n");
    lastTurnDirection = 1;
  }
  // 左急转弯
  else if (Sensor[0] == line && Sensor[1] == line && Sensor[2] == underground && Sensor[3] == underground) {
    motor.speed(115);
    motor.right();
    Serial.print("左急转弯\n");
    lastTurnDirection = -1;
  }
  // 右急转弯
  else if (Sensor[0] == underground && Sensor[1] == underground && Sensor[2] == line && Sensor[3] == line) {
    motor.speed(115);
    motor.left();
    Serial.print("右急转弯\n");
    lastTurnDirection = 1;
  }
  // 直角弯（所有传感器都丢失线）
  else if (Sensor[0] == underground && Sensor[1] == underground && Sensor[2] == underground && Sensor[3] == underground) {
    if (lastTurnDirection == -1) {
      motor.speed(110);
      motor.right();
      Serial.print("继续左转\n");
    } else if (lastTurnDirection == 1) {
      motor.speed(110);
      motor.left();
      Serial.print("继续右转\n");
    } else {
      motor.speed(100);
      motor.backward();
      Serial.print("搜索线\n");
    }
  }
  else if (Sensor[0] == line && Sensor[1] == line && Sensor[2] == line && Sensor[3] == line) {
    if (lastTurnDirection == -1) {
      motor.speed(110);
      motor.right();
      Serial.print("继续左转\n");
    } else if (lastTurnDirection == 1) {
      motor.speed(110);
      motor.left();
      Serial.print("继续右转\n");
    } else {
      motor.speed(100);
      motor.backward();
      Serial.print("搜索线\n");
    }
  }
  // 如果其他所有传感器组合都未满足条件，可以选择一个默认行为，比如停止或者直行
  else {
    motor.speed(100);
    motor.backward();
    Serial.print("默认行为：直行\n");
  }
  // 这里没有使用delay，以便更快地响应传感器的变化
}
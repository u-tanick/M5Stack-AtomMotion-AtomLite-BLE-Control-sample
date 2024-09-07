#include <M5Unified.h>
#include <M5AtomicMotion.h>
#include <FastLED.h>

M5AtomicMotion AtomicMotion;

#define LED_PIN       27
#define NUM_LEDS      1
static CRGB leds[NUM_LEDS];

boolean doMotion_servo = false;
boolean doMotion_motor = false;

uint8_t ch = 0;
uint8_t speed = 90;

void setLed(CRGB color) {
  // change RGB to GRB
  uint8_t t = color.r;
  color.r = color.g;
  color.g = t;
  leds[0] = color;
  FastLED.show();
}

void setup()
{
  auto cfg = M5.config();
  M5.begin(cfg);    // M5Stackをcfgの設定で初期化
  uint8_t sda = 25; // AtomLite
  uint8_t scl = 21; // AtomLite
  // uint8_t sda = 38; // AtomS3
  // uint8_t scl = 39; // AtomS3

  while (
      !AtomicMotion.begin(&Wire, M5_ATOMIC_MOTION_I2C_ADDR, sda, scl, 100000))
  {
    Serial.println("Atomic Motion begin failed");
    delay(1000);
  }

  FastLED.addLeds<WS2811, LED_PIN, RGB>(leds, NUM_LEDS);
  FastLED.setBrightness(255 * 15 / 100);

  for (int i = 0; i < 4; i++){
    setLed(CRGB::Green);
    delay(500);
    setLed(CRGB::Black);
    delay(500);
  }

  Serial.println("Atomic Motion Test");
}

void loop()
{
  if (M5.BtnA.wasPressed())
  {
    doMotion_servo = !doMotion_servo;
    doMotion_motor = !doMotion_motor;
    setLed(CRGB::White);
    delay(2000);
    setLed(CRGB::Black);
  }

  if (doMotion_motor)
  {
    // for (int ch = 0; ch < 2; ch++)
    // {
    setLed(CRGB::Red);
    delay(2000);
    AtomicMotion.setMotorSpeed(ch, speed);
    Serial.printf("Motor Channel %d: %d \n", ch,
                  AtomicMotion.getMotorSpeed(ch));
    // }
    delay(1500);
    AtomicMotion.setMotorSpeed(ch, 0);
    Serial.printf("Motor Channel %d: %d \n", ch,
                  AtomicMotion.getMotorSpeed(ch));
  }

  delay(1000);
  // if (doMotion_servo)
  // {
  //   setLed(CRGB::Blue);
  //   for (int ch = 0; ch < 4; ch++)
  //   {
  //     AtomicMotion.setServoAngle(ch, 180);
  //     Serial.printf("Servo Channel %d: %d \n", ch,
  //                   AtomicMotion.getServoAngle(ch));
  //   }
  //   delay(1000);
  //   for (int ch = 0; ch < 4; ch++)
  //   {
  //     AtomicMotion.setServoAngle(ch, 90);
  //     Serial.printf("Servo Channel %d: %d \n", ch,
  //                   AtomicMotion.getServoAngle(ch));
  //   }
  //   delay(1000);
  //   setLed(CRGB::Red);
  //   for (int ch = 0; ch < 4; ch++)
  //   {
  //     AtomicMotion.setServoAngle(ch, 0);
  //     Serial.printf("Servo Channel %d: %d \n", ch,
  //                   AtomicMotion.getServoAngle(ch));
  //   }
  //   delay(1000);
  //   for (int ch = 0; ch < 4; ch++)
  //   {
  //     AtomicMotion.setServoAngle(ch, 90);
  //     Serial.printf("Servo Channel %d: %d \n", ch,
  //                   AtomicMotion.getServoAngle(ch));
  //   }
  //   delay(1000);
  //   setLed(CRGB::Black);
  // }

  M5.update();
}

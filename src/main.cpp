#include <M5Unified.h>
#include <M5AtomicMotion.h>
#include <FastLED.h>

M5AtomicMotion AtomicMotion;

// #define PROD_OR_TEST
#ifdef PROD_OR_TEST
constexpr bool prod_flg = true;
#else
constexpr bool prod_flg = false;
#endif

boolean doMotion_flg = false;

uint8_t ch = 0;
uint8_t speed = 90;

#define LED_PIN 27
#define NUM_LEDS 1
static CRGB leds[NUM_LEDS];

void setLed(CRGB color)
{
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

  for (int i = 0; i < 4; i++)
  {
    setLed(CRGB::Green);
    delay(500);
    setLed(CRGB::Black);
    delay(500);
  }

  Serial.println("Atomic Motion Test");
}

void loop()
{
  M5.update();

  if (M5.BtnA.wasPressed())
  {
    doMotion_flg = !doMotion_flg;
    setLed(CRGB::White);
    delay(2000);
    setLed(CRGB::Black);
  }

  // prod : Motor Control Sample
  if (prod_flg && doMotion_flg)
  {
    setLed(CRGB::Red);
    delay(2000);
    AtomicMotion.setMotorSpeed(ch, speed);
    Serial.printf("Motor Channel %d: %d \n", ch,
                  AtomicMotion.getMotorSpeed(ch));
    delay(1500);
    AtomicMotion.setMotorSpeed(ch, 0);
    Serial.printf("Motor Channel %d: %d \n", ch,
                  AtomicMotion.getMotorSpeed(ch));
    delay(1000);
  }

  // not prod : Servo Control Sample
  if (!prod_flg && doMotion_flg)
  {
    AtomicMotion.setServoAngle(ch, 135);
    Serial.printf("Servo Channel %d: %d \n", ch,
                  AtomicMotion.getServoAngle(ch));
    delay(1000);
    AtomicMotion.setServoAngle(ch, 45);
    Serial.printf("Servo Channel %d: %d \n", ch,
                  AtomicMotion.getServoAngle(ch));
    delay(1000);
    AtomicMotion.setServoAngle(ch, 0);
    Serial.printf("Servo Channel %d: %d \n", ch,
                  AtomicMotion.getServoAngle(ch));
    delay(1000);
  }
}

#include <M5Unified.h>
#include <M5AtomicMotion.h>
#include <FastLED.h>
#include <NimBLEDevice.h>
#include <vector>
using namespace std;

M5AtomicMotion AtomicMotion;

// ======================================================================
// AtomicMotion関連
// ======================================================================
#define PROD_OR_TEST
#ifdef PROD_OR_TEST
constexpr bool prod_flg = true;
#else
constexpr bool prod_flg = false;
#endif
boolean doMotion_flg = false;

// ---------------------------------------------------------------
uint8_t ch = 0;
uint8_t speed = 90;

#define LED_PIN 27
#define NUM_LEDS 1
static CRGB leds[NUM_LEDS];

// ---------------------------------------------------------------
void setLed(CRGB color)
{
  // change RGB to GRB
  uint8_t t = color.r;
  color.r = color.g;
  color.g = t;
  leds[0] = color;
  FastLED.show();
}

// ======================================================================
// BLE関連
// Original Source: H2zero
// https://qiita.com/coppercele/items/4ce0e8858a92410c81e3
// ======================================================================
// u_int8_t color_num = 0;

void scanEndedCB(NimBLEScanResults results);

// UUID HID
static NimBLEUUID serviceUUID("1812");
// UUID Report Charcteristic
static NimBLEUUID charUUID("2a4d");

static NimBLEAdvertisedDevice *advDevice;
// static NimBLEAdvertisedDevice* advDevice;

static bool doConnect = false;
static uint32_t scanTime = 0; /** 0 = scan forever */

// ------------------------------------------------------------------------
// リモートボタン押下時にnotifyCBから呼ばれる関数
// この関数は一回のボタン操作で2重に呼び出されるため、この中でフラグ操作する場合 false -> true -> false と元に戻ってしまう
// 連続動作させたい場合は、2回目の通信データが 00:00 となるためその値を判別してフラグ操作をスキップする
void motionGoStop()
{
  Serial.print("------------------ Moton GO / Stop ---------------------\n");
  // TODO フラグ操作スキップ
  doMotion_flg = !doMotion_flg;
  setLed(CRGB::White);
  delay(2000);
  setLed(CRGB::Black);
}

// ------------------------------------------------------------------------
class ClientCallbacks : public NimBLEClientCallbacks
{
  void onConnect(NimBLEClient *pClient)
  {
    Serial.println("Connected");
    /** After connection we should change the parameters if we don't need fast response times.
     *  These settings are 150ms interval, 0 latency, 450ms timout.
     *  Timeout should be a multiple of the interval, minimum is 100ms.
     *  I find a multiple of 3-5 * the interval works best for quick response/reconnect.
     *  Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
     */
    pClient->updateConnParams(120, 120, 0, 60);
  };

  void onDisconnect(NimBLEClient *pClient)
  {
    Serial.print(pClient->getPeerAddress().toString().c_str());
    Serial.println(" Disconnected - Starting scan");
    NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
  };

  /** Called when the peripheral requests a change to the connection parameters.
   *  Return true to accept and apply them or false to reject and keep
   *  the currently used parameters. Default will return true.
   */
  bool onConnParamsUpdateRequest(NimBLEClient *pClient, const ble_gap_upd_params *params)
  {
    if (params->itvl_min < 24)
    { /** 1.25ms units */
      return false;
    }
    else if (params->itvl_max > 40)
    { /** 1.25ms units */
      return false;
    }
    else if (params->latency > 2)
    { /** Number of intervals allowed to skip */
      return false;
    }
    else if (params->supervision_timeout > 100)
    { /** 10ms units */
      return false;
    }

    return true;
  };

  /********************* Security handled here **********************
   ****** Note: these are the same return values as defaults ********/
  uint32_t onPassKeyRequest()
  {
    Serial.println("Client Passkey Request");
    /** return the passkey to send to the server */
    return 123456;
  };

  bool onConfirmPIN(uint32_t pass_key)
  {
    Serial.print("The passkey YES/NO number: ");
    Serial.println(pass_key);
    /** Return false if passkeys don't match. */
    return true;
  };

  /** Pairing proces\s complete, we can check the results in ble_gap_conn_desc */
  void onAuthenticationComplete(ble_gap_conn_desc *desc)
  {
    if (!desc->sec_state.encrypted)
    {
      Serial.println("Encrypt connection failed - disconnecting");
      /** Find the client with the connection handle provided in desc */
      NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
      return;
    }
  };
};

/** Define a class to handle the callbacks when advertisments are received */
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks
{

  void onResult(NimBLEAdvertisedDevice *advertisedDevice)
  {
    Serial.print("Advertised Device found: ");
    //        Serial.println(advertisedDevice->toString().c_str());
    Serial.printf("name:%s, address:%s ", advertisedDevice->getName().c_str(), advertisedDevice->getAddress().toString().c_str());
    Serial.printf("UUID:%s\n", advertisedDevice->haveServiceUUID() ? advertisedDevice->getServiceUUID().toString().c_str() : "none");

    if (advertisedDevice->isAdvertisingService(serviceUUID))
    {
      Serial.println("Found Our Service");
      /** stop scan before connecting */
      NimBLEDevice::getScan()->stop();
      /** Save the device reference in a global for the client to use*/
      advDevice = advertisedDevice;
      /** Ready to connect now */
      doConnect = true;
    }
  };
};

const uint8_t START = 0x08;
const uint8_t SELECT = 0x04;

const int INDEX_BUTTON = 6;

bool on = false;

/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic *pRemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  std::string str = (isNotify == true) ? "Notification" : "Indication";
  str += " from ";
  str += pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString();
  str += ": Service = " + pRemoteCharacteristic->getRemoteService()->getUUID().toString();
  str += ", Characteristic = " + pRemoteCharacteristic->getUUID().toString();
  //    str += ", Value = " + std::string((char*)pData, length);
  Serial.print(str.c_str());
  Serial.print("\ndata: ");
  for (int i = 0; i < length; i++)
  {
    // uint8_tを頭0のstringで表示する
    Serial.printf("%02X ", pData[i]);
  }
  Serial.print("\n");
  if (pData[1] & 0x28)
  {
    digitalWrite(GPIO_NUM_10, LOW);
    on = true;
  }
  if (pData[0] & 0x01)
  {
    if (on)
    {
      on = false;
      return;
    }
    digitalWrite(GPIO_NUM_10, HIGH);
  }
  Serial.print("------------------ Call Self Method ---------------------\n");
  // color_num = random(0,100) % 3;
  // changeColorLCD(color_num);
  motionGoStop();
  Serial.print("------------------ End Self Method ---------------------\n");
}

/** Callback to process the results of the last scan or restart it */
void scanEndedCB(NimBLEScanResults results)
{
  Serial.println("Scan Ended");
}

/** Create a single global instance of the callback class to be used by all clients */
static ClientCallbacks clientCB;

/** Handles the provisioning of clients and connects / interfaces with the server */
bool connectToServer()
{
  NimBLEClient *pClient = nullptr;

  /** Check if we have a client we should reuse first **/
  if (NimBLEDevice::getClientListSize())
  {
    /** Special case when we already know this device, we send false as the
     *  second argument in connect() to prevent refreshing the service database.
     *  This saves considerable time and power.
     */
    pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
    if (pClient)
    {
      if (!pClient->connect(advDevice, false))
      {
        Serial.println("Reconnect failed");
        return false;
      }
      Serial.println("Reconnected client");
    }
    /** We don't already have a client that knows this device,
     *  we will check for a client that is disconnected that we can use.
     */
    else
    {
      pClient = NimBLEDevice::getDisconnectedClient();
    }
  }

  /** No client to reuse? Create a new one. */
  if (!pClient)
  {
    if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS)
    {
      Serial.println("Max clients reached - no more connections available");
      return false;
    }

    pClient = NimBLEDevice::createClient();

    Serial.println("New client created");

    pClient->setClientCallbacks(&clientCB, false);
    /** Set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout.
     *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
     *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
     *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
     */
    pClient->setConnectionParams(12, 12, 0, 51);
    /** Set how long we are willing to wait for the connection to complete (seconds), default is 30. */
    pClient->setConnectTimeout(5);

    if (!pClient->connect(advDevice))
    {
      /** Created a client but failed to connect, don't need to keep it as it has no data */
      NimBLEDevice::deleteClient(pClient);
      Serial.println("Failed to connect, deleted client");
      return false;
    }
  }

  if (!pClient->isConnected())
  {
    if (!pClient->connect(advDevice))
    {
      Serial.println("Failed to connect");
      return false;
    }
  }

  Serial.print("Connected to: ");
  Serial.println(pClient->getPeerAddress().toString().c_str());
  Serial.print("RSSI: ");
  Serial.println(pClient->getRssi());

  /** Now we can read/write/subscribe the charateristics of the services we are interested in */
  NimBLERemoteService *pSvc = nullptr;
  //  NimBLERemoteCharacteristic *pChr = nullptr;
  std::vector<NimBLERemoteCharacteristic *> *pChrs = nullptr;

  NimBLERemoteDescriptor *pDsc = nullptr;

  pSvc = pClient->getService(serviceUUID);
  if (pSvc)
  { /** make sure it's not null */
    pChrs = pSvc->getCharacteristics(true);
  }

  if (pChrs)
  { /** make sure it's not null */

    for (int i = 0; i < pChrs->size(); i++)
    {
      if (pChrs->at(i)->canNotify())
      {
        /** Must send a callback to subscribe, if nullptr it will unsubscribe */
        if (!pChrs->at(i)->registerForNotify(notifyCB))
        {
          /** Disconnect if subscribe failed */
          pClient->disconnect();
          return false;
        }
      }
    }
  }

  else
  {
    Serial.println("DEAD service not found.");
  }

  Serial.println("Done with this device!");
  return true;
}

// ======================================================================
// setup & loop関連
// ======================================================================
void setup()
{
  auto cfg = M5.config();
  M5.begin(cfg);    // M5Stackをcfgの設定で初期化

  // ---------------------------------------------------------------
  // AtmicMotion関連
  // ---------------------------------------------------------------
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
  // ---------------------------------------------------------------
  // BLE関連
  // Original Source: H2zero
  // https://qiita.com/coppercele/items/4ce0e8858a92410c81e3
  // ---------------------------------------------------------------
  pinMode(GPIO_NUM_10, OUTPUT);
  digitalWrite(GPIO_NUM_10, HIGH);
  /** Initialize NimBLE, no device name spcified as we are not advertising */
  NimBLEDevice::init("");

  /** Set the IO capabilities of the device, each option will trigger a different pairing method.
   *  BLE_HS_IO_KEYBOARD_ONLY    - Passkey pairing
   *  BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
   *  BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
   */
  // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY); // use passkey
  // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison
  /** 2 different ways to set security - both calls achieve the same result.
   *  no bonding, no man in the middle protection, secure connections.
   *
   *  These are the default values, only shown here for demonstration.
   */
  // NimBLEDevice::setSecurityAuth(false, false, true);
  NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);

  /** Optional: set the transmit power, default is 3db */
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */

  /** Optional: set any devices you don't want to get advertisments from */
  // NimBLEDevice::addIgnored(NimBLEAddress ("aa:bb:cc:dd:ee:ff"));
  /** create new scan */
  NimBLEScan *pScan = NimBLEDevice::getScan();

  /** create a callback that gets called when advertisers are found */
  pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());

  /** Set scan interval (how often) and window (how long) in milliseconds */
  pScan->setInterval(10000);
  pScan->setWindow(9999);

  /** Active scan will gather scan response data from advertisers
   *  but will use more energy from both devices
   */
  pScan->setActiveScan(true);
  /** Start scanning for advertisers for the scan time specified (in seconds) 0 = forever
   *  Optional callback for when scanning stops.
   */
  pScan->start(scanTime, scanEndedCB);

  // ---------------------------------------------------------------
  // RGB LEDの初期化（状態を把握するために利用）
  // ---------------------------------------------------------------
  FastLED.addLeds<WS2811, LED_PIN, RGB>(leds, NUM_LEDS);
  FastLED.setBrightness(255 * 15 / 100);

  for (int i = 0; i < 4; i++)
  {
    setLed(CRGB::Blue);
    delay(500);
    setLed(CRGB::Black);
    delay(500);
  }

  Serial.println("Done AtomicMotion and BLE Setup.");
}

// ---------------------------------------------------------------
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

  // ---------------------------------------------------------------
  // AtmicMotion関連
  // ---------------------------------------------------------------
  // prod : Motor Control Sample
  if (prod_flg && doMotion_flg)
  {
    // setLed(CRGB::Red);
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

  // ---------------------------------------------------------------
  // BLE関連
  // ---------------------------------------------------------------
  if (doConnect)
  {
    Serial.println("doConnect!");
    if (connectToServer())
    {
      Serial.println("Success! we should now be getting notifications, scanning for more!");
      setLed(CRGB::Green);
    }
    else
    {
      Serial.println("Failed to connect, starting scan");
    }
    doConnect = false;
  }
  /** Found a device we want to connect to, do it now */
  delay(1);

}

#include "DigitalIn.h"
#include "DigitalOut.h"
#include "PinNameAliases.h"
#include "ThisThread.h"
#include "ble/BLE.h"
#include "events/EventQueue.h"
#include "gatt_server_process.h"
#include "mbed-trace/mbed_trace.h"
#include "platform/Callback.h"
#include <cstdio>
#include <cstdlib>

using mbed::callback;
using namespace std::literals::chrono_literals;

/* parameters */
float thresh = 1.0;         // cm
float tankHeight = 11.0;    // cm
float setPoint = 0.0;       // cm
float inverseSpeed = 58.2;  // us/cm
int controlLoopFreq = 1000; // ms
int triggerTime = 50;      // us

/* dependent variables */
float currLevel = 0.0;       // cm
float waterLevelError = 0.0; // cm

/* ports */
DigitalOut trigger(D6);
DigitalIn echo(D10);
DigitalOut innerPump(D4);
DigitalOut outerPump(D5);

/* utilities */
Timer timer;
Thread readThread;

/**
 * Reads the current water level and error in the controlled tank.
 */
void readCurrentLevel() {
  timer.reset();
  trigger = 1;
  wait_us(triggerTime);
  trigger = 0;
  while (!echo);
  timer.start();
  while (echo);
  timer.stop();
  float distance = timer.read_us() / inverseSpeed;
  currLevel = tankHeight - distance;
  waterLevelError = currLevel - setPoint;
  printf("currLevel= %d!\n", (int)(10 * currLevel));
  printf("waterLevelError= %d!\n", (int)(10 * waterLevelError));
}

/**
 * Tells the pumps what to do based on current error and threshold.
 */
void commandPumps() {
  if (abs(waterLevelError) < thresh) {
    printf("both pumps off!\n");
    innerPump = 0;
    outerPump = 0;
  } else if (waterLevelError > 0) {
    printf("pump water OUT!\n");
    innerPump = 1;
    outerPump = 0;
  } else {
    printf("pump water IN!\n");
    innerPump = 0;
    outerPump = 1;
  }
}

void controlLoop() {
  while (true) {
    readCurrentLevel();
    commandPumps();
    ThisThread::sleep_for(controlLoopFreq);
  }
}

/**
 *
 */
class WaterTankService : public ble::GattServer::EventHandler {
public:
  WaterTankService()
      : _set_point_char("0a924ca7-87cd-4699-a3bd-abdcd9cf126a",
                        (int)(setPoint * 10)),
        _current_level_char("8dd6a1b7-bc75-4741-8a26-264af75807de", 0),
        _tank_height_char("63718360-318a-4805-99a5-b7bd2593e30f",
                          (int)(tankHeight * 10)),
        _trigger_time_char("008c2ddc-c6db-4a7f-a51c-90824bf67a56",
                           (int)triggerTime),
        _threshold_char("4aae2aa4-bae8-46fc-8a09-d3aec4a126d4",
                        (int)(thresh * 10)),

        _water_tank_service(
            /* uuid */ "51311102-030e-485f-b122-f8f381aa84ed",
            /* characteristics */ _water_tank_characteristics,
            /* numCharacteristics */ sizeof(_water_tank_characteristics) /
                sizeof(_water_tank_characteristics[0])) {
    /* update internal pointers (value, descriptors and characteristics array)
     */
    _water_tank_characteristics[0] = &_set_point_char;
    _water_tank_characteristics[1] = &_current_level_char;
    _water_tank_characteristics[2] = &_tank_height_char;
    _water_tank_characteristics[3] = &_trigger_time_char;
    _water_tank_characteristics[4] = &_threshold_char;

    /* setup authorization handlers */
    _set_point_char.setWriteAuthorizationCallback(
        this, &WaterTankService::authorize_client_write);
  }

  void start(BLE &ble, events::EventQueue &event_queue) {
    _server = &ble.gattServer();
    _event_queue = &event_queue;

    printf("Registering demo service\r\n");
    ble_error_t err = _server->addService(_water_tank_service);

    if (err) {
      printf("Error %u during demo service registration.\r\n", err);
      return;
    }

    /* register handlers */
    _server->setEventHandler(this);

    printf("clock service registered\r\n");
    printf("service handle: %u\r\n", _water_tank_service.getHandle());
    printf("set_point characteristic value handle %u\r\n",
           _set_point_char.getValueHandle());
    printf("current_level characteristic value handle %u\r\n",
           _current_level_char.getValueHandle());

    _event_queue->call_every(
        1000ms, callback(this, &WaterTankService::check_current_level));
  }

  /* GattServer::EventHandler */
private:
  /**
   * Handler called when a notification or an indication has been sent.
   */
  void onDataSent(const GattDataSentCallbackParams &params) override {
    printf("sent updates\r\n");
  }

  /**
   * Handler called after an attribute has been written.
   */
  void onDataWritten(const GattWriteCallbackParams &params) override {
    printf("data written:\r\n");
    printf("connection handle: %u\r\n", params.connHandle);
    printf("attribute handle: %u\r\n", params.handle);

    if (params.handle == _set_point_char.getValueHandle()) {
      setPoint = params.data[0] / 10.0;
      printf("user wrote: setPoint\r\n");
    } else if (params.handle == _tank_height_char.getValueHandle()) {
      tankHeight = params.data[0] / 10.0;
      printf("user wrote: tankHeight\r\n");
    } else if (params.handle == _trigger_time_char.getValueHandle()) {
      triggerTime = params.data[0];
      printf("user wrote: triggerTime\r\n");
    } else if (params.handle == _threshold_char.getValueHandle()) {
      thresh = params.data[0] / 10.0;
      printf("user wrote: thresh\r\n");
    } else {
      printf("user attempted to write to unknown attribute\r\n");
    }
  }

  /**
   * Handler called after an attribute has been read.
   */
  void onDataRead(const GattReadCallbackParams &params) override {
    printf("data read:\r\n");
    printf("connection handle: %u\r\n", params.connHandle);
    printf("attribute handle: %u", params.handle);

    if (params.handle == _set_point_char.getValueHandle()) {
      printf(" (set_point characteristic)\r\n");
    } else if (params.handle == _current_level_char.getValueHandle()) {
      printf(" (current_level characteristic)\r\n");
    } else {
      printf("\r\n");
    }
  }

  /**
   * Handler called after a client has subscribed to notification or indication.
   *
   * @param handle Handle of the characteristic value affected by the change.
   */
  void
  onUpdatesEnabled(const GattUpdatesEnabledCallbackParams &params) override {
    printf("update enabled on handle %d\r\n", params.attHandle);
  }

  /**
   * Handler called after a client has cancelled his subscription from
   * notification or indication.
   *
   * @param handle Handle of the characteristic value affected by the change.
   */
  void
  onUpdatesDisabled(const GattUpdatesDisabledCallbackParams &params) override {
    printf("update disabled on handle %d\r\n", params.attHandle);
  }

  /**
   * Handler called when an indication confirmation has been received.
   *
   * @param handle Handle of the characteristic value that has emitted the
   * indication.
   */
  void onConfirmationReceived(
      const GattConfirmationReceivedCallbackParams &params) override {
    printf("confirmation received on handle %d\r\n", params.attHandle);
  }

private:
  /**
   * Handler called when a write request is received.
   *
   * This handler verify that the value submitted by the client is valid before
   * authorizing the operation.
   */
  void authorize_client_write(GattWriteAuthCallbackParams *e) {
    printf("characteristic %u write authorization\r\n", e->handle);

    if (e->offset != 0) {
      printf("Error invalid offset\r\n");
      e->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_OFFSET;
      return;
    }

    if (e->len != 1) {
      printf("Error invalid len\r\n");
      e->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_ATT_VAL_LENGTH;
      return;
    }

    e->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
  }

  /**
   * Updates the current water level characteristic periodically.
   */
  void check_current_level(void) {

    ble_error_t err = _current_level_char.set(*_server, (int)(10 * currLevel));

    if (err) {
      printf("write of the second value returned error %u\r\n", err);
      return;
    }
  }

private:
  template <typename T> class BaseCharacteristic : public GattCharacteristic {
  public:
    BaseCharacteristic(const UUID &uuid, const T &initial_value,
                       uint8_t properties)
        : GattCharacteristic(uuid, &_value, sizeof(_value), sizeof(_value),
                             properties, nullptr, 0, false),
          _value(initial_value) {}

    ble_error_t get(GattServer &server, T &dst) const {
      uint16_t value_length = sizeof(dst);
      return server.read(getValueHandle(), &dst, &value_length);
    }

    ble_error_t set(GattServer &server, const uint8_t &value,
                    bool local_only = false) const {
      return server.write(this->getValueHandle(), &value, sizeof(value),
                          local_only);
    }

  protected:
    T _value;
  };

  template <typename T>
  class ReadWriteNotifyIndicateCharacteristic : public BaseCharacteristic<T> {
  public:
    ReadWriteNotifyIndicateCharacteristic(const UUID &uuid,
                                          const T &initial_value)
        : BaseCharacteristic<T>(
              uuid, initial_value,
              GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE |
                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY |
                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_INDICATE) {}
  };

  template <typename T>
  class ReadNotifyIndicateCharacteristic : public BaseCharacteristic<T> {
  public:
    ReadNotifyIndicateCharacteristic(const UUID &uuid, const T &initial_value)
        : BaseCharacteristic<T>(
              uuid, initial_value,
              GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY |
                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_INDICATE) {}
  };

private:
  GattServer *_server = nullptr;
  events::EventQueue *_event_queue = nullptr;
  GattService _water_tank_service;
  GattCharacteristic *_water_tank_characteristics[5];
  ReadWriteNotifyIndicateCharacteristic<uint8_t> _set_point_char;
  ReadNotifyIndicateCharacteristic<uint8_t> _current_level_char;
  ReadWriteNotifyIndicateCharacteristic<uint8_t> _tank_height_char;
  ReadWriteNotifyIndicateCharacteristic<uint8_t> _trigger_time_char;
  ReadWriteNotifyIndicateCharacteristic<uint8_t> _threshold_char;
};

int main() {

  mbed_trace_init();

  BLE &ble = BLE::Instance();
  events::EventQueue event_queue;
  WaterTankService demo_service;

  /* this process will handle basic ble setup and advertising for us */
  GattServerProcess ble_process(event_queue, ble);

  /* once it's done it will let us continue with our demo */
  ble_process.on_init(callback(&demo_service, &WaterTankService::start));

  readThread.start(controlLoop);
  ble_process.start();
  return 0;
}

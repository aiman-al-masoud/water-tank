#include "ble/BLE.h"
#include "events/EventQueue.h"
#include "gatt_server_process.h"
#include "mbed-trace/mbed_trace.h"
#include "platform/Callback.h"


using mbed::callback;
using namespace std::literals::chrono_literals;

/**
 *
 */
class WaterTankService : public ble::GattServer::EventHandler {
public:
  WaterTankService()
      : _set_point_char("0a924ca7-87cd-4699-a3bd-abdcd9cf126a", 0),
        _current_level_char("8dd6a1b7-bc75-4741-8a26-264af75807de", 0),
        _clock_service(
            /* uuid */ "51311102-030e-485f-b122-f8f381aa84ed",
            /* characteristics */ _clock_characteristics,
            /* numCharacteristics */ sizeof(_clock_characteristics) /
                sizeof(_clock_characteristics[0])) {
    /* update internal pointers (value, descriptors and characteristics array)
     */
    _clock_characteristics[0] = &_set_point_char;
    _clock_characteristics[1] = &_current_level_char;

    /* setup authorization handlers */
    _set_point_char.setWriteAuthorizationCallback(
        this, &WaterTankService::authorize_client_write);
    // _current_level_char.setWriteAuthorizationCallback(this,
    // &WaterTankService::authorize_client_write);
  }

  void start(BLE &ble, events::EventQueue &event_queue) {
    _server = &ble.gattServer();
    _event_queue = &event_queue;

    printf("Registering demo service\r\n");
    ble_error_t err = _server->addService(_clock_service);

    if (err) {
      printf("Error %u during demo service registration.\r\n", err);
      return;
    }

    /* register handlers */
    _server->setEventHandler(this);

    printf("clock service registered\r\n");
    printf("service handle: %u\r\n", _clock_service.getHandle());
    printf("minute characteristic value handle %u\r\n",
           _set_point_char.getValueHandle());
    printf("second characteristic value handle %u\r\n",
           _current_level_char.getValueHandle());

    _event_queue->call_every(
        1000ms, callback(this, &WaterTankService::increment_second));
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
    printf("attribute handle: %u", params.handle);

    if (params.handle == _set_point_char.getValueHandle()) {
      printf(" (minute characteristic)\r\n");
    }
    // } else if (params.handle == _current_level_char.getValueHandle()) {
    //     printf(" (second characteristic)\r\n");
    // }
    else {
      printf("\r\n");
    }
    printf("write operation: %u\r\n", params.writeOp);
    printf("offset: %u\r\n", params.offset);
    printf("length: %u\r\n", params.len);
    printf("data: ");

    for (size_t i = 0; i < params.len; ++i) {
      printf("%02X", params.data[i]);
    }

    printf("\r\n");
  }

  /**
   * Handler called after an attribute has been read.
   */
  void onDataRead(const GattReadCallbackParams &params) override {
    printf("data read:\r\n");
    printf("connection handle: %u\r\n", params.connHandle);
    printf("attribute handle: %u", params.handle);

    if (params.handle == _set_point_char.getValueHandle()) {
      printf(" (minute characteristic)\r\n");
    } else if (params.handle == _current_level_char.getValueHandle()) {
      printf(" (second characteristic)\r\n");
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
   * Increment the second counter.
   */
  void increment_second(void) {
    uint8_t second = 0;
    ble_error_t err = _current_level_char.get(*_server, second);
    if (err) {
      printf("read of the second value returned error %u\r\n", err);
      return;
    }

    second = (second + 1) % 60;

    err = _current_level_char.set(*_server, second);
    if (err) {
      printf("write of the second value returned error %u\r\n", err);
      return;
    }

    if (second == 0) {
      increment_minute();
    }
  }

  /**
   * Increment the minute counter.
   */
  void increment_minute(void) {
    uint8_t minute = 0;
    ble_error_t err = _set_point_char.get(*_server, minute);
    if (err) {
      printf("read of the minute value returned error %u\r\n", err);
      return;
    }

    minute = (minute + 1) % 60;

    err = _set_point_char.set(*_server, minute);
    if (err) {
      printf("write of the minute value returned error %u\r\n", err);
      return;
    }
  }

private:
  /**
   * Read, Write, Notify, Indicate  Characteristic declaration helper.
   *
   * @tparam T type of data held by the characteristic.
   */
  template <typename T>
  class ReadWriteNotifyIndicateCharacteristic : public GattCharacteristic {
  public:
    /**
     * Construct a characteristic that can be read or written and emit
     * notification or indication.
     *
     * @param[in] uuid The UUID of the characteristic.
     * @param[in] initial_value Initial value contained by the characteristic.
     */
    ReadWriteNotifyIndicateCharacteristic(const UUID &uuid,
                                          const T &initial_value)
        : GattCharacteristic(
              /* UUID */ uuid,
              /* Initial value */ &_value,
              /* Value size */ sizeof(_value),
              /* Value capacity */ sizeof(_value),
              /* Properties */
                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE |
                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY |
                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_INDICATE,
              /* Descriptors */ nullptr,
              /* Num descriptors */ 0,
              /* variable len */ false),
          _value(initial_value) {}

    /**
     * Get the value of this characteristic.
     *
     * @param[in] server GattServer instance that contain the characteristic
     * value.
     * @param[in] dst Variable that will receive the characteristic value.
     *
     * @return BLE_ERROR_NONE in case of success or an appropriate error code.
     */
    ble_error_t get(GattServer &server, T &dst) const {
      uint16_t value_length = sizeof(dst);
      return server.read(getValueHandle(), &dst, &value_length);
    }

    /**
     * Assign a new value to this characteristic.
     *
     * @param[in] server GattServer instance that will receive the new value.
     * @param[in] value The new value to set.
     * @param[in] local_only Flag that determine if the change should be kept
     * locally or forwarded to subscribed clients.
     */
    ble_error_t set(GattServer &server, const uint8_t &value,
                    bool local_only = false) const {
      return server.write(getValueHandle(), &value, sizeof(value), local_only);
    }

  private:
    uint8_t _value;
  };

  // ----------------------------
  template <typename T>
  class ReadNotifyIndicateCharacteristic : public GattCharacteristic {
  public:
    /**
     * Construct a characteristic that can be read or written and emit
     * notification or indication.
     *
     * @param[in] uuid The UUID of the characteristic.
     * @param[in] initial_value Initial value contained by the characteristic.
     */
    ReadNotifyIndicateCharacteristic(const UUID &uuid,
                                          const T &initial_value)
        : GattCharacteristic(
              /* UUID */ uuid,
              /* Initial value */ &_value,
              /* Value size */ sizeof(_value),
              /* Value capacity */ sizeof(_value),
              /* Properties */
                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY |
                  GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_INDICATE,
              /* Descriptors */ nullptr,
              /* Num descriptors */ 0,
              /* variable len */ false),
          _value(initial_value) {}

    /**
     * Get the value of this characteristic.
     *
     * @param[in] server GattServer instance that contain the characteristic
     * value.
     * @param[in] dst Variable that will receive the characteristic value.
     *
     * @return BLE_ERROR_NONE in case of success or an appropriate error code.
     */
    ble_error_t get(GattServer &server, T &dst) const {
      uint16_t value_length = sizeof(dst);
      return server.read(getValueHandle(), &dst, &value_length);
    }

    /**
     * Assign a new value to this characteristic.
     *
     * @param[in] server GattServer instance that will receive the new value.
     * @param[in] value The new value to set.
     * @param[in] local_only Flag that determine if the change should be kept
     * locally or forwarded to subscribed clients.
     */
    ble_error_t set(GattServer &server, const uint8_t &value,
                    bool local_only = false) const {
      return server.write(getValueHandle(), &value, sizeof(value), local_only);
    }

  private:
    uint8_t _value;
  };

  // ----------------------------


private:
  GattServer *_server = nullptr;
  events::EventQueue *_event_queue = nullptr;

  GattService _clock_service;

  GattCharacteristic *_clock_characteristics[2];

  ReadWriteNotifyIndicateCharacteristic<uint8_t> _set_point_char;
  ReadNotifyIndicateCharacteristic<uint8_t> _current_level_char;
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

  ble_process.start();

  return 0;
}

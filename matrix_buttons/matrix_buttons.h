#pragma once

#include "esphome/components/key_provider/key_provider.h"
#include "esphome/components/script/script.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <cstdlib>
#include <utility>

namespace esphome {
namespace matrix_buttons {

class MatrixButtonsListener {
 public:
  virtual void button_pressed(int row, int col){};
  virtual void button_released(int row, int col){};
  virtual void key_pressed(uint8_t key){};
  virtual void key_released(uint8_t key){};
};

class MatrixKeyTrigger : public Trigger<uint8_t> {};

class MatrixStateTrigger : public Trigger<bool, int, int, int, int, int,uint8_t> {};

class MatrixButtons : public key_provider::KeyProvider, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_columns(std::vector<GPIOPin *> pins) { columns_ = std::move(pins); };
  void set_rows(std::vector<GPIOPin *> pins) { rows_ = std::move(pins); };
  void set_keys(std::string keys) { keys_ = std::move(keys); };
  void set_passive_rows(bool passive_rows) { passive_rows_ = passive_rows; };
  void set_active_high(bool active_high) { active_ = active_high; };
  void set_multi_keys(uint multi_keys) { multi_keys_ = multi_keys; };
  void set_debounce_time(uint debounce_time) { debounce_time_ = static_cast<stime_t>(debounce_time >> 2); };
  void set_scan_delay(uint scan_delay) { this->scan_delay_ = scan_delay; };
  void set_script_id(script::Script<bool,int> *state_script) { this->state_script_ = state_script ; };

  void register_listener(MatrixButtonsListener *listener);
  void register_key_trigger(MatrixKeyTrigger *trig);
  void register_state_trigger(MatrixStateTrigger *trig);

 protected:
  // settings
  std::vector<GPIOPin *> rows_;
  std::vector<GPIOPin *> columns_;
  std::string keys_;
  script::Script<bool, int> *state_script_{nullptr};
  bool passive_rows_{false};
  bool active_{false};
  uint multi_keys_{1};
  uint debounce_time_ = 0;
  uint scan_delay_{0};

  // state
  using stime_t = uint8_t;              // 8 bits should be enough for debouncing, and it reduces the vector size
  std::vector<stime_t> button_start_;   // time when the button state changed (for debouncing)
  std::vector<bool>    button_state_;   // current state of the button (pressed or released)
  std::vector<bool>    button_chnge_;   // true if the button state has changed and needs to be processed
  std::vector<bool>    button_newst_;   // requested new state of the button (pressed or released)
  uint num_cols_{0};
  uint num_rows_{0};
  uint current_pos_{0};
  uint max_pos_{0};
  int unprocessed_keys_{0};
  int pressed_keys_{0};

  // listeners and triggers
  std::vector<MatrixButtonsListener *> listeners_{};
  std::vector<MatrixKeyTrigger *> key_triggers_;
  std::vector<MatrixStateTrigger *> state_triggers_;

  // debug
  #ifdef _DEBUG_MATRIX_BUTTONS
    uint32_t cnt_loops_{0};
    uint32_t first_loop_time_{0};
    uint32_t last_loop_time_{0};
    uint32_t max_loop_interval_{0};
    uint32_t max_scan_duration_{0};
    uint32_t sum_scan_duration_{0};
    stime_t disabled_at_{0};
  #endif // _DEBUG_MATRIX_BUTTONS
};

}  // namespace matrix_buttons
}  // namespace esphome

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
  using stime_t = uint8_t;    // 8 bits should be enough for debouncing, and it reduces the vector size
  std::vector<stime_t> button_start_;
  std::vector<bool>    button_state_;
  std::vector<bool>    button_chnge_;
  std::vector<bool>    button_newst_;
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

  /* Alternative:
  struct button_state_t {
    stime_t sstime : 7; // double scaled time to 7 bits
    bool pressed : 1;
    stime_t get_time() const { return static_cast<stime_t>(sstime << 1); }
    void set_time(uint8_t now) { sstime = static_cast<stime_t>(now >> 1); }
    }
  };
  union packed_state_t {
    stime_t stime; // scaled time to 8 bits
    ButtonState bits;
  };
  std::vector<PackedState> button_state_{0};

  button_state[pos].bits.pressed = state ;
  button_state[pos].stime = now;
  if (now - button_state[pos].bits.get_time() > debouce_time_) ...
  ...
*/
/* Alternative 2 :
  std::vector<bool> button_state_, button_change_ ;
  pressed_keys_ == std::count(burron_state_.begin(), button_state_.end()) ;
  auto it = std::find(button_changed_.begin(), button_changed_.end(), true);
  while (it != button_changed_.end()){
    pos=std::distance(button_changed_.begin, it);

    *it = false;
    it=std::find(std::next(it), button_changed_.end(), true);
  }

  slightly faster:
  // Cache the size, as it doesn't change after setup
  const size_t n = button_changed_.size(); 

  for (size_t i = 0; i < n; ++i) {
      if (button_changed_[i]) {
          // Handle change...
      }
  }

  Since you're scanning 144 buttons, are you using yield() or delay(0) inside your loop to ensure the ESP32's background tasks (like Wi-Fi) don't get starved?

  if (this->my_modbus_script->is_running()) {
    return; // Don't fire the next button yet; wait for the current Modbus task to finish
}
*/
  


}  // namespace matrix_buttons
}  // namespace esphome

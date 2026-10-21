#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "matrix_buttons.h"

namespace esphome {
namespace matrix_buttons {

static const char *const TAG = "matrix_buttons";

constexpr gpio::Flags operator~(gpio::Flags arg) {
  return static_cast<gpio::Flags>(~static_cast<uint8_t>(arg));
}

void MatrixButtons::setup() {
  num_rows_ = this->rows_.size();
  num_cols_ = this->columns_.size();
  max_pos_ = num_rows_ * num_cols_;
  current_pos_ = max_pos_;
  button_start_.resize(max_pos_, 0);
  button_state_.resize(max_pos_, 0);
  button_newst_.resize(max_pos_, 0);
  button_chnge_.resize(max_pos_, 0);

  for (auto *pin : this->rows_) {
    if (passive_rows_) {
      // set rows as input, but keep other flags as defined by user (notably the inverted flag !)
      pin->pin_mode((pin->get_flags() | gpio::FLAG_INPUT) & ~gpio::FLAG_OUTPUT);
    } else {
      // set rows as inactive output, but keep other flags as defined by user (notably the inverted flag !)
      pin->pin_mode((pin->get_flags() & ~gpio::FLAG_INPUT) | gpio::FLAG_OUTPUT);    
      pin->digital_write(! active_); 
    }
    pin->setup();
  }
  for (auto *pin : this->columns_) {
    // set columns as input, but keep other flags as defined by user (notably the inverted flag !)
    pin->pin_mode((pin->get_flags() | gpio::FLAG_INPUT) & ~gpio::FLAG_OUTPUT);
    pin->setup();
  }

  // initialize button states (avoid generating triggers for buttons that are still pressed)
  uint pos=0;
  for (auto *row : this->rows_) {
    // active row
    if (passive_rows_)
      row->pin_mode(gpio::FLAG_OUTPUT);
    row->digital_write(active_);

    // wait for the output voltage to settle accross the optocouplers and others
    if (scan_delay_ > 0)
      delay_microseconds_safe(scan_delay_);

    // scan columns
    for (auto *col : this->columns_) {
      button_state_[pos]=col->digital_read() == active_;
      button_newst_[pos]=button_state_[pos];
      if (button_state_[pos]) pressed_keys_++ ;
      pos++;
    } /* for col */

    // desactive row
    row->digital_write(! active_);
    if (passive_rows_)
      row->pin_mode(gpio::FLAG_INPUT) ;
  } /* for row */
}

void MatrixButtons::loop() {
  stime_t now = static_cast<stime_t>(App.get_loop_component_start_time() >> 2); // or millis()
  uint pos; 

  if (now < static_cast<stime_t>(4)) {
    // 0 to 3 are reserved values for active_start_ to mark key states during debouncing, so we can't use them as timestamps,
    // hence we jump a little bit in the future to avoid that. This may cause a slightly longer debounce.
    now=static_cast<stime_t>(4); // advancing max. 16ms should not cause issues and keeps the code simpler
  }

  #ifdef _DEBUG_MATRIX_BUTTONS
    // Check if all rows are inactive, all columns should be inactive too
    if ((disabled_at_>0) && (now - disabled_at_ < 224)) return ;
    disabled_at_=0;

    int num=0;
    for (auto *col : this->columns_) {
      if (col->digital_read() == active_) {
        ESP_LOGD(TAG, "Loop %ld: all rows should be inactive but column %d is active", cnt_loops_, num);
        disabled_at_ = now ; // disable loop for 1s to avoid scanning in unstable situations with multiple keys pressed and no diodes
        return; // wait for stable situation before scanning
      }
      num++;
    }

    if (cnt_loops_ == 0) {
      last_loop_time_ = first_loop_time_ = millis();
    } else {
      uint32_t loop_now = millis() ;
      uint32_t loop_interval = loop_now - last_loop_time_;
      if (loop_interval > max_loop_interval_){
        max_loop_interval_ = loop_interval;
        ESP_LOGD(TAG, "Loop %ld: interval max/avg: %ld/%ld ms, scan duration max: %ld ms", 
          cnt_loops_, max_loop_interval_, (cnt_loops_ > 1) ? (last_loop_time_ - first_loop_time_) / (cnt_loops_  - 1) : 0, max_scan_duration_);
      }
      last_loop_time_ = loop_now;
    }
    cnt_loops_++;
  #endif // _DEBUG_MATRIX_BUTTONS

  pos=0;
  for (auto *row : this->rows_) {
    // active row
    if (passive_rows_)
      row->pin_mode(gpio::FLAG_OUTPUT);
    row->digital_write(active_);

    // wait for the output voltage to settle accross the optocouplers and others
    if (scan_delay_ > 0)
      delay_microseconds_safe(scan_delay_);

    // scan columns
    for (auto *col : this->columns_) {
      if (button_start_[pos] > 0 && now - button_start_[pos] >= debounce_time_) {
        // debounce
        button_start_[pos] = 0;
        button_chnge_[pos] = true ;
        unprocessed_keys_ ++;
      }

      bool state = col->digital_read() == active_;
      if (state != button_newst_[pos]){
        // check if too many buttons are pressed
        if (multi_keys_ > 0){
          pressed_keys_ += state ? 1 : -1 ;
        }

        if (button_start_[pos] > 0){
          // caught a bouncing button: cancel the state change
          button_start_[pos] = 0 ;
          if (button_state_[pos] == state){
            // normal bouncing case
            button_chnge_[pos] = false ; 
            button_newst_[pos] = state ;
          } else {
            // an earlier state change was not processed
            button_newst_[pos] = state ;
          }
        } else {
          // start debouncing
          button_start_[pos] = now ;
          button_newst_[pos] = state ;
        }
      }
      
      pos++;
    } /* for col */

    // desactive row
    row->digital_write(! active_);
    if (passive_rows_)
      row->pin_mode(gpio::FLAG_INPUT) ;
  } /* for row */

  if (multi_keys_ > 0 && pressed_keys_ >= multi_keys_) {
      // if the maximum allowed keys are already pressed, we can't know which one is real, so we wait
      // till the situation becomes stable again to avoid sending wrong key events
      ESP_LOGD(TAG, "Maximum %d of %d keys pressed, waiting to stabilize.", pressed_keys_, multi_keys_);
      return ;
  }
  
  #ifdef _DEBUG_MATRIX_BUTTONS
  uint32_t scan_duration = millis() - last_loop_time_;
  sum_scan_duration_ += scan_duration;
  if (scan_duration > max_scan_duration_){
    max_scan_duration_ = scan_duration;
    if (cnt_loops_ > 10) {
      ESP_LOGD(TAG, "Scan duration avg/max: %ld/%ld ms, loop interval cnt/max/avg: %ld/%ld/%ld ms",
               sum_scan_duration_ / cnt_loops_, max_scan_duration_, cnt_loops_, max_loop_interval_,
               (last_loop_time_ - first_loop_time_) / (cnt_loops_ -1) );
    }
  }
  #endif // _DEBUG_MATRIX_BUTTONS

  // Handle one key-press on each loop()
  if (unprocessed_keys_ > 0){
    if (state_script_ != nullptr && state_script_->is_running()) {
      // Wait for script to terminate before processing new keys
      return; 
    }

    pos=current_pos_ ;
    do {
      if (++pos >= max_pos_) pos=0;

      if (button_chnge_[pos]){
        // process state change
        button_state_[pos] = ! button_state_[pos] ;

        // get our location
        auto [rownbr, colnbr]  = std::div((int) pos, (int) num_cols_);
        uint8_t key = 0;
        if (pos < (int) this->keys_.size())
          key = this->keys_[pos];
        
        // show our progress
        ESP_LOGD(TAG, "%s @ pos %d, row %d, col %d, key %c", button_state_[pos] ? "pressed" : "released", pos, rownbr,colnbr, key==0 ? '@' : (char) key);

        // trigger our listeners
        if (state_script_ != nullptr)
          state_script_->execute(button_state_[pos], static_cast<int>(pos));
        for (auto &trigger : this->state_triggers_)
          trigger->trigger(button_state_[pos], static_cast<int>(pos), static_cast<int>(rownbr), static_cast<int>(colnbr), 
            static_cast<int>(num_rows_), static_cast<int>(num_cols_), key);
        if (button_state_[pos]){
          for (auto &listener : this->listeners_)
            listener->button_pressed(static_cast<int>(rownbr), static_cast<int>(colnbr));
          if (key > 0) {
            for (auto &trigger : this->key_triggers_)
              trigger->trigger(key);
            for (auto &listener : this->listeners_)
              listener->key_pressed(key);
            this->send_key_(key);
          }
        } else {
          for (auto &listener : this->listeners_)
            listener->button_released(static_cast<int>(rownbr), static_cast<int>(colnbr));
          if (key > 0) {
            for (auto &listener : this->listeners_)
              listener->key_released(key);
          }
        } /* if button_state_[pos] */

        // mark as processed
        if (button_state_[pos] != button_newst_[pos]){
          // we've had a nasty double state change (we've been too slow or someone is frantically pressing a button)
          // so we need to still process the second one, which we will do on the next loop, unless the second one is still debouncing
          if (button_start_[pos] > 0)
            button_chnge_[pos] = false ;
        } else {
          // reset the changed state
          button_chnge_[pos] = false ;
        }

        // active_start_[pos] = (int) state | 2; // mark as released (2) or pressed (3)

        // we only process one key at a time to keep the loop() short
        current_pos_ = pos ;
        unprocessed_keys_--;
        return;
      } /* switch active_start_[pos] */
    } while (pos != current_pos_); 

    if (unprocessed_keys_ != 0){
      ESP_LOGD(TAG, "Stale unprocessed keys %d (current_pos=%d)", unprocessed_keys_, current_pos_);
      unprocessed_keys_ = 0; // should not happen, but just in case to avoid getting stuck with unprocessed keys
    }
  } /* if unprocessed_keys_ */
} /* void MatrixButtons:loop */

void MatrixButtons::dump_config() {
  ESP_LOGCONFIG(TAG, "JMatrix Keypad:\n active_high: %s\n passive_rows: %s\n multi_keys: %d\n debounce_time: %d ms\n scan_delay: %d ms\n", 
    (active_ ? "true" : "false"), (passive_rows_ ? "true" : "false"), multi_keys_, debounce_time_, scan_delay_);

  ESP_LOGCONFIG(TAG, " Rows:");
  for (auto &pin : this->rows_) 
    LOG_PIN("  Pin: ", pin);
  ESP_LOGCONFIG(TAG, " Cols:");
  for (auto &pin : this->columns_) 
    LOG_PIN("  Pin: ", pin);

  #ifdef _DEBUG_MATRIX_BUTTONS
    ESP_LOGCONFIG(TAG, " pressed_keys_: %d", pressed_keys_);
    if (cnt_loops_ > 1) {
      ESP_LOGCONFIG(TAG, "Scan duration avg/max: %ld/%ld ms, loop interval cnt/max/avg: %ld/%ld/%ld ms", 
        sum_scan_duration_ / cnt_loops_, max_scan_duration_, cnt_loops_, max_loop_interval_, (last_loop_time_ - first_loop_time_) / (cnt_loops_ - 1) );
    }

    cnt_loops_ = 0;
    first_loop_time_ = 0;
    last_loop_time_ = 0;
    max_scan_duration_ = 0;
    max_loop_interval_ = 0;
    sum_scan_duration_ = 0;
  #endif // _DEBUG_MATRIX_BUTTONS
}

void MatrixButtons::register_listener(MatrixButtonsListener *listener) { this->listeners_.push_back(listener); }

void MatrixButtons::register_key_trigger(MatrixKeyTrigger *trig) { this->key_triggers_.push_back(trig); }

void MatrixButtons::register_state_trigger(MatrixStateTrigger *trig) { this->state_triggers_.push_back(trig); }

}  // namespace matrix_buttons
}  // namespace esphome
#include "key_manager.h"
#include "backlight.h"
#include "menu_items.h"
#include "zero_calibration.h"
#include  "stdbool.h"

static uint8_t last_up_state = 1;
static uint8_t last_down_state = 1;
static uint32_t both_press_time = 0;
 uint32_t menu_timeout = 0;
static uint32_t up_press_time = 0;
static uint32_t down_press_time = 0;
// 在文件开头的静态变量部分添加
static bool up_pressed = false;
static bool down_pressed = false;
static bool up_long_triggered = false;
static bool down_long_triggered = false;
static uint32_t up_last_repeat_time = 0;
static uint32_t down_last_repeat_time = 0;
static bool both_long_triggered = false;


static void Key_Scan(uint8_t *up_state, uint8_t *down_state) {
    *up_state = HAL_GPIO_ReadPin(BACKLIGHT_UP_PORT, BACKLIGHT_UP_PIN);
    *down_state = HAL_GPIO_ReadPin(BACKLIGHT_DOWN_PORT, BACKLIGHT_DOWN_PIN);
}

static KeyEvent Process_Zeroing_Mode(uint8_t up_state, uint8_t down_state) {
    uint32_t currentTime = osKernelGetTickCount();
    KeyEvent event = KEY_EVENT_NONE;

    // 1) 双键按下时，立即屏蔽单键，优先确认
    if (!up_state && !down_state) {
        if (both_press_time == 0) {
            both_press_time = currentTime;
            both_long_triggered = false;
            up_pressed = false;   // 关键：屏蔽单键
            down_pressed = false; // 关键：屏蔽单键
        } else if (!both_long_triggered && (currentTime - both_press_time >= 4000)) {
            both_long_triggered = true;
            event = KEY_EVENT_BOTH_LONG;
        }
    } else {
        if (both_press_time > 0) {
            both_press_time = 0;
            both_long_triggered = false;
        }

        // 2) 上键：改为【释放时才触发】，且给双键操作留时间
        if (last_up_state && !up_state) {  // 按下边沿
            up_press_time = currentTime;
            up_pressed = true;
            up_long_triggered = false;
            up_last_repeat_time = currentTime;
        } else if (!last_up_state && up_state) {  // 释放边沿
            if (up_pressed && !up_long_triggered) {
                uint32_t dur = currentTime - up_press_time;
                // 条件：有防抖(≥100)，且不是准备按双键(<4000)
                if (dur >= 100 && dur < 4000) {
                    event = KEY_EVENT_UP_SHORT;
                }
            }
            up_pressed = false;
            up_long_triggered = false;
        } else if (up_pressed) {
            uint32_t hold_duration = currentTime - up_press_time;
            if (hold_duration >= 4000) {
                if (!up_long_triggered) {
                    event = KEY_EVENT_UP_LONG;
                    up_long_triggered = true;
                    up_last_repeat_time = currentTime;
                } else if (currentTime - up_last_repeat_time >= 40) {
                    event = KEY_EVENT_UP_LONG;
                    up_last_repeat_time = currentTime;
                }
            }
        }

        // 3) 下键：同上（修改点一致）
        if (last_down_state && !down_state) {
            down_press_time = currentTime;
            down_pressed = true;
            down_long_triggered = false;
            down_last_repeat_time = currentTime;
        } else if (!last_down_state && down_state) {
            if (down_pressed && !down_long_triggered) {
                uint32_t dur = currentTime - down_press_time;
                if (dur >= 100 && dur < 4000) {
                    event = KEY_EVENT_DOWN_SHORT;
                }
            }
            down_pressed = false;
            down_long_triggered = false;
        } else if (down_pressed) {
            uint32_t hold_duration = currentTime - down_press_time;
            if (hold_duration >= 4000) {
                if (!down_long_triggered) {
                    event = KEY_EVENT_DOWN_LONG;
                    down_long_triggered = true;
                    down_last_repeat_time = currentTime;
                } else if (currentTime - down_last_repeat_time >= 40) {
                    event = KEY_EVENT_DOWN_LONG;
                    down_last_repeat_time = currentTime;
                }
            }
        }
    }

    last_up_state = up_state;
    last_down_state = down_state;
    return event;
}

static KeyEvent Process_Menu_Mode(uint8_t up_state, uint8_t down_state) {
    uint32_t currentTime = osKernelGetTickCount();
    KeyEvent event = KEY_EVENT_NONE;

    // 1) 双键按下时，立即屏蔽单键，优先确认
    if (!up_state && !down_state) {
        // 按下双键瞬间，重置单键记录，避免误触发
        last_up_state = 1;
        last_down_state = 1;
        up_press_time = 0;
        down_press_time = 0;

        if (both_press_time == 0) {
            both_press_time = currentTime;
        } else if (currentTime - both_press_time > 1000) {
            both_press_time = 0;
            menu_timeout = currentTime;
            event = KEY_EVENT_BOTH_SHORT;
        }
    } else {
        if (both_press_time > 0) {
            both_press_time = 0;
        }

        // 2) 单键：改为【释放后才触发】，且确认不是按双键
        // 上键短按
        if (last_up_state && !up_state) {
            up_press_time = currentTime;
            last_up_state = up_state;
        } else if (!last_up_state && up_state) {
            uint32_t dur = currentTime - up_press_time;
            if (dur >= 100 && dur < 1000) { // 给双键确认(2000ms)留出空间
                menu_timeout = currentTime;
                event = KEY_EVENT_UP_SHORT;
            }
            last_up_state = up_state;
        }

        // 下键短按
        if (last_down_state && !down_state) {
            down_press_time = currentTime;
            last_down_state = down_state;
        } else if (!last_down_state && down_state) {
            uint32_t dur = currentTime - down_press_time;
            if (dur >= 100 && dur < 1000) {
                menu_timeout = currentTime;
                event = KEY_EVENT_DOWN_SHORT;
            }
            last_down_state = down_state;
        }
    }

    return event;
}

// 处理非菜单/非调零模式按键（背光控制）
static KeyEvent Process_Normal_Mode(uint8_t up_state, uint8_t down_state) {
    uint32_t currentTime = osKernelGetTickCount();
    
    // 上键短按：背光+（只在释放时触发）
    if (!last_up_state && up_state) {  // 释放边沿
        if (currentTime - up_press_time < 1000) {  // 短按
            Backlight_Increase();
        }
    }
    // 记录上键按下时间
    if (last_up_state && !up_state) {  // 按下边沿
        up_press_time = currentTime;
    }
    
    // 下键短按：背光-（只在释放时触发）
    if (!last_down_state && down_state) {  // 释放边沿
        if (currentTime - down_press_time < 1000) {  // 短按
            Backlight_Decrease();
        }
    }
    // 记录下键按下时间
    if (last_down_state && !down_state) {  // 按下边沿
        down_press_time = currentTime;
    }
    
    // 双按键长按检测（保持原逻辑）
    if (!up_state && !down_state) {
        if (both_press_time == 0) {
            both_press_time = currentTime;
        } else if (currentTime - both_press_time > 8000) {
            if (!sysState.is_in_menu && !sysState.is_in_zeroing) {
                Menu_Enter();
                both_press_time = 0;
                last_up_state = up_state;
                last_down_state = down_state;
                return KEY_EVENT_BOTH_LONG;
            }
        }
    } else {
        both_press_time = 0;
    }
    
    last_up_state = up_state;
    last_down_state = down_state;
    return KEY_EVENT_NONE;
}

void KeyManager_Init(void) {
    last_up_state = 1;
    last_down_state = 1;
    both_press_time = 0;
    menu_timeout = 0;
}

KeyEvent KeyManager_Process(void) {
    uint8_t up_state, down_state;
    Key_Scan(&up_state, &down_state);

    // 菜单模式优先处理
    if (sysState.is_in_menu && menu_ctrl.current_status != MENU_ZEROING) {
        return Process_Menu_Mode(up_state, down_state);
    }
    
    // 调零模式按键处理
    if (sysState.is_in_zeroing) {
        return Process_Zeroing_Mode(up_state, down_state);
    }
    
    // 非菜单/非调零模式：处理背光和进入菜单
    return Process_Normal_Mode(up_state, down_state);
}

#include "menu_items.h"
#include "backlight.h"
#include "zero_calibration.h"

extern  uint32_t menu_timeout;
extern void Reconfigure_UART_BaudRate(uint32_t baudRate);
// 声明外部任务句柄（Task_Sensor和Task_Stepper）
extern osThreadId_t sensorTaskHandle;
extern osThreadId_t stepperTaskHandle;
extern osThreadId_t defaultTaskHandle;
// 菜单控制实例化
MenuCtrl menu_ctrl = {
    .current_status = MENU_IDLE,
    .main_index = 0,
    .cf_index = 0,
    .data_index = 0,
    .offset_value = 0,
    .last_oper_time = 0,
    .self_check_done = 0,
    .blink_start_time = 0,      // 新增：闪烁开始时间
    .blink_duration = 4000,     // 新增：闪烁持续时间(ms)
    .blink_state = 0,           // 新增：闪烁状态(0:显示, 1:隐藏)
	  .is_init_blink = 0
};
// 调零状态显示处理（和Menu_SelfCheck风格一致）
void Zeroing_Display_Process(void) {
    static uint32_t lastDisplayTime = 0;
    static uint8_t display_flag = 0;
    uint32_t currentTime = osKernelGetTickCount();

	
    // 仅在调零阶段（is_in_zeroing=1）执行显示逻辑
    if(sysState.is_in_zeroing) {
        // 300ms闪烁周期（和原来自检一致）
        if(currentTime - lastDisplayTime > 400) {
            display_flag = !display_flag;
            lastDisplayTime = currentTime;
        }

        // 整数电机调零阶段
        if(sysState.zeroingStage == 0) {
            // 超时未检测→闪Err1，否则闪-||-
            if(currentTime - sysState.zeroing_timeout > ZEROING_TIMEOUT_MS && !sysState.int_sensor_detected) {
                TM1637_display(display_flag ? 24 : 21, display_flag ? 25 : 21, display_flag ? 25 : 21, display_flag ? 1 : 21);
            } else {
                TM1637_display(display_flag ? 20 : 21, display_flag ? 23 : 21, display_flag ? 22 : 21, display_flag ? 1 : 21);
            }
        }
        // 小数电机调零阶段
        else if(sysState.zeroingStage == 1) {
            // 超时未检测→闪Err2，否则闪-||-
            if(currentTime - sysState.zeroing_timeout > ZEROING_TIMEOUT_MS && !sysState.frac_sensor_detected) {
                TM1637_display(display_flag ? 24 : 21, display_flag ? 25 : 21, display_flag ? 25 : 21, display_flag ? 2 : 21);
            } else {
				  menu_ctrl.self_check_done = 1;
                TM1637_display(display_flag ? 20 : 21, display_flag ? 23 : 21, display_flag ? 22 : 21, display_flag ? 2 : 21);
            }
        }
    }
}

// 菜单初始化（开机调用）
void Menu_Init(void) {
    menu_ctrl.current_status = MENU_IDLE;
    menu_ctrl.last_oper_time = osKernelGetTickCount();

}

// 进入主菜单
void Menu_Enter(void) {
    if(sysState.is_in_zeroing || !menu_ctrl.self_check_done) return;

    osDelay(10);

	    // 关键：进入菜单 → 挂起传感器和步进电机任务
    osThreadSuspend(sensorTaskHandle);    // 停止接收外来数据
    osThreadSuspend(stepperTaskHandle);   // 停止电机动作
	
    menu_ctrl.current_status = MENU_MAIN;
    menu_ctrl.main_index = 0;
    menu_ctrl.last_oper_time = osKernelGetTickCount();
	  sysState.is_in_menu = 1; // 新增：标记进入菜单
    menu_timeout = osKernelGetTickCount(); // 新增：初始化菜单超时计时	
}

// 退出菜单（回到空闲状态）
void Menu_Exit(void) {
	
	    // 2. 触发自动调零归位（核心新增逻辑）
    // 先恢复调零状态机任务（确保调零能执行）
    osThreadResume(defaultTaskHandle);
    // 调用开机级别的自动调零函数
    enterAutoZeroingMode();
    // 等待调零流程执行（可选：根据实际需求调整等待时间）
    osDelay(200);
	
    menu_ctrl.current_status = MENU_IDLE;
	  sysState.is_in_menu = 0; // 新增：标记退出菜单
    TM1637_display(20,20,20,20); // 显示----
}

// 处理菜单按键事件
void Menu_ProcessEvent(KeyEvent event) {
    uint32_t now = osKernelGetTickCount();
	
    // 闪烁期间：初始闪烁（进入子菜单）允许切换选项，确认后闪烁仅响应退出
    if (menu_ctrl.blink_start_time > 0) {
        // 标记：通过blink_duration是否为默认值判断是否是初始闪烁
        uint8_t is_init_blink = (menu_ctrl.blink_duration == 4000) ? 0 : 1;
        
        if (is_init_blink) {
            // 初始闪烁（进入子菜单）：允许切换选项，仅退出时停止
            if (event == KEY_EVENT_BOTH_LONG || event == KEY_EVENT_MENU_TIMEOUT) {
                menu_ctrl.blink_start_time = 0;
                Menu_Exit();
                return;
            }
        } else {
            // 确认后闪烁：仅响应退出
            if (event == KEY_EVENT_BOTH_LONG || event == KEY_EVENT_MENU_TIMEOUT) {
                menu_ctrl.blink_start_time = 0;
                Menu_Exit();
            }
            return;
        }
    }

    if (!sysState.is_in_menu && menu_ctrl.current_status == MENU_IDLE) {
        return;
    }
	
    menu_ctrl.last_oper_time = now;

    switch(menu_ctrl.current_status) {
        case MENU_MAIN: 
            switch(event) {
                case KEY_EVENT_UP_SHORT: 
                    menu_ctrl.main_index = (menu_ctrl.main_index - 1 + 3) % 3;
                    break;
                case KEY_EVENT_DOWN_SHORT: 
                    menu_ctrl.main_index = (menu_ctrl.main_index + 1) % 3;
                    break;
                case KEY_EVENT_BOTH_SHORT: 
                    switch(menu_ctrl.main_index) {
                        case 0: 
                            menu_ctrl.current_status = MENU_CF_SUB;
                            menu_ctrl.cf_index = sysState.currentHeadingType; // 初始值=当前生效值
                            // 进入CF子菜单：触发当前值闪烁（设为持续闪烁，标记初始）
                            menu_ctrl.blink_start_time = now;
                            menu_ctrl.blink_state = 0;
                            menu_ctrl.blink_duration = 0; // 用0标记初始闪烁
                            break;
                        case 1: 
                            menu_ctrl.current_status = MENU_ZEROING;
                            enterZeroingMode(); 
                            break;
                        case 2: 
                            menu_ctrl.current_status = MENU_DATA_SUB;
                            // 初始值=当前波特率对应索引（4800=0,9600=1,38400=2）
                            menu_ctrl.data_index = (sysState.currentBaudRate == 4800) ? 0 : (sysState.currentBaudRate == 9600) ? 1 : 2;
                            // 进入DATA子菜单：触发当前值闪烁
                            menu_ctrl.blink_start_time = now;
                            menu_ctrl.blink_state = 0;
                            menu_ctrl.blink_duration = 0; // 初始闪烁标记
                            break;
                    }
                    break;
                case KEY_EVENT_MENU_TIMEOUT: 
                    Menu_Exit();
                    break;
                default: break;
            }
            break;

        case MENU_CF_SUB: // CF子菜单
            switch(event) {
                case KEY_EVENT_UP_SHORT: 
                    menu_ctrl.cf_index = (menu_ctrl.cf_index - 1 + 7) % 7;
						
						     // 核心：仅当切换到当前生效值时，触发闪烁
									if(menu_ctrl.cf_index == sysState.currentHeadingType) {
										menu_ctrl.blink_start_time = now;
										menu_ctrl.blink_state = 0;
										menu_ctrl.blink_duration = 0; // 持续闪烁直到确认/退出
									} else {
										menu_ctrl.blink_start_time = 0; // 非生效值，停止闪烁
									}
					
                    break;
                case KEY_EVENT_DOWN_SHORT: 
                    menu_ctrl.cf_index = (menu_ctrl.cf_index + 1) % 7;
						
						    // 核心：仅当切换到当前生效值时，触发闪烁
									if(menu_ctrl.cf_index == sysState.currentHeadingType) {
										menu_ctrl.blink_start_time = now;
										menu_ctrl.blink_state = 0;
										menu_ctrl.blink_duration = 0;
									} else {
										menu_ctrl.blink_start_time = 0; // 非生效值，停止闪烁
									}
					
                    break;
                case KEY_EVENT_BOTH_SHORT: 
                    sysState.currentHeadingType = menu_ctrl.cf_index;
                    Flash_Save_HeadingType(menu_ctrl.cf_index);
                    // 确认后：触发新值闪烁（恢复默认时长，标记确认后）
                    menu_ctrl.blink_start_time = now;
                    menu_ctrl.blink_state = 0;
                    menu_ctrl.blink_duration = 4000; // 确认后闪烁时长
                    break;
                default: break;
            }
            break;

        case MENU_DATA_SUB: // DATA子菜单
            switch(event) {
                case KEY_EVENT_UP_SHORT:
                    menu_ctrl.data_index = (menu_ctrl.data_index - 1 + 3) % 3;
						            // 先把索引转成对应的波特率值，再和生效值对比
									uint32_t curr_baud = (menu_ctrl.data_index == 0) ? 4800 : (menu_ctrl.data_index == 1) ? 9600 : 38400;
									if(curr_baud == sysState.currentBaudRate) {
										menu_ctrl.blink_start_time = now;
										menu_ctrl.blink_state = 0;
										menu_ctrl.blink_duration = 0;
									} else {
										menu_ctrl.blink_start_time = 0;
									}
                    break;
                case KEY_EVENT_DOWN_SHORT:
                    menu_ctrl.data_index = (menu_ctrl.data_index + 1) % 3;
						            // 索引转波


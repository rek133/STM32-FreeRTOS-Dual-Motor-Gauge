#include "zero_calibration.h"

extern osThreadId_t sensorTaskHandle;    // 声明外部变量
extern osThreadId_t stepperTaskHandle;
extern osThreadId_t defaultTaskHandle;
extern osMessageQueueId_t pwmQueueHandle;
extern osMessageQueueId_t keyEventQueueHandle;

/**
 * 通用函数：从指定Flash地址读取32位数据，并校验合法性
 * @param addr: Flash地址（4字节对齐）
 * @param defaultValue: 校验失败时的默认值
 * @param min/max: 合法值范围（用于校验）
 * @return: 读取的有效值
 */
static int32_t Flash_Read_Param(uint32_t addr, uint32_t defaultValue, int32_t min, int32_t max) {
    int32_t value = *(int32_t*)addr;
    // 校验：Flash擦除后是0xFFFF，未写入过/数据损坏则返回默认值
    if (value >= min && value <= max) {
        return value;
    } else {
        return defaultValue;
    }
}
/**
 * 擦除Flash指定地址所在的扇区（写入前必须擦除）
 */
static void Flash_Erase(void) {
    FLASH_EraseInitTypeDef eraseInit;
    uint32_t sectorError;

    HAL_FLASH_Unlock();  // 解锁Flash（必须）

    // 配置擦除参数（针对STM32F103C8T6，地址0x0800F800属于扇区3）
    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.PageAddress = ZERO_OFFSET_FLASH_ADDR;  // 要擦除的页地址
    eraseInit.NbPages = 1;  // 只擦除1页（足够存1个int32_t）

    HAL_FLASHEx_Erase(&eraseInit, &sectorError);  // 执行擦除

    HAL_FLASH_Lock();  // 锁定Flash
}
void Flash_Save_ZeroOffset(void) {
    // ========== 步骤1：擦除前备份同页其他参数 ==========
    int32_t fracOffset = Flash_Read_Param(FRACTION_OFFSET_FLASH_ADDR, DEFAULT_FRACTION_ZERO_OFFSET, -10000, 10000);
    uint32_t headingType = Flash_Read_Param(HEADING_TYPE_FLASH_ADDR, DEFAULT_HEADING_TYPE, 0, 6);
    uint32_t baudRate = Flash_Read_Param(BAUD_RATE_FLASH_ADDR, DEFAULT_BAUD_RATE, 4800, 38400);

    // ========== 步骤2：擦除最后一页（必须） ==========
    Flash_Erase();

    // ========== 步骤3：先恢复其他参数（避免丢失） ==========
	  HAL_FLASH_Unlock();  // 解锁Flash
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FRACTION_OFFSET_FLASH_ADDR, fracOffset);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, HEADING_TYPE_FLASH_ADDR, headingType);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, BAUD_RATE_FLASH_ADDR, baudRate);

    // ========== 步骤4：写入当前要保存的参数 ==========
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ZERO_OFFSET_FLASH_ADDR, sysState.zeroOffset);

    HAL_FLASH_Lock();
}
void Flash_Save_FractionZeroOffset(void) {
    // ========== 步骤1：备份其他参数 ==========
    int32_t intOffset = Flash_Read_Param(ZERO_OFFSET_FLASH_ADDR, DEFAULT_ZERO_OFFSET, -10000, 10000);
    uint32_t headingType = Flash_Read_Param(HEADING_TYPE_FLASH_ADDR, DEFAULT_HEADING_TYPE, 0, 6);
    uint32_t baudRate = Flash_Read_Param(BAUD_RATE_FLASH_ADDR, DEFAULT_BAUD_RATE, 4800, 38400);

    // ========== 步骤2：擦除最后一页（必须） ==========
    Flash_Erase();

    // ========== 步骤3：恢复其他参数 ==========
	  HAL_FLASH_Unlock();  // 解锁Flash
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ZERO_OFFSET_FLASH_ADDR, intOffset);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, HEADING_TYPE_FLASH_ADDR, headingType);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, BAUD_RATE_FLASH_ADDR, baudRate);

    // ========== 步骤4：写入当前参数 ==========
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FRACTION_OFFSET_FLASH_ADDR, sysState.fractionZeroOffset);

    HAL_FLASH_Lock();
}
void Flash_Save_HeadingType(uint8_t headingType) {
    // ========== 步骤1：备份其他参数 ==========
    int32_t intOffset = Flash_Read_Param(ZERO_OFFSET_FLASH_ADDR, DEFAULT_ZERO_OFFSET, -10000, 10000);
    int32_t fracOffset = Flash_Read_Param(FRACTION_OFFSET_FLASH_ADDR, DEFAULT_FRACTION_ZERO_OFFSET, -10000, 10000);
    uint32_t baudRate = Flash_Read_Param(BAUD_RATE_FLASH_ADDR, DEFAULT_BAUD_RATE, 4800, 38400);

    // ========== 步骤2：擦除最后一页（必须） ==========
    Flash_Erase();

    // ========== 步骤3：恢复其他参数 ==========
    HAL_FLASH_Unlock();  // 解锁Flash	
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ZERO_OFFSET_FLASH_ADDR, intOffset);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FRACTION_OFFSET_FLASH_ADDR, fracOffset);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, BAUD_RATE_FLASH_ADDR, baudRate);

    // ========== 步骤4：写入当前参数 ==========
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, HEADING_TYPE_FLASH_ADDR, (uint32_t)headingType);

    HAL_FLASH_Lock();
}
void Flash_Save_BaudRate(uint32_t baudRate) {
    // ========== 步骤1：备份其他参数 ==========
    int32_t intOffset = Flash_Read_Param(ZERO_OFFSET_FLASH_ADDR, DEFAULT_ZERO_OFFSET, -10000, 10000);
    int32_t fracOffset = Flash_Read_Param(FRACTION_OFFSET_FLASH_ADDR, DEFAULT_FRACTION_ZERO_OFFSET, -10000, 10000);
    uint32_t headingType = Flash_Read_Param(HEADING_TYPE_FLASH_ADDR, DEFAULT_HEADING_TYPE, 0, 6);

    // ========== 步骤2：擦除最后一页（必须） ==========
    Flash_Erase();

    // ========== 步骤3：恢复其他参数 ==========
    HAL_FLASH_Unlock();  // 解锁Flash	
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ZERO_OFFSET_FLASH_ADDR, intOffset);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FRACTION_OFFSET_FLASH_ADDR, fracOffset);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, HEADING_TYPE_FLASH_ADDR, headingType);

    // ========== 步骤4：写入当前参数 ==========
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, BAUD_RATE_FLASH_ADDR, baudRate);

    HAL_FLASH_Lock();
}
// 加载整数偏差
void Flash_Load_ZeroOffset(void) {
    int32_t savedValue = (int32_t)Flash_Read_Param(ZERO_OFFSET_FLASH_ADDR, DEFAULT_ZERO_OFFSET, -10000, 10000);
    
    // 补充你要求的注释逻辑（和原有调用等价，仅拆分显式标注）
    if (savedValue >= -10000 && savedValue <= 10000) {
        sysState.zeroOffset = savedValue;
    } else {
        sysState.zeroOffset = DEFAULT_ZERO_OFFSET;  // 首次上电或数据损坏时用默认值
    }
}

// 加载小数偏差
void Flash_Load_FractionZeroOffset(void) {
    int32_t savedValue = (int32_t)Flash_Read_Param(FRACTION_OFFSET_FLASH_ADDR, DEFAULT_FRACTION_ZERO_OFFSET, -10000, 10000);
    
    if (savedValue >= -10000 && savedValue <= 10000) {
        sysState.fractionZeroOffset = savedValue;
    } else {
        sysState.fractionZeroOffset = DEFAULT_FRACTION_ZERO_OFFSET;  // 首次上电或数据损坏时用默认值
    }
}

// 加载航向类型
void Flash_Load_HeadingType(void) {
    uint32_t savedValue = Flash_Read_Param(HEADING_TYPE_FLASH_ADDR, DEFAULT_HEADING_TYPE, 0, 6);
    
    if ( savedValue <= 6) {
        sysState.currentHeadingType = savedValue;
    } else {
        sysState.currentHeadingType = DEFAULT_HEADING_TYPE;  // 首次上电或数据损坏时用默认值
    }
}

// 加载波特率
void Flash_Load_BaudRate(void) {
    uint32_t savedValue = Flash_Read_Param(BAUD_RATE_FLASH_ADDR, DEFAULT_BAUD_RATE, 4800, 38400);
    
    if (savedValue == 4800 || savedValue == 9600 || savedValue == 38400) {
        sysState.currentBaudRate = savedValue;
    } else {
        sysState.currentBaudRate = DEFAULT_BAUD_RATE;  // 首次上电或数据损坏时用默认值
    }
}
// 新增：传感器消抖函数（解决电平抖动导致的状态切换）
static uint8_t sensorDebounce(void) {
    static uint8_t debounceCnt = 0;
    uint8_t currentState = HAL_GPIO_ReadPin(ZERO_SENSOR_PRES_PORT, ZERO_SENSOR_PRES_PIN);
    
    // 消抖逻辑：连续3次读取相同电平才确认（可根据实际调整次数）
    if (currentState == 0) { // 检测到低电平（传感器触发）
        debounceCnt++;
        if (debounceCnt >= 3) {
            debounceCnt = 3; // 防止溢出
            return 0;
        }
    } else { // 高电平（未触发）
        debounceCnt = 0;
    }
    return 1; // 未稳定触发时返回高电平，避免误判
}

// 调零状态机（共用传感器，需检测信号）
void zeroingStateMachine(void) {
    static uint32_t lastStepTime = 0;
    static uint32_t fractionLastStepTime = 0; // 小数电机步进时间戳
	
    uint32_t currentTime = osKernelGetTickCount();
    // 读取传感器状态
    uint8_t presence = sensorDebounce();

    // 替换 KeyManager_Process()：从队列读取按键事件（非阻塞）
    KeyEvent keyEvent = KEY_EVENT_NONE;
    osMessageQueueGet(keyEventQueueHandle, &keyEvent, 0, 0);  // 0=非阻塞，不卡状态机

	    // ========== 新增：阶段3：先让小数电机离开感应区 ==========
    if (sysState.zeroingStage == 3 ) {
            if(currentTime - fractionLastStepTime > 4 && sysState.fractionReverse90Steps > 0) {
                setFractionMotorDirection(0);
                stepFractionMotor();
                fractionLastStepTime = currentTime;
                sysState.fractionReverse90Steps--;
            }
            if(sysState.fractionReverse90Steps == 0) {
                sysState.fractionReverse90Steps = FRACTION_STEPS_PER_DEGREE * 180;
                setFractionMotorDirection(1);
                sysState.zeroingStage = 0;
            }
        return; // 该阶段只处理离开逻辑，不执行后续
    }
		
	  // ========== 第一步：整数电机调零 ==========
	  if (sysState.zeroingStage == 0) {
			switch(sysState.zeroingMode) {
        case 0: 
				      sysState.int_sensor_detected = 1; // 标记整数检测完成
                // 整数调零完成 → 切换到小数调零阶段
                sysState.zeroingStage = 1;
                sysState.fractionZeroingMode = 1; // 启动小数调零
                sysState.currentDutyCycle = 0.0f; // 重置占空比基准
			        sysState.zeroing_timeout = osKernelGetTickCount();
            break;
            
        case 1: // 电机转动阶段
            // 每2ms移动一步
            if(currentTime - lastStepTime > 4) {
                stepIntegerMotor();
                lastStepTime = currentTime;
            }
            
            // 检测信号
            if(presence == 0) { 
						if(!sysState.initialReverse90Done){
						     sysState.zeroingMode = 5; // 切换到90反转模式
						}else{
						     sysState.zeroingMode = 2; // 切换到反转模式
						}			 
					}
						
            break;

		  case 2: // 电机反向转动阶段
				  
			    
            if(currentTime - lastStepTime > 10 && sysState.reverseSteps > 0) {
                // 设置电机方向为反向
                setIntegerMotorDirection(1);
                stepIntegerMotor();
                lastStepTime = currentTime;
                sysState.reverseSteps--;
            }
            
            // 反转完成后
            if(sysState.reverseSteps == 0) {
						  sysState.initialReverse90Done = 0; // 重置标记，以便下次使用
                // 根据调零类型决定下一步
                if (sysState.zeroingType == 1) {
                    // 自动调零：直接完成，不进入微调
                    sysState.zeroingMode = 0;
                } else {
                    // 手动调零：进入微调模式
                    sysState.zeroingMode = 3;
                }
                setIntegerMotorDirection(1);
            }
            break;
        case 3: //微调模式 - 按键控制指针移动
				  sysState.zeroing_timeout = osKernelGetTickCount();
					// 优先处理双键长按（确认退出，屏蔽单键干扰）
					if (keyEvent == KEY_EVENT_BOTH_LONG) {
						sysState.zeroingMode = 0;
						// 保存微调结果到系统参数
						if(sysState.fineTuneSteps != 0) {
							sysState.zeroOffset += sysState.fineTuneSteps;  // 累加微调量，而非覆盖
							Flash_Save_ZeroOffset();  // 写入Flash（断电保存）
						}
						break;
					}					                
           switch(keyEvent) {
                case KEY_EVENT_UP_SHORT: // 上键短按：步进1步
                    setIntegerMotorDirection(1);
                    stepIntegerMotor();
                    sysState.fineTuneSteps++;
                    sysState.fineTuneDirection = 1;
                    break;

                case KEY_EVENT_UP_LONG: // 上键长按：步进5步
                    setIntegerMotorDirection(1);
                    stepIntegerMotor();
                    sysState.fineTuneSteps++;
                    sysState.fineTuneDirection = 1;
                    break;

                case KEY_EVENT_DOWN_SHORT : // 下键短按：步进1步
                    setIntegerMotorDirection(0);
                    stepIntegerMotor();
                    sysState.fineTuneSteps--;
                    sysState.fineTuneDirection = 0;
                    break;

                case KEY_EVENT_DOWN_LONG: // 下键长按：步进5步
                    setIntegerMotorDirection(0);
                    stepIntegerMotor();
                    sysState.fineTuneSteps--;
                    sysState.fineTuneDirection = 0;
                    break;

                default:
                    break;
            }
            break;
         case 5: // 第一次调零预处理：先反转90度，再进入正常检测流程

            // 执行90度反转
            if(currentTime - lastStepTime > 4 && sysState.reverse90Steps > 0) {
						  setIntegerMotorDirection(0);   // 设置为反转方向
                stepIntegerMotor();
                lastStepTime = currentTime;
                sysState.reverse90Steps--;
            }
            
            // 90度反转完成后，进入正常检测流程（case 1）
            if(sysState.reverse90Steps == 0) {
						  sysState.initialReverse90Done = 1; // 标记初始化完成
                setIntegerMotorDirection(1);       // 恢复正转方向
                sysState.zeroingMode = 1;          // 进入正常检测阶段
            }
         break;
       }
	   }
    // ========== 第二步：小数电机调零（整数调零完成后执行） ==========
	else if (sysState.zeroingStage == 1) {
		switch(sysState.fractionZeroingMode) {
        case 0: 
				  sysState.frac_sensor_detected = 1; // 标记小数检测完成
					// 调零结束，设置EN引脚为高电平
            HAL_GPIO_WritePin(EN_PORT, EN_PIN, GPIO_PIN_SET);
            sysState.zeroingStage = 2;
			    menu_ctrl.current_status = MENU_IDLE; // 回到菜单空闲状态
            sysState.is_in_menu = 0;              // 确保退出菜单模式
            // 小数调零最终完成
            sysState.is_in_zeroing = 0;
			    delay_us(2000);
			// ========== 调零完成：恢复sensor/stepper，挂起调零任务 ==========
			    osDelay(1000);
            if (sensorTaskHandle != NULL) osThreadResume(sensorTaskHandle);
            if (stepperTaskHandle != NULL) osThreadResume(stepperTaskHandle);
	            // 挂起自己，等待菜单调用
            osThreadSuspend(defaultTaskHandle);			
            break;
            
        case 1: // 小数电机转动检测
            if(currentTime - fractionLastStepTime > 16) { // 小数电机步进间隔
                stepFractionMotor();
                fractionLastStepTime = currentTime;
            }
            if(presence == 0) { // 共用传感器检测
                sysState.fractionZeroingMode = sysState.fractionInitialReverse90Done ? 2 : 5;
            }
            break;

        case 2: // 小数电机反向转动
            if(currentTime - fractionLastStepTime > 40 && sysState.fractionReverseSteps > 0) {
                setFractionMotorDirection(0);
                stepFractionMotor();
                fractionLastStepTime = currentTime;
                sysState.fractionReverseSteps--;
            }
            if(sysState.fractionReverseSteps == 0) {
                sysState.fractionInitialReverse90Done = 0;
                sysState.fractionZeroingMode = (sysState.zeroingType == 1) ? 0 : 3;
                setFractionMotorDirection(1);
            }
            break;

        case 3: // 小数电机微调
				  sysState.zeroing_timeout = osKernelGetTickCount(); 
					// 优先处理双键长按（确认退出，屏蔽单键干扰）
					if (keyEvent == KEY_EVENT_BOTH_LONG) {
						 sysState.fractionZeroingMode = 0; // 小数调零最终完成
						// 保存微调结果到系统参数
						if(sysState.fractionFineTuneSteps != 0) {
                    sysState.fractionZeroOffset += sysState.fractionFineTuneSteps;
                    Flash_Save_FractionZeroOffset();
						}
						break;
					}		
            switch(keyEvent) {
                case KEY_EVENT_UP_SHORT:
                    setFractionMotorDirection(1);
                    stepFractionMotor();
                    sysState.fractionFineTuneSteps++;
                    break;
                case KEY_EVENT_UP_LONG:
                    setFractionMotorDirection(1);
                    stepFractionMotor();
                    sysState.fractionFineTuneSteps++;
                    break;
                case KEY_EVENT_DOWN_SHORT:
                    setFractionMotorDirection(0);
                    stepFractionMotor();
                    sysState.fractionFineTuneSteps--;
                    break;
                case KEY_EVENT_DOWN_LONG:
                    setFractionMotorDirection(0);
                    stepFractionMotor();
                    sysState.fractionFineTuneSteps--;
                    break;
                default: break;
            }
            break;

        case 5: // 小数电机初始90度反转
            if(currentTime - fractionLastStepTime > 16 && sysState.fractionReverse90Steps > 0) {
                setFractionMotorDirection(0);
                stepFractionMotor();
                fractionLastStepTime = currentTime;
                sysState.fractionReverse90Steps--;
            }
            if(sysState.fractionReverse90Steps == 0) {
                sysState.fractionInitialReverse90Done = 1;
                setFractionMotorDirection(1);
                sysState.fractionZeroingMode = 1;
            }
            break;
    }
	}
}
// 进入自动调零模式（开机调用）
void enterAutoZeroingMode(void) {
    HAL_GPIO_WritePin(EN_PORT, EN_PIN, GPIO_PIN_RESET);
	    // 读取传感器状态
    uint8_t presence0 = HAL_GPIO_ReadPin(ZERO_SENSOR_PRES_PORT, ZERO_SENSOR_PRES_PIN);
	
    // 完全重置所有状态
    sysState.zeroingMode = 1;
    sysState.zeroingType = 1;
    sysState.uartDataReady = 0;
	  sysState.is_in_zeroing = 1;
	  sysState.zeroingStage = 0;  // 确保从阶段0开始
	if(presence0 == 0){
				sysState.zeroingStage = 3;
	}
	  sysState.int_sensor_detected = 0;   // 重置整数传感器检测标志
    sysState.frac_sensor_detected = 0;  // 重置小数传感器检测标志
	  sysState.zeroing_timeout = osKernelGetTickCount(); // 赋值为当前时间，开始计时
	
	     // 整数电机初始化
    sysState.lastDetectTime = osKernelGetTickCount();
    sysState.fineTuneSteps = 0;
    sysState.fineTuneDirection = 0;
    sysState.reverseSteps = INTEGER_STEPS_PER_DEGREE * 45 + sysState.zeroOffset;
    sysState.reverse90Steps = INTEGER_STEPS_PER_DEGREE * 90;
    setIntegerMotorDirection(1);
    
	    // 小数电机初始化（待整数调零完成后启动）
    sysState.fractionZeroingMode = 0;
    sysState.fractionReverseSteps = INTEGER_STEPS_PER_DEGREE * 20 /4 - sysState.fractionZeroOffset;
    sysState.fractionReverse90Steps = INTEGER_STEPS_PER_DEGREE * 90/4;
    sysState.fractionFineTuneSteps = 0;
	sysState.fractionFineTuneDirection = 0; // 新增：微调方向初始化
    sysState.fractionInitialReverse90Done = 0; // 新增：90度反转标记初始化
	  setFractionMotorDirection(1); // 新增：默认正转方向
	
    // 重置所有按键状态
    KeyManager_Init();

}

// 进入手动调零模式（出厂设置偏差值）
void enterZeroingMode(void) {
	// 恢复调零状态机任务
    osThreadResume(defaultTaskHandle);
    HAL_GPIO_WritePin(EN_PORT, EN_PIN, GPIO_PIN_RESET);
	    // 读取传感器状态
    uint8_t presence0 = HAL_GPIO_ReadPin(ZERO_SENSOR_PRES_PORT, ZERO_SENSOR_PRES_PIN);
	
    // 完全重置所有状态
    sysState.zeroingMode = 1;
    sysState.zeroingType = 0;
    sysState.uartDataReady = 0;
	  sysState.is_in_zeroing = 1;
	  sysState.zeroingStage = 0;  // 确保从阶段0开始
	if(presence0 == 0){
				sysState.zeroingStage = 3;
	}
	  sysState.int_sensor_detected = 0;   // 重置整数传感器检测标志
    sysState.frac_sensor_detected = 0;  // 重置小数传感器检测标志
	  sysState.zeroing_timeout = osKernelGetTickCount(); // 赋值为当前时间，开始计时
	
	// 整数电机初始化
    sysState.lastDetectTime = osKernelGetTickCount();
    sysState.fineTuneSteps = 0;
    sysState.fineTuneDirection = 0;
    sysState.reverseSteps = INTEGER_STEPS_PER_DEGREE *45 + sysState.zeroOffset;
    sysState.reverse90Steps = INTEGER_STEPS_PER_DEGREE * 90;
    setIntegerMotorDirection(1);
    
	  // 小数电机初始化
    sysState.fractionZeroingMode = 0;
    sysState.fractionReverseSteps = INTEGER_STEPS_PER_DEGREE * 20/4 - sysState.fractionZeroOffset;
    sysState.fractionReverse90Steps = INTEGER_STEPS_PER_DEGREE * 90/4;
    sysState.fractionFineTuneSteps = 0;
	
    // 重置所有按键状态
    KeyManager_Init();

}

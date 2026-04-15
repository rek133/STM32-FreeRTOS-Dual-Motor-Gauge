#ifndef SHARED_DATA_H
#define SHARED_DATA_H
#include "stm32f1xx_hal.h"
#include "cmsis_os2.h"  // 确保包含RTOS头文件


typedef struct {                     // 系统状态结构体
    // ========== 整数电机状态 ==========
    int32_t integerRemainingSteps;   // 剩余步数
    int8_t  integerDirection;        // 方向 (1:顺时针, -1:逆时针)
    uint32_t integerStepDelay;       // 电机步间延迟（ms）
    uint32_t lastIntegerStepTime;    // 上次步进时间
	  uint8_t integerMotorCompleted;  // 0：整数电机未完成；1：整数电机已完成（步数为0）
    int32_t integerCompensationError;  // 累积的补偿误差
	
	  // ========== 小数电机状态 ==========
    int32_t fractionRemainingSteps;  // 小数电机剩余步数
    int8_t  fractionDirection;       // 小数电机方向 (1:顺时针, -1:逆时针)
    float fractionStepDelay;      // 小数电机步间延迟（ms）
    uint32_t lastFractionStepTime;   // 小数电机上次步进时间
	  uint32_t fractionStepsCounter;   // 新增：小数电机步数计数器（用于联动整数电机）
	  int8_t  stepRemainder;
	  int32_t fractionStepCounter; // 小数步进计数器（有符号，支持正反累积）
	  int32_t lastDirChangeTime;
    uint8_t isDecelPhase;	
    // ========== 调零相关状态 ==========
	uint8_t zeroingStage;    // 0:整数调零中 1:小数调零中 2:调零完成
    uint8_t zeroingMode;             // 整数电机调零模式 
    uint32_t reverseSteps;           // 整数电机需要反转的步数
    uint32_t lastDetectTime;         // 上次检测到信号的时间           
    int32_t fineTuneSteps;           // 整数电机微调步数              
    uint8_t fineTuneDirection;       // 整数电机微调方向              
    int32_t zeroOffset;              // 整数电机零点偏移量
    uint8_t is_in_zeroing;           // 整体调零状态（1=调零中）
    uint8_t zeroingJustEnded;        // 调零刚结束标志
    uint8_t zeroingType;             // 调零类型（1=自动，0=手动）
    uint8_t initialReverse90Done;    // 整数电机初始90度反转完成标志
    uint32_t reverse90Steps;         // 整数电机初始90度反转步数
	uint8_t int_sensor_detected;  // 整数电机是否检测到传感器（0=未检测，1=已检测）
    uint8_t frac_sensor_detected; // 小数电机是否检测到传感器（0=未检测，1=已检测）
    uint32_t zeroing_timeout;     // 调零超时计时
    #define ZEROING_TIMEOUT_MS 30000 // 调零超时时间（30秒）
	
	  // ========== 小数电机调零相关 ==========
    uint8_t fractionZeroingMode;     // 小数电机调零模式
    uint32_t fractionReverseSteps;   // 小数电机需要反转的步数
    int32_t fractionFineTuneSteps;   // 小数电机微调步数
    uint8_t fractionFineTuneDirection; // 小数电机微调方向
    int32_t fractionZeroOffset;      // 小数电机零点偏移量
    uint8_t fractionInitialReverse90Done; // 小数电机初始90度反转完成标志
    uint32_t fractionReverse90Steps; // 小数电机初始90度反转步数
	int32_t fractionStepCount; // 小数电机步进计数器（统计已走步数）
		
	
    // ========== PWM输入处理 ==========
    float currentDutyCycle;          // 当前占空比值 (0%-200%)
    float targetDutyCycle;           // 目标占空比值 (0%-200%)
    float filteredDutyCycle;         // 滤波后的占空比（减少抖动）
    uint32_t lastPwmUpdateTime;      // 上次PWM更新时间
    float pwmChangeRate;             // PWM变化速率（%/ms）
    int16_t lastSentValue;             // 上次发送的值（死区控制用）
    float integerStepsPerPercent;    // 整数电机每1%���空比对应的步数
    uint16_t fractionStepsPerPercent;   // 小数电机每1%占空比对应的步数
    
    // ========== 步进电机参数 ==========
    uint8_t delayStableStepCnt;      // 当前延迟已走步数计数

	
    // ========== 串口输入相关 ==========
	uint32_t currentBaudRate;      // 当前波特率
    uint8_t currentHeadingType;    // 当前航向数据类型 (0:HDG, 1:HDT, 2:HDM, 3:THS)
    float rotValue;                  // 接收到的ROT值(-30到30)
    uint8_t uartDataReady;           // 串口数据就绪标志
	uint32_t uartDataLength;

    // ========== 菜单相关 ==========
    uint8_t is_in_menu;              // 菜单模式标志（1=进入菜单，0=退出菜单）
	
	// ========== 新增：显示相关状态 ==========
    float lastDisplayValue;      // 上次显示的数值（用于判断0.1刻度变化）
    uint8_t displayUpdateFlag;   // 显示更新标志（0.1刻度变化时置1）
	uint8_t noDataFlag;      // 新增：无数据标记，1表示无数据
	float displayedDutyCycle;  // 只用于显示的值
	
    uint8_t decelStepCnt;
	uint8_t isMotorMoving; 

}SystemState;

extern SystemState sysState;
// 定义互斥锁（全局）
extern osMutexId_t sysStateMutex;
// 定义互斥锁操作宏（简化代码）
#define LOCK_SYS_STATE()  osMutexAcquire(sysStateMutex, osWaitForever)
#define UNLOCK_SYS_STATE() osMutexRelease(sysStateMutex)
// 安全的时间差计算宏（解决uint32_t溢出问题）
#define TIME_DIFF(a, b)  ((uint32_t)((a) - (b)))

// ========== 步进电机基础物理参数（最核心，先定义） ==========
#define STEPS_PER_REVOLUTION    200         // 步进电机每转物理步数（1.8°/步）
#define MICROSTEPS              16          // 微步细分（0.225°/微步）

// ========== 步进电机每度步数（自动计算） ==========
#define INTEGER_STEPS_PER_DEGREE ((STEPS_PER_REVOLUTION * MICROSTEPS) / 360.0f)  // 整数电机每度步数（≈8.888步/度）
#define FRACTION_STEPS_PER_DEGREE 160                        // 小数电机每度步数（16步对应0.1°）

 
#define ROT_FILTER_WINDOW 5  // 滑动窗口大小（可根据效果调整，5~10合适）



// ========== 硬件引脚定义 ==========
#define BACKLIGHT_UP_PIN GPIO_PIN_2
#define BACKLIGHT_UP_PORT GPIOA
#define BACKLIGHT_DOWN_PIN GPIO_PIN_3
#define BACKLIGHT_DOWN_PORT GPIOA
										 
// TMC2225-SA 步进电机驱动引脚定义（整数电机）
#define MOTOR_INTEGER_DIR_PIN   GPIO_PIN_5     // 方向引脚
#define MOTOR_INTEGER_DIR_PORT  GPIOA
#define MOTOR_INTEGER_STEP_PIN  GPIO_PIN_6     // 步进引脚
#define MOTOR_INTEGER_STEP_PORT GPIOA

// TMC2225-SA 步进电机驱动引脚定义（小数电机）
#define MOTOR_FRACTION_DIR_PIN   GPIO_PIN_0     // 方向引脚
#define MOTOR_FRACTION_DIR_PORT  GPIOB
#define MOTOR_FRACTION_STEP_PIN  GPIO_PIN_1     // 步进引脚
#define MOTOR_FRACTION_STEP_PORT GPIOB

// 零位传感器引脚
#define ZERO_SENSOR_PRES_PIN   GPIO_PIN_4     // SENSOR
#define ZERO_SENSOR_PRES_PORT  GPIOB
#define EN_PIN  GPIO_PIN_5                    //  EN
#define EN_PORT GPIOB

// PWM输出引脚
#define PWM_PORT    GPIOA               
#define PWM_PIN     GPIO_PIN_8

#endif

#include "backlight.h"
#include "cmsis_os.h"


// 背光状态实例
static Backlight_State_t backlight = {
    .current_level = 2,        // 默认中等亮度
};

// 亮度等级对应的PWM占空比值(0-ARR)
// 使用非线性映射以获得更好的视觉效果
static const uint32_t brightness_map[BACKLIGHT_LEVELS] = {
    0,      // 等级0: 关闭
    250,     // 等级1: 很暗
    500,    // 等级2: 暗
    750,    // 等级3: 中等
    999,    // 等级4: 亮
};

// 初始化背光控制
void Backlight_Init(void) {
    // 启动PWM输出
    HAL_TIM_PWM_Start(&BACKLIGHT_PWM_TIM_HANDLE, BACKLIGHT_PWM_CHANNEL);
    
    // 对于高级定时器TIM1，需要使能主输出
    #ifdef TIM1
    __HAL_TIM_MOE_ENABLE(&BACKLIGHT_PWM_TIM_HANDLE);
    #endif
    // 设置初始亮度
    Backlight_SetLevel(backlight.current_level);
}

// 设置背光亮度等级
void Backlight_SetLevel(uint8_t level) {
    // 确保等级在有效范围内
    if (level >= BACKLIGHT_LEVELS) {
        level = BACKLIGHT_LEVELS - 1;
    }  
    // 更新等级
    backlight.current_level  = level;
    
    // 立即更新PWM输出
    __HAL_TIM_SET_COMPARE(&BACKLIGHT_PWM_TIM_HANDLE, BACKLIGHT_PWM_CHANNEL, 
                         brightness_map[backlight.current_level]);
   
}

// 增加背光亮度
void Backlight_Increase(void) {  
    uint8_t new_level = backlight.current_level + 1;
    // 循环增加: 如果已经是最高级，则回到最低级(0)
    if (new_level >= BACKLIGHT_LEVELS) {
        new_level = BACKLIGHT_LEVELS;
    }
    Backlight_SetLevel(new_level);
	TM1637_SetBrightness(new_level);
}

// 减少背光亮度
void Backlight_Decrease(void) { 
    uint8_t new_level;

    if (backlight.current_level == 0) {
        new_level = 0;
    } else {
        new_level = backlight.current_level - 1;
    }
    Backlight_SetLevel(new_level);
	TM1637_SetBrightness(new_level);
}

void convertToDisplayCode(float value, uint8_t *dispCode) {
    // ========== 新增：数值不变计时逻辑（核心） ==========
    static float lastValue = 0.0f;          // 记录上次传入的数值
    static uint32_t lastChangeTime = 0;     // 记录上次数值变化的时间戳
    #define VALUE_STABLE_TIME_MS 20000       // 数值不变超时时间（10秒）

    // 安全校验：防止空指针
    if (dispCode == NULL) return;

    // ========== 步骤1：0值归一化（保留原有逻辑） ==========
    if (fabsf(value) < 0.05f) {
        value = 0.0f;
    }

    // ========== 核心修复1：四舍五入加偏移量，抵消浮点数精度漂移 ==========
    float roundedValue = roundf((value + 1e-6f) * 10.0f) / 10.0f;

    // ========== 步骤2：检测数值是否变化（改用四舍五入后的值对比） ==========
    if (fabsf(roundedValue - lastValue) >= 0.01f) { // 数值变化≥0.1视为有效变化
        lastValue = roundedValue;                  // 更新上次数值（四舍五入后）
        lastChangeTime = osKernelGetTickCount();   // 重置计时
    } 


    // ========== 新增：检测无数据情况（保留原有逻辑） ==========
    if (value <= -99.0f) {  // 使用一个不可能出现的特殊值作为无数据标记
        dispCode[0] = 20; // -
        dispCode[1] = 20; // -
        dispCode[2] = 20; // -
        dispCode[3] = 20; // -
        return;  // 直接返回，不再执行后续逻辑
    }

    // ========== 步骤3：基于四舍五入后的值拆分整数/小数部分 ==========
    uint8_t isNegative = (roundedValue < 0.0f) ? 1 : 0; // 是否负数（基于四舍五入后的值）
    float absValue = fabsf(roundedValue);               // 绝对值（四舍五入后）
    int32_t integerPart = (int32_t)absValue;            // 整数部分（0~400）
    // 新增：百位拆分（解决200/300/400进位问题）
    int32_t hundredsPart = (integerPart / 100) % 10; // 百位（0=无，1=100，2=200，3=300，4=400）
    int32_t tensPart = (integerPart / 10) % 10;     // 十位（原有逻辑补全）
    int32_t unitsPart = integerPart % 10;           // 个位

    // ========== 核心修复2：小数位提取（彻底避免浮点数减法精度误差） ==========
    int32_t decimalPart = (int32_t)(roundedValue * 10) % 10; // 直接从四舍五入后的值提取
    decimalPart = abs(decimalPart); // 兼容负数，确保小数位为正
    decimalPart = (decimalPart > 9) ? 9 : (decimalPart < 0 ? 0 : decimalPart); // 边界保护

    // 3. 初始化所有位为21（黑），从右往左填充（完全保留）
    dispCode[0] = 21; // 第1位（最左）
    dispCode[1] = 21; // 第2位（左中）
    dispCode[2] = 21; // 第3位（右中，个位+小数点）
    dispCode[3] = 21; // 第4位（最右，小数位）

    // 4. 从最右侧开始填充（仅把原有integerPart%10换成unitsPart，其余不变）
    dispCode[3] = decimalPart;                          // 第4位：小数位（固定最右）
    dispCode[2] = 10 + unitsPart;                       // 第3位：个位（带小数点）

    // 5. 处理十位/负号/百位（仅加百位进位判断，其余保留原有逻辑）
    if (integerPart >= 10) {
        dispCode[1] = tensPart; // 第2位：显示十位数字

        // 新增：百位进位判断（核心修复200变100的问题）
        if (hundredsPart > 0) {
            dispCode[0] = isNegative ? 20 : hundredsPart; // 百位显示数字/负号
        }
    } else {
        // 情况2：整数部分<10（无十位）
        if (isNegative) {
            dispCode[1] = 20; // 第2位：显示负号（-），紧邻个位左侧
        }
    }
}
// TM1637驱动实现 
static const uint8_t tab[] = {                          // 共阴数码管段码表 
  0x3F,/*0*/
	0x06,/*1*/
	0x5B,/*2*/
	0x4F,/*3*/
	0x66,/*4*/
	0x6D,/*5*/
	0x7D,/*6*/
	0x07,/*7*/
	0x7F,/*8*/
	0x6F,/*9*/
	
  0xBF, // 0. 1011 1111
  0x86, // 1. 1000 0110
  0xDB, // 2. 1101 1011
  0xCF, // 3. 1100 1111
  0xE6, // 4. 1110 0110
  0xED, // 5. 1110 1101
  0xFD, // 6. 1111 1101
  0x87, // 7. 1000 0111
  0xFF, // 8. 1111 1111
  0xEF, // 9. 1110 1111
	
	0x40, // '-'
  0x00, // 黑屏
  0x70, // '|-'
	0x46, // '-|'
	0x79, // 'E'
	0x50, // 'r'
	0x39, // 'C'
	0x71, // 'F'
	0x0F, // ']'
	0x5E, // 'D'
	
	0x77, // 'A'
	0x07, // 'T'
	0x76, // 'H'
	0x73, // 'P'
	0x6D, // 'S'
	0x3D, // 'G'
	0x37, // 'M'
	
};

// 使用DWT计数器实现精确微秒延时（非阻塞版本）
void TM1637_DELAY(uint32_t microseconds) {
    uint32_t start = DWT->CYCCNT;
    // 计算需要的CPU周期数
    uint32_t cycles = (SystemCoreClock / 1000000) * microseconds;
    
    // 短延时：直接忙等待（适合<100μs）
    if (microseconds < 100) {
        while ((DWT->CYCCNT - start) < cycles) {
            // 空循环
        }
    } 
    // 长延时：允许任务切换
    else {
        uint32_t elapsed;
        do {
            elapsed = DWT->CYCCNT - start;
            // 每等待100个周期就让出CPU一次
            if ((elapsed % 100) == 0) {
                taskYIELD(); // 让出CPU给其他任务
            }
        } while (elapsed < cycles);
    }
}
void TM1637_start(void) {                                      // TM1637开始信号
  TM1637_CLK_HIGH();
	
	TM1637_DIO_HIGH();

	TM1637_DELAY(2);
	
  TM1637_DIO_LOW();
	
}
void TM1637_stop(void) {                                       // 停止信号
    TM1637_CLK_LOW();
    TM1637_DELAY(2);
    
    TM1637_DIO_LOW();
    TM1637_DELAY(2);
    
    TM1637_CLK_HIGH();
    TM1637_DELAY(2);
    
    TM1637_DIO_HIGH();
    TM1637_DELAY(2);
}
void TM1637_Write(uint8_t data) {                              // 写入函数
    unsigned char  i;
 
        for(i=0;i<8;i++)
        {
            TM1637_CLK_LOW();       
            
            if(data&0x01)
            {
               TM1637_DIO_HIGH();
            }
            else
            {
               TM1637_DIO_LOW();
            }
            
            TM1637_DELAY(3);
        
				data>>=1;
				
            TM1637_CLK_HIGH();
			
            TM1637_DELAY(3);
        
        }

}

void TM1637_ack(){                                             //应答信号
	  uint8_t i;
	
    TM1637_CLK_LOW();
    
    TM1637_DELAY(5);

	  while(HAL_GPIO_ReadPin(TM1637_DIO_PORT,TM1637_DIO_PIN) == 0x01&&(i<250))
		i++;

    TM1637_CLK_HIGH();
    TM1637_DELAY(2);
 
     
    TM1637_CLK_LOW();

 
 
}

void TM1637_display(uint8_t a,uint8_t b,uint8_t c,uint8_t d) {             // 显示数据

        TM1637_start(); 
        TM1637_Write(0x40);
        TM1637_ack();
			TM1637_stop();
      
        TM1637_start();
        
		    TM1637_Write(0xc0);
	      TM1637_ack();
	
			TM1637_Write(tab[a]);
			TM1637_ack();
	
			TM1637_Write(tab[b]);
			TM1637_ack();
	
			TM1637_Write(tab[c]);
			TM1637_ack();
	
			TM1637_Write(tab[d]);
			TM1637_ack();
	
			TM1637_stop();
	
}
		


void TM1637_SetBrightness(uint8_t level) {                     // 设置亮度 
  TM1637_start();
  	if (level == 0) {
        // 发送关闭显示命令 (0x80)
        TM1637_Write(0x80);
    } else {
        // 设置亮度 (0x88 | (level-1))，将1-7映射到0-6
        TM1637_Write(0x88 | ((level - 1) & 0x07));
    }
	TM1637_ack();
  TM1637_stop();
}

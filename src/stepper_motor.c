#include "stepper_motor.h"
#include "shared_data.h"
#include <stdlib.h>  
#include <math.h>  

void delay_us(uint32_t us) {
    if (us == 0) return;
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = (SystemCoreClock / 1000000) * us;
    while ((DWT->CYCCNT - start) < cycles);
}

// ==================== 宽脉冲保留（力矩核心） ====================
void stepIntegerMotor(void) {     
    uint32_t pulse_us = 200;  
    HAL_GPIO_WritePin(MOTOR_INTEGER_STEP_PORT, MOTOR_INTEGER_STEP_PIN, GPIO_PIN_SET);
    delay_us(pulse_us); 
    HAL_GPIO_WritePin(MOTOR_INTEGER_STEP_PORT, MOTOR_INTEGER_STEP_PIN, GPIO_PIN_RESET);
    delay_us(pulse_us);  
} 

void setIntegerMotorDirection(uint8_t clockwise) {     
    HAL_GPIO_WritePin(MOTOR_INTEGER_DIR_PORT, MOTOR_INTEGER_DIR_PIN, 
                     clockwise ? GPIO_PIN_SET : GPIO_PIN_RESET);
    delay_us(100);  
}

void stepFractionMotor(void) {     
    uint32_t pulse_us = 200;
    HAL_GPIO_WritePin(MOTOR_FRACTION_STEP_PORT, MOTOR_FRACTION_STEP_PIN, GPIO_PIN_SET);
    delay_us(pulse_us); 
    HAL_GPIO_WritePin(MOTOR_FRACTION_STEP_PORT, MOTOR_FRACTION_STEP_PIN, GPIO_PIN_RESET);
    delay_us(pulse_us);
} 

void setFractionMotorDirection(uint8_t clockwise) {     
    HAL_GPIO_WritePin(MOTOR_FRACTION_DIR_PORT, MOTOR_FRACTION_DIR_PIN, 
                     clockwise ? GPIO_PIN_SET : GPIO_PIN_RESET);
    delay_us(100);
}

// ==================== 线性加减速配置（核心参数） ====================
#define FAST_DELAY 1.0f    // 最快速度（最终目标延时）
#define SLOW_DELAY 10.0f    // 最慢速度（初始/减速后延时）
#define ACC_DEC_THRESHOLD 100.0f // 减速阈值：剩余距离<100开始减速
#define START_STEP_NUM 30  // 启动前30步强制最慢（避免启动丢步）
#define ACC_STEP 0.1f      // 加速步长（每步减少0.1ms，从8→1需70步，线性渐变）
#define DEC_STEP 0.1f      // 减速步长（每步增加0.1ms，从1→8需70步，线性渐变）
// 新增：每个延迟值稳定走的步数（可调，建议10步）
#define DELAY_STABLE_STEPS  10  

void moveMotorTask(void) {  
    uint32_t currentTime = osKernelGetTickCount();
    uint32_t delay_ms = (uint32_t)roundf(sysState.fractionStepDelay);
	
	
    // ========== 1. 核心参数计算（先算目标方向/剩余步数，不立即更新状态） ==========
    float rawDiff = sysState.targetDutyCycle - sysState.currentDutyCycle;
    float dutyDifference;
    if (rawDiff > 1800.0f) {
        dutyDifference = rawDiff - 3600.0f; 
    } else if (rawDiff < -1800.0f) {
        dutyDifference = rawDiff + 3600.0f; 
    } else {
        dutyDifference = rawDiff; 
    }
    int8_t targetDirection = (dutyDifference > 0) ? 1 : -1;
    uint32_t targetRemainingSteps = abs((int32_t)(dutyDifference * 8));

	if (sysState.isMotorMoving == 0) {
        sysState.fractionRemainingSteps = targetRemainingSteps;
    }
			
    // ========== 2. 换向检测（仅标记状态，不跳过执行） ==========
    // 换向条件：电机有剩余步数 + 目标方向≠当前方向 → 标记进入换向减速
    if (sysState.isMotorMoving == 0 && sysState.fractionRemainingSteps > 0 && 
        targetDirection != sysState.fractionDirection) {
        sysState.isMotorMoving = 1; 
		sysState.delayStableStepCnt = 0; // 新增：换向时重置稳定步数计数
    }

    // ========== 3. 统一的电机移动执行逻辑（核心：无论换向/正常，都走步进） ==========
    if (sysState.fractionRemainingSteps > 0 && 
        TIME_DIFF(currentTime, sysState.lastFractionStepTime) >= delay_ms) {

        // ------------ 3.1 换向减速逻辑（优先执行，替代正常加减速） ------------
        if (sysState.isMotorMoving) {
            // 换向减速：线性减速到最慢（只减速，不加速）
            if (sysState.fractionStepDelay < SLOW_DELAY) {
                sysState.fractionStepDelay += DEC_STEP;
                if (sysState.fractionStepDelay > SLOW_DELAY) {
                    sysState.fractionStepDelay = SLOW_DELAY;
                }
            } else {
                // 减速到最慢后：完成换向（切换方向+重置状态）
                sysState.isMotorMoving = 0;                    // 退出换向减速
                sysState.fractionDirection = targetDirection; // 切换到新方向
                sysState.decelStepCnt = 0;                    // 重置启动加速计数
                sysState.fractionRemainingSteps = targetRemainingSteps; // 更新剩余步数
						  sysState.delayStableStepCnt = 0; // 新增：换向完成重置计数
                // 同步更新硬件方向
                setFractionMotorDirection(sysState.fractionDirection == 1);
                setIntegerMotorDirection(sysState.fractionDirection == 1);
            }
        }
        // ------------ 3.2 正常移动加减速逻辑（换向减速时不执行） ------------
else {
    float currentDistance = fabs(sysState.targetDutyCycle - sysState.currentDutyCycle);
    if (currentDistance > 1800.0f) {
        currentDistance = 3600.0f - currentDistance;
    }

    // 启动稳定（前30步最慢）
    if (sysState.decelStepCnt < START_STEP_NUM) {
        sysState.fractionStepDelay = SLOW_DELAY;
        sysState.decelStepCnt++;
        sysState.delayStableStepCnt = 0; // 重置计数
    } else {
        // 先判断当前延迟是否走够稳定步数
        if (sysState.delayStableStepCnt >= DELAY_STABLE_STEPS) {
            // 走够10步，才允许改延迟
            if (currentDistance > ACC_DEC_THRESHOLD) {
                // 加速：每10步减0.1ms
                if (sysState.fractionStepDelay > FAST_DELAY) {
                    sysState.fractionStepDelay -= ACC_STEP;
                    if (sysState.fractionStepDelay < FAST_DELAY) {
                        sysState.fractionStepDelay = FAST_DELAY;
                    }
                }
            } else { 
                // 减速：每10步加0.1ms
                if (sysState.fractionStepDelay < SLOW_DELAY) {
                    sysState.fractionStepDelay += DEC_STEP;
                    if (sysState.fractionStepDelay > SLOW_DELAY) {
                        sysState.fractionStepDelay = SLOW_DELAY;
                    }
                }
            }
            sysState.delayStableStepCnt = 0; // 重置计数
        } else {
            // 没走够10步，延迟不变
            sysState.delayStableStepCnt++;
        }
    }
}

        // 3. 执行步进（步数精准扣减）
        stepFractionMotor();
        sysState.fractionRemainingSteps--; 
        sysState.lastFractionStepTime = currentTime;
        
        // 4. 位置更新（原有逻辑完全保留）
        if (sysState.fractionDirection == 1) {
            sysState.currentDutyCycle += 0.125f;
            if (sysState.currentDutyCycle >= 3600.0f) {
                sysState.currentDutyCycle -= 3600.0f;
            }
        } else {
            sysState.currentDutyCycle -= 0.125f;
            if (sysState.currentDutyCycle < 0.0f) {
                sysState.currentDutyCycle += 3600.0f;
            }
        }
        sysState.displayedDutyCycle = sysState.currentDutyCycle;
        
        // 5. 整数电机同步（原有逻辑完全保留）
        sysState.fractionStepCounter += (sysState.fractionDirection == 1) ? 1 : -1;
        if (sysState.fractionStepCounter > 18 || sysState.fractionStepCounter < -18) {
            sysState.fractionStepCounter = 0;
        }
        if (sysState.fractionStepCounter >= 9) {
            stepIntegerMotor();
            sysState.fractionStepCounter -= 9;
        } else if (sysState.fractionStepCounter <= -9) {
            stepIntegerMotor();
            sysState.fractionStepCounter += 9;
        }
        
        // 更新延时（用于下一次判断）
        delay_ms = (uint32_t)roundf(sysState.fractionStepDelay);
    }
}

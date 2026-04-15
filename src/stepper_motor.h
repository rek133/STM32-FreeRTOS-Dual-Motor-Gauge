#include "stdint.h"
#include "shared_data.h"
#include "cmsis_os.h"
#include "math.h"
#include "main.h"

void simple_delay(uint32_t count);
void delay_us(uint32_t us);
void stepIntegerMotor(void);
void setIntegerMotorDirection(uint8_t clockwise);
void stepFractionMotor(void);
void setFractionMotorDirection(uint8_t clockwise);
void calculateMoveToTarget(float dutyCycle);
void moveMotorTask(void);
void executeFilteredCommand(float filteredTarget);
float calculateWeightedAverage(void);

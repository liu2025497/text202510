#include <reg52.h>

sbit START = P1^0;        // 启动按钮（低有效）
sbit REVERSE_SW = P1^1;   // 反转开关：高=正转，低=反转
sbit STOP = P1^2;         // 停止按钮（低有效）
sbit PUMP_MODE = P1^3;    // 油泵模式：高=协同，低=独立

// 指示灯（共阳极：低亮）
sbit LED_POWER = P3^0;    // 电源灯（常亮）
sbit LED_START = P3^1;    // 启动灯
sbit LED_STOP = P3^2;     // 停止灯
sbit LED_PUMP = P3^3;     // 油泵灯
sbit LED_MOTOR_F = P3^4;  // 正转灯
sbit LED_MOTOR_R = P3^5;  // 反转灯
sbit LED_COOL = P3^6;     // 冷却灯
sbit LED_DIR_WAIT = P3^7; // 方向等待灯

// L298驱动
sbit IN1 = P2^2;          // 电机正转控制
sbit IN2 = P2^3;          // 电机反转控制
sbit IN3 = P2^4;          // 油泵方向（固定低）
sbit IN4 = P2^5;          // 油泵使能（高=启动）

// 数码管相关
sbit DUAN = P2^6;         // 段锁存
sbit WEI = P2^7;          // 位锁存
#define DataPort P0       // 段码数据口
unsigned char code duanMa[] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F}; // 共阴极段码
unsigned char code weiMa[] = {0xFE,0xFD,0xFB,0xF7};                               // 位码（低有效）

// 全局变量
unsigned char dispBuf[4] = {0,0,0,0};
unsigned int workCount = 0;           // 工作次数（0-4）
unsigned int pumpPreTimer = 0;        // 油泵预热计时（10秒）
unsigned int motorTimer = 0;          // 电机运行计时（20秒）
unsigned int coolTimer = 0;           // 冷却计时（10秒）
unsigned int dirWaitTimer = 0;        // 方向等待计时（10秒）

// 状态标志
bit pumpPreEn = 0;         // 油泵预热使能（前10秒）
bit motorEn = 0;           // 电机运行使能（后20秒）
bit cooling = 0;           // 冷却中
bit dirWait = 0;           // 方向等待中
bit currDir;               // 当前运行方向（生效方向）
bit targetDir;             // 目标方向（开关实时值）
bit dirChanged = 0;        // 方向变更标记（周期内是否切换）
bit needCool = 0;          // 冷却需求标记（第5次运行后为1）
bit timerEn = 0;           // 定时器总使能

// 函数声明
void Timer0Init();
void display();
void updateDisplay();
void pumpControl();
void motorControl(bit on);
void checkDirChange();
void delayMs(unsigned int ms);
void delayUs(unsigned int us);

// 主函数
void main() {
    // 初始化
    currDir = REVERSE_SW;
    targetDir = REVERSE_SW;
    LED_POWER = 0;  // 电源灯常亮
    LED_START = 1;
    LED_STOP = 1;
    LED_PUMP = 1;
    LED_MOTOR_F = 1;
    LED_MOTOR_R = 1;
    LED_COOL = 1;
    LED_DIR_WAIT = 1;
    IN1 = 0; IN2 = 0; IN3 = 0; IN4 = 0;

    Timer0Init();
    EA = 1;

    while(1) {
        checkDirChange();
        pumpControl();
        updateDisplay();
        display();

        // 停止逻辑：重置所有状态
        if(STOP == 0) {
            delayMs(10);
            if(STOP == 0) {
                LED_STOP = 0;
                pumpPreEn = 0;
                motorEn = 0;
                cooling = 0;
                dirWait = 0;
                dirChanged = 0;
                needCool = 0;  // 重置冷却需求
                timerEn = 0;
                workCount = 0;
                pumpPreTimer = 0;
                motorTimer = 0;
                coolTimer = 0;
                dirWaitTimer = 0;
                motorControl(0);
                // 关闭指示灯
                LED_DIR_WAIT = 1;
                LED_START = 1;
                LED_COOL = 1;
                LED_PUMP = 1;
                // 同步方向
                currDir = targetDir;
                while(STOP == 0);
                LED_STOP = 1;
            }
        }

        // 方向等待阶段：等待结束后处理冷却需求
        if(dirWait) {
            LED_DIR_WAIT = 0;
            if(dirWaitTimer >= 10000) {
                dirWait = 0;
                dirWaitTimer = 0;
                currDir = targetDir;    // 等待结束更新方向
                dirChanged = 0;
                LED_DIR_WAIT = 1;

                // 若有冷却需求，等待结束后自动进入冷却
                if(needCool) {
                    cooling = 1;
                    needCool = 0;       // 清除冷却需求
                    coolTimer = 0;
                    timerEn = 1;
                } else {
                    timerEn = 0;
                }
            }
            continue;
        }

        // 冷却阶段：正常执行
        if(cooling) {
            LED_COOL = 0;
            if(coolTimer >= 10000) {
                cooling = 0;
                coolTimer = 0;
                LED_COOL = 1;
                timerEn = 0;
            }
            continue;
        }

        // 启动逻辑：仅无运行/无冷却/无等待时可启动
        if(START == 0 && !pumpPreEn && !motorEn && !cooling && !dirWait) {
            delayMs(10);
            if(START == 0) {
                LED_START = 0;
                pumpPreEn = 1;
                timerEn = 1;
                pumpPreTimer = 0;
                while(START == 0);
            }
        }

        // 油泵预热阶段结束：处理方向变更
        if(pumpPreEn && pumpPreTimer >= 10000) {
            pumpPreEn = 0;
            // 有方向变更→触发等待，无变更→启动电机
            if(dirChanged) {
                dirWait = 1;
                dirWaitTimer = 0;
                timerEn = 1;
                dirChanged = 0;
            } else {
                motorEn = 1;
                motorTimer = 0;
                currDir = targetDir;
                motorControl(1);
            }
        }

        // 电机运行阶段结束：处理方向变更+冷却需求
        if(motorEn && motorTimer >= 20000) {
            motorEn = 0;
            motorControl(0);
            workCount++;

            // 标记冷却：第5次运行后为1
            needCool = (workCount >= 5) ? 1 : 0;
            if(needCool) workCount = 0;  // 重置工作次数

            // 有方向变更→触发等待，无变更→直接冷却
            if(dirChanged) {
                dirWait = 1;
                dirWaitTimer = 0;
                timerEn = 1;
                dirChanged = 0;
            } else if(needCool) {
                cooling = 1;
                needCool = 0;
                coolTimer = 0;
                timerEn = 1;
            } else {
                timerEn = 0;
            }
            LED_START = 1;
        }
    }
}

// 定时器0初始化（1ms中断）
void Timer0Init() {
    TMOD &= 0xF0;
    TMOD |= 0x01;
    TH0 = 0xFC;
    TL0 = 0x18;
    ET0 = 1;
    TR0 = 1;
}


void Timer0_ISR() interrupt 1 {
    TH0 = 0xFC;
    TL0 = 0x18;
    if(timerEn) {
        if(pumpPreEn) pumpPreTimer++;
        if(motorEn) motorTimer++;
        if(cooling) coolTimer++;
        if(dirWait) dirWaitTimer++;
    }
}

// 数码管扫描
void display() {
    static unsigned char pos = 0;
    DataPort = 0x00;
    DUAN = 1; DUAN = 0;
    DataPort = weiMa[pos];
    WEI = 1; WEI = 0;
    DataPort = duanMa[dispBuf[pos]];
    DUAN = 1; DUAN = 0;
    delayUs(200);
    pos = (pos + 1) % 4;
}

// 更新显示数据
void updateDisplay() {
    unsigned int remainTime = 0;
    if(pumpPreEn) remainTime = 10 - pumpPreTimer/1000;
    else if(motorEn) remainTime = 20 - motorTimer/1000;
    else if(cooling) remainTime = 10 - coolTimer/1000;
    else if(dirWait) remainTime = 10 - dirWaitTimer/1000;
    else remainTime = 0;

    dispBuf[0] = workCount / 10;
    dispBuf[1] = workCount % 10;
    dispBuf[2] = remainTime / 10;
    dispBuf[3] = remainTime % 10;
}

// 油泵控制
void pumpControl() {
    bit pumpRun = 0;
    if(pumpPreEn) pumpRun = 1;
    else if(motorEn && PUMP_MODE == 1) pumpRun = 1;
    IN4 = pumpRun;
    LED_PUMP = !pumpRun;
}

// 电机控制
void motorControl(bit on) {
    if(on) {
        if(currDir) { // 正转（REVERSE_SW=高）
            IN1 = 1; IN2 = 0;
            LED_MOTOR_F = 0; LED_MOTOR_R = 1;
        } else { // 反转（REVERSE_SW=低）
            IN1 = 0; IN2 = 1;
            LED_MOTOR_F = 1; LED_MOTOR_R = 0;
        }
    } else {
        IN1 = 0; IN2 = 0;
        LED_MOTOR_F = 1; LED_MOTOR_R = 1;
    }
}

// 检测方向切换
void checkDirChange() {
    targetDir = REVERSE_SW;

    // 冷却阶段：直接更新方向
    if(cooling && targetDir != currDir) {
        currDir = targetDir;
        return;
    }

    // 方向等待阶段：不处理
    if(dirWait) return;

    // 非运行且非冷却：立即触发等待
    if(!pumpPreEn && !motorEn && !cooling && targetDir != currDir) {
        dirWait = 1;
        dirWaitTimer = 0;
        timerEn = 1;
        return;
    }

    // 油泵/电机运行阶段：标记变更
    if((pumpPreEn || motorEn) && targetDir != currDir) {
        dirChanged = 1;
    }
}

// 延时函数
void delayMs(unsigned int ms) {
    while(ms--) delayUs(1000);
}
void delayUs(unsigned int us) {
    while(us--);
}
#include <reg52.h>

sbit START = P1^0;
sbit REVERSE_SW = P1^1;
sbit STOP = P1^2;
sbit PUMP_MODE = P1^3;

sbit LED_POWER = P3^0;
sbit LED_START = P3^1;
sbit LED_STOP = P3^2;
sbit LED_PUMP = P3^3;
sbit LED_MOTOR_F = P3^4;
sbit LED_MOTOR_R = P3^5;
sbit LED_COOL = P3^6;
sbit LED_DIR_WAIT = P3^7;

sbit IN1 = P2^2;
sbit IN2 = P2^3;
sbit IN3 = P2^4;
sbit IN4 = P2^5;

sbit DUAN = P2^6;
sbit WEI = P2^7;
#define DataPort P0
unsigned char code duanMa[] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};
unsigned char code weiMa[] = {0xFE,0xFD,0xFB,0xF7};

unsigned char dispBuf[4] = {0,0,0,0};
unsigned int workCount = 0;
unsigned int pumpPreTimer = 0;
unsigned int motorTimer = 0;
unsigned int coolTimer = 0;
unsigned int dirWaitTimer = 0;

bit pumpPreEn = 0;
bit motorEn = 0;
bit cooling = 0;
bit dirWait = 0;
bit currDir;
bit targetDir;
bit dirChanged = 0;
bit needCool = 0;
bit timerEn = 0;

void Timer0Init();
void display();
void updateDisplay();
void pumpControl();
void motorControl(bit on);
void checkDirChange();
void delayMs(unsigned int ms);
void delayUs(unsigned int us);

void main() {
    currDir = REVERSE_SW;
    targetDir = REVERSE_SW;
    LED_POWER = 0;
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

        if(STOP == 0) {
            delayMs(10);
            if(STOP == 0) {
                LED_STOP = 0;
                pumpPreEn = 0;
                motorEn = 0;
                cooling = 0;
                dirWait = 0;
                dirChanged = 0;
                needCool = 0;
                timerEn = 0;
                workCount = 0;
                pumpPreTimer = 0;
                motorTimer = 0;
                coolTimer = 0;
                dirWaitTimer = 0;
                motorControl(0);
                LED_DIR_WAIT = 1;
                LED_START = 1;
                LED_COOL = 1;
                LED_PUMP = 1;
                currDir = targetDir;
                while(STOP == 0);
                LED_STOP = 1;
            }
        }

        if(dirWait) {
            LED_DIR_WAIT = 0;
            if(dirWaitTimer >= 10000) {
                dirWait = 0;
                dirWaitTimer = 0;
                currDir = targetDir;
                dirChanged = 0;
                LED_DIR_WAIT = 1;

                if(needCool) {
                    cooling = 1;
                    needCool = 0;
                    coolTimer = 0;
                    timerEn = 1;
                } else {
                    timerEn = 0;
                }
            }
            continue;
        }

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

        if(pumpPreEn && pumpPreTimer >= 10000) {
            pumpPreEn = 0;
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

        if(motorEn && motorTimer >= 20000) {
            motorEn = 0;
            motorControl(0);
            workCount++;

            needCool = (workCount >= 5) ? 1 : 0;
            if(needCool) workCount = 0;

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

void updateDisplay() {
    unsigned int remainTime = 0;
+    if(pumpPreEn) remainTime = 10 - pumpPreTimer/1000;
    else if(motorEn) remainTime = 20 - motorTimer/1000;
    else if(cooling) remainTime = 10 - coolTimer/1000;
    else if(dirWait) remainTime = 10 - dirWaitTimer/1000;
    else remainTime = 0;

    dispBuf[0] = workCount / 10;
    dispBuf[1] = workCount % 10;
    dispBuf[2] = remainTime / 10;
    dispBuf[3] = remainTime % 10;
}

void pumpControl() {
    bit pumpRun = 0;
+    if(pumpPreEn) pumpRun = 1;
+    else if(motorEn && PUMP_MODE == 1) pumpRun = 1;
+    IN4 = pumpRun;
+    LED_PUMP = !pumpRun;
}

void motorControl(bit on) {
    if(on) {
        if(currDir) {
            IN1 = 1; IN2 = 0;
            LED_MOTOR_F = 0; LED_MOTOR_R = 1;
        } else {
            IN1 = 0; IN2 = 1;
            LED_MOTOR_F = 1; LED_MOTOR_R = 0;
        }
    } else {
        IN1 = 0; IN2 = 0;
        LED_MOTOR_F = 1; LED_MOTOR_R = 1;
    }
}

void checkDirChange() {
    targetDir = REVERSE_SW;

    if(cooling && targetDir != currDir) {
        currDir = targetDir;
        return;
    }

    if(dirWait) return;

    if(!pumpPreEn && !motorEn && !cooling && targetDir != currDir) {
        dirWait = 1;
        dirWaitTimer = 0;
        timerEn = 1;
        return;
    }

    if((pumpPreEn || motorEn) && targetDir != currDir) {
        dirChanged = 1;
    }
}

void delayMs(unsigned int ms) {
    while(ms--) delayUs(1000);
}
void delayUs(unsigned int us) {
    while(us--);
}
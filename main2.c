#include <reg52.h>

sbit START = P1^0;        
sbit REVERSE_SW = P1^1;   
sbit STOP = P1^2;         
sbit PUMP_MODE = P1^3;    

// ָʾ�ƣ���������������
sbit LED_POWER = P3^0;    // ��Դ�ƣ�������
sbit LED_START = P3^1;    // ������
sbit LED_STOP = P3^2;     // ֹͣ��
sbit LED_PUMP = P3^3;     // �ͱõ�
sbit LED_MOTOR_F = P3^4;  // ��ת��
sbit LED_MOTOR_R = P3^5;  // ��ת��
sbit LED_COOL = P3^6;     // ��ȴ��
sbit LED_DIR_WAIT = P3^7; // ����ȴ���

// L298����
sbit IN1 = P2^2;          // �����ת����
sbit IN2 = P2^3;          // �����ת����
sbit IN3 = P2^4;          // �ͱ÷��򣨹̶��ͣ�
sbit IN4 = P2^5;          // �ͱ�ʹ�ܣ���=������

// ��������
sbit DUAN = P2^6;         // ������
sbit WEI = P2^7;          // λ����
#define DataPort P0       // �������ݿ�
unsigned char code duanMa[] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F}; // ����������
unsigned char code weiMa[] = {0xFE,0xFD,0xFB,0xF7};                               // λ�루����Ч��

// ȫ�ֱ���
unsigned char dispBuf[4] = {0,0,0,0};
unsigned int workCount = 0;           // ����������0-4��
unsigned int pumpPreTimer = 0;        // �ͱ�Ԥ�ȼ�ʱ��10�룩
unsigned int motorTimer = 0;          // ������м�ʱ��20�룩
unsigned int coolTimer = 0;           // ��ȴ��ʱ��10�룩
unsigned int dirWaitTimer = 0;        // ����ȴ���ʱ��10�룩

// ״̬��־
bit pumpPreEn = 0;         // �ͱ�Ԥ��ʹ�ܣ�ǰ10�룩
bit motorEn = 0;           // �������ʹ�ܣ���20�룩
bit cooling = 0;           // ��ȴ��
bit dirWait = 0;           // ����ȴ���
bit currDir;               // ��ǰ���з�����Ч����
bit targetDir;             // Ŀ�귽�򣨿���ʵʱֵ��
bit dirChanged = 0;        // ��������ǣ��������Ƿ��л���
bit needCool = 0;          // ��ȴ�����ǣ���5�����к�Ϊ1��
bit timerEn = 0;           // ��ʱ����ʹ��

// ��������
void Timer0Init();
void display();
void updateDisplay();
void pumpControl();
void motorControl(bit on);
void checkDirChange();
void delayMs(unsigned int ms);
void delayUs(unsigned int us);

// ������
void main() {
    // ��ʼ��
    currDir = REVERSE_SW;
    targetDir = REVERSE_SW;
    LED_POWER = 0;  // ��Դ�Ƴ���
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

        // ֹͣ�߼�����������״̬
        if(STOP == 0) {
            delayMs(10);
            if(STOP == 0) {
                LED_STOP = 0;
                pumpPreEn = 0;
                motorEn = 0;
                cooling = 0;
                dirWait = 0;
                dirChanged = 0;
                needCool = 0;  // ������ȴ����
                timerEn = 0;
                workCount = 0;
                pumpPreTimer = 0;
                motorTimer = 0;
                coolTimer = 0;
                dirWaitTimer = 0;
                motorControl(0);
                // �ر�ָʾ��
                LED_DIR_WAIT = 1;
                LED_START = 1;
                LED_COOL = 1;
                LED_PUMP = 1;
                // ͬ������
                currDir = targetDir;
                while(STOP == 0);
                LED_STOP = 1;
            }
        }

        // ����ȴ��׶Σ��ȴ�����������ȴ����
        if(dirWait) {
            LED_DIR_WAIT = 0;
            if(dirWaitTimer >= 10000) {
                dirWait = 0;
                dirWaitTimer = 0;
                currDir = targetDir;    // �ȴ��������·���
                dirChanged = 0;
                LED_DIR_WAIT = 1;

                // ������ȴ���󣬵ȴ��������Զ�������ȴ
                if(needCool) {
                    cooling = 1;
                    needCool = 0;       // �����ȴ����
                    coolTimer = 0;
                    timerEn = 1;
                } else {
                    timerEn = 0;
                }
            }
            continue;
        }

        // ��ȴ�׶Σ�����ִ��
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

        // �����߼�����������/����ȴ/�޵ȴ�ʱ������
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

        // �ͱ�Ԥ�Ƚ׶ν���������������
        if(pumpPreEn && pumpPreTimer >= 10000) {
            pumpPreEn = 0;
            // �з������������ȴ����ޱ�����������
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

        // ������н׶ν���������������+��ȴ����
        if(motorEn && motorTimer >= 20000) {
            motorEn = 0;
            motorControl(0);
            workCount++;

            // �����ȴ����5�����к�Ϊ1
            needCool = (workCount >= 5) ? 1 : 0;
            if(needCool) workCount = 0;  // ���ù�������

            // �з������������ȴ����ޱ����ֱ����ȴ
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

// ��ʱ��0��ʼ����1ms�жϣ�
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

// �����ɨ��
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

// ������ʾ����
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

// �ͱÿ���
void pumpControl() {
    bit pumpRun = 0;
    if(pumpPreEn) pumpRun = 1;
    else if(motorEn && PUMP_MODE == 1) pumpRun = 1;
    IN4 = pumpRun;
    LED_PUMP = !pumpRun;
}

// �������
void motorControl(bit on) {
    if(on) {
        if(currDir) { // ��ת��REVERSE_SW=�ߣ�
            IN1 = 1; IN2 = 0;
            LED_MOTOR_F = 0; LED_MOTOR_R = 1;
        } else { // ��ת��REVERSE_SW=�ͣ�
            IN1 = 0; IN2 = 1;
            LED_MOTOR_F = 1; LED_MOTOR_R = 0;
        }
    } else {
        IN1 = 0; IN2 = 0;
        LED_MOTOR_F = 1; LED_MOTOR_R = 1;
    }
}

// ��ⷽ���л�
void checkDirChange() {
    targetDir = REVERSE_SW;

    // ��ȴ�׶Σ�ֱ�Ӹ��·���
    if(cooling && targetDir != currDir) {
        currDir = targetDir;
        return;
    }

    // ����ȴ��׶Σ�������
    if(dirWait) return;

    // �������ҷ���ȴ�����������ȴ�
    if(!pumpPreEn && !motorEn && !cooling && targetDir != currDir) {
        dirWait = 1;
        dirWaitTimer = 0;
        timerEn = 1;
        return;
    }

    // �ͱ�/������н׶Σ���Ǳ��
    if((pumpPreEn || motorEn) && targetDir != currDir) {
        dirChanged = 1;
    }
}

// ��ʱ����
void delayMs(unsigned int ms) {
    while(ms--) delayUs(1000);
}
void delayUs(unsigned int us) {
    while(us--);
}
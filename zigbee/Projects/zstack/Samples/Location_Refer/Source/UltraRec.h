
/* ����ͷ�ļ� */
/********************************************************************/
#include "hal_lcd.h"
#include "hal_board_cfg.h"
#include "OSAL_Timers.h"
/********************************************************************/

/* ���ر��� */
/********************************************************************/
static bool YelLed_State;      //�Ƶ�״̬����ʾ���յ��˳�����
static bool RedLed_State = 0;      //���״̬��������

static bool bFirstRcv = 1;            //�ж��Ƿ��һ�ν���

//static bool bRFFirstRcv = TRUE;

//static bool bRcvFinish = 0;          //�ж��Ƿ�������

//static uint32 ui_T4Overflow = 0;      //��ʱ��4���������
//static unsigned int ui_T4Count;             //��ʱ��4�ļ���ֵ



typedef struct {
    uint16         timerCapture;
    uint32         overflowCapture;
}MAC_T2_TIME;


/********************************************************************/

//���غ���
/********************************************************************/
//void halSetTimer1Period(uint16 period);
void setGain(unsigned char ucGainNo);
void initP0();
/********************************************************************/


/* �궨�� */
/********************************************************************/
#define Signal P0_0 
#define GCA    P0_2 
#define GCB    P0_3 
#define GCC    P0_4 
#define GCD    P0_5 
#define Inhi   P0_7

//#define TIME_OUT 15     //�ʱ�䣬��15����δ���յ����㳬ʱ

//#define INHIBIT_TIME 880  //����������30cm

/*��ʱ��1��ʼ��*/ 
//32��Ƶ moduloģʽ һ��32M/32=1M һ�μ�������1us
//#define TIMER1_INIT() do{ T1CTL = 0x0A; TIMIF = ~0x40;} while (0)

/*��ʱ��1��������ж�*/
//#define TIMER1_ENABLE_OVERFLOW_INT(val)  (TIMIF = (val)?(TIMIF|0x40):(TIMIF&~0x40))

/*��ʱ��1������ر�*/  //��Ҫ������ʱ��1�е�ͨ��0
//#define TIMER1_RUN(value) (T1STAT = (value)?T1STAT|0x21:T1STAT&~0x21)

/* ���TIMER3��4�жϱ�־λ */
/********************************************************************/
//#define CLR_TIMER34_IF( bitMask ) TIMIF=(TIMIF&0x40)|(0x3F&(~bitMask))


//��ʱ��4 ����Ϊ  1��Ƶ �������� ��0-255
//#define TIMER34_INIT(timer) do{ T##timer##CTL = 0x08; TIMIF = 0x00;} while (0)  

// ��ʱ��4ʹ��
//#define TIMER4_RUN(value)      (T4CTL = (value) ? T4CTL | 0x10 : T4CTL & ~0x10)
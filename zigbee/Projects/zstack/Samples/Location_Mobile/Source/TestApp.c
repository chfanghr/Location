/*********************************************************************
  Filename:       Mobile.c
  Revised:        $Date: 2007-05-31 13:07:52 -0700 (Thu, 31 May 2007) $
  Revision:       $Revision: 14480 $

  Description: Blind Node for the Z-Stack Location Profile.
				
  Copyright (c) 2006 by Texas Instruments, Inc.
  All Rights Reserved.  Permission to use, reproduce, copy, prepare
  derivative works, modify, distribute, perform, display or sell this
  software and/or its documentation for any purpose is prohibited
  without the express written consent of Texas Instruments, Inc.
  �޸ĳ�sink�ڵ㷢�͹㲥��Ϣ���ƶ��ڵ���յ��㲥��Ϣ��Ϳ�ʼ���ͳ������źţ��ο��ڵ���ճ������ź�
  Ҫע��û��DS18B20���ƶ��ڵ�Ҫ�رն�ʱ�����¶ȵĹ��ܣ�����Ῠס��
*********************************************************************/

/*********************************************************************
 * INCLUDES
 */

#include "OSAL.h"
#include "OSAL_NV.h"
#include "MT.h"
#include "AF.h"
#include "ZDApp.h"

#include "OnBoard.h"
#include "hal_key.h"
#if defined ( LCD_SUPPORTED )
#include "hal_lcd.h"
#include "hal_led.h"
#endif

#include "LocationProfile.h"
#include "TestApp.h"
#include "UltraSend.h"
#include "DS18B20.h"
#include "math.h"         //pel+ pow(,)

#include "mac_mcu.h"      //���Բ��Է��䳬�����͵�Ų���ʱ���

/*********************************************************************
 * CONSTANTS
 */

#define MOBILE_TEMP_PERIOD    10000   //10sһ�η����¶���Ϣ
#define MOBILE_ID             1       //�ƶ��ڵ��ID�ţ�ÿ���ƶ��ڵ��ID��������ͬ
#define DELAY_TIME            18      //�ӳ�ʱ�䣬ʵ���ӳ���2*DELAY_TIME ms���˴��ӳ�����ο��ڵ��ӳ���ͬ��ʱ��

/* ���TIMER3��4�жϱ�־λ */
/********************************************************************/
#define CLR_TIMER34_IF( bitMask ) TIMIF=(TIMIF&0x40)|(0x3F&(~bitMask))

//��ʱ��3 ����Ϊ  128��Ƶ up-downģʽ
#define TIMER34_INIT(timer) do{ T##timer##CTL = 0xEB; TIMIF = 0x00;} while (0)  

// ��ʱ��3ʹ��
#define TIMER3_RUN(value)      (T3CTL = (value) ? T3CTL | 0x10 : T3CTL & ~0x10)


/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

uint8 Mobile_TaskID;
static uint8 transId;
static unsigned char t3count = 0;

/*********************************************************************
 * LOCAL VARIABLES
 */

static const cId_t Mobile_InputClusterList[] =
{
  LOCATION_ULTRA_BLORDCAST,       //����sink�ڵ�Ĺ㲥�ź�
};


static const cId_t Mobile_OutputClusterList[] =
{
  LOCATION_COOR_TEMPURATE,        //�����¶ȸ�Э����
};

static const SimpleDescriptionFormat_t Mobile_SimpleDesc =
{
  LOCATION_MOBILE_ENDPOINT,
  LOCATION_PROFID,
  LOCATION_MOBILE_DEVICE_ID,
  LOCATION_DEVICE_VERSION,
  LOCATION_FLAGS,
  
  sizeof(Mobile_InputClusterList),
  (cId_t *)Mobile_InputClusterList,
  //0,
  //(cId_t *) NULL
  sizeof(Mobile_OutputClusterList),
  (cId_t *)Mobile_OutputClusterList
};

static const endPointDesc_t epDesc =
{
  LOCATION_MOBILE_ENDPOINT,
  &Mobile_TaskID,
  (SimpleDescriptionFormat_t *)&Mobile_SimpleDesc,
  noLatencyReqs
};

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void processMSGCmd( afIncomingMSGPacket_t *pckt );
void startBroadcast( void );
void halSetTimer3Period(uint8 period);


/*********************************************************************
 * @fn      Mobile_Init
 *
 * @brief   Initialization function for the Generic App Task.
 *
 * @param   task_id - the ID assigned by OSAL.
 *
 * @return  none
 */
void Mobile_Init( uint8 task_id )
{
  Mobile_TaskID = task_id;

  //state = eBnIdle;

  afRegister( (endPointDesc_t *)&epDesc );
  RegisterForKeys( Mobile_TaskID );

#if defined ( LCD_SUPPORTED )
  HalLcdWriteString( "Location-Blind", HAL_LCD_LINE_1 );
#endif

  //�������豸��ʼ��
  initP1();
  
  TIMER34_INIT(3);
  halSetTimer3Period(250); //��ʾ�ӳ�2ms ע�⣺ʵ��д��T1CC0�Ĵ�����ֵӦС��255
  IEN1 |= (0x01 << 3);             // ʹ��Timer3���ж�
  EA = 1;                      //����ʹ��
  
  HAL_TURN_OFF_LED1();         // Ϩ��LED_G��LED_R��LED_Y ��ʹ���̵ƣ���Ϊ����ʹ����P1_0��
  HAL_TURN_OFF_LED2();       
  HAL_TURN_OFF_LED3();
  
}

/*********************************************************************
 * @fn      Mobile_ProcessEvent
 *
 * @brief   Generic Application Task event processor.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - events to process.
 *
 * @return  none
 */
uint16 Mobile_ProcessEvent( uint8 task_id, uint16 events )
{
   afIncomingMSGPacket_t *MSGpkt;
  if ( events & SYS_EVENT_MSG )
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( Mobile_TaskID );

    while( MSGpkt )
    {
      switch ( MSGpkt->hdr.event )
      {
      case AF_INCOMING_MSG_CMD:
        processMSGCmd( MSGpkt );
        break;

      case ZDO_STATE_CHANGE:
        {
          #if defined( RTR_NWK )
                  //NLME_PermitJoiningRequest( 0 );
          #endif
        /* Broadcast the X,Y location for any passive listeners in order to
         * register this node.
         */
        
        //���������ȡ�¶ȣ������䴫��Э����
        //sendTemperature();
        //osal_start_timerEx( Mobile_TaskID, MOBILE_TEMP_EVT, MOBILE_TEMP_PERIOD );  //�������ڷ����¶���Ϣ��ʱ
          
                      
          break;
        }

      default:
        break;
      }
      // Release the memory
      osal_msg_deallocate( (uint8 *)MSGpkt );
      
      // Next
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( Mobile_TaskID );
      
    }

    return ( events ^ SYS_EVENT_MSG );  // Return unprocessed events.
  }

  if ( events & MOBILE_TEMP_EVT )    
  {
    sendTemperature();             //���÷����¶�
    osal_start_timerEx( Mobile_TaskID, MOBILE_TEMP_EVT, MOBILE_TEMP_PERIOD );
    
    return ( events ^ MOBILE_TEMP_EVT );
  }
  
  return 0;  // Discard unknown events.
}

/*********************************************************************
 * @fn      processMSGCmd
 *
 * @brief   Data message processor callback.
 *
 * @param   none
 *
 * @return  none
 */
static void processMSGCmd( afIncomingMSGPacket_t *pkt )
{
  switch ( pkt->clusterId )
  {
  case LOCATION_ULTRA_BLORDCAST:
    {
      //1���ƶ��ڵ�ֱ�ӷ��䳬����������Ҫ�ȴ�
      if(MOBILE_ID == 1)
      {
        startBroadcast();
      }
      //�����ڵ㿪����ʱ�����ȴ�(n-1)*24ms��ʱ�䣬�ٷ��䳬����
      else
      {
        TIMER3_RUN(TRUE);
      }
    }
    break;

  default:
    break;
  }
}

/*********************************************************************
 * @fn      startBlast
 *
 * @brief   Start a sequence of blasts and calculate position.
 *
 * @param   none
 *
 * @return  none
 */
void startBroadcast( void )
{
 
  //�ȷ��䳬�����������Ų�������ʾ������������֮����2ms�����˵�Ų�
  if (RedLedState == 0)
  {
    HAL_TURN_ON_LED2();     //�ı�һ��LED_Y��״̬����ʾ���ڷ��䳬����
    RedLedState = 1;
  }
  else 
  {
    HAL_TURN_OFF_LED2();
    RedLedState = 0;
  }
  
  EA = 0;                                       //�ر��жϣ������ж������չ�
  SendUltra(SquareWaveTimes);                   //���䳬����
  EA = 1;                                        //���ж�  
      
  //P1_1 = 1;  
}

/*********************************************************************
 * �������ƣ�halSetTimer3Period
 * ��    �ܣ����ö�ʱ��3��ʱ����
 * ��ڲ�����period   ��ʱ����       
 * ���ڲ�������
 * �� �� ֵ����
 ********************************************************************/
void halSetTimer3Period(uint8 period)
{
  /* ��T3CC0д�����ռ���ֵperiod */
  T3CC0 = period & 0xFF;             // ��period��ֵд��T3CC0
}

/*********************************************************************
 * �������ƣ�T3_IRQ
 * ��    �ܣ���ʱ��1�жϷ����� ע����ں���������ж���ں�����ͬ ʵ��Ч�������룬����ȥ
 * ��ڲ�������
 * ���ڲ�������
 * �� �� ֵ����
 ********************************************************************/

//_PRAGMA(vector=T1_VECTOR) __near_func __interrupt void halTimer1Isr(void);  
HAL_ISR_FUNCTION( halTimer3Isr, T3_VECTOR)
{
  IEN1 &= ~0x03;    //�رն�ʱ��3�ж�ʹ��
  if(TIMIF & 0x01) //ȷ���Ƿ������ʱ�ж�
  {
    t3count++;
    if(t3count == DELAY_TIME*(MOBILE_ID-1))     //2*12=24ms
    {
      t3count = 0;
      startBroadcast();             //���÷��͹㲥�¼�
      
      //����ʱ��T1��0��Ϊ�´��ж���׼��
      TIMER3_RUN(FALSE);
      T3CTL |= 0x04;       //������ֵ��0
    }    
  }
  
  TIMIF &= ~0x01;  
  //T1STAT &= ~0x21;  //�����ʱ��3�ı�־λ
  //IRCON &= ~0x02;   //ͬ��
  IEN1 |= 0x03;    //�򿪶�ʱ��3�ж�ʹ��
}


































/*************************************************************************************************************
DS18B20�ĳ���
*******************************************************************/
/*********************************************************************
 * �������ƣ�initDS18B20
 * ��    �ܣ���ʼ��P1_0��P1_1�˿�Ϊ���GPIO,��Ϊ�鿴���ã�ֻ�и������˿���������ɴ�20mA
 * ��ڲ�������
 * ���ڲ�������
 * �� �� ֵ����
 ********************************************************************/
void init_1820()
{
  SET_DQ_OUT;   //��DQ����Ϊ���
  DQ = 0;       //��DQ��Ϊ�͵�ƽ����480u
  halMcuWaitUs(550);
  DQ = 1;       //�ø�
  
  SET_DQ_IN;    //��DQ����Ϊ����
  
  halMcuWaitUs(40); //�ͷ����ߺ�ȴ�15-60us
  while(DQ)     //�ȴ��ظ�,��ȡDQ��ֵ��Ϊ1
  {

  }  
  
  halMcuWaitUs(200);
  SET_DQ_OUT;
  DQ = 1;       //�ص���ʼDQ=1��
  
  halMcuWaitUs(1); //�ӳ�1us
   
}


/*********************************************************************
 * �������ƣ�sendTemperature
 * ��    �ܣ���ȡ�¶ȣ���������ת�� 
 * ��ڲ�������
 * ���ڲ������¶�ֵ
 * �� �� ֵ����
 ********************************************************************/
void sendTemperature(void)
{
  static uint16 sensor_temperature_value;
  sensor_temperature_value = read_data(); // ��ȡ�¶�ֵ
  
  float temperature=0.0;
  int n;
  uint16 roll = 0x0800;
  for(n=7;n>=-4;n--)
  {
    if(sensor_temperature_value&roll)
    {
      temperature += pow(2,n);
    }
     roll = roll>>1;
  }
  
                
  uint16 tempData;
  tempData = (int)(temperature*100);
  unsigned char moblieNodeTemperature[2]; 
  if (temperature >0 && temperature < 40)  //��������ݲ����з���
  {
    moblieNodeTemperature[0] = ((uint8 *)&tempData)[1];      //�¶ȸ�λ 
    moblieNodeTemperature[1] = ((uint8 *)&tempData)[0];      //�¶ȵ�λ
    //moblieNodeTemperature[0] = ((uint8 *)&sensor_temperature_value)[1];      //�¶ȸ�λ 
    //moblieNodeTemperature[1] = ((uint8 *)&sensor_temperature_value)[0];      //�¶ȵ�λ
   }
  
  //��������ݽ��������Ϊ0xFFFF
  else
  {
    moblieNodeTemperature[0] = 0xFF;      //�¶ȸ�λ 
    moblieNodeTemperature[1] = 0xFF;      //�¶ȵ�λ
  }
  
  afAddrType_t Coord_DstAddr;
  Coord_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
  Coord_DstAddr.endPoint = LOCATION_DONGLE_ENDPOINT;
  Coord_DstAddr.addr.shortAddr = 0x0000;  //��ʾ���ݰ����������е����нڵ�㲥������0xFFFC��·�������ղ�������֪�ν� 
  AF_DataRequest( &Coord_DstAddr, (endPointDesc_t *)&epDesc,
                 LOCATION_COOR_TEMPURATE, 2,
                 moblieNodeTemperature, &transId,
                 AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ); 
  
  /*
  //������¶����ݷ��ʹ�����
  else
  {
    unsigned char errorReportData[2];
    errorReportData[0] = ERROR_TEMP_DATA;
    errorReportData[1] = MOBILE_ID;
    afAddrType_t Coord_DstAddr;
    Coord_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
    Coord_DstAddr.endPoint = LOCATION_DONGLE_ENDPOINT;
    Coord_DstAddr.addr.shortAddr = 0x0000;  //��ʾ���ݰ����������е����нڵ�㲥������0xFFFC��·�������ղ�������֪�ν� 
    AF_DataRequest( &Coord_DstAddr, (endPointDesc_t *)&epDesc,
                   LOCATION_COOR_ERROR, 2,
                   errorReportData, &transId,
                   AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ); 
  }
  */
}
          
/*********************************************************************
 * �������ƣ�read_data
 * ��    �ܣ���ȡ�¶ȣ���������ת�� 
 * ��ڲ�������
 * ���ڲ�������
 * �� �� ֵ����
 ********************************************************************/
uint16 read_data(void)
{
  uint8 temh,teml;
  uint16 temp;
  
  init_1820();          //��λ18b20
  write_1820(0xcc);     // ����ת������ ����ROM
  write_1820(0x44);     //����ת��
  halMcuWaitUs(500);  //I/O �߾ͱ������ٱ��� 500ms �ߵ�ƽ
  
  init_1820();
  write_1820(0xcc);
  write_1820(0xbe);     //��ȡ�ݴ����� CRC �ֽ�
  
  teml=read_1820(); //������
  temh=read_1820();
  
  //sensor_temperature_value[0]=teml;
  //sensor_temperature_value[1]=temh;
  
  temp= temh;//�������ֽ����ϵ�һ��unsigned int��
  temp<<=8;
  temp |= teml;
  
  return temp;
}

/*********************************************************************
 * �������ƣ�read_1820
 * ��    �ܣ���ȡ�¶ȣ���������ת�� 
 * ��ڲ�������
 * ���ڲ������¶�ֵ
 * �� �� ֵ����
 ********************************************************************/
uint8 read_1820(void)
{
  uint8 temp,k,n;
  temp=0;
  for(n=0;n<8;n++)
  {   SET_DQ_OUT;
      DQ = 1;
      halMcuWaitUs(1);
      DQ = 0;
      halMcuWaitUs(1); //��ʱ1us
  
      DQ = 1;            
      SET_DQ_IN;
      halMcuWaitUs(6);//������ʱ6us
  
      k = DQ;         //������,�ӵ�λ��ʼ
      if(k)
      temp|=(1<<n);
      else
      temp&=~(1<<n);
      //Delay_nus(60); //60~120us
      halMcuWaitUs(50);
      SET_DQ_OUT;
  
  }
  return (temp);
}

/*********************************************************************
 * �������ƣ�write_1820
 * ��    �ܣ���ȡ�¶ȣ���������ת�� 
 * ��ڲ�������
 * ���ڲ������¶�ֵ
 * �� �� ֵ����
 ********************************************************************/
void write_1820(uint8 x)
{
  uint8 m;
  SET_DQ_OUT;            
  for(m=0;m<8;m++)
   {  
     DQ = 1;
     halMcuWaitUs(1);
     DQ = 0;          //����
     if(x&(1<<m))    //д���ݣ��ӵ�λ��ʼ
      DQ = 1;         //Ҫ��15us�����ݷ���д�����ݣ�0��1���͵�������
     else
      DQ = 0;
     halMcuWaitUs(40);//����15��60us

     DQ = 1;
     halMcuWaitUs(3);//��ʱ3us,����дʱ���������Ҫ1us�Ļָ���
   }
  DQ = 1;
}



/*********************************************************************
 * �������ƣ� halMcuWaitUs
 * ��    �ܣ�æ�ȴ����ܡ��ȴ�ָ����΢��������ͬ��ָ�������ʱ��������
 *           ����ͬ��һ�����ڵĳ���ʱ��ȡ����MCLK��(TI�е�lightSwitch�еĺ���)
 * ��ڲ�����usec  ��ʱʱ������λWie΢��
 * ���ڲ�������
 * �� �� ֵ����
 * ע    �⣺�˹��ܸ߶�������MCU�ܹ��ͱ�������
 ********************************************************************/
#pragma optimize = none
void halMcuWaitUs(unsigned int usec)
{
  usec >>= 1;
  while(usec--)
  {
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
    asm("NOP");
  }
}
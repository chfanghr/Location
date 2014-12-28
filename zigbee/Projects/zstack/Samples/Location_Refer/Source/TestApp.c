/*********************************************************************
  Filename:       RefNode.c
  Revised:        $Date: 2007-02-15 15:11:28 -0800 (Thu, 15 Feb 2007) $
  Revision:       $Revision: 13481 $

  Description: Reference Node for the Z-Stack Location Profile.
				
  Copyright (c) 2006 by Texas Instruments, Inc.
  All Rights Reserved.  Permission to use, reproduce, copy, prepare
  derivative works, modify, distribute, perform, display or sell this
  software and/or its documentation for any purpose is prohibited
  without the express written consent of Texas Instruments, Inc.
*********************************************************************/

/*********************************************************************
 * INCLUDES
 */

#include "OSAL.h"
#include "OSAL_Nv.h"
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
#include "OSAL_Clock.h"
//#include "hal_timer.h"
#include "mac_mcu.h"

#include "UltraRec.h"
/*********************************************************************
 * CONSTANTS
 */
#define LOCATION_REFER_ID     3 //�ǲο��ڵ�Ľڵ�ţ����벻ͬ�ڵ��ʱ����Ҫ����
#define RECV_TIMES 4            //�˴�Ҫ��sink�ڵ�Ĺ㲥������ͬ  ֮��д��LocationProfile.h�У�����һ��������
//#define TIMEDIFF_MSG_LENGTH   5  //TIMEDIFF_MSG_LENGTH = ��SeqNo RefNo��2bytes + Data��3bytes�� ;    ʱ���ĳ��� ��Ϊ����ֻ��Ϊ����
//#define RECV_TIMEOUT          400 //��һ�ν��յ��㲥�źź�ʼ��ʱ��ʱ���Ӧ�ô��ڷ������-1(3)*���100�˴�ȡֵ400ms
//#define RECV_DELAY_PERIOD     25  //�ο��ڵ��ӳٽ��յ�ʱ�䣬25ms���ӳٽ���3�Σ��˴�Ҫ���ƶ��ڵ�һ��
#define REFER_RECV_DELAY_TIMES      4 //һ�ι㲥����4�γ������źţ������ӳٽ���

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

MAC_T2_TIME RfArrivalTime;
MAC_T2_TIME UltraArrivalTime;
byte Refer_TaskID;
static byte transId;
static unsigned int ui_sequence;  //��ȡ���к�
static unsigned int ui_mobileID;  //��ȡ�ƶ��ڵ����
unsigned char recTimes = 0;       //���յĴ��������ڼ�����յĴ���
int32 i_timeDifference[RECV_TIMES];  //����ʱ����Ticks���� *31.25*10**(-7)= ʱ���
//int32 i_timeDifference;             //ֻ�㲥һ�γ���������ֻ����һ�ε�ֵ
//int32 i_timeDiff[RECV_TIMES];     //����ʱ�������
unsigned char timeDiffMSG[LOCATION_REFER_POSITION_RSP_LENGTH]; 
//unsigned char errorData[LOCATION_REFER_ERROR_POSITION_RSP_LENGTH];
//bool b_ErrorData = 0;             //�жϽ��յ���������ȷ�Ļ��Ǵ����

unsigned char recvDelayTimes = 2;      //��ʼ������Ϊ�Ѿ������������ˣ������ӳٽ��յ�

static unsigned char t3count = 0;
/*********************************************************************
 * LOCAL VARIABLES
 */

static const cId_t Refer_InputClusterList[] =
{
  LOCATION_ULTRA_BLORDCAST
};

static const cId_t Refer_OutputClusterList[] =
{
  LOCATION_REFER_DISTANCE_RSP
};

static const SimpleDescriptionFormat_t Refer_SimpleDesc =
{
  LOCATION_REFER_ENDPOINT,
  LOCATION_PROFID,
  LOCATION_REFER_DEVICE_ID,
  LOCATION_DEVICE_VERSION,
  LOCATION_FLAGS,

  sizeof(Refer_InputClusterList),
  (cId_t*)Refer_InputClusterList,

  sizeof(Refer_OutputClusterList),
  (cId_t*)Refer_OutputClusterList
};

static const endPointDesc_t epDesc =
{
  LOCATION_REFER_ENDPOINT,
  &Refer_TaskID,
  (SimpleDescriptionFormat_t *)&Refer_SimpleDesc,
  noLatencyReqs
};





/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void processMSGCmd( afIncomingMSGPacket_t *pkt );
static void checkTime();
static MAC_T2_TIME getTime();
//static void startRecWork(void);
static void halProcessKeyInterrupt (void);
static void sendData(void);
void halSetTimer3Period(uint8 period);

/*********************************************************************
 * @fn      Refer_Init
 *
 * @brief   Initialization function for this OSAL task.
 *
 * @param   task_id - the ID assigned by OSAL.
 *
 * @return  none
 */
void Refer_Init( byte task_id )
{
  Refer_TaskID = task_id;

  // Register the endpoint/interface description with the AF.
  afRegister( (endPointDesc_t *)&epDesc );

  // Register for all key events - This app will handle all key events.
  RegisterForKeys( Refer_TaskID );

#if defined ( LCD_SUPPORTED )
  HalLcdWriteString( "Location-RefNode", HAL_LCD_LINE_2 );
#endif
  
  
  
}

/*********************************************************************
 * @fn      RefNode_ProcessEvent
 *
 * @brief   Generic Application Task event processor.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - Bit map of events to process.
 *
 * @return  none
 */
uint16 Refer_ProcessEvent( byte task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;
  if ( events & SYS_EVENT_MSG )
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive(Refer_TaskID);
    while (MSGpkt)
    {
      switch ( MSGpkt->hdr.event )
      {
      case AF_DATA_CONFIRM_CMD:
        break;
 
      case AF_INCOMING_MSG_CMD:
        {
          processMSGCmd( MSGpkt );
          break;
        }

      case ZDO_STATE_CHANGE:      
        initP0();         //��ʼ���˿ںͶ�ʱ��
        break;

      default:
        break;
      }
      
      // Release the memory
      osal_msg_deallocate( (uint8 *)MSGpkt );

      // Next
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( Refer_TaskID );        
    }

    // Return unprocessed events.
    return ( events ^ SYS_EVENT_MSG );
  }
  
  /*
  // Delay 25ms for receiving the next US signal
  if ( events & RECV_DELAY_EVT )    
  {    
    //�����ӳ�25ms֮��ļ�������ֵ����Ϊ����ʱ��
    RfArrivalTime = getTime();
    IEN2 |= 0x10;             //��P1���ж�ʹ��
    bFirstRcv = TRUE;         //�����´λ��ܽ���
    
    if(recvDelayTimes == REFER_RECV_DELAY_TIMES)
    {
      recvDelayTimes = 2;
    }
    else
    {
      osal_start_timerEx( Refer_TaskID, RECV_DELAY_EVT, RECV_DELAY_PERIOD );
      recvDelayTimes++;
    }
     
    // return unprocessed events
    return ( events ^ RECV_DELAY_EVT );
  }
  */
    
  // send the data by delaying with 10ms*ID
  if ( events & REFER_DELAYSEND_EVT )    
  {
    afAddrType_t coorAddr;
    coorAddr.addrMode = (afAddrMode_t)Addr16Bit; //��������
    coorAddr.endPoint = LOCATION_DONGLE_ENDPOINT; //Ŀ�Ķ˿ں�
    coorAddr.addr.shortAddr = 0x0000;            //Э���������ַ
    AF_DataRequest( &coorAddr, (endPointDesc_t *)&epDesc,
                             LOCATION_REFER_DISTANCE_RSP, LOCATION_REFER_POSITION_RSP_LENGTH,
                             timeDiffMSG, &transId, AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );     
    /*
    if (b_ErrorData == 0)
    {
      AF_DataRequest( &coorAddr, (endPointDesc_t *)&epDesc,
                             LOCATION_REFER_DISTANCE_RSP, LOCATION_REFER_POSITION_RSP_LENGTH,
                             timeDiffMSG, &transId, AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );      
    }
    else
    {
      AF_DataRequest( &coorAddr, (endPointDesc_t *)&epDesc,
                             LOCATION_REFER_DISTANCE_RSP, 3,
                             errorData, &transId, AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );  
    }
    */
    
    // return unprocessed events
    return ( events ^ REFER_DELAYSEND_EVT );
  }
  
  return 0;  // Discard unknown events
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
      //���ö�ʱ��
      //osal_start_timerEx( Refer_TaskID, RECV_DELAY_EVT, RECV_DELAY_PERIOD );
      
      //������ʱ��T3��ʱ��25ms
      TIMER3_RUN(TRUE);
      
      //���µ�Ų�����ʱ�䣬��Ϊ��ʼ�����ʱ��
      RfArrivalTime = getTime();            
      //IEN2 |= 0x10;             //��P1���ж�ʹ��
      bFirstRcv = TRUE;         //�����´λ��ܽ���
      
      ui_sequence = pkt->cmd.Data[0];         //��ȡ���к�
      ui_mobileID = pkt->cmd.Data[1];         //��ȡ�ƶ��ڵ��
   
      break;
    }
      
    default:
      break;
    }
}

/*********************************************************************
 * @fn      getTime
 *
 * @brief   get the signal arrival time.
 *
 * @param   MAC_T2_TIME - store the signal arrival time.
 *          
 *          
 x
 * @return  none
 */
static MAC_T2_TIME getTime()
{
  MAC_T2_TIME signalArrivalTime;
  //��ȡMAC���ʱ�� from�������� ԭ������mac_mcu.c
  halIntState_t  s;
  
  // for efficiency, the 32-bit value is encoded using endian abstracted indexing 
  HAL_ENTER_CRITICAL_SECTION(s);
  
  // This T2 access macro allows accessing both T2MOVFx and T2Mx //
  MAC_MCU_T2_ACCESS_OVF_COUNT_VALUE();
  //MAC_MCU_T2_ACCESS_OVF_CAPTURE_VALUE();
  
  // Latch the entire T2MOVFx first by reading T2M0. 
  T2M0;
  ((uint8 *)&signalArrivalTime.overflowCapture)[0] = T2MOVF0;
  ((uint8 *)&signalArrivalTime.overflowCapture)[1] = T2MOVF1;
  ((uint8 *)&signalArrivalTime.overflowCapture)[2] = T2MOVF2;
  ((uint8 *)&signalArrivalTime.overflowCapture)[3] = 0;
  
  MAC_MCU_T2_ACCESS_COUNT_VALUE();
  //MAC_MCU_T2_ACCESS_CAPTURE_VALUE();
  signalArrivalTime.timerCapture = T2M1 << 8;
  signalArrivalTime.timerCapture |= T2M0;
  
  HAL_EXIT_CRITICAL_SECTION(s);
  return signalArrivalTime;
}      
      
/*********************************************************************
 * @fn      checkTime
 *
 * @brief   
 *
 * @param   
 *          
 *          
 x
 * @return  none
 */
static void checkTime()
{
  //������4�γ������ķ���
  
  if (recTimes < RECV_TIMES)        //�ж��Ƿ����������Ӧ�Ĵ���
  {
         
    if (UltraArrivalTime.overflowCapture == 0 & UltraArrivalTime.timerCapture == 0)  //�жϳ������Ƿ���յ��ˣ���ʾ������δ���յ�
    {
      i_timeDifference[recTimes] = 0;
    }
    else
    {
      //��Ų��ȵ��ļ��㷽��
      i_timeDifference[recTimes] = ((UltraArrivalTime.overflowCapture *10240) + UltraArrivalTime.timerCapture) - ((RfArrivalTime.overflowCapture *10240) + RfArrivalTime.timerCapture);
      
      //�������ȵ��ļ��㷽��
      //i_timeDifference = ((RfArrivalTime.overflowCapture *10240) + RfArrivalTime.timerCapture) - ((UltraArrivalTime.overflowCapture *10240) + UltraArrivalTime.timerCapture);
    }
  
    if (i_timeDifference[recTimes] > 941177 | i_timeDifference[recTimes] <0)   // 941177*31.25ns *340m/s = 10m  ��Ϊ���Եļ��޾��� ������������Ϊ����
    {
      i_timeDifference[recTimes] = 0;
    }

    //ui_T4time = ui_T4Overflow * 256 + ui_T4Count;
      
    //UltraArrivalTime.overflowCapture = 0;  //������ʱ������
    //UltraArrivalTime.timerCapture = 0;
    //RfArrivalTime.overflowCapture = 0;  
    //RfArrivalTime.timerCapture = 0;

    recTimes++;
  }
  if ( recTimes == RECV_TIMES )        //�ж��Ƿ��������RECV_TIMES����  
  {    
    //unsigned char timeDiffMSG[REFER_TIMEDIFF_MSG_LEN];  
    //unsigned char timeDiffMSG[8];           //����T4��T2��ʱ���Ĳ��
    //static unsigned int const timeDiffMsgLength;
    //timeDiffMsgLength = 2 + 4 * RECV_TIMES;      //ʱ���ĳ���
    
    //IEN2 &= ~0x10;       //�رն˿�1�ж�ʹ�� 
    //�������ݸ�Э����
    sendData();
    recTimes = 0;         //�����մ�����0  
  }
  
  /*
  //ֻ����һ�γ������ķ���
  
  if (UltraArrivalTime.overflowCapture == 0 & UltraArrivalTime.timerCapture == 0)  //�жϳ������Ƿ���յ��ˣ���ʾ������δ���յ�
  {
    i_timeDifference = 0;
  }
  else
  {
    //��Ų��ȵ��ļ��㷽��
    i_timeDifference = ((UltraArrivalTime.overflowCapture *10240) + UltraArrivalTime.timerCapture) - ((RfArrivalTime.overflowCapture *10240) + RfArrivalTime.timerCapture);
    
    //�������ȵ��ļ��㷽��
    //i_timeDifference = ((RfArrivalTime.overflowCapture *10240) + RfArrivalTime.timerCapture) - ((UltraArrivalTime.overflowCapture *10240) + UltraArrivalTime.timerCapture);
  }

  if (i_timeDifference > 941177 | i_timeDifference <0)   // 941177*31.25ns *340m/s = 10m  ��Ϊ���Եļ��޾��� ������������Ϊ����
  {
    i_timeDifference = 0;
  }
  */
  //ui_T4time = ui_T4Overflow * 256 + ui_T4Count;
    
  UltraArrivalTime.overflowCapture = 0;  //������ʱ������
  UltraArrivalTime.timerCapture = 0;
  RfArrivalTime.overflowCapture = 0;  
  RfArrivalTime.timerCapture = 0;
  
  //sendData();
}

/*********************************************************************
 * �������ƣ�sendData
 * ��    �ܣ������������ݺ�����ڣ�����Э����
 * ��ڲ�������
 * ���ڲ�������
 * �� �� ֵ����
 ********************************************************************/
void sendData(void)
{
  /*
  //test
  timeDiffMSG[LOCATION_REFER_POSITION_RSP_SEQUENCE] =  ui_sequence;      //���к�  ע��˴���֡û��MSG_TYPE
  timeDiffMSG[LOCATION_REFER_POSITION_RSP_FIXID] = LOCATION_REFER_ID;         //�ο��ڵ�ID
  
  int n;
  for(n=0;n<RECV_TIMES;n++)
  {
    timeDiffMSG[4*n+2] = ((uint8 *)&i_timeDifference[n])[3];      //ʱ�������λ 
    timeDiffMSG[4*n+3] = ((uint8 *)&i_timeDifference[n])[2];      //ʱ���߰�λ T2��ʱ��
    timeDiffMSG[4*n+4] = ((uint8 *)&i_timeDifference[n])[1];      //ʱ����а�λ
    timeDiffMSG[4*n+5] = ((uint8 *)&i_timeDifference[n])[0];      //ʱ���Ͱ�λ
  }
  timeDiffMSG[4*n+2]='\n';
  */
  
  
  //����4�εĽ������ƽ��ֵ�ķ���
  
  uint32 ui_totalTimeDiff = 0;
  int32 i_averageTime = 0;
  int32 i_goodTimeDifference[RECV_TIMES];
  unsigned int ui_goodValue = 0;
  unsigned int goodValueTimes = 0;
  int n;
  for (n=0;n<RECV_TIMES;n++)
  {
    if(i_timeDifference[n] != 0)
    {
      ui_totalTimeDiff += i_timeDifference[n];
      i_goodTimeDifference[ui_goodValue] = i_timeDifference[n];
      ui_goodValue++;
    }
  }
  if (ui_goodValue != 0)
  {
    i_averageTime = ui_totalTimeDiff / ui_goodValue;
    goodValueTimes = ui_goodValue;
    for (n=0;n<goodValueTimes;n++)
    {
      //10cm = Tick * 31.25 * 10**(-7) * 340 ==> Tick = 9412 ,��ƽ��ȡ����ֵ�����Ƿ����������֮��
      //�����ˣ���֪����ô�����ֵ���Լ�д
      uint32 compare;
      if((i_goodTimeDifference[n] - i_averageTime) >= 0)
      {
        compare = i_goodTimeDifference[n]-i_averageTime;
      }
      else
      {
        compare = i_averageTime - i_goodTimeDifference[n];
      }
       
      if (compare>=9412)
      {
        i_goodTimeDifference[n] = 0;
        ui_goodValue--;
      }
    }
    if (ui_goodValue ==0)
    {
      //there is no good value;
      i_averageTime = 0;
    }
    else
    {
      ui_totalTimeDiff = 0;
      for(n=0;n<goodValueTimes;n++)
      {
        ui_totalTimeDiff += i_goodTimeDifference[n];
      }
      i_averageTime = ui_totalTimeDiff / ui_goodValue;
    }
  }
  else 
  {
    i_averageTime = 0;
  }
  
  //����Ϊ��ȷ������
  //b_ErrorData = 0;
  
  //unsigned char timeDiffMSG[LOCATION_REFER_POSITION_RSP_LENGTH]; 
  timeDiffMSG[LOCATION_REFER_POSITION_RSP_SEQUENCE] =  ui_sequence;      //���к�  ע��˴���֡û��MSG_TYPE
  timeDiffMSG[LOCATION_REFER_POSITION_RSP_MOBID] =  ui_mobileID;      //�ƶ��ڵ�ID
  timeDiffMSG[LOCATION_REFER_POSITION_RSP_REFID] = LOCATION_REFER_ID;         //�ο��ڵ�ID
  
  
  if (i_averageTime!=0)
  {  
    //���ǵ�������ԶΪ10m����Tick�����ֵΪ10/340/31.25*10**9=941176,24λ���ܴ��꣬Ϊ�˽�ʡ��������ֻѡ��24λ��
    timeDiffMSG[LOCATION_REFER_POSITION_RSP_DSITANCE_HIGH] = ((uint8 *)&i_averageTime)[2];      //ʱ���߰�λ  
    timeDiffMSG[LOCATION_REFER_POSITION_RSP_DSITANCE_MIDD] = ((uint8 *)&i_averageTime)[1];      //ʱ����а�λ
    timeDiffMSG[LOCATION_REFER_POSITION_RSP_DSITANCE_LOW] = ((uint8 *)&i_averageTime)[0];      //ʱ���Ͱ�λ
   }  
  //�������ݽ�����ΪȫFF
  else
  {
    timeDiffMSG[LOCATION_REFER_POSITION_RSP_DSITANCE_HIGH] = 0xFF;      //ʱ���߰�λ  
    timeDiffMSG[LOCATION_REFER_POSITION_RSP_DSITANCE_MIDD] = 0xFF;      //ʱ����а�λ
    timeDiffMSG[LOCATION_REFER_POSITION_RSP_DSITANCE_LOW] = 0xFF;      //ʱ���Ͱ�λ
  }
  
  
  
  int temp;
  for(temp = 0;temp < RECV_TIMES;temp++)
  {
    i_timeDifference[temp] = 0;               //������ʱ�����������
  } 
  
  
    
    /*
    int n;
    for(n=0;n<RECV_TIMES;n++)
    {
      timeDiffMSG[4*n+2] = ((uint8 *)&i_timeDifference[n])[3];      //ʱ�������λ 
      timeDiffMSG[4*n+3] = ((uint8 *)&i_timeDifference[n])[2];      //ʱ���߰�λ T2��ʱ��
      timeDiffMSG[4*n+4] = ((uint8 *)&i_timeDifference[n])[1];      //ʱ����а�λ
      timeDiffMSG[4*n+5] = ((uint8 *)&i_timeDifference[n])[0];      //ʱ���Ͱ�λ
    }
    */
    //timeDiffMSG[4*n+2]='\n';                                       //ֱ����Э��������ӣ����������еĴ���      //���\n ��ֹ�����ڽ��ն˽���
      
    //timeDiffMSG[2] = ((uint8 *)&ui_T4time)[3];      //ʱ�����߰�λ  T4��ʱ��
    //timeDiffMSG[3] = ((uint8 *)&ui_T4time)[2];      //ʱ���߰�λ  
    //timeDiffMSG[4] = ((uint8 *)&ui_T4time)[1];      //ʱ����а�λ
    //timeDiffMSG[5] = ((uint8 *)&ui_T4time)[0];      //ʱ���Ͱ�λ
    
    //timeDiffMSG[RP_TIMEDIFF_HIGH] = ((uint8 *)&ui_T4time)[2];      //ʱ���߰�λ  T4��ʱ��
    //timeDiffMSG[RP_TIMEDIFF_MIDD] = ((uint8 *)&ui_T4time)[1];      //ʱ����а�λ
    //timeDiffMSG[RP_TIMEDIFF_LOW] = ((uint8 *)&ui_T4time)[0];      //ʱ���Ͱ�λ
    
    
  //����һ�γ������ķ���,�������ƶ��ڵ�ID
  /*
  timeDiffMSG[LOCATION_REFER_POSITION_RSP_SEQUENCE] =  ui_sequence;      //���к�  ע��˴���֡û��MSG_TYPE
  timeDiffMSG[LOCATION_REFER_POSITION_RSP_MOBID] =  ui_mobileID;      //�ƶ��ڵ�ID
  timeDiffMSG[LOCATION_REFER_POSITION_RSP_REFID] = LOCATION_REFER_ID;         //�ο��ڵ�ID
  
  if (i_timeDifference!=0)
  {  
    //���ǵ�������ԶΪ10m����Tick�����ֵΪ10/340/31.25*10**9=941176,24λ���ܴ��꣬Ϊ�˽�ʡ��������ֻѡ��24λ��
    timeDiffMSG[LOCATION_REFER_POSITION_RSP_DSITANCE_HIGH] = ((uint8 *)&i_timeDifference)[2];      //ʱ���߰�λ  
    timeDiffMSG[LOCATION_REFER_POSITION_RSP_DSITANCE_MIDD] = ((uint8 *)&i_timeDifference)[1];      //ʱ����а�λ
    timeDiffMSG[LOCATION_REFER_POSITION_RSP_DSITANCE_LOW] = ((uint8 *)&i_timeDifference)[0];      //ʱ���Ͱ�λ
   }  
  //�������ݽ�����ΪȫFF
  else
  {
    timeDiffMSG[LOCATION_REFER_POSITION_RSP_DSITANCE_HIGH] = 0xFF;      //ʱ���߰�λ  
    timeDiffMSG[LOCATION_REFER_POSITION_RSP_DSITANCE_MIDD] = 0xFF;      //ʱ����а�λ
    timeDiffMSG[LOCATION_REFER_POSITION_RSP_DSITANCE_LOW] = 0xFF;      //ʱ���Ͱ�λ
  }
  */
  
  //����һ���¼����ӳ�10ms*ID��ʱ�䷢����Ϣ��������ײ
  unsigned int delaySend;
  delaySend = 10 * LOCATION_REFER_ID;
  osal_start_timerEx( Refer_TaskID, REFER_DELAYSEND_EVT, delaySend );
  //i_timeDifference = 0;
  
}

/*********************************************************************
 * �������ƣ�startRecWork
 * ��    �ܣ��������պ������
 * ��ڲ�������
 * ���ڲ�������
 * �� �� ֵ����
 ********************************************************************/
//void startRecWork(void)
//{
  //ucGainNo = 11;        //����T1��ʱ���޷���������Ϊֱ������Ϊ������棬
  /*
  HalTimerInit ();
  HalTimerConfig(HAL_TIMER_3,HAL_TIMER_MODE_CTC,
                 HAL_TIMER_CHANNEL_SINGLE,HAL_TIMER_CH_MODE_OUTPUT_COMPARE,
                 TRUE,TurnP0CallBack);
  HalTimerStart(HAL_TIMER_3,100);
  halTimerSetPrescale(HAL_TIMER_3,0x0A);
  */
  
  //���ö�ʱʱ�䣬ʱ�䵽��ϵͳ����REFER_SETGAIN_EVT�¼�
  //osal_start_timerEx( Refer_TaskID, REFER_SETGAIN_EVT, uiGainTime[ucGainNo] );
  
  
  //ucGainNo = 0;
  //halSetTimer1Period(uiGainTime[ucGainNo]);
  //setGain(ucGainNo);

  //TIMER1_ENABLE_OVERFLOW_INT(TRUE);
  //IEN1 |=(0x01<<1);         //ʹ��Timer1���ж�
  //TIMER1_RUN(TRUE);
  
  
  //HAL_ENABLE_INTERRUPTS();   //ʹ��ȫ���ж�
  
  //Inhi = 0;
//}

/*********************************************************************
*********************************************************************/

/*********************************************************************
 * �������ƣ�halSetTimer1Period
 * ��    �ܣ����ö�ʱ��1��ʱ����
 * ��ڲ�����period   ��ʱ����       
 * ���ڲ�������
 * �� �� ֵ����
 ********************************************************************/
/*
void halSetTimer1Period(uint16 period)
{
  //��T1CC0д�����ռ���ֵperiod 
  T1CC0L = period & 0xFF;                      // ��period�ĵ�8λд��T1CC0L
  T1CC0H = ((period & 0xFF00)>>8);             // ��period�ĸ�8λд��T1CC0H
}
*/

/*********************************************************************
 * �������ƣ�setGain
 * ��    �ܣ�����GCA GCB GCC GCD �������С
 * ��ڲ�������
 * ���ڲ�������
 * �� �� ֵ����
 ********************************************************************/
void setGain(unsigned char ucGainNo)
{
  switch (ucGainNo)
  {
    case 0:
    {
      GCA = 0;
      GCB = 0;
      GCC = 0;
      GCD = 0;
      break;
    }  
    case 1:
    {
      GCA = 1;
      GCB = 0;
      GCC = 0;
      GCD = 0;
      break;
    }  
    case 2:
    {
      GCA = 0;
      GCB = 1;
      GCC = 0;
      GCD = 0;
      break;
    }  
    case 3:
    {
      GCA = 1;
      GCB = 1;
      GCC = 0;
      GCD = 0;
      break;
    }  
    case 4:
    {
      GCA = 0;
      GCB = 0;
      GCC = 1;
      GCD = 0;
      break;
    }  
    case 5:
    {
      GCA = 1;
      GCB = 0;
      GCC = 1;
      GCD = 0;
      break;
    }  
    case 6:
    {
      GCA = 0;
      GCB = 1;
      GCC = 1;
      GCD = 0;
      break;
    }  
    case 7:
    {
      GCA = 1;
      GCB = 1;
      GCC = 1;
      GCD = 0;
      break;
    }  
    case 8:
    {
      GCA = 0;
      GCB = 0;
      GCC = 0;
      GCD = 1;
      break;
    }  
    case 9:
    {
      GCA = 1;
      GCB = 0;
      GCC = 0;
      GCD = 1;
      break;
    }  
    case 10:
    {
      GCA = 0;
      GCB = 1;
      GCC = 0;
      GCD = 1;
      break;
    }  
    
    default:
    {
      GCA = 1;
      GCB = 1;
      GCC = 0;
      GCD = 1;
      break;
    }  
  }
}

/*********************************************************************
 * �������ƣ�T1_IRQ
 * ��    �ܣ���ʱ��1�жϷ����� ע����ں���������ж���ں�����ͬ ʵ��Ч�������룬����ȥ
 * ��ڲ�������
 * ���ڲ�������
 * �� �� ֵ����
 ********************************************************************/
/*
_PRAGMA(vector=T1_VECTOR) __near_func __interrupt void halTimer1Isr(void);  
HAL_ISR_FUNCTION( halTimer1Isr, T1_VECTOR)
{
  IEN1 &= ~0x02;    //�رն�ʱ��1�ж�ʹ��
  if(T1STAT & 0x21) //ȷ���Ƿ������ʱ�ж�
  {
    ucGainNo++;
    if(ucGainNo <11)
    {
      halSetTimer1Period(uiGainTime[ucGainNo]);
      setGain(ucGainNo);
    }
    else 
    {
      if(ucGainNo < TIME_OUT)
      {
        setGain(11);
      }
      else          //��ʱ��δ���յ��������ź�
      {
        TIMER1_RUN(FALSE);    //�رն�ʱ��1
        ucGainNo = 0;
        setGain(ucGainNo);
        bRcvFinish = 1;    //����Ϊ�������
      }
    }
    
  }
  T1STAT &= ~0x21;  //�����ʱ��1�ı�־λ
  IRCON &= ~0x02;   //ͬ��
  IEN1 |= 0x02;    //�򿪶�ʱ��1�ж�ʹ��
  
}
*/

/*********************************************************************
 * �������ƣ�P1ISR
 * ��    �ܣ�IO1�жϷ�������P1_2��⵽�½��غ󼴻���� ע����ں���������ж���ں�����ͬ
 * ��ڲ�������
 * ���ڲ�������
 * �� �� ֵ����
 ********************************************************************/
HAL_ISR_FUNCTION(P1_IRQ,P1INT_VECTOR)  
 {
    if(P1IFG>0)
    {
      halProcessKeyInterrupt();
      
      P1IFG = 0;
    }
    P1IF = 0;             //����жϱ�ǣ�λ��IRCON�Ĵ�����
 }

/*********************************************************************
 * �������ƣ�halProcessKeyInterrupt
 * ��    �ܣ���ʱ��3�жϷ�������P0_0��⵽�½��غ󼴻���� ע����ں���������ж���ں�����ͬ
 * ��ڲ�������
 * ���ڲ�������
 * �� �� ֵ����
 ********************************************************************/
void halProcessKeyInterrupt (void)
{
    if (bFirstRcv == TRUE)        //�ж��Ƿ��ǵ�һ�ν���
    {       
      //P1_1 = 1;               //����ʹ��
      //TIMER4_RUN(TRUE);       //������ʱ��4
     
      UltraArrivalTime = getTime();

      if(YelLed_State == 0)
      {
        HAL_TURN_ON_LED3();
        YelLed_State = 1;
      }
      else
      {
        HAL_TURN_OFF_LED3();
        YelLed_State = 0;
      }
      
      bFirstRcv = FALSE;//����Ϊ�Ѿ����չ���
      //IEN2 &= ~0x10;       //�رն˿�1�ж�ʹ��  
      checkTime();         //�������󵽼���
      
      
      //TIMER1_RUN(FALSE);//�յ���������͹رն�ʱ��1������ʼ������
      //ucGainNo = 0;
      //setGain(ucGainNo);
      
      
    }
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
    if(t3count == 12)     //2*12=24ms
    {
      t3count = 0;
      
      //���µ�Ų�����ʱ�䣬��Ϊ��ʼ�����ʱ��
      RfArrivalTime = getTime(); 
      
      //P1_1 = ~P1_1;               //����ʹ��
      //IEN2 |= 0x10;             //��P1���ж�ʹ��
      bFirstRcv = TRUE;         //�����´λ��ܽ���
      
      if(recvDelayTimes == REFER_RECV_DELAY_TIMES)
      {
        recvDelayTimes = 2;
        //����ʱ��T3��0��Ϊ�´��ж���׼��
        T3CTL |= 0x04;       //������ֵ��0
        TIMER3_RUN(FALSE);
      }
      else
      {
        recvDelayTimes++;
      }
    }    
  }
  
  TIMIF &= ~0x01;  
  //T1STAT &= ~0x21;  //�����ʱ��3�ı�־λ
  //IRCON &= ~0x02;   //ͬ��
  IEN1 |= 0x03;    //�򿪶�ʱ��3�ж�ʹ��
}

/*********************************************************************
 * �������ƣ�T4_IRQ
 * ��    �ܣ���ʱ��4�жϷ����� ע����ں���������ж���ں�����ͬ
 * ��ڲ�������
 * ���ڲ�������
 * �� �� ֵ����
 ********************************************************************/
/*
HAL_ISR_FUNCTION( halTimer4Isr, T4_VECTOR)
{
  //TIMER4_RUN(FALSE);    //�˴����ܹرռ�ʱ����ΪӲ���ӳ�Ҳ�����ڴ�
  EA = FALSE;           //�ر���ʹ��
  if(TIMIF & 0x08)
  {
    ui_T4Overflow++;
  }
  TIMIF &= ~0x08;
  EA = TRUE;
  //TIMER4_RUN(TRUE);
}
*/

/*********************************************************************
 * �������ƣ�initP1
 * ��    �ܣ���ʼ��
 * ��ڲ�������
 * ���ڲ�������
 * �� �� ֵ����
 ********************************************************************/
void initP0()
{
   P0SEL &= ~0xBD;              //��P0_2 P0_3 P0_4 P0_5 P0_7��ΪGPIO
   P0DIR |= 0xBC;               //��P0_2��P0_3 P0_4 P0_5 P0_7��Ϊ���
   
   //P1SEL &= ~0x01;              //��ʼ��P1_1 ����Ϊͨ��I/O��� ʾ��������ʹ�� P4�ϵ�4��
   //P1DIR &= ~0x01;
   //P1_1 = 0;
   
   P1SEL &= ~0x04;              //��P1_2����ΪGPIO   
   //P0SEL |= 0x01;             //��P0_0����Ϊ�жϵ�����   datasheet�����д��
   P1DIR &= ~0x04;              //��P1_2����Ϊ����
   P1INP &= ~0x04;                //��P1_2����Ϊ����/����
   P2INP &= ~0x40;                  //�˿�1Ϊ����
      
   P1IEN |= 0x04;               //P1_2����Ϊ�ж�ʹ�ܷ�ʽ
   PICTL |= 0x02;               //P1_2Ϊ�½��ش���     
   IEN2 |= 0x10;                //�򿪶˿�1�ж�ʹ��  
   P1IFG  = 0;                  //��λ�˿�1�ı�־λ
   P1IF  = 0; 
   
   //IP1 |= 0x20;                //����P0INT�����ȼ����
   //IP0 |= 0x20;
   
   //��ʼ����������Ϊ0
   
   int temp;
    for(temp = 0;temp < RECV_TIMES;temp++)
    {
      i_timeDifference[temp] = 0;               //������ʱ�����������
    }
   
   //i_timeDifference = 0;
   
    //ui_MAC_RFArrivalTime = pkt ->timestamp;
    //startRecWork(); 
   
   UltraArrivalTime.overflowCapture = 0;  //��ʼ��������ʱ������
   UltraArrivalTime.timerCapture = 0;
   RfArrivalTime.overflowCapture = 0;  
   RfArrivalTime.timerCapture = 0;
   
   //TIMER34_INIT(4);                // ʹ�ܶ�ʱ��4������ж� 
   //IEN1 |= (0x01 << 4);             // ʹ��Timer4���ж�T4IE 
   
   //TIMER1_INIT();               //��ʼ����ʱ��1 ʹ�ܶ�ʱ��1������ж�
   //T1CCTL0 |= 0x44;             //����ʱ��1��ͨ��0�ж����󣬲�������Ϊ����Ƚ�ģʽ
   
   ucGainNo = 11;        //����T1��ʱ���޷���������Ϊֱ������Ϊ������棬
   setGain(ucGainNo);
   Inhi = 0;
   
   YelLed_State = 0;          //��ʼ���Ƶ�״̬Ϊ�ر�   
   
   TIMER34_INIT(3);
   halSetTimer3Period(250); //��ʾ�ӳ�2ms ע�⣺ʵ��д��T1CC0�Ĵ�����ֵӦС��255
   IEN1 |= (0x01 << 3);             // ʹ��Timer3���ж�

   EA = 1;                      //����ʹ��
}

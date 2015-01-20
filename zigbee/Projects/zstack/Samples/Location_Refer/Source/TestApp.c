/*********************************************************************
  Filename:       RefNode.c
  Revised:        $Date: 2007-02-15 15:11:28 -0800 (Thu, 15 Feb 2007) $
  Revision:       $Revision: 13481 $

  Description: Reference Node for the Z-Stack Location Profile.
				
  Copyright (c) 2006 by Texas Instruments, Inc.
  All Rights Reserved.  Permission to use, reprodue, copy, prepare
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
#include "mac_mcu.h"
//#include "stdlib.h"            //pel+ abs()

#include "UltraRec.h"
/*********************************************************************
 * CONSTANTS
 */

#define LAST_MOBILE_ID         4  //���һ���ƶ��ڵ��ID��Ҳ�������Ϊһ���е��ƶ��ڵ�ĸ���

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

static unsigned int ui_sequence;                                       //��ȡ���к�
int32 i_timeDifference[LAST_MOBILE_ID];                                //����ʱ����Ticks���� *31.25*10**(-7)= ʱ���
unsigned char timeDiffMSG[R2C_DIFF_TIME_LENGTH]; 
unsigned char recvMobiID = 1;                                          //���ڱ�־���ڽ��ܵ��ƶ��ڵ��
unsigned char t3count = 0;

unsigned char refer_id = 1;           //�ǲο��ڵ�Ľڵ�ţ����벻ͬ�ڵ��ʱ����Ҫ����
unsigned char recv_timeout = 160;     //����һ�����кŵĳ�ʱʱ�䣬������ڴ˻�δ�����ʾδ������ֱ�ӷ���
unsigned char delay_time = 18;        //ʵ���ӳ���2*delay_time��ָÿ��ͬ����˵��ӳ�ʱ��

/*********************************************************************
 * LOCAL VARIABLES
 */

static const cId_t Refer_InputClusterList[] =
{
  CID_S2MR_BROADCAST,
  CID_C2A_GET_BASIC_VALUE,
  CID_C2A_SET_BASIC_VALUE   
};

static const cId_t Refer_OutputClusterList[] =
{
  CID_R2C_DIFF_TIME,
  CID_A2C_RP_BASIC_VALUE,
  CID_A2C_SUCCESS_RESPONSE
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
static void halProcessKeyInterrupt (void);
static void sendData(void);
void halSetTimer3Period(uint8 period);
void getBasicValue();
void setBasicValue(afIncomingMSGPacket_t *pkt);
void successResponse(uint8 result);

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
      
   //��ʼ����������Ϊ0
   int temp;
   for(temp = 0;temp < LAST_MOBILE_ID;temp++)
   {
     i_timeDifference[temp] = 0;               //������ʱ�����������
   }
   
   UltraArrivalTime.overflowCapture = 0;  //��ʼ��������ʱ������
   UltraArrivalTime.timerCapture = 0;
   RfArrivalTime.overflowCapture = 0;  
   RfArrivalTime.timerCapture = 0;
   
   
   //����T1��ʱ���޷���������Ϊֱ������Ϊ������棬
   GCA = 1;
   GCB = 1;
   GCC = 0;
   GCD = 1;
   Inhi = 0;
   
   RedLed_State = 0;          //��ʼ�����״̬Ϊ�ر�   
   
   TIMER34_INIT(3);
   halSetTimer3Period(250); //��ʾ�ӳ�2ms ע�⣺ʵ��д��T1CC0�Ĵ�����ֵӦС��255
   IEN1 |= (0x01 << 3);             // ʹ��Timer3���ж�

   EA = 1;                      //����ʹ��
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
HAL_ISR_FUNCTION( halTimer3Isr, T3_VECTOR)
{
  IEN1 &= ~0x03;    //�رն�ʱ��3�ж�ʹ��
  if(TIMIF & 0x01) //ȷ���Ƿ������ʱ�ж�
  {
    t3count++;
    if(t3count == delay_time)     //2*12=24ms
    {
      t3count = 0;
      
      //���µ�Ų�����ʱ�䣬��Ϊ��ʼ�����ʱ��
      RfArrivalTime = getTime(); 
      
      //P1_1 = ~P1_1;               //����ʹ��
      //IEN2 |= 0x10;             //��P1���ж�ʹ��
      bFirstRcv = TRUE;         //�����´λ��ܽ���
      recvMobiID++;             //������ʱ��ʱ��ͽ����սڵ��ID+1
      
      if(recvMobiID == LAST_MOBILE_ID)
      {
        //����ʱ��T3��0��Ϊ�´��ж���׼��
        TIMER3_RUN(FALSE);
        T3CTL |= 0x04;       //������ֵ��0
      }
    }    
  }
  
  TIMIF &= ~0x01;  
  //T1STAT &= ~0x21;  //�����ʱ��3�ı�־λ
  //IRCON &= ~0x02;   //ͬ��
  IEN1 |= 0x03;    //�򿪶�ʱ��3�ж�ʹ��
}

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
     
      UltraArrivalTime = getTime();

      if(RedLed_State == 0)
      {
        HAL_TURN_ON_LED2();
        RedLed_State = 1;
      }
      else
      {
        HAL_TURN_OFF_LED2();
        RedLed_State = 0;
      }
      
      bFirstRcv = FALSE;//����Ϊ�Ѿ����չ���
      //IEN2 &= ~0x10;       //�رն˿�1�ж�ʹ��  
      checkTime();         //�������󵽼���
            
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
        //��ʼ��ʱ���������ݷ��͸�Э����   
        getBasicValue();
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
  
  // ��ʱ�������еĽ��
  if ( events & RECV_TIMEOUT_EVT )    
  {
    sendData();
        
    // return unprocessed events
    return ( events ^ RECV_TIMEOUT_EVT );
  }
  
  
  // send the data by delaying with 10ms*ID
  if ( events & REFER_DELAYSEND_EVT )    
  {
    afAddrType_t coorAddr;
    coorAddr.addrMode = (afAddrMode_t)Addr16Bit; //��������
    coorAddr.endPoint = LOCATION_DONGLE_ENDPOINT; //Ŀ�Ķ˿ں�
    coorAddr.addr.shortAddr = 0x0000;            //Э���������ַ
    AF_DataRequest( &coorAddr, (endPointDesc_t *)&epDesc,
                             CID_R2C_DIFF_TIME, R2C_DIFF_TIME_LENGTH,
                             timeDiffMSG, &transId, AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );   
    //��������������
    int i;
    for(i=0;i<R2C_DIFF_TIME_LENGTH;i++)
    {
      timeDiffMSG[i] = 0;     
    }
       
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
    case CID_S2MR_BROADCAST:
    {
      //������ʱ��T3��ʱ��24ms
      TIMER3_RUN(TRUE);
            
      //���µ�Ų�����ʱ�䣬��Ϊ��ʼ�����ʱ��
      RfArrivalTime = getTime();            
      //IEN2 |= 0x10;             //��P1���ж�ʹ��
      bFirstRcv = TRUE;         //�����´λ��ܽ���
      recvMobiID = 1;  
      
      ui_sequence = pkt->cmd.Data[0];         //��ȡ���к�
      
      //���ó�ʱʱ�䣬����ȡ��4���ƶ��ڵ�ķ��͸�Э����
        osal_start_timerEx( Refer_TaskID, RECV_TIMEOUT_EVT, recv_timeout );
   
      break;
    }
    
    case CID_C2A_GET_BASIC_VALUE:
    {
      getBasicValue();
      break;
    }
      
    case CID_C2A_SET_BASIC_VALUE:
    {
      setBasicValue(pkt);   
      break;
    }
      
    default:
      break;
    }
}

/*********************************************************************
 * @fn      getBasicValue
 *
 * @brief   get basic value and send to Coor
 *
 * @param   none
 *
 * @return  none
 */
void getBasicValue()
{
  //�յ���ѯ��Ϣ��˸�̵�3�Σ�ÿ��300ms
  HalLedBlink(HAL_LED_1, 3, 50, 300);
  
  //��������Ϣ���ظ�Э����
  unsigned char theMessageData[R2C_RP_BASIC_VALUE_LENGTH];     
  theMessageData[R2C_RP_BASIC_VALUE_NODE_TYPE] = NT_REF_NODE;
  theMessageData[R2C_RP_BASIC_VALUE_REF_ID] = refer_id;
  theMessageData[R2C_RP_BASIC_VALUE_RECV_TIME_OUT] = recv_timeout;
  theMessageData[R2C_RP_BASIC_VALUE_RECV_DELAY_TIME] = delay_time * 2;     //�趨��ֵʵ����*2�˵�
  
  afAddrType_t coorAddr;
  coorAddr.addrMode = (afAddrMode_t)Addr16Bit; //��������
  coorAddr.endPoint = LOCATION_DONGLE_ENDPOINT; //Ŀ�Ķ˿ں�
  coorAddr.addr.shortAddr = 0x0000;            //Э���������ַ
  AF_DataRequest( &coorAddr, (endPointDesc_t *)&epDesc,
                           CID_A2C_RP_BASIC_VALUE, R2C_RP_BASIC_VALUE_LENGTH,
                           theMessageData, &transId, AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ); 
}

/*********************************************************************
 * @fn      setBasicValue
 *
 * @brief   set basic value and send a success response to Coor
 *
 * @param   none
 *
 * @return  none
 */
void setBasicValue(afIncomingMSGPacket_t *pkt)
{
  //��Э�������͹���������д��ref�ڵ���
  refer_id = pkt->cmd.Data[C2R_SET_BASIC_VALUE_REF_ID];
  recv_timeout = pkt->cmd.Data[C2R_SET_BASIC_VALUE_RECV_TIME_OUT];
  delay_time = pkt->cmd.Data[C2R_SET_BASIC_VALUE_RECV_DELAY_TIME] / 2;      //���õ�ֵ��Ҫ����2����ʹ��
  
  //�����óɹ���Ϣ���ظ�Э����
  successResponse(1);
}    

/*********************************************************************
 * @fn      successResponse
 *
 * @brief   send the success response to the coor
 *
 * @param   none
 *
 * @return  none
 */
void successResponse(uint8 result)
{
  //�յ�������Ϣ��˸�̵�3�Σ�ÿ��300ms
  HalLedBlink(HAL_LED_1, 3, 50, 300);
  
  //�����óɹ���Ϣ���ظ�Э����
  unsigned char theMessageData[A2C_SUCCESS_RESPONSE_LENGTH];     
  theMessageData[A2C_SUCCESS_RESPONSE_NODE_TYPE] = NT_REF_NODE;
  theMessageData[A2C_SUCCESS_RESPONSE_NODE_ID] = refer_id;        
  theMessageData[A2C_SUCCESS_RESPONSE_RESULT] = result;         //1��ʾ���óɹ�
  
  afAddrType_t coorAddr;
  coorAddr.addrMode = (afAddrMode_t)Addr16Bit; //��������
  coorAddr.endPoint = LOCATION_DONGLE_ENDPOINT; //Ŀ�Ķ˿ں�
  coorAddr.addr.shortAddr = 0x0000;            //Э���������ַ
  AF_DataRequest( &coorAddr, (endPointDesc_t *)&epDesc,
                           CID_A2C_SUCCESS_RESPONSE, A2C_SUCCESS_RESPONSE_LENGTH,
                           theMessageData, &transId, AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );
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
  //180ms��ѯ����ķ���
  if (UltraArrivalTime.overflowCapture == 0 & UltraArrivalTime.timerCapture == 0)  //�жϳ������Ƿ���յ��ˣ���ʾ������δ���յ�
  {
    i_timeDifference[recvMobiID-1] = 0;
  }
  else
  {
    //��Ų��ȵ��ļ��㷽��
    i_timeDifference[recvMobiID-1] = ((UltraArrivalTime.overflowCapture *10240) + UltraArrivalTime.timerCapture) - ((RfArrivalTime.overflowCapture *10240) + RfArrivalTime.timerCapture);
    
    //ȥ������10��С��0�Ĵ�������
    if (i_timeDifference[recvMobiID-1] > 941177 | i_timeDifference[recvMobiID-1] <0)   // 941177*31.25ns *340m/s = 10m  ��Ϊ���Եļ��޾��� ������������Ϊ����
    {
      i_timeDifference[recvMobiID-1] = 0xFFFFFF;
    }  
  }
 
  if(recvMobiID == LAST_MOBILE_ID)
  {
    osal_stop_timerEx(Refer_TaskID,RECV_TIMEOUT_EVT);
    sendData();
  }
   
  UltraArrivalTime.overflowCapture = 0;  //������ʱ������
  UltraArrivalTime.timerCapture = 0;
  RfArrivalTime.overflowCapture = 0;  
  RfArrivalTime.timerCapture = 0;
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
  timeDiffMSG[R2C_DIFF_TIME_SEQ] =  ui_sequence;      //���к�  ע��˴���֡û��MSG_TYPE
  timeDiffMSG[R2C_DIFF_TIME_REFID] = refer_id;         //�ο��ڵ�ID
  
  int n;
  for(n=0;n<LAST_MOBILE_ID;n++)
  {
    timeDiffMSG[3*n+2] = ((uint8 *)&i_timeDifference[n])[2];      //ʱ���߰�λ T2��ʱ��
    timeDiffMSG[3*n+3] = ((uint8 *)&i_timeDifference[n])[1];      //ʱ����а�λ
    timeDiffMSG[3*n+4] = ((uint8 *)&i_timeDifference[n])[0];      //ʱ���Ͱ�λ
  }
  
  //����һ���¼���1�Ųο��ڵ�ֱ�ӷ��ͣ������ڵ��ӳ�5ms*(ID-1)��ʱ�䷢����Ϣ��������ײ
  if (refer_id == 1)
  {
    afAddrType_t coorAddr;
    coorAddr.addrMode = (afAddrMode_t)Addr16Bit; //��������
    coorAddr.endPoint = LOCATION_DONGLE_ENDPOINT; //Ŀ�Ķ˿ں�
    coorAddr.addr.shortAddr = 0x0000;            //Э���������ַ
    AF_DataRequest( &coorAddr, (endPointDesc_t *)&epDesc,
                             CID_R2C_DIFF_TIME, R2C_DIFF_TIME_LENGTH,
                             timeDiffMSG, &transId, AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );
    
    //��������������
    int i;
    for(i=0;i<R2C_DIFF_TIME_LENGTH;i++)
    {
      timeDiffMSG[i] = 0;     
    }
  }
  else
  {
    unsigned int delaySend;
    delaySend = 5 * (refer_id-1);
    osal_start_timerEx( Refer_TaskID, REFER_DELAYSEND_EVT, delaySend );
  }
  
   //������ʱ�����������
  int temp;
  for(temp = 0;temp < LAST_MOBILE_ID;temp++)
  {
    i_timeDifference[temp] = 0;              
  } 
}

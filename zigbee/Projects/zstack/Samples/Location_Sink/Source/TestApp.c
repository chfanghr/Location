/*********************************************************************
  Filename:       RefNode.c
  Revised:        $Date: 2007-02-15 15:11:28 -0800 (Thu, 15 Feb 2007) $
  Revision:       $Revision: 13481 $

  Description: Sinkence Node for the Z-Stack Location Profile.
				
  Copyright (c) 2006 by Texas Instruments, Inc.
  All Rights Reserved.  Permission to use, reproduce, copy, prepare
  derivative works, modify, distribute, perform, display or sell this
  software and/or its documentation for any purpose is prohibited
  without the express written consent of Texas Instruments, Inc.
  �޸Ķ�λ�㷨��sink�ڵ㣬���������ԵĹ㲥�����ݶ�Ϊ3s һ�������ڷ���10�ι㲥�ź�
  �޸���-DMAX_BCAST = 22 Ĭ��Ϊ9
  2014_12_20 �޸��㷨��Ϊ����Ӧ����ƶ��ڵ㣬��Ҫ������ѯ�㲥
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

/*********************************************************************
 * CONSTANTS
 */
#define SINK_BROADCAST_PERIOD 120 //�㲥����һ��Ϊ120ms����һ��,��λΪms

#define SINK_BROADCAST_TIMES  4     //ÿ�δ������ڵķ������

#define SEQUENCE                200  //���кţ������ж��Ƿ��յ�����ͬһ������

#define MOBILEN_NO            4       //�ƶ��ڵ�ĸ���


/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
byte Sink_TaskID;
unsigned char uc_sequence = 100;        //��Ϊ������100-200���ͽ�β��0x0a�ֿ�
unsigned char uc_mobileID = 1;          //��һ�ι㲥��1�Žڵ㿪ʼ
unsigned char broadcastTimes = 0;       //���ڹ㲥����
uint8 transId;
bool GreLedState = 0;
static byte transId;


/*********************************************************************
 * FUNCATION
*/
void initP1();
void startBroadcast();
/*********************************************************************/
 // LOCAL VARIABLES
 
/*
static const cId_t Sink_InputClusterList[] =
{
  LOCATION_ULTRA_BLORDCAST
};
*/
static const cId_t Sink_OutputClusterList[] =
{
  LOCATION_ULTRA_BLORDCAST,
};

static const SimpleDescriptionFormat_t Sink_SimpleDesc =
{
  LOCATION_SINK_ENDPOINT,
  LOCATION_PROFID,
  LOCATION_SINK_DEVICE_ID,
  LOCATION_DEVICE_VERSION,
  LOCATION_FLAGS,

  /*sizeof(Sink_InputClusterList),
  (cId_t*)Sink_InputClusterList,*/
  0,
  (cId_t *) NULL,
  sizeof(Sink_OutputClusterList),
  (cId_t*)Sink_OutputClusterList
};

static const endPointDesc_t epDesc =
{
  LOCATION_SINK_ENDPOINT,
  &Sink_TaskID,
  (SimpleDescriptionFormat_t *)&Sink_SimpleDesc,
  noLatencyReqs
};


/*********************************************************************
 * �������ƣ�initP1
 * ��    �ܣ���ʼ��
 * ��ڲ�������
 * ���ڲ�������
 * �� �� ֵ����
 ********************************************************************/
void initP1()
{
   P1SEL &= ~0x02;              //��P1_1��ΪGPIO
   P1DIR |= 0x02;               //��P1_1��Ϊ���
   P1_1 = 0;                  
}

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/*********************************************************************
 * @fn      Sink_Init
 *
 * @brief   Initialization function for this OSAL task.
 *
 * @param   task_id - the ID assigned by OSAL.
 *
 * @return  none
 */
void Sink_Init( byte task_id )
{
  Sink_TaskID = task_id;

  // Register the endpoint/interface description with the AF.
  afRegister( (endPointDesc_t *)&epDesc );

  // Register for all key events - This app will handle all key events.
  RegisterForKeys( Sink_TaskID );

#if defined ( LCD_SUPPORTED )
  HalLcdWriteString( "Location-RefNode", HAL_LCD_LINE_2 );
#endif
  //�豸��ʼ��
  initP1();
  
  HAL_TURN_OFF_LED1();         // Ϩ��LED_G��LED_R��LED_Y ��ʹ���̵ƺͺ�ƣ���Ϊ����ʹ����P1_0�ں�P1_1
  HAL_TURN_OFF_LED2();       
  HAL_TURN_OFF_LED3();
}

/*********************************************************************
 * @fn      SinkNode_ProcessEvent
 *
 * @brief   Generic Application Task event processor.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - Bit map of events to process.
 *
 * @return  none
 */
uint16 Sink_ProcessEvent( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;
  if ( events & SYS_EVENT_MSG )
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( Sink_TaskID );
    while( MSGpkt )
    {
      switch ( MSGpkt->hdr.event )
      {
      case KEY_CHANGE:
        break;

      case AF_INCOMING_MSG_CMD:
        break;

      case ZDO_STATE_CHANGE:
        osal_start_timerEx( Sink_TaskID, SINK_BROADCAST_EVT, SINK_BROADCAST_PERIOD );  //���ù㲥��ʱ                                      
        break;

      default:
        break;
      }
      // Release the memory
      osal_msg_deallocate( (uint8 *)MSGpkt );
      
      // Next
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( Sink_TaskID );
    }
    
    // Return unprocessed events.
    return ( events ^ SYS_EVENT_MSG );  
  }
  
  // Send a message out - This event is generated by a timer
  if ( events & SINK_BROADCAST_EVT )    
  {
    startBroadcast();
    // Setup to send period message again
    osal_start_timerEx( Sink_TaskID, SINK_BROADCAST_EVT, SINK_BROADCAST_PERIOD );
      
    // return unprocessed events
    return ( events ^ SINK_BROADCAST_EVT );
  }
  
  // Discard unknown events.
  return 0;  
}

/*********************************************************************
 * @fn      startBlast
 *
 * @brief   Start a sequence of blasts
 *
 * @param   none
 *
 * @return  none
 */
void startBroadcast( void )
{
  //�����ԵĹ㲥
  if (GreLedState == 0)
  {
    HAL_TURN_ON_LED1();     //�ı�һ��LED_G��״̬����ʾ���ڷ��䳬����
    GreLedState = 1;
  }
  else 
  {
    HAL_TURN_OFF_LED1();
    GreLedState = 0;
  }  
  
  if (uc_mobileID > MOBILEN_NO)
  {
    uc_mobileID = 1;
    if (uc_sequence < SEQUENCE)     //���÷������к�
    {
      uc_sequence++;
    }
    else
    {
      uc_sequence = 100;            
    }
  }
  
  //�����Ų�
  unsigned char theMessageData[2];     //��������Ϊ���к� �ƶ��ڵ��
  theMessageData[0] = uc_sequence;
  theMessageData[1] = uc_mobileID;
  afAddrType_t broadcast_DstAddr;
  broadcast_DstAddr.addrMode = (afAddrMode_t)AddrBroadcast;
  broadcast_DstAddr.endPoint = LOCATION_REFER_ENDPOINT;           //��ʱδ�����ι㲥���ֶ˿ڵ����⣬ֻ�ܽ���ͬ���͵Ľڵ��ENDPOINT����Ϊһ����
  broadcast_DstAddr.addr.shortAddr = 0xFFFF;  //��ʾ���ݰ����������е����нڵ�㲥������0xFFFC��·�������ղ�������֪�ν� 
  if( AF_DataRequest( &broadcast_DstAddr, (endPointDesc_t *)&epDesc,
                 LOCATION_ULTRA_BLORDCAST, 2,
                 theMessageData, &transId,
                 0, 1        //AF_DISCV_ROUTE AF_DEFAULT_RADIUS    //����Ϊһ��,ֱ�ӽ��й㲥
                 ) == afStatus_SUCCESS)
  {
    P1_1 = ~P1_1;
  }  
  
  uc_mobileID++;    //���ƶ��ڵ��ID���Լӣ�������һ���ƶ��ڵ��ͬ���ź�
}


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
#include "mac_mcu.h"

/*********************************************************************
 * CONSTANTS
 */
#define SEQUENCE                200  //���кţ������ж��Ƿ��յ�����ͬһ������

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
byte Sink_TaskID;
uint8 transId;
static byte transId;

unsigned char uc_sequence = 100;        //��Ϊ������100-200���ͽ�β��0x0a�ֿ�
unsigned char sink_broadcast_period = 180; //�㲥����һ��Ϊ180ms����һ��,��λΪms,4���ƶ��ڵ�ļ����˰�

/*********************************************************************
 * FUNCATION
*/
void initP1();
static void processMSGCmd( afIncomingMSGPacket_t *pkt );
void getBasicValue();
void setBasicValue(afIncomingMSGPacket_t *pkt);
void successResponse(uint8 result);
void startBroadcast();

/*********************************************************************/
 // LOCAL VARIABLES
 
static const cId_t Sink_InputClusterList[] =
{
  CID_C2A_GET_BASIC_VALUE,
  CID_C2A_SET_BASIC_VALUE
};

static const cId_t Sink_OutputClusterList[] =
{
  CID_S2MR_BROADCAST,
  CID_A2C_RP_BASIC_VALUE,
  CID_A2C_SUCCESS_RESPONSE
};

static const SimpleDescriptionFormat_t Sink_SimpleDesc =
{
  LOCATION_SINK_ENDPOINT,
  LOCATION_PROFID,
  LOCATION_SINK_DEVICE_ID,
  LOCATION_DEVICE_VERSION,
  LOCATION_FLAGS,
  sizeof(Sink_InputClusterList),
  (cId_t*)Sink_InputClusterList,
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
        case AF_INCOMING_MSG_CMD:
        {
          processMSGCmd( MSGpkt );
          break;
        }
  
        case ZDO_STATE_CHANGE:
        {
          //��ʼ��ʱ���������ݷ��͸�Э����   
          getBasicValue();
          osal_start_timerEx( Sink_TaskID, SINK_BROADCAST_EVT, sink_broadcast_period );  //���ù㲥��ʱ                                      
          break;
        }
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
    osal_start_timerEx( Sink_TaskID, SINK_BROADCAST_EVT, sink_broadcast_period );
      
    // return unprocessed events
    return ( events ^ SINK_BROADCAST_EVT );
  }
  
  // Discard unknown events.
  return 0;  
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
  unsigned char theMessageData[S2C_RP_BASIC_VALUE_LENGTH];     
  theMessageData[S2C_RP_BASIC_VALUE_NODE_TYPE] = NT_SINK_NODE;
  theMessageData[S2C_RP_BASIC_VALUE_BROAD_CYC] = sink_broadcast_period;
  
  afAddrType_t coorAddr;
  coorAddr.addrMode = (afAddrMode_t)Addr16Bit; //��������
  coorAddr.endPoint = LOCATION_DONGLE_ENDPOINT; //Ŀ�Ķ˿ں�
  coorAddr.addr.shortAddr = 0x0000;            //Э���������ַ
  AF_DataRequest( &coorAddr, (endPointDesc_t *)&epDesc,
                           CID_A2C_RP_BASIC_VALUE, S2C_RP_BASIC_VALUE_LENGTH,
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
  //��Э�������͹���������д��sink�ڵ���
  sink_broadcast_period = pkt->cmd.Data[C2S_SET_BASIC_VALUE_BROAD_CYC];
  
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
  theMessageData[A2C_SUCCESS_RESPONSE_NODE_TYPE] = NT_SINK_NODE;
  theMessageData[A2C_SUCCESS_RESPONSE_NODE_ID] = 1;             //sink�ڵ�ֻ��1�����ʽڵ�Ϊ1
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
  if (uc_sequence < SEQUENCE)     //���÷������к�
  {
    uc_sequence++;
  }
  else
  {
    uc_sequence = 100;            
  }
    
  //�����Ų�
  unsigned char theMessageData[1];     //��������Ϊ���к� �ƶ��ڵ��
  theMessageData[0] = uc_sequence;
  afAddrType_t broadcast_DstAddr;
  broadcast_DstAddr.addrMode = (afAddrMode_t)AddrBroadcast;
  broadcast_DstAddr.endPoint = LOCATION_REFER_ENDPOINT;           //��ʱδ�����ι㲥���ֶ˿ڵ����⣬ֻ�ܽ���ͬ���͵Ľڵ��ENDPOINT����Ϊһ����
  broadcast_DstAddr.addr.shortAddr = 0xFFFF;  //��ʾ���ݰ����������е����нڵ�㲥������0xFFFC��·�������ղ�������֪�ν� 
  if( AF_DataRequest( &broadcast_DstAddr, (endPointDesc_t *)&epDesc,
                 CID_S2MR_BROADCAST, 1,
                 theMessageData, &transId,
                 0, 1        //AF_DISCV_ROUTE AF_DEFAULT_RADIUS    //����Ϊһ��,ֱ�ӽ��й㲥
                 ) == afStatus_SUCCESS)
  {
    P1_1 = ~P1_1;             //�ı�һ��LED_R��״̬����ʾ������㲥�ź�
  }  
}


/*********************************************************************
  Filename:       TestApp.c
  Revised:        $Date: 2007-03-26 11:53:55 -0700 (Mon, 26 Mar 2007) $
  Revision:       $Revision: 13853 $

  Description: This application resides in a coor and enables a PC GUI (or
    other application) to send and recieve location messages.


  Copyright (c) 2007 by Texas Instruments, Inc.
  All Rights Reserved.  Permission to use, reproduce, copy, prepare
  derivative works, modify, distribute, perform, display or sell this
  software and/or its documentation for any purpose is prohibited
  without the express written consent of Texas Instruments, Inc.
*********************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "OSAL.h"
#include "MT.h"
#include "MT_APP.h"
#include "AF.h"
#include "ZDApp.h"

#include "OnBoard.h"
#include "hal_key.h"
#if defined ( LCD_SUPPORTED )
#include "hal_lcd.h"
#include "hal_led.h"//����+
#include "mt_uart.h"//����+��For SPI
#endif

#include "LocationProfile.h"
#include "TestApp.h"


#if !defined ( MT_TASK )
  #error      // No need for this module if MT_TASK isn't defined
#endif

/*********************************************************************
 * MACROS
 */

#define MAX_DATA_LENGTH       16 //���������4*4 �ο��ڵ��(1) + �������ݣ�3�� �ܹ���4���ڵ� 
#define TOTAL_DATA_LENGTH     16 //MSG_TYPE + SEQ_NO + MOB_ID + 12 + '\n'
#define REFER_NODES_NUMBERS    4 //��ʾ�������вο��ڵ�ĸ���
#define MOBILE_NODES_NUMBERS   4 //��ʾ���������ƶ��ڵ�ĸ���
/*********************************************************************
 * GLOBAL VARIABLES
 */

uint8 LocationDongle_TaskID;
static byte transId;

unsigned int lastSeqNo = 0;
unsigned int thisSeqNo = 0;
unsigned char delayTimes = 1;                                           //��¼�ӳٷ��͵Ĵ�����һ����Ҫ�ӳٷ���3��
uint32 referDataBuf[MOBILE_NODES_NUMBERS+1][REFER_NODES_NUMBERS+1];     //5*5�ľ������е�һ�������ж��Ƿ��Ѿ�����
uint8 buf[MOBILE_NODES_NUMBERS][TOTAL_DATA_LENGTH];                     //����ķ�������

unsigned char delay_send_times = 10;                                    //��ʾÿ�������ӳٷ��͸���λ����ʱ����

unsigned char sinkNetAddr[2] = {0,0};                                               //�洢sink�ڵ�������ַ
unsigned char mobileNetAddr[MOBILE_NODES_NUMBERS][2] = {{0,0},{0,0},{0,0},{0,0}};   //�洢mobile�ڵ�������ַ
unsigned char referNetAddr[REFER_NODES_NUMBERS][2] = {{0,0},{0,0},{0,0},{0,0}};     //�洢refer�ڵ�������ַ

/*********************************************************************
 * CONSTANTS
 */

static const cId_t LocationDongle_InputClusterList[] =
{  
  CID_A2C_RP_BASIC_VALUE,
  CID_A2C_SUCCESS_RESPONSE,
  CID_M2C_TEMPERATURE,
  CID_M2C_REQ_POSITION, 
  CID_R2C_DIFF_TIME  
};


static const cId_t LocationDongle_OutputClusterList[] =
{
  CID_C2A_GET_BASIC_VALUE,
  CID_C2A_SET_BASIC_VALUE,
  CID_C2M_RP_POSITION,
  CID_C2M_SET_JUDGE
};


static const SimpleDescriptionFormat_t LocationDongle_SimpleDesc =
{
  LOCATION_DONGLE_ENDPOINT,
  LOCATION_PROFID,
  LOCATION_DONGLE_DEVICE_ID,
  LOCATION_DEVICE_VERSION,
  LOCATION_FLAGS,
  sizeof(LocationDongle_InputClusterList),
  (cId_t *)LocationDongle_InputClusterList,
  sizeof(LocationDongle_OutputClusterList),
  (cId_t *)LocationDongle_OutputClusterList
};

static const endPointDesc_t epDesc =
{
  LOCATION_DONGLE_ENDPOINT,
  &LocationDongle_TaskID,
  (SimpleDescriptionFormat_t *)&LocationDongle_SimpleDesc,
  noLatencyReqs
};


/*********************************************************************
 * LOCAL VARIABLES
 */

uint8 LocationDongle_TransID;  // This is the unique message ID (counter)

/*********************************************************************
 * LOCAL FUNCTIONS
 */

void LocationDongle_Init( uint8 task_id );
uint16 LocationDongle_ProcessEvent( uint8 task_id, UINT16 events );
void LocationDongle_ProcessMSGCmd( afIncomingMSGPacket_t *pckt );
void LocationDongle_MTMsg( uint8 len, uint8 *msg ); 
void SPIMgr_ProcessZToolData( uint8 port,uint8 event);

void successResponse(uint8 nodeType, uint8 nodeID, uint8 result);
void getBasicValue(uint8 nodeType, uint8 nodeID, uint8 netAddrHi, uint8 netAddrLo, byte endPoint);
/*********************************************************************
 * @fn      LocationDongle_Init
 *
 * @brief   Initialization function for the Generic App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notificaiton ... ).
 *
 * @param   task_id - the ID assigned by OSAL.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */
void LocationDongle_Init( uint8 task_id )
{
  halUARTCfg_t uartConfig;
  
  LocationDongle_TaskID = task_id;
  LocationDongle_TransID = 0;

  //����+��
  uartConfig.configured           = TRUE;              // 2x30 don't care - see uart driver.
  uartConfig.baudRate             = HAL_UART_BR_19200;
  uartConfig.flowControl          = FALSE;
  uartConfig.flowControlThreshold = 64;                // 2x30 don't care - see uart driver.
  uartConfig.rx.maxBufSize        = 128;                // 2x30 don't care - see uart driver.
  uartConfig.tx.maxBufSize        = 128;              // 2x30 don't care - see uart driver.
  uartConfig.idleTimeout          = 6;                  // 2x30 don't care - see uart driver.
  uartConfig.intEnable            = TRUE;              // 2x30 don't care - see uart driver.
  uartConfig.callBackFunc         = SPIMgr_ProcessZToolData;
  HalUARTOpen (HAL_UART_PORT_0, &uartConfig);
  // Register the endpoint/interface description with the AF
  afRegister( (endPointDesc_t *)&epDesc );

  // Register for all key events - This app will handle all key events
  RegisterForKeys( LocationDongle_TaskID );

  // Update the display
  #if defined ( LCD_SUPPORTED )
    HalLcdWriteString( "Location-Dongle", HAL_LCD_LINE_2 );
  #endif
}

/*********************************************************************
 * @fn      LocationDongle_ProcessEvent
 *
 * @brief   Generic Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  none
 */
uint16 LocationDongle_ProcessEvent( uint8 task_id, UINT16 events )
{
  afIncomingMSGPacket_t *MSGpkt;

  if ( events & SYS_EVENT_MSG )
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( LocationDongle_TaskID );
    while ( MSGpkt )
    {
      switch ( MSGpkt->hdr.event )
      {
        case MT_SYS_APP_MSG:  // Z-Architect Messages
          HalLedBlink(HAL_LED_2,1,50,200);
          //LocationDongle_MTMsg( ((mtSysAppMsg_t *)MSGpkt)->appDataLen, ((mtSysAppMsg_t *)MSGpkt)->appData );
          break;
        case AF_INCOMING_MSG_CMD:
          LocationDongle_ProcessMSGCmd( MSGpkt );
          break;
          
        case ZDO_STATE_CHANGE:
          //��ʾ�Լ���NETID
          HalLcdWriteStringValue( "NetID:", NLME_GetShortAddr(), 16, HAL_LCD_LINE_3 );
          break;

        default:
          break;
      }

      osal_msg_deallocate( (uint8 *)MSGpkt );
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( LocationDongle_TaskID );
    }

    // Return unprocessed events
    return ( events ^ SYS_EVENT_MSG );
  }

  // send the data by delaying with 10ms*ID
  if ( events & COOR_DELAYSEND_EVT )    
  {
    //����c���޷���ȡһ�����飬ֻ�ܱ�����ȡ�����������ƶ��ڵ�1��ֵ
    uint8 sendbuf[TOTAL_DATA_LENGTH];
    int temp;
    for(temp=0;temp<TOTAL_DATA_LENGTH;temp++)
    {
      sendbuf[temp] = buf[delayTimes][temp];
    }
    HalUARTWrite(HAL_UART_PORT_0,sendbuf,TOTAL_DATA_LENGTH);
    
    //��ɷ��������
    if(delayTimes == 3)
    {
      //��������������
      int m,n;
      for(m=0;m<MOBILE_NODES_NUMBERS;m++)
      {
        for(n=0;n<TOTAL_DATA_LENGTH;n++)
        {
          buf[m][n] = 0;     
        }
      }
      delayTimes = 1;
    }
    //��δ��ɽڵ㷢�����趨��ʱ��
    else
    {
      delayTimes++;
      osal_start_timerEx( LocationDongle_TaskID, COOR_DELAYSEND_EVT, delay_send_times );
    }
       
    // return unprocessed events
    return ( events ^ COOR_DELAYSEND_EVT );
  }
  
  // Discard unknown events
  return 0;
}


/*********************************************************************
 * @fn      LocationDongle_ProcessMSGCmd
 *
 * @brief   All incoming messages are sent out the serial port
 *          as an MT SYS_APP_MSG.
 *
 * @param   Raw incoming MSG packet structure pointer.
 *
 * @return  none
 */
void LocationDongle_ProcessMSGCmd( afIncomingMSGPacket_t *pkt )
{
  //uint8 buf[TIMEDIFF_MSG_LENGTH];       //���ճ���
  switch(pkt->clusterId)
  {
    //�����ڵ㷢�Ͳ�ѯ������Ӧ��Э����
    case CID_A2C_RP_BASIC_VALUE:
    {
      unsigned char nodeType = pkt->cmd.Data[0];  //��ѯ������Ӧ֡�ĵ�һ���ֽ��ǽڵ�����
      switch(nodeType)
      {
        //��Ӧ�ڵ�Ϊsink�ڵ�
        case NT_SINK_NODE:
        {
          uint8 buf[C2PC_RP_BASIC_VALUE_LENGTH_S];       //���ճ���
          buf[C2PC_RP_BASIC_VALUE_MSG_TYPE_S] = MT_C2PC_RP_BASIC_VALUE;
          buf[C2PC_RP_BASIC_VALUE_NODE_TYPE_S] = pkt->cmd.Data[S2C_RP_BASIC_VALUE_NODE_TYPE];
          buf[C2PC_RP_BASIC_VALUE_BROAD_CYC_S] = pkt->cmd.Data[S2C_RP_BASIC_VALUE_BROAD_CYC];
          buf[C2PC_RP_BASIC_VALUE_END_S] = '\n';
          
          //����ȡ��sink�����ַ�洢��������
          sinkNetAddr[0] = HI_UINT16(pkt->srcAddr.addr.shortAddr);
          sinkNetAddr[1] = LO_UINT16(pkt->srcAddr.addr.shortAddr);
      
          HalUARTWrite(HAL_UART_PORT_0,buf,C2PC_RP_BASIC_VALUE_LENGTH_S);
          break;
        }
        
        //��Ӧ�ڵ�Ϊmobile�ڵ�
        case NT_MOB_NODE:
        {
          uint8 buf[C2PC_RP_BASIC_VALUE_LENGTH_M];       //���ճ���
          buf[C2PC_RP_BASIC_VALUE_MSG_TYPE_M] = MT_C2PC_RP_BASIC_VALUE;
          buf[C2PC_RP_BASIC_VALUE_NODE_TYPE_M] = pkt->cmd.Data[M2C_RP_BASIC_VALUE_NODE_TYPE];
          buf[C2PC_RP_BASIC_VALUE_MOB_ID_M] = pkt->cmd.Data[M2C_RP_BASIC_VALUE_MOB_ID];
          buf[C2PC_RP_BASIC_VALUE_TEAM_ID_M] = pkt->cmd.Data[M2C_RP_BASIC_VALUE_TEAM_ID];
          buf[C2PC_RP_BASIC_VALUE_TEMP_CYC_M] = pkt->cmd.Data[M2C_RP_BASIC_VALUE_TEMP_CYC];
          buf[C2PC_RP_BASIC_VALUE_SEND_DELAY_TIME_M] = pkt->cmd.Data[M2C_RP_BASIC_VALUE_SEND_DELAY_TIME];
          buf[C2PC_RP_BASIC_VALUE_END_M] = '\n';
      
          //����ȡ��mobile�����ַ�洢��������
          mobileNetAddr[(pkt->cmd.Data[M2C_RP_BASIC_VALUE_MOB_ID]-1)][0] = HI_UINT16(pkt->srcAddr.addr.shortAddr);
          mobileNetAddr[(pkt->cmd.Data[M2C_RP_BASIC_VALUE_MOB_ID]-1)][1] = LO_UINT16(pkt->srcAddr.addr.shortAddr);
          
          HalUARTWrite(HAL_UART_PORT_0,buf,C2PC_RP_BASIC_VALUE_LENGTH_M);
          break;
        }
        
        //��Ӧ�ڵ�Ϊrefer�ڵ�
        case NT_REF_NODE:
        {
          uint8 buf[C2PC_RP_BASIC_VALUE_LENGTH_R];       //���ճ���
          buf[C2PC_RP_BASIC_VALUE_MSG_TYPE_R] = MT_C2PC_RP_BASIC_VALUE;
          buf[C2PC_RP_BASIC_VALUE_NODE_TYPE_R] = pkt->cmd.Data[R2C_RP_BASIC_VALUE_NODE_TYPE];
          buf[C2PC_RP_BASIC_VALUE_REF_ID_R] = pkt->cmd.Data[R2C_RP_BASIC_VALUE_REF_ID];
          buf[C2PC_RP_BASIC_VALUE_RECV_TIME_OUT_R] = pkt->cmd.Data[R2C_RP_BASIC_VALUE_RECV_TIME_OUT];
          buf[C2PC_RP_BASIC_VALUE_RECV_DELAY_TIME_R] = pkt->cmd.Data[R2C_RP_BASIC_VALUE_RECV_DELAY_TIME];
          buf[C2PC_RP_BASIC_VALUE_END_R] = '\n';
      
          //����ȡ��refer�����ַ�洢��������
          referNetAddr[(pkt->cmd.Data[R2C_RP_BASIC_VALUE_REF_ID]-1)][0] = HI_UINT16(pkt->srcAddr.addr.shortAddr);
          referNetAddr[(pkt->cmd.Data[R2C_RP_BASIC_VALUE_REF_ID]-1)][1] = LO_UINT16(pkt->srcAddr.addr.shortAddr);
          
          HalUARTWrite(HAL_UART_PORT_0,buf,C2PC_RP_BASIC_VALUE_LENGTH_R);
          break;
        }
        
        default:
        break;
      } 
      break; 
    }
    
    //�����ڵ㷢�����óɹ���Ӧ��Э����,Э���������ݷ���������
    case CID_A2C_SUCCESS_RESPONSE:
    {
      successResponse(pkt->cmd.Data[A2C_SUCCESS_RESPONSE_NODE_TYPE],
                      pkt->cmd.Data[A2C_SUCCESS_RESPONSE_NODE_ID],
                      pkt->cmd.Data[A2C_SUCCESS_RESPONSE_RESULT]);
      break; 
    }
    
    //�ƶ��ڵ㷢���¶���Ϣ��Э������Э���������ݷ���������  
    case CID_M2C_TEMPERATURE:
    {
      uint8 buf[C2PC_TEMPERATURE_DATA_LENGTH];       //���ճ���
      buf[C2PC_TEMPERATURE_DATA_MSG_TYPE] = MT_C2PC_TEMPERATURE_DATA;
      buf[C2PC_TEMPERATURE_DATA_HIGH] = pkt->cmd.Data[M2C_TEMPERATURE_DATA_HIGH];
      buf[C2PC_TEMPERATURE_DATA_LOW] = pkt->cmd.Data[M2C_TEMPERATURE_DATA_LOW];
      buf[C2PC_TEMPERATURE_DATA_END] = '\n';
  
      HalUARTWrite(HAL_UART_PORT_0,buf,C2PC_TEMPERATURE_DATA_LENGTH);
      break; 
    }
    
    //�ƶ��ڵ㷢��λ��������Ϣ��Э������Э���������ݷ���������
    case CID_M2C_REQ_POSITION:
    {
      uint8 buf[C2PC_REQ_POSITION_LENGTH];       //���ճ���
      buf[C2PC_REQ_POSITION_MSG_TYPE] = MT_C2PC_REQ_POSITION;
      buf[C2PC_REQ_POSITION_REQ_MOB_ID] = pkt->cmd.Data[M2C_REQ_POSITION_REQ_MOB_ID];
      buf[C2PC_REQ_POSITION_GET_MOB_ID] = pkt->cmd.Data[M2C_REQ_POSITION_GET_MOB_ID];
      buf[C2PC_REQ_POSITION_END] = '\n';
  
      HalUARTWrite(HAL_UART_PORT_0,buf,C2PC_REQ_POSITION_LENGTH);
      break; 
    }
    
    //�ο��ڵ㷢��ʱ�����Ϣ��Э������Э������������ݷ���������  
    case CID_R2C_DIFF_TIME:
    {
      thisSeqNo  = pkt->cmd.Data[R2C_DIFF_TIME_SEQ];
      
      //�������кź��ƶ��ڵ�ID���Ƿ���ͬ�����䣬�ƶ��ڵ�����кŲ�ͬ������ݽ�������
      if (thisSeqNo != lastSeqNo)
      {
        //����ȡ��ֵ�Ž���������
        int mobileNo,refNo;
        for(mobileNo=1;mobileNo<=MOBILE_NODES_NUMBERS;mobileNo++)
        {
          buf[mobileNo-1][C2PC_DIFF_TIME_MSG_TYPE] = MT_C2PC_DIFF_TIME;
          buf[mobileNo-1][C2PC_DIFF_TIME_SEQ] = lastSeqNo;
          buf[mobileNo-1][C2PC_DIFF_TIME_MOBID]  = mobileNo;
          for(refNo=1;refNo<=REFER_NODES_NUMBERS;refNo++)
          {
            buf[mobileNo-1][refNo*3] = ((uint8 *)&referDataBuf[mobileNo][refNo])[2];        //ʱ���߰�λ 
            buf[mobileNo-1][refNo*3+1] = ((uint8 *)&referDataBuf[mobileNo][refNo])[1];      //ʱ����а�λ
            buf[mobileNo-1][refNo*3+2] = ((uint8 *)&referDataBuf[mobileNo][refNo])[0];      //ʱ���Ͱ�λ
          } 
          buf[mobileNo-1][C2PC_DIFF_TIME_END]  = '\n';
        }
        
        //����c���޷���ȡһ�����飬ֻ�ܱ�����ȡ��ֱ�ӷ����ƶ��ڵ�1��ֵ
        uint8 sendbuf[TOTAL_DATA_LENGTH];
        int temp;
        for(temp=0;temp<TOTAL_DATA_LENGTH;temp++)
        {
          sendbuf[temp] = buf[0][temp];
        }
        HalUARTWrite(HAL_UART_PORT_0,sendbuf,TOTAL_DATA_LENGTH); 
        //���ö�ʱ�����ӳٷ��͸���λ������ֹ���ڴ������
        osal_start_timerEx( LocationDongle_TaskID, COOR_DELAYSEND_EVT, delay_send_times );
        
        //��շ��仺������
        int m,n;
        for (m=0;m<=MOBILE_NODES_NUMBERS;m++)
        {
          for (n=0;n<=REFER_NODES_NUMBERS;n++)
          {
            referDataBuf[m][n] = 0;
          }
        }
              
        //����λ�ȡ��ͬ���кŵĴ浽���仺��������
        referDataBuf[0][pkt->cmd.Data[R2C_DIFF_TIME_REFID]] = 1;
        //������uint8�����ݺϳ�һ��uint32�Ĳ�����������,ֱ��<<16λ�ᱨ���棬Ҫ����ȡ��ֵ����Ϊ32λ�����������ֲ����������  
        for(mobileNo=1;mobileNo<=MOBILE_NODES_NUMBERS;mobileNo++)
        {
          referDataBuf[mobileNo][pkt->cmd.Data[R2C_DIFF_TIME_REFID]] = \
            (unsigned long)(pkt->cmd.Data[3*mobileNo-1])<<16 |      \
            (unsigned long)(pkt->cmd.Data[3*mobileNo])<<8 |      \
            (pkt->cmd.Data[3*mobileNo+1]);
        }
      }
      else
      {
        //��Ϊ������������󣬲ο��ڵ���ܷ��Ͷ�����ͬ��������Ϣ��Э��������Ҫ��ȥ��ͬ������
        bool b_exist = false;
        if(referDataBuf[0][pkt->cmd.Data[R2C_DIFF_TIME_REFID]] == 1)
        {
          b_exist = true;
        }
        
        if (b_exist == false)
        {
          referDataBuf[0][pkt->cmd.Data[R2C_DIFF_TIME_REFID]] = 1;
          //������uint8�����ݺϳ�һ��uint32�Ĳ�����������
          int mobileNo;
          for(mobileNo=1;mobileNo<=MOBILE_NODES_NUMBERS;mobileNo++)
          {
            referDataBuf[mobileNo][pkt->cmd.Data[R2C_DIFF_TIME_REFID]] = \
              (unsigned long)(pkt->cmd.Data[3*mobileNo-1])<<16|      \
              (unsigned long)(pkt->cmd.Data[3*mobileNo])<<8 |      \
              (pkt->cmd.Data[3*mobileNo+1]);
          }
        }
      }
      
      lastSeqNo = thisSeqNo;     
      break;  
    } 
       
    default:
      break;
  }
}


/*********************************************************************
 * @fn      LocationDongle_MTMsg
 *
 * @brief   All outcoming messages are sent from the serial port
 *          
 *
 * @param   Raw incoming MSG packet structure pointer.
 *
 * @return  none
 */
void LocationDongle_MTMsg( uint8 len, uint8 *msg )
{
  unsigned char msgType = msg[0];     //���д���λ��ͨ�����ڷ���Э��������Ϣ��һ���ֽڶ�������֡���� 
  switch(msgType)
  {
    //��λ����Э����������ѯ�ڵ�������Ϣ
    case MT_PC2C_GET_BASIC_VALUE:
    {
      unsigned char nodeType = msg[PC2C_GET_BASIC_VALUE_NODE_TYPE];
      switch(nodeType)
      {
        //��ʾ��ѯ����coor�ڵ㣬��ֱ�ӷ�����Ӧ��Ϣ
        case NT_COOR_NODE:
        {
          //�������ò�������λ��
          uint8 buf[C2PC_RP_BASIC_VALUE_LENGTH_C];       //���ճ���
          buf[C2PC_RP_BASIC_VALUE_MSG_TYPE_C] = MT_C2PC_RP_BASIC_VALUE;
          buf[C2PC_RP_BASIC_VALUE_NODE_TYPE_C] = NT_COOR_NODE;
          buf[C2PC_RP_BASIC_VALUE_DELAY_TIME_C] = delay_send_times;        
          buf[C2PC_RP_BASIC_VALUE_END_C] = '\n';
          
          //�յ�������Ϣ��˸�̵�3�Σ�ÿ��300ms
          HalLedBlink(HAL_LED_1, 3, 50, 300);
      
          HalUARTWrite(HAL_UART_PORT_0,buf,C2PC_RP_BASIC_VALUE_LENGTH_C);
          break;
        }
         
        //��ʾ��ѯ����sink�ڵ㣬����sink�ڵ㷢�Ͳ�ѯ��Ϣ
        case NT_SINK_NODE:
        {
          //sink�ڵ�ֻ��1���ʱ��Ϊ1
          getBasicValue(NT_SINK_NODE,1,sinkNetAddr[0],sinkNetAddr[1],LOCATION_SINK_ENDPOINT);
          break;
        }
        
        //��ʾ��ѯ����mobile�ڵ㣬����mobile�ڵ㷢�Ͳ�ѯ��Ϣ
        case NT_MOB_NODE:
        {
          getBasicValue(NT_MOB_NODE,msg[PC2C_GET_BASIC_VALUE_NODE_ID],
                        mobileNetAddr[(msg[PC2C_GET_BASIC_VALUE_NODE_ID]-1)][0],
                        mobileNetAddr[(msg[PC2C_GET_BASIC_VALUE_NODE_ID]-1)][1],
                        LOCATION_MOBILE_ENDPOINT);
          break;
        }
        
        //��ʾ��ѯ����refer�ڵ㣬����refer�ڵ㷢�Ͳ�ѯ��Ϣ
        case NT_REF_NODE:
        {
          getBasicValue(NT_REF_NODE,msg[PC2C_GET_BASIC_VALUE_NODE_ID],
                        referNetAddr[(msg[PC2C_GET_BASIC_VALUE_NODE_ID]-1)][0],
                        referNetAddr[(msg[PC2C_GET_BASIC_VALUE_NODE_ID]-1)][1],
                        LOCATION_REFER_ENDPOINT);
          break;
        }
        
        default:
        break;
      }
      break;
    }
    
    //��λ����Э�����������ýڵ�������Ϣ
    case MT_PC2C_SET_BASIC_VALUE:
    {
      unsigned char nodeType = msg[1];   //�������ýڵ�������Ϣ�ĵڶ����ֽڶ��ǽڵ�����
      switch(nodeType)
      {
        case NT_COOR_NODE:
        {
          //��������Ϣд��coor�ڵ�
          delay_send_times = msg[PC2C_SET_BASIC_VALUE_DELAY_TIME_C];
          
          //�������óɹ���Ӧ����λ��
          uint8 buf[C2PC_SUCCESS_RESPONSE_LENGTH];       //���ճ���
          buf[C2PC_SUCCESS_RESPONSE_MSG_TYPE] = MT_C2PC_SUCCESS_RESPONSE;
          buf[C2PC_SUCCESS_RESPONSE_NODE_TYPE] = NT_COOR_NODE;
          buf[C2PC_SUCCESS_RESPONSE_NODE_ID] = 1;         //Э����ֻ��һ������д1
          buf[C2PC_SUCCESS_RESPONSE_RESULT] = 1;          //�����1����ʾ�ɹ���0��ʾʧ��
          buf[C2PC_SUCCESS_RESPONSE_END] = '\n';
      
          //�յ�������Ϣ��˸�̵�3�Σ�ÿ��300ms
          HalLedBlink(HAL_LED_1, 3, 50, 300);
          
          HalUARTWrite(HAL_UART_PORT_0,buf,C2PC_SUCCESS_RESPONSE_LENGTH);
          break;
        }
        
        case NT_SINK_NODE:
        {
          //�����ýڵ�������Ϣ������sink�ڵ�
          unsigned char theMessageData[C2S_SET_BASIC_VALUE_LENGTH];     
          theMessageData[C2S_SET_BASIC_VALUE_BROAD_CYC] = msg[PC2C_SET_BASIC_VALUE_BROAD_CYC_S];
          
          afAddrType_t sinkAddr;
          sinkAddr.addrMode = (afAddrMode_t)Addr16Bit; //��������
          sinkAddr.endPoint = LOCATION_SINK_ENDPOINT; //Ŀ�Ķ˿ں�
          sinkAddr.addr.shortAddr = BUILD_UINT16(sinkNetAddr[1],sinkNetAddr[0]);
          AF_DataRequest( &sinkAddr, (endPointDesc_t *)&epDesc,
                                   CID_C2A_SET_BASIC_VALUE, C2S_SET_BASIC_VALUE_LENGTH,
                                   theMessageData, &transId, AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );    
          break;
        }
        
        case NT_MOB_NODE:
        {
          //�����ýڵ�������Ϣ������mobile�ڵ�
          unsigned char theMessageData[C2M_SET_BASIC_VALUE_LENGTH];     
          theMessageData[C2M_SET_BASIC_VALUE_MOB_ID] = msg[PC2C_SET_BASIC_VALUE_MOB_ID_M];
          theMessageData[C2M_SET_BASIC_VALUE_TEAM_ID] = msg[PC2C_SET_BASIC_VALUE_TEAM_ID_M];
          theMessageData[C2M_SET_BASIC_VALUE_TEMP_CYC] = msg[PC2C_SET_BASIC_VALUE_TEMP_CYC_M];
          theMessageData[C2M_SET_BASIC_VALUE_SEND_DELAY_TIME] = msg[PC2C_SET_BASIC_VALUE_SEND_DELAY_TIME_M];
          
          afAddrType_t mobileAddr;
          mobileAddr.addrMode = (afAddrMode_t)Addr16Bit; //��������
          mobileAddr.endPoint = LOCATION_MOBILE_ENDPOINT; //Ŀ�Ķ˿ں�
          mobileAddr.addr.shortAddr = BUILD_UINT16(mobileNetAddr[(msg[PC2C_GET_BASIC_VALUE_NODE_ID]-1)][1],
                                                   mobileNetAddr[(msg[PC2C_GET_BASIC_VALUE_NODE_ID]-1)][0]);
          AF_DataRequest( &mobileAddr, (endPointDesc_t *)&epDesc,
                                   CID_C2A_SET_BASIC_VALUE, C2M_SET_BASIC_VALUE_LENGTH,
                                   theMessageData, &transId, AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );
          break;
        }
        
        case NT_REF_NODE:
        {
          //�����ýڵ�������Ϣ������refer�ڵ�
          unsigned char theMessageData[C2R_SET_BASIC_VALUE_LENGTH];     
          theMessageData[C2R_SET_BASIC_VALUE_REF_ID] = msg[PC2C_SET_BASIC_VALUE_REF_ID_R];
          theMessageData[C2R_SET_BASIC_VALUE_RECV_TIME_OUT] = msg[PC2C_SET_BASIC_VALUE_RECV_TIME_OUT_R];
          theMessageData[C2R_SET_BASIC_VALUE_RECV_DELAY_TIME] = msg[PC2C_SET_BASIC_VALUE_RECV_DELAY_TIME_R];
          
          afAddrType_t referAddr;
          referAddr.addrMode = (afAddrMode_t)Addr16Bit; //��������
          referAddr.endPoint = LOCATION_REFER_ENDPOINT; //Ŀ�Ķ˿ں�
          referAddr.addr.shortAddr = BUILD_UINT16(referNetAddr[(msg[PC2C_GET_BASIC_VALUE_NODE_ID]-1)][1],
                                                  referNetAddr[(msg[PC2C_GET_BASIC_VALUE_NODE_ID]-1)][0]);
          AF_DataRequest( &referAddr, (endPointDesc_t *)&epDesc,
                                   CID_C2A_SET_BASIC_VALUE, C2R_SET_BASIC_VALUE_LENGTH,
                                   theMessageData, &transId, AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );
          break;
        }
        
        default:
        break;

      }
      break;
    }
    
    //��λ����Э����������Ӧ�ƶ��ڵ�λ��������Ϣ
    case MT_PC2C_RP_POSITION:
    {
      //����Ӧ��Ϣ����������ƶ��ڵ�
      unsigned char theMessageData[C2M_RP_POSITION_LENGTH];     
      theMessageData[C2M_RP_POSITION_GET_MOB_ID] = msg[PC2C_RP_POSITION_GET_MOB_ID];
      theMessageData[C2M_RP_POSITION_X_HI] = msg[PC2C_RP_POSITION_X_HI];
      theMessageData[C2M_RP_POSITION_X_LO] = msg[PC2C_RP_POSITION_X_LO];
      theMessageData[C2M_RP_POSITION_Y_HI] = msg[PC2C_RP_POSITION_Y_HI];
      theMessageData[C2M_RP_POSITION_Y_LO] = msg[PC2C_RP_POSITION_Y_LO];
      
      afAddrType_t mobileAddr;
      mobileAddr.addrMode = (afAddrMode_t)Addr16Bit; //��������
      mobileAddr.endPoint = LOCATION_MOBILE_ENDPOINT; //Ŀ�Ķ˿ں�
      mobileAddr.addr.shortAddr = BUILD_UINT16(mobileNetAddr[(msg[PC2C_RP_POSITION_REQ_MOB_ID]-1)][1],
                                               mobileNetAddr[(msg[PC2C_RP_POSITION_REQ_MOB_ID]-1)][0]);
      AF_DataRequest( &mobileAddr, (endPointDesc_t *)&epDesc,
                               CID_C2M_RP_POSITION, C2M_RP_POSITION_LENGTH,
                               theMessageData, &transId, AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );     
      break;
    }
    
    //��λ����Э����������ʼ��Ϣ
    case MT_PC2C_SET_JUDGE:
    {
      //����Ӧ��Ϣ����������ƶ��ڵ�
      unsigned char theMessageData[C2M_SET_JUDGE_LENGTH];     
      theMessageData[C2M_SET_JUDGE_ACTION] = msg[PC2C_SET_JUDGE_ACTION];
      
      afAddrType_t mobileAddr;
      mobileAddr.addrMode = (afAddrMode_t)Addr16Bit; //��������
      mobileAddr.endPoint = LOCATION_MOBILE_ENDPOINT; //Ŀ�Ķ˿ں�
      mobileAddr.addr.shortAddr = BUILD_UINT16(mobileNetAddr[(msg[PC2C_SET_JUDGE_MOB_ID]-1)][1],
                                               mobileNetAddr[(msg[PC2C_SET_JUDGE_MOB_ID]-1)][0]);
      AF_DataRequest( &mobileAddr, (endPointDesc_t *)&epDesc,
                               CID_C2M_SET_JUDGE, C2M_SET_JUDGE_LENGTH,
                               theMessageData, &transId, AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );
      break;
    }  
    
    default:
    break;
  }
}

/*********************************************************************
 * @fn      SPIMgr_ProcessZToolData
 *
 * @brief   receive and send by serial
 *
 * @param   none
 *
 * @return  none
 */
void SPIMgr_ProcessZToolData ( uint8 port, uint8 event )
{
  uint16 rxlen;//�������ݳ���
  uint8* psbuf;//���մ洢��ָ��
     
  if (event & (HAL_UART_RX_FULL | HAL_UART_RX_ABOUT_FULL | HAL_UART_RX_TIMEOUT))
  {
    rxlen = Hal_UART_RxBufLen( HAL_UART_PORT_0 );     //��ý��ջ��������ݳ���
    psbuf = osal_mem_alloc(rxlen);                    //����rxlen�����ڴ沢��ָ�븳��psbuf
    HalUARTRead (HAL_UART_PORT_0, psbuf, rxlen);      //�����ջ��������ݵ���
    LocationDongle_MTMsg(rxlen,psbuf);                //�Խ��յ������ݽ��д���
    osal_mem_free( psbuf );                           //�ͷŷ�����ڴ�
  }
}


/*********************************************************************
 * @fn      successResponse
 *
 * @brief   send seccess response to the PC
 *
 * @param   none
 *
 * @return  none
 */
void successResponse(uint8 nodeType, uint8 nodeID, uint8 result)
{
  uint8 buf[C2PC_SUCCESS_RESPONSE_LENGTH];       //���ճ���
  buf[C2PC_SUCCESS_RESPONSE_MSG_TYPE] = MT_C2PC_SUCCESS_RESPONSE;
  buf[C2PC_SUCCESS_RESPONSE_NODE_TYPE] = nodeType;
  buf[C2PC_SUCCESS_RESPONSE_NODE_ID] = nodeID;
  buf[C2PC_SUCCESS_RESPONSE_RESULT] = result;
  buf[C2PC_SUCCESS_RESPONSE_END] = '\n';

  HalUARTWrite(HAL_UART_PORT_0,buf,C2PC_SUCCESS_RESPONSE_LENGTH);
}


/*********************************************************************
 * @fn      getBasicValue
 *
 * @brief   PC send to C for getting the basic value of any other type of nodes
 *
 * @param   none
 *
 * @return  none
 */
void getBasicValue(uint8 nodeType, uint8 nodeID, uint8 netAddrHi, uint8 netAddrLo, byte endPoint)
{
  //�жϽڵ��Ƿ��Ѿ������ˣ������˲��ܽ��в�ѯ
  if(netAddrHi != 0 || netAddrLo != 0)
  {
    //����ѯ�ڵ�������Ϣ������sink�ڵ�            
    afAddrType_t Addr;
    Addr.addrMode = (afAddrMode_t)Addr16Bit;  //��������
    Addr.endPoint = endPoint;                 //Ŀ�Ķ˿ں�
    Addr.addr.shortAddr = BUILD_UINT16(netAddrLo,netAddrHi);    //���Ⱥ�����Ϊ0��ʾ����Ϣ
    AF_DataRequest( &Addr, (endPointDesc_t *)&epDesc,
                    CID_C2A_GET_BASIC_VALUE, 0,                          
                    0, &transId, AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );
  }
  //�����ʾδ��������ʾ��ѯ����
  else
  {
    successResponse(nodeType,nodeID,0);  //0��ʾʧ��                    
  }
}

/*********************************************************************
*********************************************************************/

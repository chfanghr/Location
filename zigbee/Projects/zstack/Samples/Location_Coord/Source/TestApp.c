/*********************************************************************
  Filename:       LocationDongle.c
  Revised:        $Date: 2007-03-26 11:53:55 -0700 (Mon, 26 Mar 2007) $
  Revision:       $Revision: 13853 $

  Description: This application resides in a dongle and enables a PC GUI (or
    other application) to send and recieve OTA location messages.

          Key control:
            SW1:  N/A
            SW2:  N/A
            SW3:  N/A
            SW4:  N/A

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
//#define RECV_TIMES              4   //����Ѹó���д��LocationProfile�У�Ҫ��sink�ڵ㷢��һ��
//#define TIMEDIFF_MSG_LENGTH   7  //TIMEDIFF_MSG_LENGTH = 3 + 3 + 1;    ʱ���ĳ��� ��Ϊ����ֻ��Ϊ����
//#define TEMP_MSG_LENGTH       3   //��Ϣ���� �¶�ֵ(��Ҫ����100) \n
#define MAX_DATA_LENGTH       16 //���������4*4 �ο��ڵ��(1) + �������ݣ�3�� �ܹ���4���ڵ� 
#define TOTAL_DATA_LENGTH     20 //MSG_TYPE + SEQ_NO + MOB_ID + 16 + '\n'
#define REFER_NODES_NUMBERS           4  //��ʾ�������вο��ڵ�ĸ���

/*********************************************************************
 * GLOBAL VARIABLES
 */

uint8 LocationDongle_TaskID;
unsigned int lastSeqNo = 0;
unsigned int thisSeqNo = 0;
unsigned int lastMobID = 0;
unsigned int thisMobID = 0;
unsigned int sameSeqTimes = 0;
uint8 referDataBuf[MAX_DATA_LENGTH];
bool b_existList[4];



/*********************************************************************
 * CONSTANTS
 */



static const cId_t LocationDongle_InputClusterList[] =
{
  //Location_MOBILE_FIND_RSP,
  //LOCATION_MOBILE_CONFIG_RSP,
  //LOCATION_REFER_CONFIG_RSP,
  LOCATION_REFER_DISTANCE_RSP,           //pel+
  LOCATION_COOR_TEMPURATE,
  //LOCATION_COOR_ERROR
    
  //Location_REFER_TEMPERATURE_RSP
};

/*
static const cId_t LocationDongle_OutputClusterList[] =
{
  //Location_MOBILE_FIND_REQ,
  //LOCATION_MOBILE_CONFIG_REQ,
  //LOCATION_REFER_CONFIG_REQ
  //Location_REFER_TEMPERATURE_REQ
};
*/

static const SimpleDescriptionFormat_t LocationDongle_SimpleDesc =
{
  LOCATION_DONGLE_ENDPOINT,
  LOCATION_PROFID,
  LOCATION_DONGLE_DEVICE_ID,
  LOCATION_DEVICE_VERSION,
  LOCATION_FLAGS,

  sizeof(LocationDongle_InputClusterList),
  (cId_t *)LocationDongle_InputClusterList,
  
  0,
  (cId_t *)NULL
  /*sizeof(LocationDongle_OutputClusterList),
  (cId_t *)LocationDongle_OutputClusterList*/
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
UINT16 LocationDongle_ProcessEvent( uint8 task_id, UINT16 events );
void LocationDongle_ProcessMSGCmd( afIncomingMSGPacket_t *pckt );
//void LocationDongle_MTMsg( uint8 len, uint8 *msg );  ���ڽ׶�δʹ��Э�����������
void SPIMgr_ProcessZToolData( uint8 port,uint8 event);
//void LocationHandleKeys( uint8 keys );

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
UINT16 LocationDongle_ProcessEvent( uint8 task_id, UINT16 events )
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
        /*
        case KEY_CHANGE:
          LocationHandleKeys(((keyChange_t *)MSGpkt)->keys );
          break;
        */
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

  // Discard unknown events
  return 0;
}

/*********************************************************************
 * Event Generation Functions
 */
/*********************************************************************
 * @fn      keyChange
 *
 * @brief   Handles all key events for this device.
 *
 * @param   keys - bit field for key events. Valid entries:
 *                 EVAL_SW4
 *                 EVAL_SW3
 *                 EVAL_SW2
 *                 EVAL_SW1
 *
 * @return  none
 */
/*
void LocationHandleKeys( uint8 keys )
{
  if ( keys & HAL_KEY_SW_1 )
  {
#if defined ( ZBIT )
    osal_start_timerEx(LocationDongle_TaskID, DONGLE_TIMER_EVT, DONGLE_TIMER_DLY);
    HalLedBlink(HAL_LED_1,1,50,100);
#endif
  }

  if ( keys & HAL_KEY_SW_2 )
  {
  }

  if ( keys & HAL_KEY_SW_3 )
  {
  }

  if ( keys & HAL_KEY_SW_4 )
  {
  }
}
*/
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
  //pel+ ��Ӳο��ڵ㷢�����λ�ø�Э����  
  case LOCATION_REFER_DISTANCE_RSP:
    thisSeqNo  = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_SEQUENCE];
    thisMobID = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_MOBID];
    
    //�������кź��ƶ��ڵ�ID���Ƿ���ͬ�����䣬�ƶ��ڵ�����кŲ�ͬ������ݽ�������
    if (thisSeqNo != lastSeqNo || thisMobID != lastMobID)
    {
      //��������ݻ��յ�ȫFF��δ�յ���������ʹ��ȫ00���

      //��ʼ�����ж��Ƿ����
      
      
      //unsigned int tempMsgLength;
      //tempMsgLength = 3 + sameSeqTimes * 4;   //MSG_TYPE SEQ_NO 4*DATA '\n'
      uint8 buf[TOTAL_DATA_LENGTH];
      //uint8* buf;
      //buf = osal_mem_alloc(tempMsgLength);        
      buf[TIMEDIFF_MSG_TYPE]  = RP_BLOADCAST_TIME;
      buf[TIMEDIFF_SEQUENCE]  = lastSeqNo;
      buf[TIMEDIFF_MOBID]  = lastMobID;
      int n;
      for(n=0;n<sameSeqTimes;n++)
      {
        buf[4*n+3] = referDataBuf[4*n];    
        buf[4*n+4] = referDataBuf[4*n+1];      
        buf[4*n+5] = referDataBuf[4*n+2];
        buf[4*n+6] = referDataBuf[4*n+3];
      }
      
      //δ�յ������������ݱ��Ϊ0
      if(sameSeqTimes != REFER_NODES_NUMBERS)
      {
        int i;
        for(i=0;i<REFER_NODES_NUMBERS;i++)
        {
          if(b_existList[i] != 1)
          {
            buf[4*n+3] = i+1;           //�ڵ�� 
            buf[4*n+4] = 0;      
            buf[4*n+5] = 0;
            buf[4*n+6] = 0;
            n++;
          }
        }
      }
      buf[4*n+3] = '\n';
      
      HalUARTWrite(HAL_UART_PORT_0,buf,TOTAL_DATA_LENGTH); 
      //osal_mem_free( buf );
      
      //��շ��仺������
      for (n=0;n<MAX_DATA_LENGTH;n++)
      {
        referDataBuf[n] = 0;
      }
      //��ձ�Ƿ�������
      for(n=0;n<4;n++)
      {
        b_existList[n]=0;
      }
      
      /*
      //���ʹ����룬��ʾ��ȡ�Ķ�λ������3��
      else
      {
        uint8 buf[ERROR_LESS_MSG_LENGTH];       //���ճ���
        buf[ERROR_LESS_MSG_TYPE] = RP_ERROR_DATA;
        buf[ERROR_LESS_TYPE] = ERROR_LESS_DATA;
        buf[ERROR_LESS_END] = '\n';
    
        HalUARTWrite(HAL_UART_PORT_0,buf,ERROR_LESS_MSG_LENGTH);
      }
      */
      
      //����λ�ȡ��ͬ���кŵĴ浽���仺��������
      referDataBuf[0]  = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_REFID];
      referDataBuf[1]  = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_DSITANCE_HIGH];
      referDataBuf[2]  = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_DSITANCE_MIDD];
      referDataBuf[3]  = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_DSITANCE_LOW];
      //�������б���Ϊ1��ʾ������
      b_existList[(pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_REFID])-1] = 1;
      sameSeqTimes = 1;
    }
    else
    {
      //��Ϊ������������󣬲ο��ڵ���ܷ��Ͷ�����ͬ��������Ϣ��Э��������Ҫ��ȥ��ͬ������
      bool b_exist = false;
      int temp;
      for(temp=0;temp<sameSeqTimes;temp++)
      {
        if(referDataBuf[4*temp] == pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_REFID])
        {
          b_exist = true;
        }
      }
      if (b_exist == false)
      {
        referDataBuf[4*sameSeqTimes]  = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_REFID];
        referDataBuf[1+4*sameSeqTimes]  = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_DSITANCE_HIGH];
        referDataBuf[2+4*sameSeqTimes]  = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_DSITANCE_MIDD];
        referDataBuf[3+4*sameSeqTimes]  = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_DSITANCE_LOW];
        b_existList[(pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_REFID])-1] = 1;
        sameSeqTimes++;
      }
    }
    
    lastSeqNo = thisSeqNo;
    lastMobID = thisMobID;
    
    
    /*
    buf[TIMEDIFF_MSG_TYPE]      = RP_BLOADCAST_TIME;
    buf[TIMEDIFF_SEQUENCE]  = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_SEQUENCE];
    buf[TIMEDIFF_FIXID]  = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_FIXID];
    buf[TIMEDIFF_TIMEDIFF_HIGH]  = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_DSITANCE_HIGH];
    buf[TIMEDIFF_TIMEDIFF_MIDD]  = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_DSITANCE_MIDD];
    buf[TIMEDIFF_TIMEDIFF_LOW]  = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_DSITANCE_LOW];
    buf[TIMEDIFF_END] = '\n';
    */
    
    /*
    int n;
    for(n=0;n<RECV_TIMES;n++)
    {
      buf[4*n+3] = pkt->cmd.Data[4*n+2];    //ע�⣡�ο��ڵ���û�д�MSG_TYPE�����λ��Ҫע��
      buf[4*n+4] = pkt->cmd.Data[4*n+3];      
      buf[4*n+5] = pkt->cmd.Data[4*n+4];
      buf[4*n+6] = pkt->cmd.Data[4*n+5];
    }
    buf[4*n+3] = '\n';
    */

    //HalUARTWrite(HAL_UART_PORT_0,buf,REFER_TIMEDIFF_MSG_LEN);
    //HalUARTWrite(HAL_UART_PORT_0,buf,TIMEDIFF_MSG_LENGTH);
    
    
    break;  
    
    //pel+ ��� �ƶ��ڵ㷢���¶���Ϣ��Э����  
  case LOCATION_COOR_TEMPURATE:
    {
      uint8 buf[TEMP_MSG_LENGTH];       //���ճ���
      buf[TEMP_MSG_TYPE] = RP_TEMPERATURE_DATA;
      buf[TEMP_DATA_LOW] = pkt->cmd.Data[0];
      buf[TEMP_DATA_HIGH] = pkt->cmd.Data[1];
      buf[TEMP_END] = '\n';
  
      HalUARTWrite(HAL_UART_PORT_0,buf,TEMP_MSG_LENGTH);
      
      break; 
    }
    
    /*
    //pel+ ��� �ƶ��ڵ㷢�ͳ�����¶���Ϣ�Ͳο��ڵ㷢�ͳ���Ķ�λ��Ϣ��Э����
  case LOCATION_COOR_ERROR:
  {
    //Error_typeΪ3��ʾ�ǳ�����¶����ݣ�����Ҫ���кţ���֡������4��
    if (pkt->cmd.Data[0] == 3)
    {
      uint8 buf[ERROR_TEMP_MSG_LENGTH];       //���ճ���
      buf[ERROR_TEMP_MSG_TYPE] = RP_ERROR_DATA;
      buf[ERROR_TEMP_TYPE] = pkt->cmd.Data[0];
      buf[ERROR_TEMP_NODE_ID] = pkt->cmd.Data[1];
      buf[ERROR_TEMP_END] = '\n';
  
      HalUARTWrite(HAL_UART_PORT_0,buf,ERROR_TEMP_MSG_LENGTH);
    }
    //Error_type��Ϊ3��ʾ������Ƕ�λ���ݣ���Ҫ���кţ���֡����Ϊ5��
    else
    {
      uint8 buf[ERROR_POS_MSG_LENGTH];       //���ճ���
      buf[ERROR_POS_MSG_TYPE] = RP_ERROR_DATA;
      buf[ERROR_POS_TYPE] = pkt->cmd.Data[0];
      buf[ERROR_POS_NODE_ID] = pkt->cmd.Data[1];
      buf[ERROR_POS_SEQ_NO] = pkt->cmd.Data[2];
      buf[ERROR_POS_END] = '\n';
  
      HalUARTWrite(HAL_UART_PORT_0,buf,ERROR_POS_MSG_LENGTH);
    }
    break;     
  }  
 */ 
    
  default:
    break;
  }
}

/*********************************************************************
 * @fn      LocationDongle_MTMsg
 *
 * @brief   Process Monitor and Test messages
 *
 * @param   len - number of bytes
 * @param   msg - pointer to message
 *          0 - Lo byte destination address
 *          1 - Hi byte destination address
 *          2 - endpoint
 *          3 - lo byte cluster ID
 *          4 - hi byte cluster ID
 *          5 - data length
 *          6 - first byte of data
 *
 * @return  none
 */

/*
void LocationDongle_MTMsg( uint8 len, uint8 *msg )
{

  afAddrType_t dstAddr;
  cId_t clusterID;
  uint8 msgType = msg[MSG_TYPE];
  uint8 dataLen;
  uint8 sendMsg[12];
  //HalLcdWriteString( "TEST!", HAL_LCD_LINE_4 );

  switch(msgType)
  {
    case MOBILE_CFG_MSG:
          dstAddr.addr.shortAddr = BUILD_UINT16( msg[M_SELF_ADDR_LO], 
                                                 msg[M_SELF_ADDR_HI] );
          clusterID = LOCATION_MOBILE_CONFIG_REQ;
          dstAddr.endPoint = LOCATION_MOBILE_ENDPOINT;
          dataLen = LOCATION_MOBILE_CONFIG_REQ_LENGTH;
          sendMsg[LOCATION_MOBILE_CONFIG_REQ_FIXID]     = msg[M_FIXID];
          sendMsg[LOCATION_MOBILE_CONFIG_REQ_MODE]      = msg[M_LOC_MODE];
          sendMsg[LOCATION_MOBILE_CONFIG_REQ_DSTADDR_H] = msg[M_DST_ADDR_HI];
          sendMsg[LOCATION_MOBILE_CONFIG_REQ_DSTADDR_L] = msg[M_DST_ADDR_LO];
          sendMsg[LOCATION_MOBILE_CONFIG_REQ_PERIOD]    = msg[M_LOC_PERIOD];
    break;

    case REFER_CFG_MSG:
          dstAddr.addr.shortAddr = BUILD_UINT16( msg[R_NETID_LO], 
                                                 msg[R_NETID_HI] );
          clusterID = LOCATION_REFER_CONFIG_REQ;
          dstAddr.endPoint = LOCATION_REFER_ENDPOINT;
          dataLen = LOCATION_REFER_CONFIG_REQ_LENGTH;
          sendMsg[LOCATION_REFER_CONFIG_REQ_X] = msg[R_POSITION_X];
          sendMsg[LOCATION_REFER_CONFIG_REQ_Y] = msg[R_POSITION_Y];
          sendMsg[LOCATION_REFER_CONFIG_REQ_PERIOD] = msg[R_TEMP_PERIOD];
          sendMsg[LOCATION_REFER_CONFIG_REQ_DST_ADDR_HI] = msg[R_DST_ADDR_HI];
          sendMsg[LOCATION_REFER_CONFIG_REQ_DST_ADDR_LO] = msg[R_DST_ADDR_LO];
    break;

    default:
    break;
  }

  dstAddr.addrMode = afAddr16Bit;

  (void)AF_DataRequest( &dstAddr, (endPointDesc_t*)&epDesc, 
                        clusterID, dataLen, sendMsg,
                        &LocationDongle_TransID, 0, AF_DEFAULT_RADIUS );
}
*/


void SPIMgr_ProcessZToolData ( uint8 port, uint8 event )
{
  uint16 rxlen;//�������ݳ���
  uint8* psbuf;//���մ洢��ָ��
     
  if (event & (HAL_UART_RX_FULL | HAL_UART_RX_ABOUT_FULL | HAL_UART_RX_TIMEOUT))
  {
    rxlen = Hal_UART_RxBufLen( HAL_UART_PORT_0 );//��ý��ջ��������ݳ���
    psbuf = osal_mem_alloc(rxlen);//����rxlen�����ڴ沢��ָ�븳��psbuf
    HalUARTRead (HAL_UART_PORT_0, psbuf, rxlen);//�����ջ��������ݵ���
    //LocationDongle_MTMsg(rxlen,psbuf);  ���ڽ׶�ֱ�ӽ��������������
    osal_mem_free( psbuf );//�ͷŷ�����ڴ�
  }
}

/*********************************************************************
*********************************************************************/

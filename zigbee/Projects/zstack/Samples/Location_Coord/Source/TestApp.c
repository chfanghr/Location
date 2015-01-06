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
#define TOTAL_DATA_LENGTH     16 //MSG_TYPE + SEQ_NO + MOB_ID + 12 + '\n'
#define REFER_NODES_NUMBERS           4  //��ʾ�������вο��ڵ�ĸ���
#define MOBILE_NODES_NUMBERS          4  //��ʾ���������ƶ��ڵ�ĸ���
#define DELAY_SEND_TIMES              10 //��ʾÿ�������ӳٷ��͸���λ����ʱ����
/*********************************************************************
 * GLOBAL VARIABLES
 */

uint8 LocationDongle_TaskID;
unsigned int lastSeqNo = 0;
unsigned int thisSeqNo = 0;
unsigned int lastMobID = 0;
unsigned int thisMobID = 0;
unsigned char delayTimes = 1;     //��¼�ӳٷ��͵Ĵ�����һ����Ҫ�ӳٷ���3��
uint32 referDataBuf[MOBILE_NODES_NUMBERS+1][REFER_NODES_NUMBERS+1];    //5*5�ľ������е�һ�������ж��Ƿ��Ѿ�����
uint8 buf[MOBILE_NODES_NUMBERS][TOTAL_DATA_LENGTH];         //����ķ�������
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
      osal_start_timerEx( LocationDongle_TaskID, COOR_DELAYSEND_EVT, DELAY_SEND_TIMES );
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
  //pel+ ��Ӳο��ڵ㷢�����λ�ø�Э����  
  case LOCATION_REFER_DISTANCE_RSP:
    thisSeqNo  = pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_SEQUENCE];
    
    //�������кź��ƶ��ڵ�ID���Ƿ���ͬ�����䣬�ƶ��ڵ�����кŲ�ͬ������ݽ�������
    if (thisSeqNo != lastSeqNo)
    {
      //����ȡ��ֵ�Ž���������
      int mobileNo,refNo;
      for(mobileNo=1;mobileNo<=MOBILE_NODES_NUMBERS;mobileNo++)
      {
        buf[mobileNo-1][TIMEDIFF_MSG_TYPE] = RP_BLOADCAST_TIME;
        buf[mobileNo-1][TIMEDIFF_SEQUENCE] = lastSeqNo;
        buf[mobileNo-1][TIMEDIFF_MOBID]  = mobileNo;
        for(refNo=1;refNo<=REFER_NODES_NUMBERS;refNo++)
        {
          buf[mobileNo-1][refNo*3] = ((uint8 *)&referDataBuf[mobileNo][refNo])[2];        //ʱ���߰�λ 
          buf[mobileNo-1][refNo*3+1] = ((uint8 *)&referDataBuf[mobileNo][refNo])[1];      //ʱ����а�λ
          buf[mobileNo-1][refNo*3+2] = ((uint8 *)&referDataBuf[mobileNo][refNo])[0];      //ʱ���Ͱ�λ
        } 
        buf[mobileNo-1][15]  = '\n';
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
      osal_start_timerEx( LocationDongle_TaskID, COOR_DELAYSEND_EVT, DELAY_SEND_TIMES );
      
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
      referDataBuf[0][pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_REFID]] = 1;
      //������uint8�����ݺϳ�һ��uint32�Ĳ�����������,ֱ��<<16λ�ᱨ���棬Ҫ����ȡ��ֵ����Ϊ32λ�����������ֲ����������  
      for(mobileNo=1;mobileNo<=MOBILE_NODES_NUMBERS;mobileNo++)
      {
        referDataBuf[mobileNo][pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_REFID]] = \
          (unsigned long)(pkt->cmd.Data[3*mobileNo-1])<<16 |      \
          (unsigned long)(pkt->cmd.Data[3*mobileNo])<<8 |      \
          (pkt->cmd.Data[3*mobileNo+1]);
      }
    }
    else
    {
      //��Ϊ������������󣬲ο��ڵ���ܷ��Ͷ�����ͬ��������Ϣ��Э��������Ҫ��ȥ��ͬ������
      bool b_exist = false;
      if(referDataBuf[0][pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_REFID]] == 1)
      {
        b_exist = true;
      }
      
      if (b_exist == false)
      {
        referDataBuf[0][pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_REFID]] = 1;
        //������uint8�����ݺϳ�һ��uint32�Ĳ�����������
        int mobileNo;
        for(mobileNo=1;mobileNo<=MOBILE_NODES_NUMBERS;mobileNo++)
        {
          referDataBuf[mobileNo][pkt->cmd.Data[LOCATION_REFER_POSITION_RSP_REFID]] = \
            (unsigned long)(pkt->cmd.Data[3*mobileNo-1])<<16|      \
            (unsigned long)(pkt->cmd.Data[3*mobileNo])<<8 |      \
            (pkt->cmd.Data[3*mobileNo+1]);
        }
      }
    }
    
    lastSeqNo = thisSeqNo;     
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
  default:
    break;
  }
}

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

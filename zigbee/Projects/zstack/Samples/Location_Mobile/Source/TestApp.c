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


#include "mac_mcu.h"      //���Բ��Է��䳬�����͵�Ų���ʱ���

/*********************************************************************
 * CONSTANTS
 */

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
unsigned char mobile_id = 4;                               //�ƶ��ڵ��ID�ţ�ÿ���ƶ��ڵ��ID��������ͬ
unsigned char team_id = 2;                                 //�ƶ��ڵ�����ID�ţ�Ҫ�ǵ��޸ģ�Ĭ��ʱ1��2���Ƕ���1,3��4�Ƕ���2
unsigned char delay_time = 50;                             //�ӳ�ʱ�䣬ʵ���ӳ���2*delay_time ms���˴��ӳ�����ο��ڵ��ӳ���ͬ��ʱ��
uint8 teamMobileData[4][2] = {{0,0},{0,0},{0,0},{0,0}};    //Ĭ����һ�����ѣ�,�����3������0�о�Ϊ1���ƶ��ڵ㣬��1�о�Ϊ2�ţ��Դ����ơ���һ�ֽ�Ϊ�����ַ�߰�λ���ڶ��ֽ�λ�����ַ�Ͱ�λ
/*********************************************************************
 * LOCAL VARIABLES
 */

static const cId_t Mobile_InputClusterList[] =
{
  CID_S2MR_BROADCAST,       //����sink�ڵ�Ĺ㲥�ź�
  CID_C2A_GET_BASIC_VALUE,
  CID_C2A_SET_BASIC_VALUE,
  CID_C2M_RP_POSITION,
  CID_C2M_SET_JUDGE,
  CID_M2M_TEAM_DATA,
  CID_M2M_TEAM_CONTROL
};


static const cId_t Mobile_OutputClusterList[] =
{
  CID_M2M_TEAM_DATA,
  CID_M2M_TEAM_CONTROL,
  CID_M2C_REQ_POSITION,
  CID_A2C_RP_BASIC_VALUE,
  CID_A2C_SUCCESS_RESPONSE
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
void startSendUS();
void halSetTimer3Period(uint8 period);
void getBasicValue();
void setBasicValue(afIncomingMSGPacket_t *pkt);
void responsePosition(afIncomingMSGPacket_t *pkt);
void setJudge();
void successResponse(uint8 result);
void sendTeamData(afAddrMode_t AddrType, uint8 netAddr_HI, uint8 netAddr_LO);
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
 * ��    �ܣ���ʱ��3�жϷ����� ע����ں���������ж���ں�����ͬ
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
    if(t3count == delay_time*(mobile_id-1))     //2*18=36ms
    {
      t3count = 0;
      startSendUS();             //���÷��ͳ������¼�
      
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
        //��ʼ��ʱ���������ݷ��͸�Э����          
        getBasicValue();
        //��ʼ���㲥�ýڵ�Ļ���������Ϣ�������ڵ�
        sendTeamData((afAddrMode_t)AddrBroadcast,0xFF,0xFF);   
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
    //����sink�ڵ�Ĺ㲥�ź�
    case CID_S2MR_BROADCAST:
    {
      //1���ƶ��ڵ�ֱ�ӷ��䳬����������Ҫ�ȴ�
      if(mobile_id == 1)
      {
        startSendUS();
      }
      //�����ڵ㿪����ʱ�����ȴ�(n-1)*36ms��ʱ�䣬�ٷ��䳬����
      else
      {
        TIMER3_RUN(TRUE);
      }
    }
    break;

    //����coor�ڵ�Ĳ�ѯ�����ź�    
    case CID_C2A_GET_BASIC_VALUE:
    {
      getBasicValue();
    }
    break;
    
    //����coor�ڵ�����������ź�
    case CID_C2A_SET_BASIC_VALUE:
    {
      setBasicValue(pkt);
    }
    break;
    
    //����coor�ڵ����Ӧ�ڵ�λ���ź�
    case CID_C2M_RP_POSITION:
    {
      responsePosition(pkt);
    }
    break;
    
    //����coor�ڵ����ʼ�ź�
    case CID_C2M_SET_JUDGE:
    {
      setJudge();
    }
    break;
    
    //���յ�����mobile�ڵ�Ķ�����Ϣ
    case CID_M2M_TEAM_DATA:
    {
      //�ж����Team_ID��ͬ�򣬽��������ַ��¼������
      if(pkt->cmd.Data[M2M_TEAM_DATA_TEAM_ID] == team_id)
      {
        teamMobileData[(pkt->cmd.Data[M2M_TEAM_DATA_MOB_ID]-1)][0] = HI_UINT16(pkt->srcAddr.addr.shortAddr);
        teamMobileData[(pkt->cmd.Data[M2M_TEAM_DATA_MOB_ID]-1)][1] = LO_UINT16(pkt->srcAddr.addr.shortAddr);
        
        //����жϽ��յ����ǹ㲥��ʽ������͵����źŸ����͵Ľڵ�
        if(pkt->wasBroadcast == true)
        {
          sendTeamData((afAddrMode_t)Addr16Bit, teamMobileData[(pkt->cmd.Data[M2M_TEAM_DATA_MOB_ID]-1)][0],
                                                teamMobileData[(pkt->cmd.Data[M2M_TEAM_DATA_MOB_ID]-1)][1]);
        }
      }
    }
    break;
    
    //���յ�����mobile�ڵ�Ŀ�����Ϣ
    case CID_M2M_TEAM_CONTROL:
    {  
      //�����յ�������ͨ�����ڷ��͸�robot
      //add here!
      
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
void startSendUS()
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
  //ĳЩ��Ӳ�����������⣬�ʽ��ƶ��ڵ������ȡ��������֮�󻻸���ɫ�ĵ�
  //�յ���ѯ��Ϣ��˸�̵�3�Σ�ÿ��300ms
  //HalLedBlink(HAL_LED_1, 3, 50, 300);
  
  //��������Ϣ���ظ�Э����
  unsigned char theMessageData[M2C_RP_BASIC_VALUE_LENGTH];     
  theMessageData[M2C_RP_BASIC_VALUE_NODE_TYPE] = NT_MOB_NODE;
  theMessageData[M2C_RP_BASIC_VALUE_MOB_ID] = mobile_id;
  theMessageData[M2C_RP_BASIC_VALUE_TEAM_ID] = team_id;
  theMessageData[M2C_RP_BASIC_VALUE_SEND_DELAY_TIME] = delay_time * 2;        //ͬrefer�ڵ㣬��Ҫ����2
  
  afAddrType_t coorAddr;
  coorAddr.addrMode = (afAddrMode_t)Addr16Bit; //��������
  coorAddr.endPoint = LOCATION_DONGLE_ENDPOINT; //Ŀ�Ķ˿ں�
  coorAddr.addr.shortAddr = 0x0000;            //Э���������ַ
  AF_DataRequest( &coorAddr, (endPointDesc_t *)&epDesc,
                           CID_A2C_RP_BASIC_VALUE, M2C_RP_BASIC_VALUE_LENGTH,
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
  //��Э�������͹���������д��mobile�ڵ���
  mobile_id = pkt->cmd.Data[C2M_SET_BASIC_VALUE_MOB_ID];
  team_id = pkt->cmd.Data[C2M_SET_BASIC_VALUE_TEAM_ID];
  delay_time = pkt->cmd.Data[C2M_SET_BASIC_VALUE_SEND_DELAY_TIME] / 2;    //ͬrefer�ڵ㣬��Ҫ����2

  //�����ܳɹ�����Ϣ���ظ�Э����
  successResponse(1);
}

/*********************************************************************
 * @fn      responsePosition
 *
 * @brief   get the request Position from Coor and send to serial to robot
 *
 * @param   none
 *
 * @return  none
 */
void responsePosition(afIncomingMSGPacket_t *pkt)
{
  //ͨ�����ڷ��͸�����С�������λ����Ϣ
  //add here!
  
  //�����ܳɹ�����Ϣ���ظ�Э����
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
  //ĳЩ��Ӳ�����������⣬�ʽ��ƶ��ڵ������ȡ��������֮�󻻸���ɫ�ĵ�
  //�յ�������Ϣ��˸�̵�3�Σ�ÿ��300ms
  HalLedBlink(HAL_LED_1, 3, 50, 300);
  
  //�����óɹ���Ϣ���ظ�Э����
  unsigned char theMessageData[A2C_SUCCESS_RESPONSE_LENGTH];     
  theMessageData[A2C_SUCCESS_RESPONSE_NODE_TYPE] = NT_MOB_NODE;
  theMessageData[A2C_SUCCESS_RESPONSE_NODE_ID] = mobile_id;
  theMessageData[A2C_SUCCESS_RESPONSE_RESULT] = result;        //1��ʾ���óɹ�
  
  afAddrType_t coorAddr;
  coorAddr.addrMode = (afAddrMode_t)Addr16Bit; //��������
  coorAddr.endPoint = LOCATION_DONGLE_ENDPOINT; //Ŀ�Ķ˿ں�
  coorAddr.addr.shortAddr = 0x0000;            //Э���������ַ
  AF_DataRequest( &coorAddr, (endPointDesc_t *)&epDesc,
                           CID_A2C_SUCCESS_RESPONSE, A2C_SUCCESS_RESPONSE_LENGTH,
                           theMessageData, &transId, AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );
}

/*********************************************************************
 * @fn      setJudge
 *
 * @brief   Set the judge Command from Coor and send back success response
 *
 * @param   none
 *
 * @return  none
 */
void setJudge()
{
  //ͨ�����ڷ��͸�����С��
  //add here!
  
  
  //�����óɹ���Ϣ���ظ�Э����
  successResponse(1);
}

/*********************************************************************
 * @fn      requestPosition
 *
 * @brief   get the request Position from serial and send to Coor
 *
 * @param   none
 *
 * @return  none
 */
void requestPosition()
{
  //�Ӵ��ڻ�ȡ��Ҫ��Ϥλ�õ��ƶ��ڵ�ID
  //add here!
  
  //������λ����Ϣ���͸�Э����
  unsigned char theMessageData[M2C_REQ_POSITION_LENGTH];     
  theMessageData[M2C_REQ_POSITION_REQ_MOB_ID] = mobile_id;    //������ƶ��ڵ�ID
  theMessageData[M2C_REQ_POSITION_GET_MOB_ID] = mobile_id;    //�˲�������Ҫ��Ϥλ�õ��ƶ��ڵ�ID�������������ƶ��ڵ�
  
  afAddrType_t coorAddr;
  coorAddr.addrMode = (afAddrMode_t)Addr16Bit; //��������
  coorAddr.endPoint = LOCATION_DONGLE_ENDPOINT; //Ŀ�Ķ˿ں�
  coorAddr.addr.shortAddr = 0x0000;            //Э���������ַ
  AF_DataRequest( &coorAddr, (endPointDesc_t *)&epDesc,
                           CID_M2C_REQ_POSITION, M2C_REQ_POSITION_LENGTH,
                           theMessageData, &transId, AF_DISCV_ROUTE, AF_DEFAULT_RADIUS );
}


/*********************************************************************
 * @fn      sendTeamData
 *
 * @brief   send the team information to the other mobile node
 *
 * @param   none
 *
 * @return  none
 */
void sendTeamData(afAddrMode_t AddrType, uint8 netAddr_HI, uint8 netAddr_LO)
{
  //�����Ų�
  unsigned char theMessageData[M2M_TEAM_DATA_LENGTH];    
  theMessageData[M2M_TEAM_DATA_TEAM_ID] = team_id;
  theMessageData[M2M_TEAM_DATA_MOB_ID] = mobile_id;
  
  afAddrType_t dstAddr;
  dstAddr.addrMode = (afAddrMode_t)AddrType;
  dstAddr.endPoint = LOCATION_MOBILE_ENDPOINT;        
  dstAddr.addr.shortAddr = BUILD_UINT16(netAddr_LO,netAddr_HI);  
  AF_DataRequest( &dstAddr, (endPointDesc_t *)&epDesc,
                 CID_M2M_TEAM_DATA, M2M_TEAM_DATA_LENGTH,
                 theMessageData, &transId,
                 AF_DISCV_ROUTE, AF_DEFAULT_RADIUS);  
}


/*********************************************************************
 * @fn      sendTeamControl
 *
 * @brief   send the team control information to the other mobile node
 *
 * @param   none
 *
 * @return  none
 */
void sendTeamControl(uint8 get_mob_id, uint8 action_HI, uint8 action_LO)
{
  //�����Ų�
  unsigned char theMessageData[M2M_TEAM_CONTROL_LENGTH];    
  theMessageData[M2M_TEAM_CONTROL_REQ_MOB_ID] = mobile_id;
  theMessageData[M2M_TEAM_CONTROL_GET_MOB_ID] = get_mob_id;
  theMessageData[M2M_TEAM_CONGROL_ACTION_HI] = action_HI;
  theMessageData[M2M_TEAM_CONGROL_ACTION_LO] = action_LO;
  
  afAddrType_t dstAddr;
  dstAddr.addrMode = (afAddrMode_t)Addr16Bit;
  dstAddr.endPoint = LOCATION_MOBILE_ENDPOINT;        
  dstAddr.addr.shortAddr = BUILD_UINT16(teamMobileData[get_mob_id-1][0],
                                        teamMobileData[get_mob_id-1][0]);  
  AF_DataRequest( &dstAddr, (endPointDesc_t *)&epDesc,
                 CID_M2M_TEAM_CONTROL, M2M_TEAM_CONTROL_LENGTH,
                 theMessageData, &transId,
                 AF_DISCV_ROUTE, AF_DEFAULT_RADIUS);  
}





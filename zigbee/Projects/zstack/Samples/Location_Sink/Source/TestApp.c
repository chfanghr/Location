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

#include "DS18B20.h"
#include "math.h"         //pel+ pow(,)

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
unsigned int sink_broadcast_period = 500; //�㲥����һ��Ϊ180ms����һ��,��λΪms,4���ƶ��ڵ�ļ����˰�
unsigned char sink_temp_cycle  = 10;                     //��λΪ�룬��ʵ��ʹ��ʱ����Ҫ*1000����Ϊ������λms��

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
  CID_S2C_TEMPERATURE,        //�����¶ȸ�Э����
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
  
          //���������ȡ�¶ȣ������䴫��Э����
          //sendTemperature();
          //osal_start_timerEx( Sink_TaskID, SINK_TEMP_EVT, sink_temp_cycle*1000 );  //�������ڷ����¶���Ϣ��ʱ       
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
    
  if ( events & SINK_TEMP_EVT )    
  {
    sendTemperature();             //���÷����¶�
    osal_start_timerEx( Sink_TaskID, SINK_TEMP_EVT, sink_temp_cycle*1000 );
    
    return ( events ^ SINK_TEMP_EVT );
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
  theMessageData[S2C_RP_BASIC_VALUE_TEMP_CYC] = sink_temp_cycle;
  
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
  sink_temp_cycle = pkt->cmd.Data[C2S_SET_BASIC_VALUE_TEMP_CYC];
  
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
  unsigned char sinkNodeTemperature[2]; 
  if (temperature >0 && temperature < 40)  //��������ݲ����з���
  {
    sinkNodeTemperature[0] = ((uint8 *)&tempData)[1];      //�¶ȸ�λ 
    sinkNodeTemperature[1] = ((uint8 *)&tempData)[0];      //�¶ȵ�λ
    //moblieNodeTemperature[0] = ((uint8 *)&sensor_temperature_value)[1];      //�¶ȸ�λ 
    //moblieNodeTemperature[1] = ((uint8 *)&sensor_temperature_value)[0];      //�¶ȵ�λ
   }
  
  //��������ݽ��������Ϊ0xFFFF
  else
  {
    sinkNodeTemperature[0] = 0xFF;      //�¶ȸ�λ 
    sinkNodeTemperature[1] = 0xFF;      //�¶ȵ�λ
  }
  
  afAddrType_t Coord_DstAddr;
  Coord_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
  Coord_DstAddr.endPoint = LOCATION_DONGLE_ENDPOINT;
  Coord_DstAddr.addr.shortAddr = 0x0000;  //��ʾ���ݰ����������е����нڵ�㲥������0xFFFC��·�������ղ�������֪�ν� 
  AF_DataRequest( &Coord_DstAddr, (endPointDesc_t *)&epDesc,
                 CID_S2C_TEMPERATURE, 2,
                 sinkNodeTemperature, &transId,
                 AF_DISCV_ROUTE, AF_DEFAULT_RADIUS ); 
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
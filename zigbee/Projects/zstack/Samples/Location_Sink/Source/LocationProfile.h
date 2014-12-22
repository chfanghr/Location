#ifndef LOCATIONPROFILE_H
#define LOCATIONPROFILE_H

/*Endpoint */
#define LOCATION_REFER_ENDPOINT				210
#define LOCATION_MOBILE_ENDPOINT				210   //��ʱ����Ϊһ���ģ������㲥���ܽ���
//#define LOCATION_MOBILE_ENDPOINT				211  
#define LOCATION_DONGLE_ENDPOINT 			212
#define LOCATION_SINK_ENDPOINT 			213

#define LOCATION_PROFID						0xC003
#define LOCATION_DEVICE_VERSION				0
#define LOCATION_FLAGS						0


/*Device ID*/
#define LOCATION_REFER_DEVICE_ID				0x0010        
#define	LOCATION_MOBILE_DEVICE_ID			0x0011
#define LOCATION_DONGLE_DEVICE_ID			0x0012
#define LOCATION_SINK_DEVICE_ID			        0x0013

/*Cluster IDs */
#define LOCATION_ULTRA_BLORDCAST                   0x0010//mobile����sink�ڵ�㲥����
#define LOCATION_COOR_TEMPURATE                    0x0011//mobile���͵�ǰ�¶ȸ�coor�ڵ�
#define LOCATION_REFER_DISTANCE_RSP                0x0012//refer����Ultra����ʱ���
//#define LOCATION_COOR_ERROR                        0x0013//����Э���������뱨��




/*Default values */


/*Message Format*/
#define   RP_BLOADCAST_TIME         1   //���ؾ���ʱ�������֡
#define   RP_TEMPERATURE_DATA       2   //�����¶�����֡
#define   RP_ERROR_DATA             3   //���ش���֡�ı���

// LOCATION_RSSI_BLAST
// �����κ���Ϣ���㲥

// LOCATION_RSSI_REQ
// �����κ���Ϣ���㲥

/*

*/

// LOCATION_MOBILE_FIND_REQ
// �����κ���Ϣ�����͸�ĳһ�ض���mobile


//LOCATION_REFFR_POSITION_RSP
//test
//#define LOCATION_REFER_POSITION_RSP_LENGTH 		19//��Ϣ����
#define LOCATION_REFER_POSITION_RSP_LENGTH 		6//��Ϣ����
#define LOCATION_REFER_POSITION_RSP_SEQUENCE            0//���к�
#define LOCATION_REFER_POSITION_RSP_MOBID		1//�ƶ��ڵ�ID
#define LOCATION_REFER_POSITION_RSP_REFID		2//�ο��ڵ�ID
#define LOCATION_REFER_POSITION_RSP_DSITANCE_HIGH		3//���������ľ���
#define LOCATION_REFER_POSITION_RSP_DSITANCE_MIDD		4//���������ľ���
#define LOCATION_REFER_POSITION_RSP_DSITANCE_LOW		5//���������ľ���

//LOCATION_REFFR_POSITION_RSP
#define LOCATION_REFER_ERROR_POSITION_RSP_LENGTH 		3//��Ϣ����
#define LOCATION_REFER_ERROR_POSITION_MSG_TYPE                  0//��������
#define LOCATION_REFER_ERROR_POSITION_RSP_FIXID		        1//�̶�ID
#define LOCATION_REFER_ERROR_POSITION_SEQ_NO		        2//���������ľ���


//Э���� ������Ϣ֡
#define	    TIMEDIFF_MSG_LENGTH		                  7
#define     TIMEDIFF_MSG_TYPE                             0
#define     TIMEDIFF_SEQUENCE                             1//���к�
#define     TIMEDIFF_FIXID       			  2//ID��
#define     TIMEDIFF_TIMEDIFF_HIGH   			  3//ʱ���߰�λ
#define     TIMEDIFF_TIMEDIFF_MIDD   			  4//ʱ����а�λ
#define     TIMEDIFF_TIMEDIFF_LOW   			  5//ʱ���Ͱ�λ
#define     TIMEDIFF_END       			          6//��ֹ��\n

//Э���� �¶�֡
#define	    TEMP_MSG_LENGTH		          4
#define     TEMP_MSG_TYPE                         0
#define     TEMP_DATA_LOW                         1//��Ҫ����100
#define     TEMP_DATA_HIGH                        2//
#define     TEMP_END       			  3//��ֹ��\n

//�������ݽ�����λ��������ʾ
/*
//�������� ����
#define     ERROR_NOT_GET_US                      1//δ���յ��������ź�
#define     ERROR_US_DATA                         2//���յ��ĳ������ź��Ǵ�������
#define     ERROR_TEMP_DATA                       3//���յ����¶������Ǵ�������
#define     ERROR_LESS_DATA                       4//���յ��Ķ�λ��Ϣ����3��

//Э���� �¶ȴ���֡
#define	    ERROR_TEMP_MSG_LENGTH		  4
#define     ERROR_TEMP_MSG_TYPE                   0
#define     ERROR_TEMP_TYPE                       1//��������
#define     ERROR_TEMP_NODE_ID                    2//�ڵ�ID
#define     ERROR_TEMP_END       		  3//��ֹ��\n


//Э���� ��λ����֡
#define	    ERROR_POS_MSG_LENGTH		  5
#define     ERROR_POS_MSG_TYPE                    0
#define     ERROR_POS_TYPE                        1//��������
#define     ERROR_POS_NODE_ID                     2//�ڵ�ID
#define     ERROR_POS_SEQ_NO       		  3//��ֹ��\n
#define     ERROR_POS_END       		  4//��ֹ��\n

//Э���� ��λ������3�Ĵ���֡
#define	    ERROR_LESS_MSG_LENGTH		  3
#define     ERROR_LESS_MSG_TYPE                   0
#define     ERROR_LESS_TYPE                       1//��������
#define     ERROR_LESS_END       		  2//��ֹ��\n
*/
#endif //#ifndef LOCATIONPROFILE_H
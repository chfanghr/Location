#ifndef LOCATIONPROFILE_H
#define LOCATIONPROFILE_H

/*Endpoint */
#define LOCATION_MOBILE_ENDPOINT			210   //��ʱ����Ϊһ���ģ������㲥���ܽ���
#define LOCATION_REFER_ENDPOINT				210
#define LOCATION_DONGLE_ENDPOINT 			212
#define LOCATION_SINK_ENDPOINT 			        213
//#define LOCATION_MOBILE_ENDPOINT			211 

#define LOCATION_PROFID					0xC003
#define LOCATION_DEVICE_VERSION				0
#define LOCATION_FLAGS					0


/*Device ID*/
#define LOCATION_REFER_DEVICE_ID			0x0010        
#define	LOCATION_MOBILE_DEVICE_ID			0x0011
#define LOCATION_DONGLE_DEVICE_ID			0x0012
#define LOCATION_SINK_DEVICE_ID			        0x0013

/*Cluster IDs */
#define CID_A2C_RP_BASIC_VALUE                0x0010//�����ڵ���coor�ڵ㷵�ز�ѯ����
#define CID_A2C_SUCCESS_RESPONSE              0x0011//�����ڵ���coor�ڵ���Ӧ���óɹ�
#define CID_C2A_GET_BASIC_VALUE               0x0012//coor�ڵ��������ڵ㷢����ѯ����
#define CID_C2A_SET_BASIC_VALUE               0x0013//coor�ڵ��������ڵ㷢����������
#define CID_S2MR_BROADCAST                    0x0014//sink�ڵ���mobile��refer�ڵ�㲥�ź�
#define CID_M2M_TEAM_DATA                     0x0015//mobile�ڵ�������mobile�ڵ㷢�Ͷ����ź�
#define CID_M2M_TEAM_CONTROL                  0x0016//mobile�ڵ�������mobile�ڵ㷢�Ϳ����ź�
#define CID_M2C_REQ_POSITION                  0x0017//mobile�ڵ���coor�ڵ㷢��λ�ò�ѯ����
#define CID_C2M_RP_POSITION                   0x0018//coor�ڵ���mobile����λ�ò�ѯ��Ӧ
#define CID_C2M_SET_JUDGE                     0x0019//coor�ڵ���mobile������ʼ����
#define CID_R2C_DIFF_TIME                     0x001A//refer�ڵ㷢ʱ����coor�ڵ�
#define CID_S2C_TEMPERATURE                   0x001B//sink�ڵ���coor�ڵ㷢���¶�����



/*
��д���ͣ�
  �����PC��ʾ��λ����C��ʾЭ������S��ʾsink�ڵ㣻M��ʾ�ƶ��ڵ㣻R��ʾ�ο��ڵ�;A��ʾsink���ƶ���ο��ڵ�
  C2PC����ʾ��Э����������λ��
  MT��ʾMessage Type;NT��ʾNode Type;CID��ʾCluster ID
*/
/*Message Format*/
#define MT_C2PC_DIFF_TIME              1   //Э����������λ������ʱ�������֡
#define MT_C2PC_TEMPERATURE_DATA       2   //Э����������λ���¶�����֡
#define MT_C2PC_SUCCESS_RESPONSE       3   //Э����������λ�����óɹ�����Ӧ֡
#define MT_C2PC_RP_BASIC_VALUE_C       4   //Э����������λ��Э�����Ļ�����Ϣ֡
#define MT_C2PC_RP_BASIC_VALUE_S       5   //Э����������λ��sink�ڵ�Ļ�����Ϣ֡
#define MT_C2PC_RP_BASIC_VALUE_M       6   //Э����������λ��mobile�ڵ�Ļ�����Ϣ֡
#define MT_C2PC_RP_BASIC_VALUE_R       7   //Э����������λ��refer�ڵ�Ļ�����Ϣ֡
#define MT_C2PC_REQ_POSITION           8   //Э����������λ���ڵ�λ��������Ϣ֡

#define MT_PC2C_GET_BASIC_VALUE       11   //��λ������Э���������ѯ�ڵ�Ļ�����Ϣ
#define MT_PC2C_SET_BASIC_VALUE_C     12   //��λ������Э��������Э�����ڵ�Ļ�����Ϣ
#define MT_PC2C_SET_BASIC_VALUE_S     13   //��λ������Э��������sink�ڵ�Ļ�����Ϣ
#define MT_PC2C_SET_BASIC_VALUE_M     14   //��λ������Э��������mobile�ڵ�Ļ�����Ϣ
#define MT_PC2C_SET_BASIC_VALUE_R     15   //��λ������Э��������refer�ڵ�Ļ�����Ϣ
#define MT_PC2C_RP_POSITION           16   //��λ������Э������Ӧ�ƶ��ڵ�λ�õ�������Ϣ֡
#define MT_PC2C_SET_JUDGE             17   //��λ������Э�������ÿ�ʼ��������Ϣ֡

/*Node Type*/
#define NT_COOR_NODE   1         
#define NT_SINK_NODE   2
#define NT_MOB_NODE    3
#define NT_REF_NODE    4

/*C2PC*/
//����ʱ�������֡
/*
   3-5λΪ1�Ųο��ڵ��ʱ������ݣ�6-8λΪ2�Ųο��ڵ��ʱ������ݣ�
   9-11Ϊ3�Ųο��ڵ��ʱ������ݣ�12-14λΪ4�Ųο��ڵ��ʱ�������
   ���ڲ�������ѭ���ķ������ʲ���Ҫ�г������֡
*/
#define C2PC_DIFF_TIME_LENGTH        16 //֡����
#define C2PC_DIFF_TIME_MSG_TYPE       0 //֡����
#define C2PC_DIFF_TIME_SEQ            1 //���к�
#define C2PC_DIFF_TIME_MOBID          2 //�ƶ��ڵ�ID
#define C2PC_DIFF_TIME_END           15 //��ֹ�� \n

//�¶�����֡
#define C2PC_TEMPERATURE_DATA_LENGTH    4 //֡����
#define C2PC_TEMPERATURE_DATA_MSG_TYPE  0 //֡����
#define C2PC_TEMPERATURE_DATA_HIGH      1 //�¶�����֡��8λ  ��Ҫ����100                      
#define C2PC_TEMPERATURE_DATA_LOW       2 //�¶�����֡��8λ
#define C2PC_TEMPERATURE_DATA_END       3 //��ֹ��\n

//���óɹ���Ӧ֡
#define C2PC_SUCCESS_RESPONSE_LENGTH    5 //֡����
#define C2PC_SUCCESS_RESPONSE_MSG_TYPE  0 //֡����
#define C2PC_SUCCESS_RESPONSE_NODE_TYPE 1 //�ɹ���Ӧ�Ľڵ�����
#define C2PC_SUCCESS_RESPONSE_NODE_ID   2 //�ɹ���Ӧ�Ľڵ�ID
#define C2PC_SUCCESS_RESPONSE_RESULT    3 //�ɹ���Ӧ�Ľ����1��ʾ�ɹ���0��ʾʧ��
#define C2PC_SUCCESS_RESPONSE_END       4 //��ֹ��\n

//���ڵ�Ļ�����Ϣ֡
//C�Ļ�����Ϣ֡
#define C2PC_RP_BASIC_VALUE_LENGTH_C      3 //֡����
#define C2PC_RP_BASIC_VALUE_MSG_TYPE_C    0 //֡����
#define C2PC_RP_BASIC_VALUE_DELAY_TIME_C  1 //�ӳٷ��͸���λ����ʱ��
#define C2PC_RP_BASIC_VALUE_END_C         2 //��ֹ��\n
//S�Ļ�����Ϣ֡
#define C2PC_RP_BASIC_VALUE_LENGTH_S      4 //֡����
#define C2PC_RP_BASIC_VALUE_MSG_TYPE_S    0 //֡����
#define C2PC_RP_BASIC_VALUE_BROAD_CYC_S   1 //Sink�ڵ�Ĺ㲥����
#define C2PC_RP_BASIC_VALUE_TEMP_CYC_S    2 //Sink�ڵ㷢���¶�����
#define C2PC_RP_BASIC_VALUE_END_S         3 //��ֹ��\n
//M�Ļ�����Ϣ֡
#define C2PC_RP_BASIC_VALUE_LENGTH_M           5 //֡����
#define C2PC_RP_BASIC_VALUE_MSG_TYPE_M         0 //֡����
#define C2PC_RP_BASIC_VALUE_MOB_ID_M           1 //�ƶ��ڵ�ID��
#define C2PC_RP_BASIC_VALUE_TEAM_ID_M          2 //�ƶ��ڵ����ID��
#define C2PC_RP_BASIC_VALUE_SEND_DELAY_TIME_M  3 //�ƶ��ڵ㷢���ӳ�����
#define C2PC_RP_BASIC_VALUE_END_M              4 //��ֹ��\n
//R�Ļ�����Ϣ֡
#define C2PC_RP_BASIC_VALUE_LENGTH_R           5 //֡����
#define C2PC_RP_BASIC_VALUE_MSG_TYPE_R         0 //֡����
#define C2PC_RP_BASIC_VALUE_REF_ID_R           1 //�ο��ڵ�ID��
#define C2PC_RP_BASIC_VALUE_RECV_TIME_OUT_R    2 //�ο��ڵ���ճ�ʱʱ��
#define C2PC_RP_BASIC_VALUE_RECV_DELAY_TIME_R  3 //�ο��ڵ�����ӳ�����
#define C2PC_RP_BASIC_VALUE_END_R              4 //��ֹ��\n

//�ƶ��ڵ�λ��������Ϣ֡
#define C2PC_REQ_POSITION_LENGTH           4 //֡����
#define C2PC_REQ_POSITION_MSG_TYPE         0 //֡����
#define C2PC_REQ_POSITION_REQ_MOB_ID       1 //������ƶ��ڵ��ID
#define C2PC_REQ_POSITION_GET_MOB_ID       2 //��Ҫ��ȡ���ƶ��ڵ��ID
#define C2PC_REQ_POSITION_END              3 //��ֹ��\n

/*PC2C*/
//�����ѯ�ڵ�Ļ�����Ϣ
#define PC2C_GET_BASIC_VALUE_LENGTH         3 //֡����
#define PC2C_GET_BASIC_VALUE_MSG_TYPE       0 //֡����
#define PC2C_GET_BASIC_VALUE_NODE_TYPE      1 //��ѯ�ڵ�Ľڵ�����
#define PC2C_GET_BASIC_VALUE_NODE_ID        2 //��ѯ�ڵ�Ľڵ�ID��

//���ýڵ�Ļ�����Ϣ
//����C�Ļ�����Ϣ֡
#define PC2C_SET_BASIC_VALUE_LENGTH_C      2 //֡����
#define PC2C_SET_BASIC_VALUE_MSG_TYPE_C    0 //֡����
#define PC2C_SET_BASIC_VALUE_DELAY_TIME_C  1 //�ӳٷ��͸���λ����ʱ��
//����S�Ļ�����Ϣ֡
#define PC2C_SET_BASIC_VALUE_LENGTH_S      3 //֡����
#define PC2C_SET_BASIC_VALUE_MSG_TYPE_S    0 //֡����
#define PC2C_SET_BASIC_VALUE_BROAD_CYC_S   1 //Sink�ڵ�Ĺ㲥����
#define PC2C_SET_BASIC_VALUE_TEMP_CYC_S    2 //Sink�ڵ㷢���¶�����
//����M�Ļ�����Ϣ֡
#define PC2C_SET_BASIC_VALUE_LENGTH_M           4 //֡����
#define PC2C_SET_BASIC_VALUE_MSG_TYPE_M         0 //֡����
#define PC2C_SET_BASIC_VALUE_MOB_ID_M           1 //�ƶ��ڵ�ID��
#define PC2C_SET_BASIC_VALUE_TEAM_ID_M          2 //�ƶ��ڵ����ID��
#define PC2C_SET_BASIC_VALUE_SEND_DELAY_TIME_M  3 //�ƶ��ڵ㷢���ӳ�����
//����R�Ļ�����Ϣ֡
#define PC2C_SET_BASIC_VALUE_LENGTH_R           4 //֡����
#define PC2C_SET_BASIC_VALUE_MSG_TYPE_R         0 //֡����
#define PC2C_SET_BASIC_VALUE_REF_ID_R           1 //�ο��ڵ�ID��
#define PC2C_SET_BASIC_VALUE_RECV_TIME_OUT_R    2 //�ο��ڵ���ճ�ʱʱ��
#define PC2C_SET_BASIC_VALUE_RECV_DELAY_TIME_R  3 //�ο��ڵ�����ӳ�����

//��Ӧ�ƶ��ڵ�λ�õ�������Ϣ֡
#define PC2C_RP_POSITION_LENGTH           7 //֡����
#define PC2C_RP_POSITION_MSG_TYPE         0 //֡����
#define PC2C_RP_POSITION_REQ_MOB_ID       1 //�����ѯ���ƶ��ڵ�ID��
#define PC2C_RP_POSITION_GET_MOB_ID       2 //����ѯ���ƶ��ڵ�ID��
#define PC2C_RP_POSITION_X_HI             3 //����ѯ���ƶ��ڵ������Xֵ�߰�λ
#define PC2C_RP_POSITION_X_LO             4 //����ѯ���ƶ��ڵ������Xֵ�Ͱ�λ
#define PC2C_RP_POSITION_Y_HI             5 //����ѯ���ƶ��ڵ������Yֵ�߰�λ
#define PC2C_RP_POSITION_Y_LO             6 //����ѯ���ƶ��ڵ������Yֵ�Ͱ�λ

//���ÿ�ʼ��������Ϣ֡
#define PC2C_SET_JUDGE_LENGTH             3 //֡����
#define PC2C_SET_JUDGE_MSG_TYPE           0 //֡����
#define PC2C_SET_JUDGE_MOB_ID             1 //��Ҫ���õ��ƶ��ڵ��ID��
#define PC2C_SET_JUDGE_ACTION             2 //�����ʩ

/*S2C*/
//��Ӧ����������Ϣ
#define S2C_RP_BASIC_VALUE_LENGTH       3 //֡����
#define S2C_RP_BASIC_VALUE_NODE_TYPE    0 //�ڵ�����
#define S2C_RP_BASIC_VALUE_BROAD_CYC    1 //Sink�ڵ�Ĺ㲥����
#define S2C_RP_BASIC_VALUE_TEMP_CYC     2 //Sink�ڵ㷢���¶�����

//�����¶���Ϣ
#define S2C_TEMPERATURE_DATA_LENGTH     2 //֡����
#define S2C_TEMPERATURE_DATA_HIGH       0 //�¶����ݸ�8λ
#define S2C_TEMPERATURE_DATA_LOW        1 //�¶����ݵ�8λ

/*C2S*/
//���û�����Ϣ֡
#define C2S_SET_BASIC_VALUE_LENGTH       2 //֡����
#define C2S_SET_BASIC_VALUE_BROAD_CYC    0 //Sink�ڵ�Ĺ㲥����
#define C2S_SET_BASIC_VALUE_TEMP_CYC     1 //Sink�ڵ㷢���¶�����

/*M2M*/
//����or���ͱ��������ƶ��ڵ�Ķ�����Ϣ
#define M2M_TEAM_DATA_LENGTH            2 //֡����
#define M2M_TEAM_DATA_TEAM_ID           0 //�ƶ��ڵ����ID��
#define M2M_TEAM_DATA_MOB_ID            1 //�ƶ��ڵ�ID��

//����or���ͱ��������ƶ��ڵ�Ŀ�����Ϣ  
#define M2M_TEAM_CONTROL_LENGTH         4 //֡����
#define M2M_TEAM_CONTROL_REQ_MOB_ID     0 //������Ƶ��ƶ��ڵ�ID�ţ�Ҳ�����Լ���ID��
#define M2M_TEAM_CONTROL_GET_MOB_ID     1 //��Ҫ���Ƶ��ƶ��ڵ�ID��
#define M2M_TEAM_CONGROL_ACTION_HI      2 //���Ʋ����߰�λ
#define M2M_TEAM_CONGROL_ACTION_LO      3 //���Ʋ����Ͱ�λ


/*M2C*/
//��Ӧ����������Ϣ
#define M2C_RP_BASIC_VALUE_LENGTH           4 //֡����
#define M2C_RP_BASIC_VALUE_NODE_TYPE        0 //�ڵ�����
#define M2C_RP_BASIC_VALUE_MOB_ID           1 //�ƶ��ڵ�ID��
#define M2C_RP_BASIC_VALUE_TEAM_ID          2 //�ƶ��ڵ����ID��
#define M2C_RP_BASIC_VALUE_SEND_DELAY_TIME  3 //�ƶ��ڵ㷢���ӳ�����

//�ڵ�λ��������Ϣ
#define M2C_REQ_POSITION_LENGTH         2 //֡����
#define M2C_REQ_POSITION_REQ_MOB_ID     0 //������ƶ��ڵ��ID
#define M2C_REQ_POSITION_GET_MOB_ID     1 //��Ҫ��ȡ���ƶ��ڵ��ID

/*C2M*/
//���û�����Ϣ֡
#define C2M_SET_BASIC_VALUE_LENGTH            3 //֡����
#define C2M_SET_BASIC_VALUE_MOB_ID            0 //�ƶ��ڵ�ID��
#define C2M_SET_BASIC_VALUE_TEAM_ID           1 //�ƶ��ڵ����ID��
#define C2M_SET_BASIC_VALUE_SEND_DELAY_TIME   2 //�ƶ��ڵ㷢���ӳ�����

//��ȡ�ƶ��ڵ�λ�õ�������Ϣ֡
#define C2M_RP_POSITION_LENGTH           5 //֡����
#define C2M_RP_POSITION_GET_MOB_ID       0 //����ѯ���ƶ��ڵ�ID��
#define C2M_RP_POSITION_X_HI             1 //����ѯ���ƶ��ڵ������Xֵ�߰�λ
#define C2M_RP_POSITION_X_LO             2 //����ѯ���ƶ��ڵ������Xֵ�Ͱ�λ
#define C2M_RP_POSITION_Y_HI             3 //����ѯ���ƶ��ڵ������Yֵ�߰�λ
#define C2M_RP_POSITION_Y_LO             4 //����ѯ���ƶ��ڵ������Yֵ�Ͱ�λ

//���ÿ�ʼ��������Ϣ֡
#define C2M_SET_JUDGE_LENGTH             1 //֡����
#define C2M_SET_JUDGE_ACTION             0 //�����ʩ

/*R2C*/
//��Ӧ����������Ϣ
#define R2C_RP_BASIC_VALUE_LENGTH                      4 //֡����
#define R2C_RP_BASIC_VALUE_NODE_TYPE                   0 //�ڵ�����
#define R2C_RP_BASIC_VALUE_REF_ID                      1 //�ο��ڵ�ID��
#define R2C_RP_BASIC_VALUE_RECV_TIME_OUT               2 //�ο��ڵ���ճ�ʱʱ��
#define R2C_RP_BASIC_VALUE_RECV_DELAY_TIME             3 //�ο��ڵ�����ӳ�����

//����ʱ�����Ϣ
/*
   2-4λΪ1���ƶ��ڵ��ʱ������ݣ�5-7λΪ2���ƶ��ڵ��ʱ������ݣ�
   8-10Ϊ3���ƶ��ڵ��ʱ������ݣ�11-13λΪ4���ƶ��ڵ��ʱ�������
   ���ڲ�������ѭ���ķ������ʲ���Ҫ�г������֡
*/
#define R2C_DIFF_TIME_LENGTH            14 //֡����
#define R2C_DIFF_TIME_SEQ                0 //���к�
#define R2C_DIFF_TIME_REFID              1 //�ο��ڵ�ID

/*C2R*/
//���û�����Ϣ֡
#define C2R_SET_BASIC_VALUE_LENGTH           3 //֡����
#define C2R_SET_BASIC_VALUE_REF_ID           0 //�ο��ڵ�ID��
#define C2R_SET_BASIC_VALUE_RECV_TIME_OUT    1 //�ο��ڵ���ճ�ʱʱ��
#define C2R_SET_BASIC_VALUE_RECV_DELAY_TIME  2 //�ο��ڵ�����ӳ�����

/*A2C*/
//��Ӧ���óɹ�
#define A2C_SUCCESS_RESPONSE_LENGTH                    3 //֡����
#define A2C_SUCCESS_RESPONSE_NODE_TYPE                 0 //�ڵ�����
#define A2C_SUCCESS_RESPONSE_NODE_ID                   1 //�ڵ�ID
#define A2C_SUCCESS_RESPONSE_RESULT                    2 //���õĽ��

/*C2A*/
//��ѯ������Ϣ
//����Ҫ֡���ݣ�ֱ�ӷ��ͼ��ɣ�������AF_DataRequest�н����ݺͳ�������Ϊ0���ɡ�

#endif //#ifndef LOCATIONPROFILE_H
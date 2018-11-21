#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <signal.h>
#include <unistd.h>

#include "Mqtt_Client.h"

// 定义用户消息结构体
MQTT_USER_MSG  mqtt_user_msg;

static void deliverMessage(MQTTString *TopicName, MQTTMessage *msg, MQTT_USER_MSG *mqtt_user_msg)
{
	// 消息质量
	mqtt_user_msg->msgqos = msg->qos;
	// 保存消息
	memcpy(mqtt_user_msg->msg,msg->payload,msg->payloadlen);
	mqtt_user_msg->msg[msg->payloadlen] = 0;
	// 保存消息长度
	mqtt_user_msg->msglenth = msg->payloadlen;
	// 消息主题
	memcpy((char *)mqtt_user_msg->topic,TopicName->lenstring.data,TopicName->lenstring.len);
	mqtt_user_msg->topic[TopicName->lenstring.len] = 0;
	// 消息ID
	mqtt_user_msg->packetid = msg->id;
	// 标明消息合法
	mqtt_user_msg->valid = 1;		
}

static int WaitForPacket(MQTT_ClientStruct User_MQTTClient, unsigned char packettype,unsigned char times)
{
	int type;
	unsigned char n = 0;
	unsigned char buf[MSG_MAX_LEN];
	int buflen = sizeof(buf);
	do
	{
		// 读取数据包
		type = MQTTClientReadPacketTimeout(&User_MQTTClient, 2);
		if(type != -1)
			MQTTClientCtl(User_MQTTClient, type);
		n++;
	}while((type != packettype)&&(n < times));
	// 收到期望的包
	if(type == packettype)
		return 0;
	else 
		return -1;
}

void MQTTClientCtl(MQTT_ClientStruct User_MQTTClient, unsigned char  packtype)
// MQTT 解析数据包
{
	MQTTMessage msg;
	int rc;
	MQTTString receivedTopic;
	unsigned int len;

	switch(packtype)
	{
		case PUBLISH:
			if(MQTTDeserialize_publish(&msg.dup,(int*)&msg.qos, &msg.retained, &msg.id, &receivedTopic,
				(unsigned char **)&msg.payload, &msg.payloadlen, User_MQTTClient.buffer, User_MQTTClient.length) != 1)
			return;	
			deliverMessage(&receivedTopic,&msg,&mqtt_user_msg);

			if(msg.qos == QOS0)
			{
				User_MQTTClient.MsgCtl(&mqtt_user_msg);
				return;
			}
			if(msg.qos == QOS1)
			{
				len =MQTTSerialize_puback(User_MQTTClient.buffer,User_MQTTClient.length,mqtt_user_msg.packetid);
				if(len == 0)
					return;
				if(User_MQTTClient.sendPacket(User_MQTTClient.sock, User_MQTTClient.buffer,len)<0)
					return;	
				User_MQTTClient.MsgCtl(&mqtt_user_msg); 
					return;												
			}

			if(msg.qos == QOS2)
			{
				len = MQTTSerialize_ack(User_MQTTClient.buffer, User_MQTTClient.length, PUBREC, 0, mqtt_user_msg.packetid);			                
				if(len == 0)
					return;
				User_MQTTClient.sendPacket(User_MQTTClient.sock, User_MQTTClient.buffer,len);	
			}
			break;
		case PUBREL:
			rc = MQTTDeserialize_ack(&msg.type,&msg.dup, &msg.id, User_MQTTClient.buffer,User_MQTTClient.length);
			if((rc != 1)||(msg.type != PUBREL)||(msg.id != mqtt_user_msg.packetid))
			return ;
			if(mqtt_user_msg.valid == 1)
			{
				User_MQTTClient.MsgCtl(&mqtt_user_msg);
			}
			len = MQTTSerialize_pubcomp(User_MQTTClient.buffer, User_MQTTClient.length, msg.id);	                   	
			if(len == 0)
				return;
			User_MQTTClient.sendPacket(User_MQTTClient.sock, User_MQTTClient.buffer, len);										
			break;
		case PUBACK:
			break;
		case PUBREC:
			break;
		case PUBCOMP:
			break;
		default:
			break;
	}
}

int MQTTMsgPublish(MQTT_ClientStruct User_MQTTClient, char *topic, char qos, char retained, unsigned char * msg, unsigned int msg_len)
// MQTT 发送数据
{
	MQTTString topicString = MQTTString_initializer;
	unsigned int packid = 0,packetidbk;
	int len;

	// 填充主题
	topicString.cstring = (char *)topic;

	// 填充数据包 ID
	if((qos == QOS1)||(qos == QOS2))
	{ 
		packid = User_MQTTClient.GetNextPackID();
	}
	else
	{
		qos = QOS0;
		retained = 0;
		packid = 0;
	}

	// 推送消息
	len = MQTTSerialize_publish(User_MQTTClient.buffer, User_MQTTClient.length, 0, qos, retained, packid, topicString, (unsigned char*)msg, msg_len);
	if(len <= 0)
		return -1;
	if(transport_sendPacketBuffer(User_MQTTClient.sock, User_MQTTClient.buffer, len) < 0)	
		return -2;	

	// 质量等级0, 不需要返回
	if(qos == QOS0)
	{
		return 0;
	}

	//等级1
	if(qos == QOS1)
	{
		//等级 PUBACK
		if(WaitForPacket(User_MQTTClient, PUBACK, 5) < 0)
			return -3;
		return 1;

	}

	//等级2
	if(qos == QOS2)	
	{
		//µÈ´ýPUBREC
		if(WaitForPacket(User_MQTTClient, PUBREC, 5) < 0)
			return -3;
		//·¢ËÍPUBREL
		len = MQTTSerialize_pubrel(User_MQTTClient.buffer, User_MQTTClient.length, 0, packetidbk);
		if(len == 0)
			return -4;
		if(transport_sendPacketBuffer(User_MQTTClient.sock, User_MQTTClient.buffer, len) < 0)	
			return -6;			
		//µÈ´ýPUBCOMP
		if(WaitForPacket(User_MQTTClient, PUBREC, 5) < 0)
			return -7;
		return 2;
	}
	// 等级错误
	return -8;
}

int MQTTClientReadPacketTimeout(MQTT_ClientStruct *User_MQTTClient, unsigned int timeout)
// MQTT 读取数据包
{
	fd_set readfd;
	struct timeval tv;

	if(timeout != 0)
	{
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		FD_ZERO(&readfd);
		FD_SET(User_MQTTClient->sock,&readfd); 

		// 等待可读事件--等待超时
		if(select(User_MQTTClient->sock+1,&readfd,NULL,NULL,&tv) == 0)
				return -1;
		// 有可读事件--没有可读事件
		if(FD_ISSET(User_MQTTClient->sock,&readfd) == 0)
				return -1;
	}

	// 读取 TCP/IP 事件
	return MQTTPacket_read(User_MQTTClient->buffer, User_MQTTClient->length, User_MQTTClient->getdata);
}

int MQTTClientSendPingReq(MQTT_ClientStruct User_MQTTClient)
// MQTT 发送心跳
{
	int rc = 0;
	int len; 
	fd_set readfd;
	struct timeval tv;

	tv.tv_sec = 5;
	tv.tv_usec = 0;

	FD_ZERO(&readfd);
	FD_SET(User_MQTTClient.sock, &readfd);			

	len = MQTTSerialize_pingreq(User_MQTTClient.buffer, User_MQTTClient.length);
	User_MQTTClient.sendPacket(User_MQTTClient.sock, User_MQTTClient.buffer, len);

	//等待可读事件
	if(select(User_MQTTClient.sock+1, &readfd, NULL, NULL, &tv) == 0)
		return MqttClientWaitReadErr;

	//有可读事件
	if(FD_ISSET(User_MQTTClient.sock, &readfd) == 0)
		return MqttClientReadErr;

	if(MQTTPacket_read(User_MQTTClient.buffer, User_MQTTClient.length, User_MQTTClient.getdata) != PINGRESP)
		return MqttClientReadErr;

	return MqttClientSuccess;
}

int MQTTClientSubscribe(MQTT_ClientStruct User_MQTTClient, char *topic, enum QoS pos)
// MQTT 订阅消息
{
	MQTTString topicString = MQTTString_initializer;  
	static unsigned short PacketID = 0;
	unsigned short packetidbk = 0;
	int req_qos,qosbk;
	int conutbk = 0;
	int rc = 0;
	int len;

	fd_set readfd;
	struct timeval tv;

	memset(User_MQTTClient.buffer, 0, User_MQTTClient.length);

	tv.tv_sec = 2;
	tv.tv_usec = 0;

	FD_ZERO(&readfd);
	FD_SET(User_MQTTClient.sock,&readfd);		

	topicString.cstring = (char *)topic;
	// 复制主题
	req_qos = pos;
	// 订阅质量

	len = MQTTSerialize_subscribe(User_MQTTClient.buffer, User_MQTTClient.length, 0, PacketID++, 1, &topicString, &req_qos);
	// 串行化订阅消息
	if(User_MQTTClient.sendPacket(User_MQTTClient.sock, User_MQTTClient.buffer, len) < 0)
		// 发送TCP数据
		return MqttClientSentErr;

	if(select(User_MQTTClient.sock+1,&readfd,NULL,NULL,&tv) == 0)
	// 等待可读事件--等待超时
		return MqttClientWaitReadErr;

	if(FD_ISSET(User_MQTTClient.sock,&readfd) == 0)
	// 有可读事件--没有可读事件
		return MqttClientReadNone;

	if(MQTTPacket_read(User_MQTTClient.buffer, User_MQTTClient.length, User_MQTTClient.getdata) != SUBACK)
	// 等待订阅返回--未收到订阅返回
		return MqttClientSubscribeNone;

	if(MQTTDeserialize_suback(&packetidbk,1, &conutbk, &qosbk, User_MQTTClient.buffer, User_MQTTClient.length) != 1)
	// 拆订阅回应包
		return MqttClientSubackErr;


	if((qosbk == 0x80)||(packetidbk != (PacketID-1)))
	// 检测返回数据的正确性
		return MqttClientDataErr;

	// 订阅成功
	return MqttClientSuccess;
}

int MQTTClientInit(MQTT_ClientStruct User_MQTTClient, MQTTPacket_connectData connectData)
// MQTT 初始化
{
	fd_set readfd;
	struct timeval tv;

	int len = 0;
	unsigned char sessionPresent,connack_rc;

	memset(User_MQTTClient.buffer, 0, User_MQTTClient.length);

	FD_ZERO(&readfd);
	FD_SET(User_MQTTClient.sock, &readfd);	

	len = MQTTSerialize_connect(User_MQTTClient.buffer,
			User_MQTTClient.length, &connectData);
	// 串行化连接消息
	if(User_MQTTClient.sendPacket(User_MQTTClient.sock, User_MQTTClient.buffer, len) < 0)
		// 发送TCP数据
		return MqttClientSentErr;

	if(select(User_MQTTClient.sock+1, &readfd, NULL, NULL, &tv) == 0)
		// 等待可读事件
		return MqttClientWaitReadErr;
	if(FD_ISSET(User_MQTTClient.sock,&readfd) == 0)
		// 有可读事件
		return MqttClientReadErr;

	if(MQTTPacket_read(User_MQTTClient.buffer, User_MQTTClient.length, User_MQTTClient.getdata) != CONNACK)
		return MqttClientReadErr;	
	// 拆解连接回应包
	if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, User_MQTTClient.buffer, User_MQTTClient.length) != 1
			|| connack_rc != 0)
		return MqttClientConnackErr;

	if(sessionPresent == 1)
		return MqttClientNoResubscribe;//不需要重新订阅
	else 
		return MqttClientSuccess;
}

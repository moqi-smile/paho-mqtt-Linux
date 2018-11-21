#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <signal.h>
#include <unistd.h>

#include <time.h> 
#include <sys/time.h> 

#include "Mqtt_Client.h"

#include<pthread.h>

// int mysock = 0;
int toStop = 0;

MQTT_USER_MSG  User_MqttMsg;

static void User_MsgCtl(MQTT_USER_MSG  *msg);
static int GetNextPackID(void);

MQTT_ClientStruct user_Client = 
{
	.sock = -1,
	.Status = DisConnect,
	.length = MQTT_BUFFERLENTH,
	.open = transport_open,
	.getdata = transport_getdata,
	.sendPacket = transport_sendPacketBuffer,
	.MsgCtl = User_MsgCtl,
	.GetNextPackID = GetNextPackID,
};

void cfinish(int sig)
{
    signal(SIGINT, NULL);
	toStop = 1;
}

void stop_init(void)
{
	signal(SIGINT, cfinish);
	signal(SIGTERM, cfinish);
}

void User_MsgCtl(MQTT_USER_MSG  *msg)
{
	printf("MQTT>>****收到客户端自己订阅的消息****\n");
	// 处理消息
	switch(msg->msgqos)
	{
		case 0:
			printf("MQTT>>消息质量: QoS0\n");
			break;
		case 1:
			printf("MQTT>>消息质量: QoS1\n");
			break;
		case 2:
			printf("MQTT>>消息质量: QoS2\n");
			break;
		default:
			printf("MQTT>>错误的消息质量\n");
			break;
	}
	printf("MQTT>>消息主题; %s\r\n",msg->topic);	
	printf("MQTT>>消息内容: %s\r\n",msg->msg);	
	printf("MQTT>>消息长度: %d\r\n",msg->msglenth);	 

	// 处理后销毁数据
	
	msg->valid  = 0;
}

static int GetNextPackID(void)
{
	 static unsigned int pubpacketid = 0;
	 return pubpacketid++;
}

void *Mqtt_ClentTask(void *argv)
{
	MQTTPacket_connectData connectData= MQTTPacket_connectData_initializer;

	int rc = 0, type;
	int PublishTime = 0;
	int heartbeatTime = 0;

    struct timeval    sys_tv, tv;
    struct timezone		tz; 
	fd_set readfd;

	tv.tv_sec = 5;
	tv.tv_usec = 0;

	connectData.willFlag = 0;
	// 创建 MQTT 客户端连接参数
	connectData.MQTTVersion = 4;
	// MQTT 版本
	connectData.clientID.cstring = ClientID;
	// 客户端ID
	connectData.keepAliveInterval = KEEPLIVE_TIME;
	// 保活间隔
	connectData.username.cstring = UserName;
	// 用户名
	connectData.password.cstring = UserPassword;
	// 用户密码
	connectData.cleansession = 1;

	user_Client.Status = DisConnect;
	while (1)
	{
		user_Client.sock = user_Client.open(HOST_NAME, HOST_PORT);
		if(user_Client.sock >= 0)
		{
			break;
		}

		printf("ReConnect\n");
		sleep(3);
	}

	rc = MQTTClientInit(user_Client, connectData);
	if (rc < MqttClientSuccess)
	{
		printf("MQTT_reconnect %d\n", rc);
		goto MQTT_reconnect;
	}

	rc = MQTTClientSubscribe(user_Client, SubscribeMsg, QOS0);
	// 订阅 SubscribeMsg
	if(rc < 0)
		goto MQTT_reconnect;

	printf("开始循环接收消息...\r\n");

	user_Client.Status = Connect;
	while (1)
	{
		gettimeofday(&sys_tv, &tz);

		FD_ZERO(&readfd);
		// 推送消息
		FD_SET(user_Client.sock,&readfd);						  

		select(user_Client.sock+1, &readfd, NULL, NULL, &tv);
		// 等待可读事件

		if(FD_ISSET(user_Client. sock, &readfd) != 0)
		{
			// 判断 MQTT 服务器是否有数据
			type = MQTTClientReadPacketTimeout(&user_Client, 0);
			if(type != -1)
				MQTTClientCtl(user_Client, type);
		} 

		if ((sys_tv.tv_sec-heartbeatTime ) > 15)
		{
			// 发送心跳包
			printf("tv_sec:%ld\n",sys_tv.tv_sec);
			heartbeatTime = sys_tv.tv_sec;

			if(MQTTClientSendPingReq(user_Client) < 0)
				goto MQTT_reconnect;
		}
		// if ((sys_tv.tv_sec -PublishTime  ) > 5)
		// {
			// printf("tv_sec:%ld\n",sys_tv.tv_sec);
			// PublishTime = sys_tv.tv_sec;


		// }
	}
MQTT_reconnect:
	printf("MQTT_reconnect\n");

	return 0;
}

int main(int argc, char *argv[])
{
	pthread_t MqttClient_pthread;
	stop_init();

	pthread_create(&MqttClient_pthread, NULL, Mqtt_ClentTask, NULL);
	// Mqtt_ClentTask(NULL);

	while (1)
	{
		printf ("Moqi\r\n");

		if (user_Client.Status == Connect)
		if (MQTTMsgPublish(user_Client, ReleaseMsg, QOS0, 0,
			(unsigned char *)"This is my first MQTT programme!",strlen("This is my first MQTT programme!")) != 0)
		{
			printf("<0\n");
		}		
		sleep(10);
	}

	return 0;
}

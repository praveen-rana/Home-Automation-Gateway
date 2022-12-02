/*
 ============================================================================
 Name        : Gateway_FW.c
 Author      : Praveen
 Version     :
 Copyright   : Your copyright notice
 Description : Gateway Firmware
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include "gpio.h"
#include <poll.h>

/***************************************** PROTOTYPES **********************************/
int main (int argc, char* const argv[]);
void* networkThread (void * arg);
void ProcessSensorDataPackets (char* networkBuffer);
void MotorControl (void);
void InitSensorDB (void);
int StringToInt (char *s, int n);
void UpdateNetworkLed (char * pCmd);
void InitNetworkStatusIndication (void);


/**************************** SENSOR DATA PACKET PARSING *******************************/
#define GATEWAY_IP									"192.168.1.103"
#define PORT_PACKET_PROTOCOL						2017

#define NETWORK_BUFFER_TOTAL_LEN 					1024
#define DATA_PACKET_TOTAL_HUMIDITY_SENSORS			2
#define DATA_PACKET_TOTAL_WATER_LEVEL_SENSORS		0
#define DATA_PACKET_TOTAL_WATER_SENSORS				0
#define DATA_PACKET_TOTAL_SENSORS					(DATA_PACKET_TOTAL_HUMIDITY_SENSORS 		\
													+	DATA_PACKET_TOTAL_WATER_LEVEL_SENSORS	\
													+	DATA_PACKET_TOTAL_WATER_SENSORS)
#define DATALEN_PER_SENSOR							20

typedef enum {
	eSensorType_Humidity = 0,
	eSensorType_WaterLevel,
	eSensorType_WaterSensor,
}eSensorType;

typedef struct {
	uint16_t analogVal;
	uint8_t digitalVal;
	uint8_t sensorNumber;		//There can be 255 sensors of each type
	uint8_t validity;
	uint8_t reserved[3];
}eSensorData;

typedef struct {
	eSensorType sensorType;
	eSensorData sensorData;
}sSensorDataPacket_t;

sSensorDataPacket_t SensorDataPackets[DATA_PACKET_TOTAL_SENSORS];


/**************************** SENSOR DATABASE & DEFINITIONS ****************************/
typedef enum
{
	HUMIDITY_SENSOR_1 = 0,
	HUMIDITY_SENSOR_2,
	TOTAL_HUMIDITY_SENSORS
}eHUMIDITY_SENSOR;

/*
 * Sensor Data
 */
typedef struct {
	uint16_t        value_analog;
	uint16_t        motorControlAnalog_threshold;
	uint8_t         motorControlAnalog;
	uint8_t			value_binary;
	uint8_t         motorControlBinary;
	uint8_t			reserved;
}sHumidity_t;

sHumidity_t HumidityValue[TOTAL_HUMIDITY_SENSORS];


/************************************* NETWORK LED CONTROL *****************************/
#define NETWORK_STATUS_UNINITIALIZED			"none"
#define NETWORK_STATUS_SOCKET_INITIALIZED		"default-on"
#define NETWORK_STATUS_SOCKET_FAILED			"none"
#define NETWORK_STATUS_CONNECTED				"heartbeat"
#define NETWORK_STATUS_CONNECTION_TIMEOUT		"default-on"
#define NETWORK_STATUS_CONNECTION_BREAK			"default-on"


/**************************** SYSTEM CALL RETURN TYPES ********************************/

typedef enum
{
	RETVAL_ERR_SOCKETS = -1,
	RETVAL_SUCCESS = 0
}eRETVAL;


/************************************ UTILITIES ***************************************/
int StringToInt (char *s, int n)
{
    int num = 0;
    int i = 0;

    // Iterate till length of the string
    for (i = 0; i < n; i++)
    {
        num = num * 10 + (s[i] - 48);
    }

    return (num);
}


/************************************* THREADS and APIs ********************************/

/*
 * Update Network LED status
 */
void UpdateNetworkLed (char * pCmd)
{
	write_trigger_values (NETWORK_STATUS_INDICATION_1, pCmd);
	write_trigger_values (NETWORK_STATUS_INDICATION_2, pCmd);
}

/*
 * Initialize Network Status indications
 */
void InitNetworkStatusIndication (void)
{
	write_trigger_values (NETWORK_STATUS_INDICATION_1, "none");
	write_trigger_values (NETWORK_STATUS_INDICATION_2, "none");
}


/*
 * Process the data received from sensor node and update sensor data base
 */
void ProcessSensorDataPackets (char* networkBuffer)
{
	uint16_t index = 0;
	char InBuffer[5];
	uint16_t value;

	for (index = 0; index < DATA_PACKET_TOTAL_SENSORS; index++)
	{
		memset(InBuffer, '\0', 5);
		strncpy (InBuffer, (const char*)networkBuffer, 4);
		value = StringToInt(InBuffer, 4);

		SensorDataPackets[index].sensorData.validity = value;

		memset(InBuffer, '\0', 5);
		strncpy (InBuffer, (const char*)(networkBuffer+4), 4);
		value = StringToInt(InBuffer, 4);

		SensorDataPackets[index].sensorType = value;

		memset(InBuffer, '\0', 5);
		strncpy (InBuffer, (const char*)(networkBuffer+8), 4);
		value = StringToInt(InBuffer, 4);

		SensorDataPackets[index].sensorData.sensorNumber = value;

		memset(InBuffer, '\0', 5);
		strncpy (InBuffer, (const char*)(networkBuffer+12), 4);
		value = StringToInt(InBuffer, 4);

		SensorDataPackets[index].sensorData.digitalVal = value;

		memset(InBuffer, '\0', 5);
		strncpy (InBuffer, (const char*)(networkBuffer+16), 4);
		value = StringToInt(InBuffer, 4);

		SensorDataPackets[index].sensorData.analogVal = value;

		networkBuffer += DATALEN_PER_SENSOR;
		#if 1
			printf("\n\n\rSensor %d value:",index);
			printf("\n\rValidity = %d", SensorDataPackets[index].sensorData.validity);
			printf("\n\rSensor Type = %d", SensorDataPackets[index].sensorType);
			printf("\n\rSensor Num = %d", SensorDataPackets[index].sensorData.sensorNumber);
			printf("\n\rDigital value = %d", SensorDataPackets[index].sensorData.digitalVal);
			printf("\n\rAnalog value = %d", SensorDataPackets[index].sensorData.analogVal);
		#endif

		HumidityValue[index].value_analog = SensorDataPackets[index].sensorData.analogVal;
		HumidityValue[index].value_binary = SensorDataPackets[index].sensorData.digitalVal;
	}
}


/*
 * Network Thread, Shall connect with Client-sensor node, receive and process the data
 */
void* networkThread (void * arg)
{
	int serverSocket_fd;
	int clientSocket_fd;
	char bufferIn[NETWORK_BUFFER_TOTAL_LEN];
	//char bufferOut[NETWORK_BUFFER_TOTAL_LEN];

	struct sockaddr_in name;
	//struct hostent* hostInfo;

	struct sockaddr_in clientName;
	socklen_t clientAddrLen;

	int nfds = 1; // Server and Client sockets
	struct pollfd pollStruct;
	struct pollfd *pfds = &pollStruct;

	printf("\n\rData Concentrator: Starting Network Thread...");
	InitNetworkStatusIndication();
	UpdateNetworkLed (NETWORK_STATUS_UNINITIALIZED);

	// Create socket
	serverSocket_fd = socket (PF_INET, SOCK_STREAM, 0);
	if (serverSocket_fd == RETVAL_ERR_SOCKETS)
	{
		printf("\nNetwork Thread failed to create Socket ! Entering dead loop...");
		UpdateNetworkLed (NETWORK_STATUS_SOCKET_FAILED);
		for (;;){sleep(5);}
	}

	// Store server address in Socket
	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = inet_addr(GATEWAY_IP);
	name.sin_port = PORT_PACKET_PROTOCOL;

	// Bind address to Socket
	if (RETVAL_ERR_SOCKETS == bind (serverSocket_fd, (struct sockaddr*) &name, sizeof(name)))
	{
		perror("\n\rError: ");
		printf("\n\rNetwork Thread failed to Bind address to Socket ! Entering dead loop...");
		UpdateNetworkLed (NETWORK_STATUS_SOCKET_FAILED);
		for (;;){sleep(5);}
	}

	// Listen to incoming connections
	if (RETVAL_ERR_SOCKETS == listen (serverSocket_fd, 10))
	{
		perror("\n\rError: ");
		printf("\n\rNetwork Thread failed to listen (mark socket as passive) on Socket ! Entering dead loop...");
		UpdateNetworkLed (NETWORK_STATUS_SOCKET_FAILED);
		for (;;){sleep(5);}
	}

	printf("\n\rData Concentrator: Network Thread ==> Waiting for new connection");
	UpdateNetworkLed (NETWORK_STATUS_SOCKET_INITIALIZED);

	for (;;)
	{
		clientSocket_fd = accept (serverSocket_fd, (struct sockaddr *)&clientName, &clientAddrLen);

		if (clientSocket_fd > 0)
		{
			UpdateNetworkLed (NETWORK_STATUS_CONNECTED);
			printf("\nNetwork Thread Accepted a connection request !");

			pfds->fd = clientSocket_fd;
			pfds->events = (POLLIN | POLLHUP | POLLERR);
			pfds->revents = 0;
			nfds = 1;

			for (;;)
			{
				poll(pfds, nfds, -1); // Wait for events on Socket

				if ((pfds->revents & POLLHUP) || (pfds->revents & POLLERR))
				{
					printf ("\n\r POLLHUP");
					printf ("\n\rDetected Connection break !");
					break;
				}
				else if (pfds->revents & POLLIN)
				{
					printf ("\n\r POLLIN");
					memset (bufferIn, '\0', NETWORK_BUFFER_TOTAL_LEN);
					read (clientSocket_fd, bufferIn, NETWORK_BUFFER_TOTAL_LEN);
					printf("\n\n\rReceived message: %s",bufferIn);
					// Process Sensor Data packets
					ProcessSensorDataPackets (bufferIn);
				}
				else
				{
					printf ("\n\r Other.. %d", pfds->revents);
				}
				pfds->revents = 0;
			}

			close (clientSocket_fd);
			UpdateNetworkLed (NETWORK_STATUS_CONNECTION_BREAK);
			printf ("\n\rListening for new connection request...");
		}
		else
		{
			printf("\nNetwork Thread failed to accept incoming connection ! Entering dead loop...");
			for (;;){sleep(5);}
		}
	}
}


/*
 * Initialize Sensor's data
 */
void InitSensorDB (void)
{
	memset (&HumidityValue, 0, sizeof(HumidityValue));
	HumidityValue[HUMIDITY_SENSOR_1].motorControlBinary = 1;
	HumidityValue[HUMIDITY_SENSOR_2].motorControlBinary = 1;
}


/*
 * Performs motor control based on binay value from sensor
 */
void MotorControl (void)
{
	if (HumidityValue[HUMIDITY_SENSOR_1].value_binary && HumidityValue[HUMIDITY_SENSOR_1].motorControlBinary)
	{
		write_trigger_values (LED_MOTOR_1, MOTOR_ON);
	}
	else
	{
		write_trigger_values (LED_MOTOR_1, MOTOR_OFF);
	}

	if (HumidityValue[HUMIDITY_SENSOR_2].value_binary && HumidityValue[HUMIDITY_SENSOR_2].motorControlBinary)
	{
		write_trigger_values (LED_MOTOR_2, MOTOR_ON);
	}
	else
	{
		write_trigger_values (LED_MOTOR_2, MOTOR_OFF);
	}
}

void create_NetworkThread(pthread_t *pThreadId)
{
	// Create network Thread
	pthread_create (pThreadId, NULL, &networkThread, NULL);
}


/*
 * Main Thread, Does decision making based on Data received from Sensors
 * Creating Network thread, any new thread must be created from here.
 */
int main (int argc, char* const argv[])
{
	pthread_t network_thr_id;
	printf("\n\rData Concentrator: Starting Main Thread...");

	// Initialize Book keeping database
	InitSensorDB ();

	create_NetworkThread (&network_thr_id);

	for (;;)
	{
		MotorControl ();
		sleep(1);
	}
	return 0;
}

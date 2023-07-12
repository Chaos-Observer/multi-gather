
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/time.h>

#include "periphery/src/i2c.h"
#include "periphery/src/serial.h"

#include "periphery/tests/test.h"

#include "tests/uart_protocol.h"
#include "tests/i2c_protocol.h"

const char *i2c_bus_path;
const char *serial_device;

volatile uint16_t ref_voltage;

extern i2c_t *i2c;
volatile RadarData* radarData;

#define DEVICE_I2C_ADDR 0x48

typedef struct Sequence_Value{
	unsigned char radar_heartRate;
	unsigned char radar_breathRate;

	unsigned char radar_heartWave[5];
	unsigned char radar_breathWave[5];
	
	unsigned char radar_breathResult;
	char radar_humanDet;
	unsigned char radar_movementValue;

	int16_t ref_heartWave;
	char ref_heartRate;
	char ref_breathRate;
}sequence_value;

int *uart_rx_date_opt(void){
	serial_t *serial;
	serial = serial_new();
	
	passert(serial != NULL);
    passert(serial_open(serial, serial_device, 115200) == 0);

	UART_Protocol();
	
	unsigned char buf[136];
	unsigned short ret =1;

	while(1){
		
		ret = serial_poll(serial,10);
		if(ret == 1){
			ret = serial_read(serial, buf, sizeof(buf), 200);
			UART_Recv(buf, ret);
            printf("serial thread read %d bytes date.\n",ret);
			UART_Service(radarData);
		}
	}	
	
	passert(serial_close(serial) == 0);
	serial_free(serial);
	printf("Serial Free.\n");
	return 0;
}


int *i2c_rx_date_opt(void){

	i2c = i2c_new();


    passert(i2c != NULL);
    passert(i2c_open(i2c, i2c_bus_path) == 0);

	/* Read byte at address 0x100 of EEPROM */
    // uint8_t msg_addr[2] = { 0x01, 0x00 };
    // uint8_t msg_data[1] = { 0xff, };
    // struct i2c_msg msgs[2] =
    //     {
    //         /* Write 16-bit address */
    //         { .addr = DEVICE_I2C_ADDR, .flags = 0, .len = 2, .buf = msg_addr },
    //         /* Read 8-bit data */
    //         { .addr = DEVICE_I2C_ADDR, .flags = I2C_M_RD, .len = 1, .buf = msg_data},
    //     };
	// i2c_transfer(i2c, msgs, 2);
    
	setAddr_ADS1115(DEVICE_I2C_ADDR);
	setGain(eGAIN_ONE);
    setMode(eMODE_SINGLE);       // single-shot mode
    setRate(eRATE_250);          // 128SPS (default)
    setOSMode(eOSMODE_SINGLE);   // Set to start a single-conversion
	while(1){
		// printf("readreg,01:%02x,02:%02x,03:%02x\n",(uint16_t)readAdsReg(DEVICE_I2C_ADDR, 0x01),(uint16_t)readAdsReg(DEVICE_I2C_ADDR, 0x02),(uint16_t)readAdsReg(DEVICE_I2C_ADDR, 0x03));
			ref_voltage = readVoltage(0);
			printf("ref_voltage is %d \n",ref_voltage);

	}

	passert(i2c_close(i2c) == 0);
	i2c_free(i2c);
	printf("I2C Free.\n");
    return 0;
}


int main(int argc, char *argv[]){
	if (argc < 3) {
        fprintf(stderr, "Usage: %s <I2C device> <Serial device>\n\n", argv[0]);
        fprintf(stderr, "Hint: for Raspberry Pi 3, enable I2C1 with:\n");
        fprintf(stderr, "   $ echo \"dtparam=i2c_arm=on\" | sudo tee -a /boot/config.txt\n");
        fprintf(stderr, "   $ sudo reboot\n");
        fprintf(stderr, "Use pins I2C1 SDA (header pin 2) and I2C1 SCL (header pin 3),\n\n");
        
        fprintf(stderr, "Hint: for Raspberry Pi 3, enable UART0 with:\n");
        fprintf(stderr, "   $ echo \"dtoverlay=pi3-disable-bt\" | sudo tee -a /boot/config.txt\n");
        fprintf(stderr, "   $ sudo systemctl disable hciuart\n");
        fprintf(stderr, "   $ sudo reboot\n");
        fprintf(stderr, "   (Note that this will disable Bluetooth)\n");
        fprintf(stderr, "Use pins UART0 TXD (header pin 8) and UART0 RXD (header pin 10),\n");
        fprintf(stderr, "connect a loopback between TXD and RXD, and run this test with:\n\n");
        
        fprintf(stderr, "and run this test with:\n");
        fprintf(stderr, "    %s /dev/i2c-1 /dev/ttyAMA0 \n\n", argv[0]);
        exit(1);
    }
	i2c_bus_path = argv[1];
	serial_device = argv[2];
	
	pthread_t thid_uart,thid_i2c;
	uint8_t x;
	
	radarData = (RadarData*)malloc(sizeof(RadarData));//不可静态使用，在进程中使用
    
	if(pthread_create(&thid_uart, NULL,(int *)uart_rx_date_opt, NULL) != 0){
		printf("uart thread create fail!\n");
		exit(1);
	}
	
	if(pthread_create(&thid_i2c, NULL,(int *)i2c_rx_date_opt, NULL) != 0){
		printf("i2c thread create fail!\n");
		exit(1);
	}

	sequence_value *oldValue;
	sequence_value *outValue;


	oldValue = (sequence_value*)malloc(sizeof(sequence_value));
	outValue = (sequence_value*)malloc(sizeof(sequence_value));
	

	struct timeval time_record;
    gettimeofday( &time_record, NULL );

	FILE *fp = fopen("./info.csv","w+");

	if (fp == NULL) {
        // fprintf(stderr, "fopen() failed.\n");
        system("touch ./info.csv");
		printf("fopen() failed. touch file.\n");
		fp = fopen("./info.csv","w+");
    }

	fprintf(fp, "TIME,BreathRate,HeartRate,HeartWave,BreathWave,HumanDet,BreathResult,MovementValue,RefHeartWave\n");
	
	usleep(1000000); //wait radarData be malloc
	
	while(1){
		// printf("main loop!\n");
		usleep(3000); 
		if(oldValue->radar_breathRate != radarData->breathe_rate){
			oldValue->radar_breathRate = radarData->breathe_rate;
			outValue->radar_breathRate = radarData->breathe_rate;
		}else{
			outValue->radar_breathRate = 255;
		}

		if(oldValue->radar_heartRate != radarData->heartbeat_rate){
			oldValue->radar_heartRate = radarData->heartbeat_rate;
			outValue->radar_heartRate = radarData->heartbeat_rate;
		}else{
			outValue->radar_heartRate = 255;
		}
		
		if((oldValue->radar_heartWave[0] == radarData->heartbeat_wave[0]) && (oldValue->radar_heartWave[1] == radarData->heartbeat_wave[1]) && (oldValue->radar_heartWave[2] == radarData->heartbeat_wave[2])){
			for(x = 0;x<5;x++){
			outValue->radar_heartWave[x] = 255;
			}
		}else{
			for(x = 0;x<5;x++){
			oldValue->radar_heartWave[x] = radarData->heartbeat_wave[x];
			outValue->radar_heartWave[x] = radarData->heartbeat_wave[x];
			}
		}
		
		if((oldValue->radar_breathWave[0] == radarData->breathe_wave[0]) && (oldValue->radar_breathWave[1] == radarData->breathe_wave[1]) && (oldValue->radar_breathWave[2] == radarData->breathe_wave[2])){
			for(x = 0;x<5;x++){
			outValue->radar_breathWave[x] = 255;
			}
		}else{
			for(x = 0;x<5;x++){
			oldValue->radar_breathWave[x] = radarData->breathe_wave[x];
			outValue->radar_breathWave[x] = radarData->breathe_wave[x];
			}
		}
		

		if(oldValue->radar_humanDet != radarData->human_det){
			oldValue->radar_humanDet = radarData->human_det;
			outValue->radar_humanDet = radarData->human_det;
		}else{
			outValue->radar_humanDet = -1;
		}

		if(oldValue->radar_breathResult != radarData->breathe_result){
			oldValue->radar_breathResult = radarData->breathe_result;
			outValue->radar_breathResult = radarData->breathe_result;
		}else{
			outValue->radar_breathResult = 255;
		}

		if(oldValue->radar_movementValue != radarData->movement_value){
			oldValue->radar_movementValue = radarData->movement_value;
			outValue->radar_movementValue = radarData->movement_value;
		}else{
			outValue->radar_movementValue = 255;
		}

		if(oldValue->ref_heartWave != ref_voltage){
			oldValue->ref_heartWave = ref_voltage;
			outValue->ref_heartWave = ref_voltage;
		}else{
			outValue->ref_heartWave = -1;
		}
		
		gettimeofday( &time_record, NULL );
		int p_time = time_record.tv_sec*1000 + time_record.tv_usec/1000 ;

		char outHeartWave[12],outBreathWave[12];

		if((outValue->radar_heartWave[0]== 255) && (outValue->radar_heartWave[1]== 255) && (outValue->radar_heartWave[2]== 255))
		{
			snprintf(outHeartWave, 4, "NULL\n");
		}else{
			snprintf(outHeartWave, 10, "%d:%d:%d:%d:%d\n",outValue->radar_heartWave[0],outValue->radar_heartWave[1],outValue->radar_heartWave[2],outValue->radar_heartWave[3],outValue->radar_heartWave[4] );
		}

		if((outValue->radar_breathWave[0]== 255) && (outValue->radar_breathWave[1]== 255) && (outValue->radar_breathWave[2]== 255))
		{
			snprintf(outBreathWave, 4, "NULL\n");
		}else{
			snprintf(outBreathWave, 10, "%d:%d:%d:%d:%d\n",outValue->radar_breathWave[0],outValue->radar_breathWave[1],outValue->radar_breathWave[2],outValue->radar_breathWave[3],outValue->radar_breathWave[4] );
		}
		
		// fprintf(fp, "%d,%s,%s,%s,%s,%s,%s,%d\n",p_time, \
		// outValue->radar_breathRate, \
		// outValue->radar_heartRate, \
		// outHeartWave, \
		// outBreathWave, \
		// outValue->radar_humanDet, \
		// outValue->radar_breathResult, \
		// outValue->radar_movementValue, \
		// outValue->ref_heartWave);

	}
	
	fclose(fp);
	printf("File fb Free.\n");
	return 0;
}

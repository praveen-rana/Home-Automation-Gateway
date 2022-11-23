/*
 ============================================================================
 Name        : gpio.h
 Version     :
 Copyright   : Your copyright notice
 Description : header for gpio configuration
 ============================================================================
 */

#ifndef GPIO_DRIVER_H_
#define GPIO_DRIVER_H_


#define SOME_BYTES          100

// Output Pins; LED
/* This is the path corresponds to USER LEDS in the 'sys' directory */
#define SYS_FS_LEDS_PATH 	"/sys/class/leds"


// LED-Motor translation
#define MOTOR_ON			"default-on"
#define MOTOR_OFF			"none"
#define LED_MOTOR_1			100				//bat100
#define LED_MOTOR_2			50				//bat50


//public function prototypes .
int write_trigger_values(uint8_t led_no, char *value);

#endif /* GPIO_DRIVER_H_ */

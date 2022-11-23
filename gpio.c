/*
 ============================================================================
 Name        : gpio.c
 Version     :
 Copyright   : Your copyright notice
 Description : simple gpio file handling functions
 ============================================================================
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "gpio.h"


/*This function writes the tigger values for the given "led_no"
 * returns 0 if success, else -1
 */
int write_trigger_values(uint8_t led_no, char *value)
{
	int fd,ret=0;
	char buf[SOME_BYTES];

	/* we are concatenating  2 strings and storing that in to 'buf'  */
	snprintf(buf,sizeof(buf),SYS_FS_LEDS_PATH "/bat%d/trigger",led_no);

	/* open the file in write mode */
	/*Returns the file descriptor for the new file. The file descriptor returned is always the smallest integer greater than zero that is still available. If a negative value is returned, then there was an error opening the file.*/
	fd = open(buf, O_WRONLY );
	if (fd <= 0) {
		perror(" write trigger error\n");
		return -1;
	}

	/* Write the 'value' in to the file designated by 'fd' */
	/*Returns the number of bytes that were written.
	  If value is negative, then the system call returned an error.
	*/
	ret = write(fd, (void*)value, strlen(value) );
	if ( ret <= 0)
	{
		printf("trigger value write error\n");
		return -1;
	}

	close(fd);

	return 0;

}

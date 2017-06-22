//============================================================================
// Name        : lin-dfu-prog.cpp
// Author      : Jacob Miller
// Version     : 1.0.0
// Copyright   :
//
// See: https://github.com/xanthium-enterprises/Serial-Port-Programming-on-Linux/blob/master/USB2SERIAL_Read/Reciever%20(PC%20Side)/SerialPort_read.c
//============================================================================
#include <stdio.h>   /* Standard input/output definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <string>  /* String function definitions */
#include <iostream>
using namespace std;

/*
 * 'open_port()' - Open serial port 1.
 *
 * Returns the file descriptor on success or -1 on error.
 */

int
open_port(void)
{
  int fd; /* File descriptor for the port */


  fd = open("/dev/ttyACM0", O_RDWR | O_NOCTTY | O_NDELAY);
  if (fd == -1)
  {
   /*
    * Could not open the port.
    */

    perror("open_port: Unable to open /dev/ttyACM0 - Try \"sudo apt-get --purge remove modemmanager\"");
  }
  else
    fcntl(fd, F_SETFL, 0);

  return (fd);
}

bool config_port(int fd)
{
	struct termios options;

	/*
	 * Get the current options for the port...
	 */

	tcgetattr(fd, &options);

	/*
	 * Set the baud rates to 1200...
	 */
	cfsetispeed(&options, B1200);
	cfsetospeed(&options, B1200);

	/*
	 * Enable the receiver and set local mode...
	 */
	options.c_cflag |= (CLOCAL | CREAD);

	// No parity (8N1)
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;

	// No HW flow control
	options.c_cflag &= ~CRTSCTS;       /* No Hardware flow Control                         */
	options.c_cflag |= CREAD | CLOCAL; /* Enable receiver,Ignore Modem Control lines       */

	/*
	 * Set the new options for the port...
	 */
	bool isSet = false;
	if(tcsetattr(fd, TCSANOW, &options) != 0) /* Set the attributes to the termios structure*/
	{
		perror("\n ERROR ! in Setting attributes");
	}
	else
	{
		printf("\n BaudRate = 1200 \n  StopBits = 1 \n  Parity   = none");
		isSet = true;
	}
	tcflush(fd, TCIFLUSH);   /* Discards old data in the rx buffer            */

	return isSet;
}

int main(int argc, char** argv) {
	cout << argv[0] << endl; // prints Hello World!!!
	int fd = open_port();
	if (fd==-1)
	{
		return 1;
	}

	if (!config_port(fd))
	{
		close(fd);
		printf("\n ERROR ! in config_port()");
		return 1;
	}
	printf("\nDevice set to DFU mode\n");
	sleep(3);
	close(fd);

	return 0;
}

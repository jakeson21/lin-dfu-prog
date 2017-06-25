/*
 * DFUProg.cpp
 *
 *  Created on: Jun 25, 2017
 *      Author: fuguru
 */

#include "DFUProg.h"
#include <stdio.h>   /* Standard input/output definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <string>  /* String function definitions */
#include <iostream>
using namespace std;


namespace DFU
{

DFUProg::DFUProg()
{
	// TODO Auto-generated constructor stub

}

DFUProg::~DFUProg()
{
	// TODO Auto-generated destructor stub
}

int DFUProg::InitDFUMode()
{
	int fd = openSerialPort();
	if (fd==-1)
	{
		return 1;
	}

	if (!configSerialPort(fd))
	{
		close(fd);
		printf("\n ERROR ! in config_port()");
		return 1;
	}
	printf("\nDevice set to DFU mode\n");
	sleep(3);
	close(fd);
}

//
// open_port() - Open serial port 1.
//
// Returns the file descriptor on success or -1 on error.
//
int DFUProg::openSerialPort()
{
  int fd; /* File descriptor for the port */


  fd = open(mPortName.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
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

bool DFUProg::configSerialPort(int fd)
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

} /* namespace DFU */

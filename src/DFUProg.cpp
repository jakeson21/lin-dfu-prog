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

#include <libusb-1.0/libusb.h>
#include "lmdfuprot.h"

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

int DFUProg::tiva_reset_device(struct dfu_if *dif)
{
	int status;
	tLMDFUQueryTivaProtocol sProt;

	status = libusb_control_transfer( dif->dev_handle,
		  /* bmRequestType */ LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
		  /* bRequest      */ USBD_DFU_REQUEST_TIVA,
		  /* wValue        */ REQUEST_TIVA_VALUE,
		  /* wIndex        */ dif->interface,
		  /* Data          */ (unsigned char*) &sProt,
		  /* wLength       */ sizeof(tLMDFUQueryTivaProtocol),
							  5000 );
	if (status == sizeof(tLMDFUQueryTivaProtocol))
	{
		// Since we got back the expected value, assume this device supports Tiva extensions
		tDFUDownloadHeader sHeader;
		sHeader.ucCommand = TIVA_DFU_CMD_RESET;
		status = libusb_control_transfer( dif->dev_handle,
			  /* bmRequestType */ LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
			  /* bRequest      */ USBD_DFU_REQUEST_DNLOAD,
			  /* wValue        */ 0,
			  /* wIndex        */ dif->interface,
			  /* Data          */ (unsigned char*) &sHeader,
			  /* wLength       */ sizeof(tDFUDownloadHeader),
								  5000 );

		printf("Releasing interface\n");
		if((status = libusb_release_interface(dif->dev_handle, dif->interface)) != 0)
		{
			printf("Error occured releasing interface [%d]\n", status);
			return status;
		}
		printf("Interface released\n");
	}

	return 0;
}

} /* namespace DFU */

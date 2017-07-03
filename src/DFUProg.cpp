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
#include <cstdlib>
using namespace std;

#include <libusb-1.0/libusb.h>
#include "lmdfuprot.h"

namespace DFU
{

DFUProg::DFUProg()
{
	// TODO Auto-generated constructor stub
	baudToStringMap[B1200] = "1200";
	baudToStringMap[B9600] = "9600";
}

DFUProg::~DFUProg()
{
	// TODO Auto-generated destructor stub
}

int DFUProg::InitDFUMode(int inBaudRate)
{
	int fd;
	if (openSerialPort(fd) != 0) { return EXIT_FAILURE; }

	if (configSerialPort(fd, inBaudRate) != 0)
	{
		close(fd);
		std::cerr << "could not configure COM port" << std::endl;
		return EXIT_FAILURE;
	}
	close(fd);
	return EXIT_SUCCESS;
}

//
// open_port() - Open serial port 1.
//
// Returns the file descriptor on success or -1 on error.
//
int DFUProg::openSerialPort(int& ioFileHandle)
{
  std::cout << "Attempting to open handle to " << mPortName << std::endl;
  if ((ioFileHandle = open(mPortName.c_str(), O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
  {
   /*
    * Could not open the port.
    */
    std::cerr << "Unable to open " << mPortName << " - Try \"sudo apt-get --purge remove modemmanager\"\n";
    return EXIT_FAILURE;
  }
  fcntl(ioFileHandle, F_SETFL, 0);

  return EXIT_SUCCESS;
}

int DFUProg::configSerialPort(int fd, int inBaudRate)
{
	struct termios options;

	/*
	 * Get the current options for the port...
	 */
	if (tcgetattr(fd, &options) < 0)
	{
		std::cerr << "stdin error" << std::endl;
		return EXIT_FAILURE;
	}

	if (cfsetispeed(&options, inBaudRate) < 0)
	{
		std::cerr << "invalid baud rate" << std::endl;
		return EXIT_FAILURE;
	}
	if (cfsetospeed(&options, inBaudRate) < 0)
	{
		std::cerr << "invalid baud rate" << std::endl;
		return EXIT_FAILURE;
	}

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
	if(tcsetattr(fd, TCSANOW, &options) != 0) /* Set the attributes to the termios structure*/
	{
		std::cerr << "ERROR ! in Setting attributes\n";
		return EXIT_FAILURE;
	}
	std::cout << "Connected at: " << baudToStringMap.at(inBaudRate) << "-none-1\n";
	tcflush(fd, TCIFLUSH);   /* Discards old data in the rx buffer            */

	return EXIT_SUCCESS;
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
	}

	return EXIT_SUCCESS;
}

} /* namespace DFU */

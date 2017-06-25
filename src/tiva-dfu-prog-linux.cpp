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

#include "DFUProg.h"

int main(int argc, char** argv) {
	cout << argv[0] << endl; // prints Hello World!!!

	// Switch from Run mode to DFU mode
	std::string portName = "/dev/ttyACM0";
	DFU::DFUProg dfu_prog;
	dfu_prog.setSerialPort(portName);
	dfu_prog.InitDFUMode();

	// Initiate DFU Transfer


	// Restart device


	return 0;
}

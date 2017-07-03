/*
 * DFUProg.h
 *
 *  Created on: Jun 25, 2017
 *      Author: fuguru
 */

#ifndef SRC_DFUPROG_H_
#define SRC_DFUPROG_H_

#include <string>  /* String function definitions */
#include <libusb-1.0/libusb.h>
#include <map>

#include "dfu.h"

namespace DFU
{

class DFUProg
{
public:
	DFUProg();
	virtual ~DFUProg();

	void setSerialPort(const std::string& inPortName) { mPortName = inPortName; }
	int InitDFUMode(int inBaudRate);

	int tiva_reset_device(struct dfu_if *dif);

private:
	int configSerialPort(int fd, int inBaudRate);
	int openSerialPort(int& ioFileHandle);

	std::string mPortName;
	std::map<int, std::string>  baudToStringMap;
};


} /* namespace DFU */

#endif /* SRC_DFUPROG_H_ */

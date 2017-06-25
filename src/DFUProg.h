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

namespace DFU
{

class DFUProg
{
public:
	DFUProg();
	virtual ~DFUProg();

	void setSerialPort(const std::string& inPortName) { mPortName = inPortName; }
	int InitDFUMode();

private:
	bool configSerialPort(int fd);
	int openSerialPort();

	std::string mPortName;
};

} /* namespace DFU */

#endif /* SRC_DFUPROG_H_ */

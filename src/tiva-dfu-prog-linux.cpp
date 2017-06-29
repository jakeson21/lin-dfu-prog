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
#include <string.h>
#include <string>  /* String function definitions */
#include <iostream>
#include <exception>
using namespace std;

#include "DFUProg.h"
#include "portable.h"
#include "dfu_file.h"
#include "dfu_load.h"
#include "dfu_util.h"
#include "dfu.h"
#include "lmdfu.h"

int verbose = 0;
struct dfu_if *dfu_root = NULL;

char *match_path = NULL;
int match_vendor = -1;
int match_product = -1;
int match_vendor_dfu = -1;
int match_product_dfu = -1;
int match_config_index = -1;
int match_iface_index = -1;
int match_iface_alt_index = -1;
const char *match_iface_alt_name = NULL;
const char *match_serial = NULL;
const char *match_serial_dfu = NULL;

int main(int argc, char** argv) {
	cout << argv[0] << endl; // prints Hello World!!!

	// Switch from Run mode to DFU mode
	std::string portName = "/dev/ttyACM0";
	DFU::DFUProg dfu_prog;
	dfu_prog.setSerialPort(portName);
	dfu_prog.InitDFUMode();

	// Initiate DFU Transfer
	int expected_size = 0;
	unsigned int transfer_size = 0;
	enum mode mode = MODE_RESET;
	struct dfu_status status;
	libusb_context *ctx;
	struct dfu_file file;
	char *end;
	int ret;
	int fd;
	const char *dfuse_options = NULL;
	int detach_delay = 5;
	uint16_t runtime_vendor;
	uint16_t runtime_product;

	memset(&file, 0, sizeof(file));

	ret = libusb_init(&ctx);
	if (ret)
	{
		std::cout << "EX_IOERR: Unable to initialize libusb: " << libusb_error_name(ret) << std::endl;
		throw std::exception();
	}

	if (verbose > 2) {
		libusb_set_debug(ctx, 255);
	}

	probe_devices(ctx);

	if (mode == MODE_LIST) {
		list_dfu_interfaces();
		return(0);
	}

	if (dfu_root == NULL) {
		std::cout << "No DFU capable USB device available" << std::endl;
		return(1);
	} else if (dfu_root->next != NULL) {
		/* We cannot safely support more than one DFU capable device
		 * with same vendor/product ID, since during DFU we need to do
		 * a USB bus reset, after which the target device will get a
		 * new address */
		std::cout << "More than one DFU capable USB device found! "
		       "Try `--list' and specify the serial number "
		       "or disconnect all but one device" << std::endl;
		return(1);
	}

	/* We have exactly one device. Its libusb_device is now in dfu_root->dev */

	printf("Opening DFU capable USB device...\n");
	ret = libusb_open(dfu_root->dev, &dfu_root->dev_handle);
	if (ret || !dfu_root->dev_handle)
	{
		std::cout << "EX_IOERR: Cannot open device: %: " << libusb_error_name(ret) << std::endl;
		throw std::exception();
	}

	printf("ID %04x:%04x\n", dfu_root->vendor, dfu_root->product);

	printf("Run-time device DFU version %04x\n",
	       libusb_le16_to_cpu(dfu_root->func_dfu.bcdDFUVersion));

	/* we're already in DFU mode, so we can skip the detach/reset
	 * procedure */
	/* If a match vendor/product was specified, use that as the runtime
	 * vendor/product, otherwise use the DFU mode vendor/product */
	runtime_vendor = match_vendor < 0 ? dfu_root->vendor : match_vendor;
	runtime_product = match_product < 0 ? dfu_root->product : match_product;

	printf("Claiming USB DFU Interface...\n");
	ret = libusb_claim_interface(dfu_root->dev_handle, dfu_root->interface);
	if (ret < 0) {
		errx(EX_IOERR, "Cannot claim interface - %s", libusb_error_name(ret));
	}

	printf("Setting Alternate Setting #%d ...\n", dfu_root->altsetting);
	ret = libusb_set_interface_alt_setting(dfu_root->dev_handle, dfu_root->interface, dfu_root->altsetting);
	if (ret < 0) {
		errx(EX_IOERR, "Cannot set alternate interface: %s", libusb_error_name(ret));
	}

status_again:
	printf("Determining device status: ");
	ret = dfu_get_status(dfu_root, &status );
	if (ret < 0) {
		errx(EX_IOERR, "error get_status: %s", libusb_error_name(ret));
	}
	printf("state = %s, status = %d\n",
	       dfu_state_to_string(status.bState), status.bStatus);

	milli_sleep(status.bwPollTimeout);

	switch (status.bState) {
	case DFU_STATE_appIDLE:
	case DFU_STATE_appDETACH:
		errx(EX_IOERR, "Device still in Runtime Mode!");
		break;
	case DFU_STATE_dfuERROR:
		printf("dfuERROR, clearing status\n");
		if (dfu_clear_status(dfu_root->dev_handle, dfu_root->interface) < 0) {
			errx(EX_IOERR, "error clear_status");
		}
		goto status_again;
		break;
	case DFU_STATE_dfuDNLOAD_IDLE:
	case DFU_STATE_dfuUPLOAD_IDLE:
		printf("aborting previous incomplete transfer\n");
		if (dfu_abort(dfu_root->dev_handle, dfu_root->interface) < 0) {
			errx(EX_IOERR, "can't send DFU_ABORT");
		}
		goto status_again;
		break;
	case DFU_STATE_dfuIDLE:
		printf("dfuIDLE, continuing\n");
		break;
	default:
		break;
	}

	if (DFU_STATUS_OK != status.bStatus ) {
		printf("WARNING: DFU Status: '%s'\n",
			dfu_status_to_string(status.bStatus));
		/* Clear our status & try again. */
		if (dfu_clear_status(dfu_root->dev_handle, dfu_root->interface) < 0)
			errx(EX_IOERR, "USB communication error");
		if (dfu_get_status(dfu_root, &status) < 0)
			errx(EX_IOERR, "USB communication error");
		if (DFU_STATUS_OK != status.bStatus)
			errx(EX_SOFTWARE, "Status is not OK: %d", status.bStatus);

		milli_sleep(status.bwPollTimeout);
	}

	printf("DFU mode device DFU version %04x\n",
	       libusb_le16_to_cpu(dfu_root->func_dfu.bcdDFUVersion));

	/* If not overridden by the user */
	if (!transfer_size) {
		transfer_size = libusb_le16_to_cpu(
		    dfu_root->func_dfu.wTransferSize);
		if (transfer_size) {
			printf("Device returned transfer size %i\n",
			       transfer_size);
		} else {
			errx(EX_IOERR, "Transfer size must be specified");
		}
	}
#ifdef HAVE_GETPAGESIZE
/* autotools lie when cross-compiling for Windows using mingw32/64 */
#ifndef __MINGW32__
	/* limitation of Linux usbdevio */
	if ((int)transfer_size > getpagesize()) {
		transfer_size = getpagesize();
		printf("Limited transfer size to %i\n", transfer_size);
	}
#endif /* __MINGW32__ */
#endif /* HAVE_GETPAGESIZE */

	if (transfer_size < dfu_root->bMaxPacketSize0) {
		transfer_size = dfu_root->bMaxPacketSize0;
		printf("Adjusted transfer size to %i\n", transfer_size);
	}

	switch (mode) {
	case MODE_UPLOAD:
		/* open for "exclusive" writing */
		fd = open(file.name, O_WRONLY | O_BINARY | O_CREAT | O_EXCL | O_TRUNC, 0666);
		if (fd < 0)
			err(EX_IOERR, "Cannot open file %s for writing", file.name);

		    if (dfuload_do_upload(dfu_root, transfer_size,
			expected_size, fd) < 0) {
			return(1);
		    }
		close(fd);
		break;

	case MODE_DOWNLOAD:
		if (((file.idVendor  != 0xffff && file.idVendor  != runtime_vendor) ||
		     (file.idProduct != 0xffff && file.idProduct != runtime_product)) &&
		    ((file.idVendor  != 0xffff && file.idVendor  != dfu_root->vendor) ||
		     (file.idProduct != 0xffff && file.idProduct != dfu_root->product))) {
			errx(EX_IOERR, "Error: File ID %04x:%04x does "
				"not match device (%04x:%04x or %04x:%04x)",
				file.idVendor, file.idProduct,
				runtime_vendor, runtime_product,
				dfu_root->vendor, dfu_root->product);
		}
		if (dfuload_do_dnload(dfu_root, transfer_size, &file) < 0)
			return(1);
		break;
	case MODE_RESET:
		break;
	case MODE_DETACH:
		if (dfu_detach(dfu_root->dev_handle, dfu_root->interface, 1000) < 0) {
			warnx("can't detach");
		}
		break;
	default:
		errx(EX_IOERR, "Unsupported mode: %u", mode);
		break;
	}

	printf("Resetting USB to switch back to runtime mode\n");
	ret = tiva_reset_device(dfu_root);
	if (ret < 0 && ret != LIBUSB_ERROR_NOT_FOUND) {
		errx(EX_IOERR, "error resetting after download: %s", libusb_error_name(ret));
	}

	libusb_release_interface(dfu_root->dev_handle, dfu_root->interface);
	libusb_close(dfu_root->dev_handle);
	dfu_root->dev_handle = NULL;
	libusb_exit(ctx);

	/* keeping handles open might prevent re-enumeration */
	disconnect_devices();

	return 0;
}

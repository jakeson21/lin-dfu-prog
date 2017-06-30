//============================================================================
// Name        : tiva-dfu-prog-linux.cpp
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
#include <getopt.h>
#include <cstdlib>
#include <string>  /* String function definitions */
#include <iostream>
#include <exception>
#include <chrono>
#include <thread>
using namespace std;

#include "DFUProg.h"
#include "portable.h"
#include "dfu_file.h"
#include "dfu_load.h"
#include "dfu_util.h"
#include "dfu.h"
#include "lmdfu.h"
#include "bin2dfuwrap.h"

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

static void help(void)
{
	fprintf(stderr, "Usage: dfu-util [options] ...\n"
		"  -h --help\t\t\tPrint this help message\n"
		"  -V --version\t\t\tPrint the version number\n"
		"  -v --verbose\t\t\tPrint verbose debug statements\n"
		"  -l --list\t\t\tList DFU capable devices and exit\n"
		"  -p --port-name <port-name>\tConnect to COM port /dev/<port-name> to send device into DFU mode.\n"
		"                            \tDefault is \"/dev/ttyACM0\"\n"
		"  -D --download <file>\t\tWrite firmware from <file> into device. Not compatible with -r option.\n"
		"  -r --reset-only\t\tOnly issues USB Reset. Not compatible with -D option.\n"
		);
	std::exit(EX_USAGE);
}

static void print_version(void)
{
	printf(PACKAGE_STRING "\n\n");
	printf("Copyright 2005-2009 Weston Schmidt, Harald Welte and OpenMoko Inc.\n"
	       "Copyright 2010-2016 Tormod Volden and Stefan Schmidt\n"
	       "This program is Free Software and has ABSOLUTELY NO WARRANTY\n"
	       "Please report bugs to " PACKAGE_BUGREPORT "\n\n");
}

static struct option opts[] = {
	{ "help", 		optional_argument, NULL, 'h' },
	{ "version", 	optional_argument, NULL, 'V' },
	{ "verbose", 	optional_argument, NULL, 'v' },
	{ "list", 		optional_argument, NULL, 'l' },
	{ "port-name",  optional_argument, NULL, 'p' },
	{ "download", 	required_argument, NULL, 'D' },
	{ "reset-only", optional_argument, NULL, 'r' },
	{ NULL, 0, NULL, 0 }
};

int main(int argc, char** argv) {
	// Initiate DFU Transfer
	int expected_size = 0;
	unsigned int transfer_size = 0;
	enum mode mode = MODE_RESET;
	struct dfu_status status;
	libusb_context *ctx;
	struct dfu_file file;
	std::string firmwareFileName;
	char *end;
	int ret;
	int fd;
	bool reset_only = false;
	bool com_trigger = false;
	const char *dfuse_options = NULL;
	int detach_delay = 5;
	uint16_t runtime_vendor;
	uint16_t runtime_product;
	std::string portName = "/dev/ttyACM0";

	while (1)
	{
		int c, option_index = 0;
		c = getopt_long(argc, argv, "hVvlp:D:r", opts,
				&option_index);
		if (c == -1)
			break;

		switch (c)
		{
			case 'h':
				help();
				break;
			case 'V':
				mode = MODE_VERSION;
				break;
			case 'v':
				verbose++;
				break;
			case 'l':
				mode = MODE_LIST;
				break;
			case 'p':
				portName = optarg;
				com_trigger = true;
				break;
			case 'D':
				mode = MODE_DOWNLOAD;
				file.name = optarg;
				firmwareFileName = optarg;
				break;
			case 'r':
				reset_only = true;
				break;
			default:
				help();
				break;
		}
	}

	if ((reset_only && mode == MODE_DOWNLOAD) || (reset_only && mode == MODE_LIST))
	{
		std::cout << "Invalid combination of args : -r -D or -l\n";
		help();
		std::exit(1);
	}
	else if (reset_only) { mode = MODE_RESET; }

	print_version();
	if (mode == MODE_VERSION)
	{
		std::exit(0);
	}

	file.idVendor 	= 0x1cbe; match_vendor  = 0x1cbe;
	file.idProduct 	= 0x00ff; match_product = 0x00ff;

	// Switch from Run mode to DFU mode
	if (com_trigger)
	{
		DFU::DFUProg dfu_prog;
		dfu_prog.setSerialPort(portName);
		dfu_prog.InitDFUMode();
	}
	else
	{
		std::cout << "Not triggering DFU mode via COM port" << std::endl;
	}

	ret = libusb_init(&ctx);
	if (ret)
	{
		std::cout << "EX_IOERR: Unable to initialize libusb: " << libusb_error_name(ret) << std::endl;
		throw std::exception();
	}

	if (verbose > 2)
	{
		libusb_set_debug(ctx, 255);
	}

	if (mode == MODE_LIST)
	{
		list_dfu_interfaces();
		return(0);
	}

	probe_devices(ctx);
	{
		auto timeout_ms = 5000;
		auto elapsed_ms = 0;
		while (dfu_root == NULL)
		{
			std::this_thread::sleep_for(chrono::milliseconds(100));
			timeout_ms -= 100;
			std::cout << "." << std::flush;
			if (timeout_ms < 0) { break; }
			probe_devices(ctx);
		}
		std::cout << std::endl;
	}

	if (dfu_root == NULL)
	{
		std::cout << "No DFU capable USB device available" << std::endl;
		std::exit(1);
	}

	if (dfu_root->next != NULL)
	{
		/* We cannot safely support more than one DFU capable device
		 * with same vendor/product ID, since during DFU we need to do
		 * a USB bus reset, after which the target device will get a
		 * new address */
		std::cout << "More than one DFU capable USB device found! "
		       "Try `--list' and specify the serial number "
		       "or disconnect all but one device" << std::endl;
		std::exit(1);
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

	std::this_thread::sleep_for(chrono::milliseconds(status.bwPollTimeout));

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

		std::this_thread::sleep_for(chrono::milliseconds(status.bwPollTimeout));
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

	Bin2DfuWrapper bin2dfu(0, verbose);
	std::size_t found;
	std::string outPath;

	switch (mode)
	{
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

			// Apply Tiva wrapper

			// Create dfu wrapped filename based on input file
			found = firmwareFileName.find_last_of(".");            // find extension seperator
			outPath = firmwareFileName.substr(0, found+1) + "dfu";   // change extension to 'dfu'
			// dfuwrap
			std::cout << "Wrapping " << firmwareFileName << " in DFU headers" << std::endl;
			if (bin2dfu.applyWrapper(firmwareFileName, outPath))
			{
				std::cout << "Error encountered during DFU wrap" << std::endl;
			}
			else
			{
				// Set path to our new DFU wrapped binary
				firmwareFileName = outPath;
				file.name = firmwareFileName.c_str();

				dfu_load_file(&file, MAYBE_SUFFIX, MAYBE_PREFIX);
				/* If the user didn't specify product and/or vendor IDs to match,
				 * use any IDs from the file suffix for device matching */
				if (match_vendor < 0 && file.idVendor != 0xffff) {
					match_vendor = file.idVendor;
					printf("Match vendor ID from file: %04x\n", match_vendor);
				}
				if (match_product < 0 && file.idProduct != 0xffff) {
					match_product = file.idProduct;
					printf("Match product ID from file: %04x\n", match_product);
				}
				if (transfer_size < dfu_root->bMaxPacketSize0) {
					transfer_size = dfu_root->bMaxPacketSize0;
					printf("Adjusted transfer size to %i\n", transfer_size);
				}

				// Download new file to device
				if (dfuload_do_dnload(dfu_root, transfer_size, &file) < 0)
					break;
					//std::exit(1);
			}
			break;

		case MODE_RESET:
			std::cout << "Operating in reset-only mode" << std::endl;
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

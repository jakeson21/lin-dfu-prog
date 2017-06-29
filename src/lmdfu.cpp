#include "dfu.h"
#include "lmdfuprot.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libusb-1.0/libusb.h>

int tiva_reset_device(struct dfu_if *dif)
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

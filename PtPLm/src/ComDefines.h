// Custom Engineering SPA
// Defines header file for C version of ComLibrary


#ifndef _COM_LYBRARY_DEFINES_CUSTOM_H
#define _COM_LYBRARY_DEFINES_CUSTOM_H 1



//   -------------------   ComLayer   -------------------:
#define SERIAL_PORT                       21
#define ETHERNET_PORT                     22
#define USB_PORT                          23
#define NO_PORT                           24

#define PORT_NOT_INITIALIZED              31
#define PORT_INITIALIZED                  32
#define PORT_CONNECTED                    33

// for function WriteFile, size of the buffer for file reading
#define COM_LAYER_FILE_READ_BUFFER_SIZE   1024



//   -------------------   ComSerial   -------------------:
#define SERIAL_SYSTEM_DEVICE_NAMES                 "/dev/ttyS"
#define MAX_SERIAL_DEV_NAME_LENGTH                 50

#define DEFAULT_MAX_SERIAL_PACKET_SIZE             1024

#define MAX_SERIAL_FAIL                            10
#define SLEEP_MICRO_SEC_SERIAL_FAIL_RETRY          100

#define DEFAULT_READ_SERIAL_TIMEOUT_SECONDS        2
#define DEFAULT_READ_SERIAL_TIMEOUT_MICRO_SECONDS  500000

#define SERIAL_AUTOBAUDRATE_READ_TIMEOUT_SEC       0
#define SERIAL_AUTOBAUDRATE_READ_TIMEOUT_MICRO_SEC 50000

#define MAX_SERIAL_LOG_LENGTH                      250

// valid bytes for the auto_baud_rate functionality (printer ID MODEL):
#define VALID_AUTOBAUDRATE_MODEL_IDS            {0x5f,0x5d,0x83}

// used in InitSerialAutoBaudRate():
#define PRINTER_ID_MODEL_TX         (const unsigned char*)"\x1D\x49\x01"
#define PRINTER_ID_TX_SIZE          3



//   -------------------   ComEth   -------------------:
#define DEFAULT_MAX_ETH_PACKET_SIZE                512

#define MAX_ETH_LOG_LENGTH                         250

//       not less than 15:
#define MAX_IP_ADDRESS_LENGTH                      100

#define DEFAULT_READ_ETH_TIMEOUT_SECONDS           2
#define DEFAULT_READ_ETH_TIMEOUT_MICRO_SECONDS     500000

#define MAX_ETH_FAIL                               10
#define ETH_READ_TIMEOUT_AFTER_READ_SOMETHING      100
#define SLEEP_MICRO_SEC_ETH_WRITE_FAIL_RETRY       500

// keep-alive for TCP (1140 sec = 19 minutes):
#define TCP_KEEP_ALIVE_SECONDS                     1140
#define TCP_KEEP_ALIVE_COUNT                       8



//   -------------------   ComUsb   -------------------:
#define USB_IF1_SYSTEM_DEVICE_NAMES                "/dev/usb/lp"

#define MAX_USB_IF1_DEV_NAME_LENGTH                50

// for interface 1 (normal interface), in bytes, recommended 64:
#define DEFAULT_MAX_USB_IF1_PACKET_SIZE            64

#define MAX_USB_INTERFACE2_SN_LENGTH               200
#define MAX_USB_LOG_LENGTH                         250

// timeout (better do not set less than 0.1 s). Not more than 1000 seconds:
#define DEFAULT_READ_USB_TIMEOUT_SECONDS           2
#define DEFAULT_READ_USB_TIMEOUT_MICRO_SECONDS     500000

#define MAX_USB_FAIL                               10
// microseconds:
#define SLEEP_MICRO_SEC_USB_WRITE_FAIL_RETRY       500
// set at least 1000~2000 microseconds:
#define USB_INTERFACE1_TIMEOUT_AFTER_READ_DATA_MICROSEC     10000
// milliseconds:
#define USB_LIBUSB_MINIMUM_VALID_TIMEOUT_MS        50
#define USB_LIBUSB_WRITE_TIMEOUT_MILLI_SEC         500
// microseconds, absolutely do not set less than 1000:
#define USB_READ_TIMEOUT_EXPIRATION_TIME           1000



//   -------------------   ComUsbIF0   -------------------:
// this size is used for all the static string buffers (do not set under 200):
#define IF0_MAXCHARBUFFER                          1024

// timeout (better do not set less than 0.1 s). Not more than 1000 seconds:
#define DEFAULT_USB_IF0_TIMEOUT_SECONDS            2
#define DEFAULT_USB_IF0_TIMEOUT_MICRO_SECONDS      500000

//byte to determine the transfer data direction:
#define BYTE_ENDPOINT_IN                  0x80
//byte request to get a string descriptor (serial number in this case):
#define BYTE_REQUEST_GET_DESCRIPTOR       0x06
//ask the third descriptor for serial number:
#define BYTE_DESC_STRING                  0x03
//first byte of the setup packet to vendor request:
#define BYTE_VENDOR_REQUEST               0xc1

//length of the device dscriptor (vith VID and PID):
#define DEVICE_DESC_LENGTH   18
//length of the setup packet control:
#define CONTROL_SETUP_SIZE   8

//first root path to check:
#define STR_DEV_BUS_USB    "/dev/bus/usb"
//if the first path is not supported, try this path:
#define STR_PROC_BUS_USB   "/proc/bus/usb"

#define IOCTL_USB_SUBMITURB   _IOR('U', 10, struct usbfs_urb)
#define IOCTL_USB_READURB     _IOW('U', 13, void *)

#define false 0
#define true 1

// interface 0 log:
#define LOG_VID_PID_SERIAL       "VID=0x%02x PID=0x%02x Serial number=%s"
#define LOG_TRY_OPEN_DIR         "Trying open dir %s"
#define LOG_ERROR_OPEN_DIR       "Error opening dir %s"
#define LOG_ERR_USB_ROOT_PATH    "Error finding and opening a device root path"
#define LOG_USB_ROOT_PATH        "Usb device root path: %s"
#define LOG_ERROR_NO_INIT        "Device not initalized"
#define LOG_ERROR_ALREADY_OPENED "Device already opened"
#define LOG_ERROR_PARAMINIT_NOT_VALID   "Init parameters not valid"
#define LOG_BUS_CHECK            "BUS Number: %d"
#define LOG_DEVICE_PATH          "Device path: %s"
#define LOG_ERR_DEVICE_OPEN      "Error opening the device %s"
#define LOG_ERR_READ_DESCRIPTOR  "Error reading the file descriptor for the device %s"
#define LOG_PARAM_DESCRIPTOR     "Device Descr:%d, VID:=0x%04x PID:0x%04x SERIAL_INDEX:%d"
#define LOG_ERR_IOCTL_SEND       "Error IOCTL Write"
#define LOG_ERR_IOCTL_PIPE       "Error IOCTL: detected endpoint stall"
#define LOG_ERR_IOCTL_LOW_LEVEL  "IOCTL: low-level error"
#define LOG_ERR_IOCTL_OVERFLOW   "Error IOCTL; overflow fail during transfer"
#define LOG_ERR_IOCTL_GENERAL    "IOCTL general error"
#define LOG_ERR_IOCTL_TIMEOUT    "IOCTL timeout error"
#define LOG_ERR_ID_LANGUAGE      "ID language not valid(<4), bytes transferred: %d"
#define LOG_ID_LANGUAGE          "ID language: %d"
#define LOG_ERR_DEVICE_NOT_FOUND "A device with VID= 0x%04x, PID=0x%04x and serial number '%s' is not currently plugged"
#define LOG_ERR_SERIAL_NOT_VALID "Serial number from string descriptor is not valid"
#define LOG_ERR_SERIAL_INCORRECT "Serial number is not as requested"
#define LOG_SERIAL_NOT_REQUESTED "Serial number was not requested, end"
#define LOG_SERIAL_NUMBER        "Serial number found: %s"
#define LOG_ERR_NOT_OPENED       "Impossible to perform operation: device not open"
#define LOG_BYTE_RECV            "RecvBuff[0x%02x]=0x%02x"
#define LOG_BYTE_READ            "Number of bytes read: %ld"
#define LOG_READ_BUFFER_FULL     "Local receive buffer is full"
#define LOG_ERR_CLOSE            "Close error"
#define LOG_PARAMETERS_SETUP     "Byte request type: 0x%02x ; byte request:0x%02x; value:0x%02x; index:0x%02x; length:0x%02x"
#define LOG_BYTES_TO_SEND        "Bytes to send: %ld"
#define LOG_BYTES_SENT           "SendBuff[0x%02x]=0x%02x"



//   -------------------   ComLog   -------------------:
#define NO_LOG_DATE_AND_TIME     0
#define LOG_DATE_AND_TIME        1

#define NO_VERBOSITY             0
#define MAX_VERBOSITY            5
#define MAX_FILE_NAME_LENGTH     199

// in this string are stored all the acceptable output streams for logging
//  (if not logging to a text file):
// #define VALID_LOG_OUTPUT_STREAMS   "/dev/ttyS0,/dev/ttyS1,/dev/ttyS2,/dev/ttyS3,/dev/ttyS4,/dev/ttyS5,/dev/ttyS6,/dev/ttyS7,/dev/ttyS8,/dev/usb/lp0,/dev/usb/lp1,/dev/usb/lp2,/dev/usb/lp3,/dev/usb/lp4,/dev/usb/lp5,/dev/usb/lp6,/dev/usb/lp7,/dev/usb/lp8,/dev/stdout,/dev/stderr"



#endif // _COM_LYBRARY_DEFINES_CUSTOM_H

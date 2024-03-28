// Custom Engineering SPA
// Errors header file for C version of ComLibrary


#ifndef _COM_LYBRARY_ERRORS_CUSTOM_H
#define _COM_LYBRARY_ERRORS_CUSTOM_H 1


#define SUCCESS                              0


#define ERR_INVALID_PORT_TYPE                2001
#define ERR_PORT_TYPE_NOT_SET                2002
#define ERR_PORT_CURRENTLY_OPEN              2003
#define ERR_WRITING_FILE                     2004
#define ERR_PORT_NOT_OPEN                    2005
#define ERR_CANT_ACCEPT_NULL_POINTER         2006


// ComUsb:
#define ERR_USB_PORT_ALREADY_OPEN            1201
#define ERR_OPENING_USB                      1202
#define ERR_CLOSING_USB                      1203
#define ERR_USB_NOT_OPEN                     1204
#define ERR_WRITING_USB                      1205
#define ERR_INVALID_USB_PORT_NAME            1206
#define ERR_USB_NOT_INITIALIZED              1207
#define ERR_READING_USB                      1209
#define ERR_USB_PARAMETERS_NOT_SET           1210
#define ERR_USB_CANT_ACCEPT_NULL_POINTER     1211
#define ERR_USB_SN_STRING_TOO_LONG           1212
#define ERR_USB_INTERFACE0_INITIALIZATION    1213


// ComUsbIF0:
//the printer has not been initialized:
#define ERR_NO_INIT                          1310
#define ERR_ALREADY_OPENED                   1311
//error opening the /dev/usb/ root path:
#define ERR_USB_ROOT_PATH                    1312
//parameters of the initialization not valid:
#define ERR_PARAMINIT_NOT_VALID              1313
#define ERR_IOCTL_SEND                       1316
//error read IOCTL endpoint stall:
#define ERR_IOCTL_PIPE                       1317
#define ERR_IOCTL_OVERFLOW                   1318
#define ERR_IOCTL_LOW_LEVEL                  1319
#define ERR_IOCTL_GENERAL                    1320
#define ERR_IOCTL_TIMEOUT                    1321
//language returned is not valid:
#define ERR_ID_LANGUAGE                      1322
//a device with these parameters is not plugged in the registry:
#define ERR_DEVICE_NOT_FOUND                 1323
//error opening the device:
#define ERR_OPEN_DEVICE                      1324
#define ERR_NOT_OPENED                       1325
#define ERR_READ_BUFFER_FULL                 1326
#define ERR_CLOSE                            1327


#endif // _COM_LYBRARY_ERRORS_CUSTOM_H

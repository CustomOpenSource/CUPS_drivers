// PtPLm : Defines the entry point for the SO application.
//

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
// #include <linux/usb.h>
#include <linux/usbdevice_fs.h>

#include "PtPLm.h"
#include "ComDefines.h"
#include "ComErrors.h"

#define MAX_USB_ADDRESS 4
#define READ_BUFFER_SIZE 12
#define ERROR_SUCCESS 0L
#define ERROR_INVALID_FUNCTION 1L
#define USB_PATH "/dev/usb/lp"

long m_lPID;
long m_lVID;
long m_lTmpVID;
long m_lTmpPID;
int m_iDescr = -1;
unsigned long m_dwReadInt0 = 0;
unsigned short m_bInitSuccess = false;
unsigned short m_bOpenSuccess = false;
char m_strSerial[IF0_MAXCHARBUFFER];
char m_strUsbPath[IF0_MAXCHARBUFFER];
char m_strTmpSerial[IF0_MAXCHARBUFFER];
long m_lUsbIf0TimeoutSec = DEFAULT_USB_IF0_TIMEOUT_SECONDS;
long m_lUsbIf0TimeoutMicroSec = DEFAULT_USB_IF0_TIMEOUT_MICRO_SECONDS;
unsigned char m_recvBuffer[IF0_MAXCHARBUFFER + 100];
unsigned char m_sendBuffer[CONTROL_SETUP_SIZE + IF0_MAXCHARBUFFER];

/*

 define internal structures

*/

// Setup packet for control transfers:
struct control_setup {
    /* Request type. Bits 0:4 determine recipient
    Bits 5:6 determine type, see
    Bit 7 determines data transfer direction */
    unsigned char bmRequestType;

    /* Command code: use of this field is
    application-specific. */
    unsigned char bRequest;

    /* unsigned short value. Varies according to request */
    unsigned short wValue;

    /* unsigned short index, varies according to request, typically used to
    pass an index or offset.
    In this case index will be 0 to indicate  interface 0 */
    unsigned short wIndex;

    /*Number of bytes to transfer */
    unsigned short wLength;
};

// struct with descriptor of iso packet for URB (USB Request Block):
struct usbfs_iso_packet_desc {
    unsigned long length;
    unsigned long actual_length;
    unsigned long status;
};

#define usbfs_urb usbdevfs_urb

// struct declaration
struct usbfs_urb m_structTmp;
struct usbfs_urb m_structSendUrb;
struct usbfs_urb *m_structRecvUrb = &m_structTmp;
struct control_setup *m_structSetup = (struct control_setup *)m_sendBuffer;

//     -------- 'PRIVATE' FUNCTIONS --------:

unsigned long InitDeviceIf0(long, long, const char *);

/* This function opens the USB connection with parameters specified with
   InitDeviceIf0().
   Return values:
   - ERR_NO_INIT
   - ERR_ALREADY_OPENED
   - ERR_OPEN_DEVICE
   - SUCCESS
*/

unsigned long OpenDeviceIf0();

unsigned long FindPathHandleIf0(char *);
unsigned long FillURBIf0(unsigned char *, unsigned short);
unsigned long FindDeviceIf0(long, long, char *);
unsigned long FillControlSetupIf0(unsigned char, unsigned char, unsigned short, unsigned short, unsigned short);
unsigned short CheckSerialNumberIf0(unsigned char, char *, int);
unsigned short CheckUsbFsIf0(const char *);
long GetDiffTimeIf0(const struct timeval *);
void SetUsbIf0Timeout(const struct timeval *);
unsigned long GetDeviceParametersIf0(int *, int *, char *);
unsigned long WriteDeviceIf0(const unsigned char *, unsigned long);

/* Read data contained in the local buffer (result of the responses to write
   operations, also old operations).
   Note that is this function is not called for many calling of WriteDeviceIf0(),
   the local buffer will fulfill (its size is IF0_MAXCHARBUFFER) and an error
   calling WriteDeviceIf0() may happen.
   Parameters:
   - buffer to store data in
   - size of the provided buffer
   - return parameter, where the function will store actually read data size
   Return values:
   - ERR_NOT_OPENED
   - SUCCESS
*/
unsigned long ReadDeviceIf0(unsigned char *, const unsigned long, unsigned long *);

/* Gets current timeout for USB interface 0 operations:
 */
void GetUsbIf0Timeout(struct timeval *);

unsigned long openUSBDevice(long lVid, long lPid, const char *strSerial) {
    unsigned long returnData = -1;

    // open inteface 0
    if (InitDeviceIf0(lVid, lPid, strSerial) == SUCCESS) {
        if (OpenDeviceIf0() == SUCCESS) {
            returnData = SUCCESS;
        } else {
            returnData = ERR_OPENING_USB;
        }
    }

    return returnData;
}

// close the opened handle of the device
unsigned long closeUSBDevice() {
    unsigned long dwErr = SUCCESS;
    // char strMessage[IF0_MAXCHARBUFFER];

    if (!m_bOpenSuccess) {
        return ERR_NOT_OPENED;
    }
    if (close(m_iDescr) < 0) {
        return ERR_CLOSE;
    }
    // clean the receive buffer
    memset(m_recvBuffer, 0, IF0_MAXCHARBUFFER);
    m_dwReadInt0 = 0;
    m_bOpenSuccess = false;
    return dwErr;
}

/*
   PRIVATE FUNCTIONS
*/
// Init the device with a specified VID, PID and serial number
unsigned long InitDeviceIf0(long lVid, long lPid, const char *strSerial) {
    unsigned long dwErr = SUCCESS;

    // 1 - CHECK IF THE DEVICE IS ALREADY OPENED
    if (m_bOpenSuccess) {
        return ERR_ALREADY_OPENED;
    }

    // set vid, pid and serial:
    memset(m_strSerial, 0, IF0_MAXCHARBUFFER);
    if (strSerial == NULL) {
        strcpy(m_strSerial, "");
    } else {
        if (strlen(strSerial) >= IF0_MAXCHARBUFFER) {
            m_bInitSuccess = false;
            return ERR_PARAMINIT_NOT_VALID;
        }
        strcpy(m_strSerial, strSerial);
    }
    m_lVID = lVid;
    m_lPID = lPid;

    m_bInitSuccess = true;
    memset(m_recvBuffer, 0, IF0_MAXCHARBUFFER);

    return dwErr;
}

// open the device to start the USB communication on interface 0
unsigned long OpenDeviceIf0() {
    unsigned long dwErr = SUCCESS;
    if (!m_bInitSuccess) {
        return ERR_NO_INIT;
    }
    if (m_bOpenSuccess) {
        return ERR_ALREADY_OPENED;
    }
    // find if a device with VID, PID and serial initalized is
    //  currently plugged to the system
    dwErr = FindDeviceIf0(m_lVID, m_lPID, m_strSerial);

    if (dwErr != SUCCESS) {
        return dwErr;
    }
    // open this device and store the descriptor
    m_iDescr = open(m_strUsbPath, O_RDWR);
    if (m_iDescr < 0) {
        return ERR_OPEN_DEVICE;
    }

    m_bOpenSuccess = true;
    return dwErr;
}

/*

  get the printer full status

*/

unsigned long GetPrinterStatus(unsigned char *bufferRecv, const unsigned long dwSize, unsigned long *dwRead) {
    unsigned long dwErr = SUCCESS;

    // send DLE EOT request
    dwErr = WriteDeviceIf0((unsigned char *)"\x10", 1);

    // return an error in write function
    if (dwErr != SUCCESS)
        return dwErr;

    // read DLE EOT response
    ReadDeviceIf0(bufferRecv, dwSize, dwRead);

    // check status respose size must be 6 bytes
    if (*dwRead != 6)
        dwErr = ERR_READING_USB;

    return dwErr;
}

/*
  Send command to printer
*/

static unsigned long SendPrinterCmdCheckACK(unsigned char *bufferSend, const unsigned long dwSize) {
    unsigned char bufferRecv[16];
    unsigned long dwRead = 0;
    unsigned long dwErr = SUCCESS;

    // send request
    dwErr = WriteDeviceIf0((unsigned char *)bufferSend, dwSize);

    // return an error in write function
    if (dwErr != SUCCESS)
        return dwErr;

    // read ACK response
    ReadDeviceIf0(bufferRecv, 16, &dwRead);

    // check status respose size must be 6 bytes
    if ((dwRead != 1) && (bufferRecv[0] != 0x06))
        dwErr = ERR_READING_USB;

    return dwErr;
}

/*
  Send clear command to printer
*/
unsigned long SendPrinterClearCmd(void) {
    unsigned long dwErr = SUCCESS;

    dwErr = SendPrinterCmdCheckACK((unsigned char *)"\xA1", 1);

    return dwErr;
}

/*
  Send clear command to printer
*/
unsigned long SendPrinterEjectCmd(int param) {
    unsigned long dwErr = SUCCESS;
    struct timeval TimeoutSaved, Timeout;

    GetUsbIf0Timeout(&TimeoutSaved);

    Timeout.tv_sec = 6;
    Timeout.tv_usec = 0;
    SetUsbIf0Timeout(&Timeout);

    if (param == EJECT_COMMAND) {
        /* Eject */
        dwErr = SendPrinterCmdCheckACK((unsigned char *)"\xB0", 1);
    } else {
        /* Retract */
        dwErr = SendPrinterCmdCheckACK((unsigned char *)"\xB1", 1);
    }

    SetUsbIf0Timeout(&TimeoutSaved);

    return dwErr;
}

// write a number of bytes from the buffer: MAX 3 bytes!
unsigned long WriteDeviceIf0(const unsigned char *bufferWrite, unsigned long dwSendLen) {
    unsigned long dwErr = SUCCESS;
    unsigned long dwParam = 0;
    int r = -1;
    int iUrbStatus = 1;
    int iTransferred = 0;
    unsigned short bResponse = false;

    unsigned char *recvBuff;
    unsigned char byteCommandId;
    unsigned char byteParam1;
    unsigned char byteParam2;
    unsigned short wValue = 0x00;
    unsigned long dwToSend;

    // 1- CHECK IF THE DEVICE IS OPENED
    if (!m_bOpenSuccess) {
        return ERR_NOT_OPENED;
    }
    // if the bytes to write are > 3, send only the FIRST 3 bytes
    if (dwSendLen > 3)
        dwToSend = 3;
    else
        dwToSend = dwSendLen;
    // number of parameters
    dwParam = dwToSend - 1;
    // to calculate the wValue (two bytes parameters) used by IOCTL,
    //  reverse the two parameter bytes: IOCTL will reverse them again,
    //  so the bytes will be transferred to the bus in the correct order

    byteCommandId = bufferWrite[0];
    if (dwParam == 0)
        wValue = 0x00;
    else if (dwParam == 1) {
        byteParam1 = bufferWrite[1];
        byteParam2 = 0x00;
        wValue = ((byteParam2 << 8) | byteParam1);
    } else if (dwParam == 2) {
        byteParam1 = bufferWrite[1];
        byteParam2 = bufferWrite[2];
        wValue = ((byteParam2 << 8) | byteParam1);
    }
    memset(m_sendBuffer, 0, CONTROL_SETUP_SIZE + IF0_MAXCHARBUFFER);
    FillControlSetupIf0(BYTE_VENDOR_REQUEST, byteCommandId, wValue,
                        0, IF0_MAXCHARBUFFER);
    // fill the URB (Usb Request Block to send)
    FillURBIf0(m_sendBuffer, CONTROL_SETUP_SIZE + IF0_MAXCHARBUFFER);
    if (ioctl(m_iDescr, IOCTL_USB_SUBMITURB, &m_structSendUrb) < 0) {
        return ERR_IOCTL_SEND;
    }

    // try to read a response
    recvBuff = m_structRecvUrb->buffer;

    struct timeval Timeval;
    gettimeofday(&Timeval, NULL);
    long lTimeout = m_lUsbIf0TimeoutSec * 1000000 + m_lUsbIf0TimeoutMicroSec;
    bResponse = false;
    m_structRecvUrb->status = 1;
    m_structRecvUrb->buffer_length = 0;
    while (GetDiffTimeIf0(&Timeval) < lTimeout) {
        // if IOCTL RESULT <0, check STATUS OF THE URB:
        // if urb status=1, wait (we are in STATUS stage in the communication)
        // if urb status is negative, interrupt the read
        r = ioctl(m_iDescr, IOCTL_USB_READURB, &m_structRecvUrb);
        iUrbStatus = m_structRecvUrb->status;
        iTransferred = m_structRecvUrb->actual_length;
        if (r < 0) {
            if (iUrbStatus == 1)
                continue;
            else if (iUrbStatus < 0)
                break;
        } else if (r == 0) {
            // the URB STATUS must be=0 and the byte transferred >0, othervise retry
            if ((iUrbStatus == 0) && (iTransferred > 0)) {
                bResponse = true;
                break;
            } else
                continue;
        }
    }
    if (!bResponse) {
        if (iUrbStatus < 0)  // system error
        {
            if (iUrbStatus == -EPIPE)  // endpoint stall
            {
                return ERR_IOCTL_PIPE;
            } else if (iUrbStatus == -EOVERFLOW)  // overflow transfer
            {
                return ERR_IOCTL_OVERFLOW;
            } else if ((iUrbStatus == -ETIME) || (iUrbStatus == -EPROTO) ||
                       (iUrbStatus == -EILSEQ))  // low-level error
            {
                return ERR_IOCTL_LOW_LEVEL;
            } else  // generic error
            {
                return ERR_IOCTL_GENERAL;
            }
        } else  // error timeout
        {
            return ERR_IOCTL_TIMEOUT;
        }
    } else  // read ok
    {
        // check if the local buffer is full
        if ((m_dwReadInt0 + iTransferred) > IF0_MAXCHARBUFFER) {
            return ERR_READ_BUFFER_FULL;
        }
        if (recvBuff != NULL) {
            memcpy(&m_recvBuffer[m_dwReadInt0], &recvBuff[8], iTransferred);
            m_dwReadInt0 += iTransferred;
            // log the bytes received
        }
    }

    return dwErr;
}

// read the stored bytes
unsigned long ReadDeviceIf0(unsigned char *bufferRecv, const unsigned long dwSize, unsigned long *dwRead) {
    unsigned long dwErr = SUCCESS;
    unsigned long dw = 0;

    // 1- CHECK IF THE DEVICE IS OPENED
    if (!m_bOpenSuccess) {
        return ERR_NOT_OPENED;
    }
    // if the buffer size is >= than the bytes actually stored, fill the
    //  receive buffer reading all the bytes and clear the local buffer.
    // if the buffer size is < than the bytes actually stored, fill the
    //  receive buffer with a number of bytes equal to the size and shift
    //  to left the bytes of the local buffer:
    if (dwSize >= m_dwReadInt0) {
        memcpy(bufferRecv, m_recvBuffer, m_dwReadInt0);
        *dwRead = m_dwReadInt0;

        // clear the buffer and reset the byte count
        memset(m_recvBuffer, 0, IF0_MAXCHARBUFFER);
        m_dwReadInt0 = 0;
    } else {
        memcpy(bufferRecv, m_recvBuffer, dwSize);
        *dwRead = dwSize;

        // shift the bytes and clean the last bytes in the buffer
        for (dw = 0; dw < (m_dwReadInt0 - dwSize); dw++)
            m_recvBuffer[dw] = m_recvBuffer[(dwSize + dw)];
        for (; dw < IF0_MAXCHARBUFFER; dw++)
            m_recvBuffer[dw] = 0x00;
        m_dwReadInt0 -= dwSize;
    }

    return dwErr;
}

void GetUsbIf0Timeout(struct timeval *Timeout) {
    Timeout->tv_sec = m_lUsbIf0TimeoutSec;
    Timeout->tv_usec = m_lUsbIf0TimeoutMicroSec;
}

void SetUsbIf0Timeout(const struct timeval *Timeout) {
    if (Timeout->tv_sec >= 0) {
        m_lUsbIf0TimeoutSec = Timeout->tv_sec;
    }

    if (Timeout->tv_usec >= 0) {
        m_lUsbIf0TimeoutMicroSec = Timeout->tv_usec;
    }
}

unsigned long GetDeviceParametersIf0(int *iPtrVID, int *iPtrPID, char *strSN) {
    // if a serial number was not specified and Interface0 is connected:
    if ((m_strSerial[0] == 0) && (m_bOpenSuccess)) {
        strcpy(strSN, m_strTmpSerial);  // read from interface0
    } else                              // not connected, or connected but was specified with Init...
    {
        strcpy(strSN, m_strSerial);
    }

    // if a Vendor ID was not specified and Interface0 is connected:
    if ((m_lVID < 0) && (m_bOpenSuccess)) {
        *iPtrVID = (int)m_lTmpVID;
    } else  // not connected, or connected but was specified with Init...
    {
        *iPtrVID = (int)m_lVID;
    }

    // if a Product ID was not specified and Interface0 is connected:
    if ((m_lPID < 0) && (m_bOpenSuccess)) {
        *iPtrPID = (int)m_lTmpPID;
    } else  // not connected, or connected but was specified with Init...
    {
        *iPtrPID = (int)m_lPID;
    }

    return SUCCESS;
}

//  ------ THE FOLLOWING FUNCTIONS SHOULD NOT BE CALLED BY THE USER ------ :

// try to open a system dir and check if it is valid
unsigned short CheckUsbFsIf0(const char *strDirName) {
    DIR *dir;
    struct dirent *entry;
    unsigned short bFound = false;

    dir = opendir(strDirName);
    if (!dir) {
        return false;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;
        /* We assume if we find any files that it must be the right place */
        bFound = true;
        break;
    }

    closedir(dir);

    return bFound;
}

// return the string with the root path to create the handle
unsigned long FindPathHandleIf0(char *strUsb) {
    unsigned long dwErr = SUCCESS;
    const char *strUsbPath = STR_DEV_BUS_USB;

    if (CheckUsbFsIf0(strUsbPath))
        strcpy(strUsb, strUsbPath);
    else {
        strUsbPath = STR_PROC_BUS_USB;
        if (CheckUsbFsIf0(strUsbPath))
            strcpy(strUsb, strUsbPath);
        else
            dwErr = ERR_USB_ROOT_PATH;
    }

    return dwErr;
}

// fill the USB Request Block to send to the device
unsigned long FillURBIf0(unsigned char *bufferToSend, unsigned short length) {
    unsigned long dwErr = SUCCESS;

    memset(&m_structSendUrb, 0, sizeof(m_structSendUrb));
    m_structSendUrb.endpoint = 0x00;
    m_structSendUrb.type = 0x02;
    //   m_structSendUrb.flags= 1<<1;  // commented for BOGUS LOG in dmesg
    m_structSendUrb.buffer = bufferToSend;
    m_structSendUrb.buffer_length = length;

    return dwErr;
}

// Fill the setup packet to send
unsigned long FillControlSetupIf0(unsigned char byteRequestType, unsigned char byteRequest, unsigned short value,
                                  unsigned short index, unsigned short length) {
    unsigned long dwErr = SUCCESS;

    // clean the struct
    memset(m_structSetup, 0, sizeof(struct control_setup));
    // set fields of the struct
    m_structSetup->bmRequestType = byteRequestType;
    m_structSetup->bRequest = byteRequest;
    m_structSetup->wValue = value;
    m_structSetup->wIndex = index;
    m_structSetup->wLength = length;

    return dwErr;
}

// function to calculate the difference time from a start time
long GetDiffTimeIf0(const struct timeval *TimeOld) {
    long lDiff, lMicroOld, lMicroNew;
    struct timeval TimeNew;
    gettimeofday(&TimeNew, NULL);
    lMicroOld = TimeOld->tv_usec;
    lMicroNew = TimeNew.tv_usec;
    lDiff = TimeNew.tv_sec - TimeOld->tv_sec;
    lDiff *= 1000000;
    lDiff += (lMicroNew - lMicroOld);
    return lDiff;
}

// given the index, extract the serial number from the
//  string descriptor and check it
unsigned short CheckSerialNumberIf0(unsigned char byteIndex, char *strSerial, int iDescr) {
    int r = -1;
    int iUrbStatus = 1;
    int iTransferred = 0;
    unsigned short bResponse = false;
    unsigned char *recvBuff;
    unsigned short wLangId;

    // buffer filled with the response data
    unsigned char bufferData[IF0_MAXCHARBUFFER];
    memset(bufferData, 0, IF0_MAXCHARBUFFER);
    // STEP 1: check the supported language
    /* Asking for the zero'th index is special - it returns a string
     * descriptor that contains all the language IDs supported by the device.
     * Typically there aren't many - often only one. The language IDs are 16
     * bit numbers, and they start at the third byte in the descriptor.
     */

    memset(m_sendBuffer, 0, CONTROL_SETUP_SIZE + IF0_MAXCHARBUFFER);
    FillControlSetupIf0(BYTE_ENDPOINT_IN, BYTE_REQUEST_GET_DESCRIPTOR,
                        BYTE_DESC_STRING << 8, 0, IF0_MAXCHARBUFFER);

    // fill the URB (Usb Request Block to send)

    FillURBIf0(m_sendBuffer, CONTROL_SETUP_SIZE + IF0_MAXCHARBUFFER);
    if (ioctl(iDescr, IOCTL_USB_SUBMITURB, &m_structSendUrb) < 0) {
        return false;
    }
    recvBuff = (unsigned char *)m_structRecvUrb->buffer;
    // try to read a response
    struct timeval Timeval;
    gettimeofday(&Timeval, NULL);
    long lTimeout = m_lUsbIf0TimeoutSec * 1000000 + m_lUsbIf0TimeoutMicroSec;
    bResponse = false;
    // timeout to ask the language and the serial
    m_structRecvUrb->status = 1;
    m_structRecvUrb->actual_length = 0;
    while (GetDiffTimeIf0(&Timeval) < lTimeout) {
        // if IOCTL RESULT <0, check STATUS OF THE URB:
        // if urb status=1, wait (we are in STATUS stage in the communication)
        // if urb status is negative, interrupt the read
        r = ioctl(iDescr, IOCTL_USB_READURB, &m_structRecvUrb);
        iUrbStatus = m_structRecvUrb->status;
        iTransferred = m_structRecvUrb->actual_length;
        if (r < 0) {
            if (iUrbStatus == 1)
                continue;
            else if (iUrbStatus < 0)
                break;
        } else if (r == 0) {
            // the URB STATUS must be=0 and bytes transferred >0, othervise retry
            if ((iUrbStatus == 0) && (iTransferred > 0)) {
                bResponse = true;
                break;
            } else
                continue;
        }
    }
    if (!bResponse) {
        if (iUrbStatus < 0)  // system error
        {
            if (iUrbStatus == -EPIPE)  // endpoint stall
            {
                return false;
            } else if (iUrbStatus == -EOVERFLOW)  // overflow transfer
            {
                return false;
            } else if ((iUrbStatus == -ETIME) || (iUrbStatus == -EPROTO) ||
                       (iUrbStatus == -EILSEQ))  // low-level error
            {
                return false;
            } else  // generic error
            {
                return false;
            }
        } else  // error timeout
        {
            return false;
        }
    } else {
        // if the bytes returned are less than 4, the response for
        //  the ID language request is invalid!
        if (iTransferred < 4) {
            return false;
        }
        recvBuff = m_structRecvUrb->buffer;
        memset(bufferData, 0, IF0_MAXCHARBUFFER);
        memcpy(bufferData, &recvBuff[8], iTransferred);
        wLangId = (bufferData[2] | (bufferData[3] << 8));

        // now we know the index of the string descriptor and the
        //  language ID, ask for the serial number
        memset(m_sendBuffer, 0, CONTROL_SETUP_SIZE + IF0_MAXCHARBUFFER);
        FillControlSetupIf0(BYTE_ENDPOINT_IN, BYTE_REQUEST_GET_DESCRIPTOR,
                            (BYTE_DESC_STRING << 8) | byteIndex, wLangId, IF0_MAXCHARBUFFER);
        FillURBIf0(m_sendBuffer, CONTROL_SETUP_SIZE + IF0_MAXCHARBUFFER);
        if (ioctl(iDescr, IOCTL_USB_SUBMITURB, &m_structSendUrb) < 0) {
            return false;
        }
        // try to read a response
        struct timeval Timeval;
        gettimeofday(&Timeval, NULL);
        long lTimeout = m_lUsbIf0TimeoutSec * 1000000 + m_lUsbIf0TimeoutMicroSec;
        bResponse = false;
        iUrbStatus = 1;
        r = -1;
        iTransferred = 0;
        m_structRecvUrb->status = 1;
        m_structRecvUrb->actual_length = 0;
        // timeout to ask the language and the serial
        while (GetDiffTimeIf0(&Timeval) < lTimeout) {
            // if IOCTL RESULT <0, check STATUS OF THE URB:
            // if urb status=1, wait (we are in STATUS stage in the communication)
            // if urb status is negative, interrupt the read
            r = ioctl(iDescr, IOCTL_USB_READURB, &m_structRecvUrb);
            iUrbStatus = m_structRecvUrb->status;
            iTransferred = m_structRecvUrb->actual_length;
            if (r < 0) {
                if (iUrbStatus == 1)
                    continue;
                else if (iUrbStatus < 0)
                    break;
            } else if (r == 0) {
                // the URB STATUS must be=0 and the byte transferred >0,
                //  othervise retry
                if (iUrbStatus == 0 && iTransferred > 0) {
                    bResponse = true;
                    break;
                } else
                    continue;
            }
        }
        if (!bResponse) {
            if (iUrbStatus < 0)  // system error
            {
                if (iUrbStatus == -EPIPE)  // endpoint stall
                {
                    return false;
                } else if (iUrbStatus == -EOVERFLOW)  // overflow transfer
                {
                    return false;
                } else if ((iUrbStatus == -ETIME) || (iUrbStatus == -EPROTO) ||
                           (iUrbStatus == -EILSEQ))  // low-level error
                {
                    return false;
                } else  // generic error
                {
                    return false;
                }
            } else  // error timeout
            {
                return false;
            }
        } else {
            // we have the serial, compare it with the one passed to the function
            memset(bufferData, 0, IF0_MAXCHARBUFFER);
            recvBuff = m_structRecvUrb->buffer;
            // memcpy(bufferData,&recvBuff[8],iTransferred);
            // FIRST BYTE: length
            // SECOND BYTE:BYTE_DESC_STRING
            if ((recvBuff[8] != iTransferred) || (recvBuff[9] != BYTE_DESC_STRING)) {
                return false;
            }
            int i, t;
            for (i = 2, t = 0; i < iTransferred; i += 2, t++)
                bufferData[t] = recvBuff[i + 8];

            strcpy(m_strTmpSerial, (const char *)bufferData);

            if (strcasecmp((const char *)bufferData, strSerial) != 0) {
                return false;
            }
        }
    }

    return true;
}

// obtain a list with all the devices plugged in the system, and check if
//  there is device with the init parameters
unsigned long FindDeviceIf0(long lVID, long lPID, char *strSerial) {
    unsigned long dwErr = SUCCESS;
    unsigned short bDeviceFound = false;
    unsigned char byteSerialIndex;
    long lV;
    long lP;
    struct dirent *entry;
    char strRootPath[IF0_MAXCHARBUFFER];
    char strBusPath[IF0_MAXCHARBUFFER];
    char strDevicePath[IF0_MAXCHARBUFFER];
    char bufDescriptor[IF0_MAXCHARBUFFER];
    int iDescr = -1;
    int iBusNum;
    int iDevAddr;
    int r = -1;

    memset(strRootPath, 0, IF0_MAXCHARBUFFER);
    dwErr = FindPathHandleIf0(strRootPath);

    if (dwErr != SUCCESS) {
        return dwErr;
    }

    DIR *dirBus1 = opendir(strRootPath);
    DIR *dirBus2;
    if (!dirBus1) {
        return ERR_USB_ROOT_PATH;
    }
    while ((entry = readdir(dirBus1))) {
        if (entry->d_name[0] == '.')
            continue;

        iBusNum = atoi(entry->d_name);
        // unknown dir entry if bus num=0, skip it
        if (iBusNum == 0)
            continue;
        memset(strBusPath, 0, IF0_MAXCHARBUFFER);
        sprintf(strBusPath, "%s/%03d", strRootPath, iBusNum);
        dirBus2 = opendir(strBusPath);
        // impossible to read this dir bus, skip it
        if (!dirBus2)
            continue;
        while ((entry = readdir(dirBus2))) {
            if (entry->d_name[0] == '.')
                continue;
            iDevAddr = atoi(entry->d_name);
            // device address not valid, skip it
            if (iDevAddr == 0)
                continue;
            memset(strDevicePath, 0, IF0_MAXCHARBUFFER);
            sprintf(strDevicePath, "%s/%03d", strBusPath, iDevAddr);
            // get descriptor of this device
            iDescr = open(strDevicePath, O_RDWR);
            // error opening the device, skip it and try an other
            if (iDescr < 0) {
                continue;
            }
            memset(bufDescriptor, 0, IF0_MAXCHARBUFFER);
            // 18 bytes for the descriptor
            r = read(iDescr, bufDescriptor, DEVICE_DESC_LENGTH);
            // error reading the descriptor for this device; skip it
            if (r < 1) {
                continue;
            }
            // get VID and PID from the descriptor
            unsigned char byteV1 = bufDescriptor[8];
            unsigned char byteV2 = bufDescriptor[9];
            unsigned char byteP1 = bufDescriptor[10];
            unsigned char byteP2 = bufDescriptor[11];
            // shift low value and high value
            lV = ((byteV2 << 8) | byteV1);
            lP = ((byteP2 << 8) | byteP1);
            // get index of SERIAL NUMBER in string descriptor
            byteSerialIndex = bufDescriptor[16];

            // check VID and PID:
            if (((lV == lVID) || (lVID == -1)) && ((lP == lPID) || (lPID == -1))) {  // check the serial number:
                if (CheckSerialNumberIf0(byteSerialIndex, strSerial, iDescr))
                    bDeviceFound = true;
                else if (strSerial[0] == 0)  // empty string, all values ok!
                    bDeviceFound = true;
                if (bDeviceFound) {  // copy the device path to global variable...
                    memset(m_strUsbPath, 0, IF0_MAXCHARBUFFER);
                    strcpy(m_strUsbPath, strDevicePath);
                    close(iDescr);
                    m_lTmpVID = lV;
                    m_lTmpPID = lP;
                    break;
                }
            }
            close(iDescr);
        }
        if (bDeviceFound)
            break;
    }
    if (!bDeviceFound) {
        return ERR_DEVICE_NOT_FOUND;
    }

    return dwErr;
}

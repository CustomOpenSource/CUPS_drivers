/**
 * 
 * Licensed to Custom S.p.A. (Custom) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. Custom licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 * 
*/

// include headers
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <cups/ppd.h>
#include <cups/cups.h>
#include <cups/raster.h>
#include <syslog.h>

#include "compression.h"
#include "dithering.h"

#include <cupsfilters/driver.h>

// include libPtPLm headers
#include "PtPLm.h"
#include "ComErrors.h"

#define VID     0x0dd4
#define PID     0x0205
int PageToPage_StatusCheck(int timeout, int pageCount);
int pageToPageEnabled = 1;

#define MIN(x,y) (((x) < (y)) ? (x) : (y))

#define DEBUG_SYSLOG 1

// PrinterSetUp structure
struct PrinterSetUp {
    float pageWidth;
    float pageHeight;
    int printSpeed;
    int bitsPerPixel;
    int darkness;
    int notchAlignment;
    int paperRotation;
    int paperPresentation;
    int continuousMode;
    int pTPAction;
    int pTPTimeout;
};

struct command {
    int    length;
    char* command;
};

char localBuf[2000];


// print quality commands
static const struct command PrintSpeedCommand [3] = {
    {5, (char[5]){0x1B, '*', 'o', '0', 'Q'}},        // high quality
    {5, (char[5]){0x1B, '*', 'o', '1', 'Q'}},        // Normal
    {5, (char[5]){0x1B, '*', 'o', '2', 'Q'}}         // high speed
};

// print density commands
static const struct command Darkness [5] = {
    {5, (char[5]){0x1B, '&', 'k', '0', 'W'}},          // -50 %
    {5, (char[5]){0x1B, '&', 'k', '1', 'W'}},          // -37 %
    {5, (char[5]){0x1B, '&', 'k', '2', 'W'}},          // -25 %
    {5, (char[5]){0x1B, '&', 'k', '3', 'W'}},          // -12.5 %
    {5, (char[5]){0x1B, '&', 'k', '4', 'W'}},          // 0 %
    {5, (char[5]){0x1B, '&', 'k', '5', 'W'}},          // +12.5 %
    {5, (char[5]){0x1B, '&', 'k', '6', 'W'}},          // +25 %
    {5, (char[5]){0x1B, '&', 'k', '7', 'W'}},          // +37 %
    {5, (char[5]){0x1B, '&', 'k', '8', 'W'}}           // +50 %
};

// cutter mode commands
static const struct command CutCommand= {
    2, (char[2]){0x1B, 'i'},  
};


// document resolution
static const struct command DocumentResolution = {
    7, (char[7]){0x1B, '*', 't', '2', '0', '0', 'R'}
};

// compression commands
static const struct command Compression [2] = {
    {5, (char[5]){0x1B, '*', 'b', '2', 'M'}},   // FE_RLE compression
    {5, (char[5]){0x1B, '*', 'b', '0', 'M'}}    // NO compression
};
 
// end page command 
static const struct command EndPage = {
    4, (char[4]){0x1B, '*', 'r', 'B'}
};

// end page feed
static const struct command EndPageFeed = {
    5,(char[5]){0x1B, '(', 'v', 0x08, 0x00}
};

// start document command
static const struct command StartPage = {
    12, (char[12]){0x18, 0x1B, '*', 'r', '0', 'D', 0x1B, '2', 0x1D, 'e', 0xFF, 0x01}
};

// end document command
static const struct command EndDoc = {
    11, (char[11]){0x1B, '*', 't', '2', '0', '0', 'R', 0x1D, 'P', 0x00, 0x00}   
};

// ignore text: prevents the printer from mistakenly printing text during a graphic print
static const struct command IgnoreText [2] = {
    {5, (char[5]){0x1B, '*', 'r', '0', 'D'}},   // ON
    {5, (char[5]){0x1B, '*', 'r', '0', 'd'}}    // OFF
};

// notch command 
static const struct command NotchAlignmentCommand[2] = {
    {2, (char[2]){0x1D, 0xF6}},     // align at the beginning
    {2, (char[2]){0x1D, 0xF8}},     // align at the end
};


// define PaperRotation command
static const struct command PaperRotation [2] = {
    {3, (char[3]){0x1B, 0x7B, 0x00}},       // disable paper rotation
    {3, (char[3]){0x1B, 0x7B, 0x01}}        // paper rotation 180°
};

 //define VKP80III continuos mode 
static const struct command ContinuosMode[2] = {
    {3, (char[3]){0x1D, 0x65, 0x12}},    // Off
    {3, (char[3]){0x1D, 0x65, 0x14}}     // On
};

// define VKP80III paper presentation command
static const struct command PaperPresentation[8] = {
    {8, (char[8]){0x1D, 'e', 0x03, 0x00, 0x1D, 'e', 0x08, 0x04}},          // 0 30 mm   
    {8, (char[8]){0x1D, 'e', 0x03, 0x00, 0x1D, 'e', 0x08, 0x05}},          // 1 40 mm
    {8, (char[8]){0x1D, 'e', 0x03, 0x00, 0x1D, 'e', 0x08, 0x0B}},          // 2 80 mm
    {8, (char[8]){0x1D, 'e', 0x03, 0x00, 0x1D, 'e', 0x08, 0x10}},          // 3 120 mm
    {8, (char[8]){0x1D, 'e', 0x03, 0x00, 0x1D, 'e', 0x08, 0x16}},          // 4 160 mm
    {8, (char[8]){0x1D, 'e', 0x03, 0x00, 0x1D, 'e', 0x08, 0x1A}},          // 5 200 mm
    {8, (char[8]){0x1D, 'e', 0x03, 0x00, 0x1D, 'e', 0x08, 0x20}}           // 6 240 mm
};

// define VKP80III eject retract timeout
//static const struct command EjectRetractTimeout[14] =
static const struct command EjectRetractTimeout[14] = {
    {2, (char[2]){'E', 0x00}},          // 0 none - eject
    {2, (char[2]){'E', 0x00}},          // 0 none - Hold 
    {2, (char[2]){'E', 0x05}},          // 1 5 sec
    {2, (char[2]){'E', 0x0A}},          // 1 10 sec
    {2, (char[2]){'E', 0x14}},          // 1 20 sec
    {2, (char[2]){'E', 0x1E}},          // 1 30 sec
    {2, (char[2]){'E', 0x3C}},          // 1 60 sec
    {2, (char[2]){'E', 0x78}},          // 1 120 sec
    {2, (char[2]){'R', 0x05}},          // 1 5 sec
    {2, (char[2]){'R', 0x0A}},          // 1 10 sec
    {2, (char[2]){'R', 0x14}},          // 1 20 sec
    {2, (char[2]){'R', 0x1E}},          // 1 30 sec
    {2, (char[2]){'R', 0x3C}},          // 1 60 sec
    {2, (char[2]){'R', 0x78}}           // 5 120 sec
};


// PAGE TO PAGE

// begin doc force eject
static const struct command beginDocForceEject = { 
    3, (char[3]){0x1D, 'e', 0x05}
};

static const struct command beginDocForceRetract = {
    3, (char[3]){0x1D, 'e', 0x02}
};

static const struct command pageToPage_action[2] = {
    {3, (char[3]){0x1D, 'e', 0x05}},        //eject
    {3, (char[3]){0x1D, 'e', 0x02}}         //retract
};
 
static const int pageToPage_actionIfXCmd[2] = {
    EJECT_COMMAND,
    RETRACT_COMMAND
};

static const struct command clearPage = {
    6, (char[6]){0x1C, 0xC0, 0x18, 0x10, 0x14, 0x17}
};

// timeout (sec)
static const int pageToPage_timeout[7] = {
    5,
    10,
    20,
    30,
    40,
    60,
    120
};

static const struct command sendNull = {
    5, (char[5]){0x1B, '*', 'r', '0', 'N'}
};


/*------------------------------------------------------------------------------------------------

	SendCommand:
	this function sends a specified command, based on command structure, to the printer

------------------------------------------------------------------------------------------------*/

void SendCommand(struct command output){

    int i;
    
    #ifdef DEBUG_SYSLOG
        char debug_buf[2000];
        memset(debug_buf,0,sizeof(debug_buf));
        for ( i = 0; i < output.length; i++){
            sprintf(debug_buf+strlen(debug_buf)," %02X", (*(output.command+i)) & 0xFF);
        }
        syslog(LOG_NOTICE,"SendCommand = %s\n",debug_buf);
    #endif

    for ( i = 0; i < output.length; i++)
        putchar(output.command[i]);

}

/*------------------------------------------------------------------------------------------------

	FindOption:
	this function scan ppd file and search a specified choice and return the result
	of the selection

------------------------------------------------------------------------------------------------*/

int FindOption(const char * choiceName, ppd_file_t * ppd){

    ppd_choice_t * choice;
    ppd_option_t * option;

    choice = ppdFindMarkedChoice(ppd, choiceName);

    if (choice == NULL){
        if ((option = ppdFindOption(ppd, choiceName)) == NULL)
            return -1;
        if ((choice = ppdFindChoice(option,option->defchoice)) == NULL)
            return -1;
    }

    return atoi(choice->choice);
}


/*------------------------------------------------------------------------------------------------

	initializeSettings:
	this function read the ppd choices and set the settings structure

------------------------------------------------------------------------------------------------*/

void initializeSettings(char * commandLineOptionSettings, struct PrinterSetUp * settings){

    ppd_file_t *    ppd         = NULL;
    cups_option_t * options     = NULL;
    int             numOptions  = 0;

    ppd = ppdOpenFile(getenv("PPD"));

    ppdMarkDefaults(ppd);

    numOptions = cupsParseOptions(commandLineOptionSettings, 0, &options);

    if ((numOptions != 0) && (options != NULL)){
        cupsMarkOptions(ppd, numOptions, options);
        cupsFreeOptions(numOptions, options);
    }

    memset(settings, 0x00, sizeof(struct PrinterSetUp));

    settings->printSpeed = FindOption("PrintSpeed", ppd); 
    settings->darkness = FindOption("Darkness", ppd);
    settings->bitsPerPixel = FindOption("BitsPerPixel", ppd);
    settings->notchAlignment = FindOption("NotchAlignment", ppd);
    settings->paperRotation = FindOption("PaperRotation", ppd);
    settings->paperPresentation = FindOption("PaperPresentation", ppd);
    settings->pTPAction = FindOption("pTPAction", ppd);
    settings->pTPTimeout = FindOption("pTPTimeout", ppd);
    settings->continuousMode = FindOption("ContinuousMode", ppd);
    
#ifdef DEBUG_SYSLOG
	syslog(LOG_NOTICE,"settings->printSpeed = %d\n",settings->printSpeed);
	syslog(LOG_NOTICE,"settings->darkness = %d\n",settings->darkness);
	syslog(LOG_NOTICE,"settings->bitsPerPixel = %d\n",settings->bitsPerPixel);
	syslog(LOG_NOTICE,"settings->notchAlignment = %d\n",settings->notchAlignment);
	syslog(LOG_NOTICE,"settings->paperRotation = %d\n",settings->paperRotation);
	syslog(LOG_NOTICE,"settings->paperPresentation = %d\n",settings->paperPresentation);
	syslog(LOG_NOTICE,"settings->pTPAction = %d\n",settings->pTPAction);
	syslog(LOG_NOTICE,"settings->pTPTimeout = %d\n",settings->pTPTimeout);
	syslog(LOG_NOTICE,"settings->continuousMode = %d\n",settings->continuousMode);
#endif

    ppdClose(ppd);
}

/*------------------------------------------------------------------------------------------------

	SendWithCompression
	this function compress a raster document linee and send it to the printer

------------------------------------------------------------------------------------------------*/

void SendWithCompression(unsigned char *bmp, int size, struct command compression_cmd) {

	int i;
	int bytes = 0;

    #ifdef DEBUG_SYSLOG
        char debug_buf_1[2000];
        memset(debug_buf_1, 0, sizeof(debug_buf_1));
        for (i = 0; i < size; i++) {
            sprintf(debug_buf_1 + strlen(debug_buf_1), " %02X", *(bmp+i) & 0xFF);
        }
    #endif
        
    if(*(compression_cmd.command + 3) == '1'){
    	// RLE compression - 1M
    	// "ESC * b size W" specifies uncompressed data size
	   	memset(localBuf,0,sizeof(localBuf));
	   	bytes = RLE_compress(bmp, size, (unsigned char *)localBuf);
	   	printf("\x1B\x2A\x62%d\x57", size);

   	} else if(*(compression_cmd.command + 3) == '2'){
   		// FE_RLE compression - 2M
   		// "ESC * b size W" specifies compressed data size
	   	memset(localBuf,0,sizeof(localBuf));
	   	bytes = FE_RLE_compress(bmp, size, (unsigned char *)localBuf);
	   	printf("\x1B\x2A\x62%d\x57", bytes);
   	}
   	
   	for(i = 0; i < bytes; i++)
   		putchar(localBuf[i]);
   		
    #ifdef DEBUG_SYSLOG
        char debug_buf_2[2000];
        memset(debug_buf_2, 0, sizeof(debug_buf_2));
        for (i = 0; i < bytes; i++){
            sprintf(debug_buf_2 + strlen(debug_buf_2), " %02X", localBuf[i] & 0xFF);
        }
    #endif
    
}


/*----------------------------------------------------------------------------------------------------

	MAIN

-----------------------------------------------------------------------------------------------------*/

int main(int argc, char *argv[]) {

    #ifdef DEBUG_SYSLOG
        syslog(LOG_NOTICE,"main start\n");
    #endif

    int cupsRasterFileDescriptor = 0;   // CUPS raster file descriptor
    int currentPageIndex = 0;           // Current page
    int numWhiteScanLines   = 0;        // Number of scanlines that were entirely black

    cups_raster_t * rasterFileStruct = NULL; // CUPS raster file
    cups_page_header2_t  cupsHeader; // CUPS page header

    struct PrinterSetUp    settings; // Configuration settings

    int currentPage = 0;
    char * uri_str;
    int usb_flag = 0;
    int status_stop_printing=0;
    unsigned long result = 0;  

    // If argc is different lower than 6 or greater than 7 return failure
    if (argc < 6 || argc > 7){
        return EXIT_FAILURE;
    }

    if (argc == 7){

        // Open the raster file passed from CUPS
        if ((cupsRasterFileDescriptor = open(argv[6], O_RDONLY)) == -1){

            // Error opening the raster file
            perror("ERROR: Unable to open raster file ");

            return EXIT_FAILURE;
        }
    }    

    #ifdef DEBUG_SYSLOG
        for(int i = 0; i < 7; i++)
            syslog(LOG_NOTICE,"argv[%i]: \"%s\"",i,argv[i]);
    #endif
    
    //verify if uri is usb
    usb_flag = 0;
    uri_str = getenv("DEVICE_URI");
    if ( (uri_str != NULL) && (strstr(uri_str, "usb:") != NULL) ) {
        usb_flag = 1;
    }    
    
    #ifdef DEBUG_SYSLOG
        syslog(LOG_NOTICE,"Evaluating page to page availability...\n");
        syslog(LOG_NOTICE,"usb_flag = %d\n", usb_flag);
        syslog(LOG_NOTICE,"uri_str = %s\n", uri_str);
    #endif
    // Page To Page available only for USB uri
    if ( (usb_flag) && ((settings.pTPAction < 3) || (settings.pTPAction == 5)) ) {
		pageToPageEnabled = 1;
		result = openUSBDevice(VID, PID, NULL);

  		// check openUSBDevice result if is not ok exit and show error number see the file ComErrors.h
  		if (result != SUCCESS) {
			pageToPageEnabled = 0;
  		}  
  		
    } else {
		pageToPageEnabled = 0;
    }

    // Read the ppd settings and put them in the settings struct
    initializeSettings(argv[5], &settings);

    // set end NULL byte at end printing
    SendCommand(sendNull);

    // set the paper rotation
    SendCommand(PaperRotation[settings.paperRotation]);
	
    // set continuos mode is request
	SendCommand(ContinuosMode[settings.continuousMode]);
	
    // IGNORE TEXT -> ON
    SendCommand(IgnoreText[0]);
    
    // SET PRINT SPEED
    if(settings.printSpeed > -1)
        SendCommand(PrintSpeedCommand[settings.printSpeed]);

    // SET PRINT DENSITY
    if(settings.darkness > -1 && settings.darkness < 10) // 10 means Don't set
        SendCommand(Darkness[settings.darkness]);

    
    // Read the CUPS raster file
    rasterFileStruct = cupsRasterOpen(cupsRasterFileDescriptor, CUPS_RASTER_READ);

    currentPageIndex = 0;

    while (cupsRasterReadHeader2(rasterFileStruct, &cupsHeader)){

        #ifdef DEBUG_SYSLOG
            syslog(LOG_NOTICE,"received cupsHeader.cupsHeight=%d\n",cupsHeader.cupsHeight);
            syslog(LOG_NOTICE,"received cupsHeader.cupsWidth=%d\n",cupsHeader.cupsWidth);
            syslog(LOG_NOTICE,"received cupsHeader.cupsBitsPerPixel=%d\n",cupsHeader.cupsBitsPerPixel);
            syslog(LOG_NOTICE,"received cupsHeader.cupsBitsPerColor=%d\n",cupsHeader.cupsBitsPerColor);
            syslog(LOG_NOTICE,"received cupsHeader.cupsBytesPerLine=%d\n",cupsHeader.cupsBytesPerLine);
            syslog(LOG_NOTICE,"received cupsHeader.cupsNumColors=%d\n",cupsHeader.cupsNumColors);
        #endif
    	
    	if ((cupsHeader.cupsHeight == 0) || (cupsHeader.cupsBytesPerLine == 0))
            break;


        // If the page contains Roll in its name, then it's a roll
    	if( strstr(cupsHeader.cupsPageSizeName, (const char*)"Roll") ){
            // -> roll
            printf("\x1B&l%dR", cupsHeader.cupsHeight);//ESC & l %d R, header.cupsHeight
            #ifdef DEBUG_SYSLOG
                syslog(LOG_NOTICE,"1B 26 6C height 52 -> 1B & l %d R -> set max page length", cupsHeader.cupsHeight);
            #endif

        } else {
            //normal
        	printf("\x1B&l%dP",cupsHeader.cupsHeight);//ESC & l %d P, header.cupsHeight
            #ifdef DEBUG_SYSLOG
                syslog(LOG_NOTICE,"1B 26 6C height 50 -> 1B & l %d P -> set page length", cupsHeader.cupsHeight);
            #endif
        }

		if (settings.pTPAction == 3)
			SendCommand(beginDocForceEject);
		if (settings.pTPAction == 4)
			SendCommand(beginDocForceRetract);

        SendCommand(StartPage);
        
    	// send document resolution
    	SendCommand(DocumentResolution);

	    printf("\033*p0Y"); // Y axis absolute move
        #ifdef DEBUG_SYSLOG
            syslog(LOG_NOTICE,"1B 2A 70 30 59 -> 1B * p 0 Y\n");
        #endif

        currentPageIndex++;

        // start compression mode
        SendCommand(Compression[0]);

    	numWhiteScanLines = 0;

        int bytesPerLine;
        unsigned char *ditheredRasterImage = NULL;
        if(settings.bitsPerPixel == 8){
            int imageBytesW = cupsHeader.cupsBytesPerLine;
            int imageH = cupsHeader.cupsHeight;
            int imageBufferLength = imageH * imageBytesW;

            // This buffer will contain a copy of the raster
            unsigned char* rasterImage = malloc(imageBufferLength);
            int ditheredImageLength = 0;

            // Copy the raster
            if (cupsRasterReadPixels(rasterFileStruct, rasterImage, imageBufferLength) < 1){
                fprintf(stderr, "Error when reading pixels");
                break;
            }

            // correctly set the bytesPerLine value
            bytesPerLine = imageBytesW / 8;
            if (imageBytesW % 8)
                bytesPerLine++;

            DitherRaw1ImageWithFloydSteinberg(rasterImage, imageBufferLength, imageBytesW, imageH, &ditheredRasterImage, &ditheredImageLength);

            #ifdef DEBUG_SYSLOG
                syslog(LOG_INFO, "ditheredImageLength %d", ditheredImageLength);
                syslog(LOG_INFO, "nRowBytesDST %d", bytesPerLine);
            #endif

        } else {
            // Save the value for later
            bytesPerLine = cupsHeader.cupsBytesPerLine;
        }
	
        // Scan the CUPS raster file for each document page
        for (int y = 0; y < cupsHeader.cupsHeight; y ++){

            // Copy the current line into rasterLine
            unsigned char rasterLine[bytesPerLine];
            if(settings.bitsPerPixel == 8){
                for(int co = 0; co < bytesPerLine; co++){
                    rasterLine[co] = ditheredRasterImage[y * bytesPerLine + co];
                }

            } else if(settings.bitsPerPixel == 1) {

                // Copy a line from the raster
                if (cupsRasterReadPixels(rasterFileStruct, rasterLine, bytesPerLine) < 1){
                    fprintf(stderr, "Error when reading pixels");
                    break;
                }
            }


            // Check if the line is completely white
            short lineIsWhite = 1;
            for (int i = bytesPerLine - 1; i >= 0; i--){
                if (((char) *(rasterLine + i)) != ((char) 0x00)){
                    lineIsWhite = 0;
                    break;
                }
            }

            // count how many blank lines you found
            if (lineIsWhite)
                numWhiteScanLines++;

    	    if (lineIsWhite == 0){
	            if (numWhiteScanLines > 0){
                    numWhiteScanLines = 0;

                    // Move Y axis to line number y (leaving them white)
                    printf("\033*p%dY", y);

                    #ifdef DEBUG_SYSLOG
                        syslog(LOG_NOTICE,"1B 2A 70 y 59 -> 1B * p val_of_y Y -> 1B * p %d Y\n", y);
                    #endif
                }

                // Send a graphic line using the selected compression
                SendWithCompression(rasterLine, bytesPerLine, Compression[0]);
            }

        }

	    // END COMPRESSION MODE COMMAND
    	SendCommand(Compression[1]);

		usleep(200 * 1000);
        
        if (pageToPageEnabled) {
            #ifdef DEBUG_SYSLOG
                syslog(LOG_NOTICE,"PAGE TO PAGE ENABLED\n");
                syslog(LOG_NOTICE,"settings.pTPAction = %d\n", settings.pTPAction);
            #endif
		
            fflush(stdout);
            if (settings.pTPAction != 5)
                status_stop_printing = PageToPage_StatusCheck(pageToPage_timeout[settings.pTPTimeout], currentPage);
            else
                status_stop_printing = PageToPage_StatusCheck(24*3600, currentPage); // 3600 sec = 1h

            #ifdef DEBUG_SYSLOG
                syslog(LOG_NOTICE,"status_stop_printing = %d\n", status_stop_printing);
            #endif
                
            if ( status_stop_printing ) {
                
                if ( status_stop_printing > 0 ) {
                    unsigned long dwResult = SUCCESS;
                
                    if (settings.pTPAction < 3){
                        #ifdef DEBUG_SYSLOG
                            syslog(LOG_NOTICE,"SendPrinterEjectCmd \n");
                        #endif
                        dwResult = SendPrinterEjectCmd(pageToPage_actionIfXCmd[settings.pTPAction]);    //pageToPage_actionIfXCmd[settings.pTPAction]);
                    }
                }
		
                if (settings.pTPAction != 5) {
                    #ifdef DEBUG_SYSLOG
                        syslog(LOG_NOTICE,"pTPAction is not 5, clearing\n");
                    #endif
                    result = SendPrinterClearCmd();
                    
                    SendCommand(clearPage);
                    while (cupsRasterReadHeader(rasterFileStruct, &cupsHeader));

                    cupsRasterClose(rasterFileStruct);

                    if (cupsRasterFileDescriptor != 0)
                        close(cupsRasterFileDescriptor);

                    closeUSBDevice();

                    return EXIT_SUCCESS;
                }
            }
	    }

        // END PAGE COMMAND
        SendCommand(EndPage);
        
        // ALIGN TO NOTCH
        if (settings.notchAlignment == 1)
            SendCommand(NotchAlignmentCommand[1]);	

        // END PAGE FEED
        SendCommand(EndPageFeed);

        // SEND CUT COMMAND
		SendCommand(CutCommand);
        
		SendCommand(PaperPresentation[settings.paperPresentation]);

		if (pageToPageEnabled)
			fflush(stdout);

    }
    
     if (pageToPageEnabled) {
        #ifdef DEBUG_SYSLOG
            syslog(LOG_NOTICE,"PAGE TO PAGE ENABLED\n");
            syslog(LOG_NOTICE,"settings.pTPAction = %d\n", settings.pTPAction);
        #endif
    
        fflush(stdout);
        if (settings.pTPAction != 5)
            status_stop_printing = PageToPage_StatusCheck(pageToPage_timeout[settings.pTPTimeout], currentPage);
        else
            status_stop_printing = PageToPage_StatusCheck(24*3600, currentPage); // 3600 sec = 1h

        #ifdef DEBUG_SYSLOG
            syslog(LOG_NOTICE,"status_stop_printing = %d\n", status_stop_printing);
        #endif
        
        if ( status_stop_printing ) {
            
            if ( status_stop_printing > 0 ) {
                unsigned long dwResult = SUCCESS;
            
                if (settings.pTPAction < 3){
                    #ifdef DEBUG_SYSLOG
                        syslog(LOG_NOTICE,"SendPrinterEjectCmd \n");
                    #endif
                    dwResult = SendPrinterEjectCmd(pageToPage_actionIfXCmd[settings.pTPAction]);//pageToPage_actionIfXCmd[settings.pTPAction]);
                }
            }
    
            if (settings.pTPAction != 5) {
                #ifdef DEBUG_SYSLOG
                    syslog(LOG_NOTICE,"pTPAction is not 5, clearing\n");
                #endif
                result = SendPrinterClearCmd();
                
                SendCommand(clearPage);
                while (cupsRasterReadHeader(rasterFileStruct, &cupsHeader));

                cupsRasterClose(rasterFileStruct);

                if (cupsRasterFileDescriptor != 0)
                    close(cupsRasterFileDescriptor);

                closeUSBDevice();

                return EXIT_SUCCESS;
            }
        }
    }

    // close the cups raster document
    cupsRasterClose(rasterFileStruct);

    if (cupsRasterFileDescriptor != 0)
        close(cupsRasterFileDescriptor);
    
    // SEND END DOCUMENT
    SendCommand(EndDoc);

    // IGNORE TEXT -> OFF
    SendCommand(IgnoreText[1]);

    #ifdef DEBUG_SYSLOG
        syslog(LOG_NOTICE,"main stop\n");
        closelog();
    #endif

    return (currentPageIndex == 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}

// define read buffer max size
#define READ_BUFFER_SIZE      2024

#define STATUS_IDLE				            0
#define STATUS_SPOOLING_NOTACTIVE		    1
#define STATUS_WAITFOR_SPOOLING_ACTIVE		2
#define STATUS_SPOOLING_ACTIVE			    3
#define STATUS_WAITFOR_SPOOLING_NOTACTIVE	4
#define STATUS_WAITFOR_TICKET_PRESENT		5
#define STATUS_CHECK_FOR_EXTRACTION		    6
#define STATUS_EXTRACTED			        7

#define WAITFOR_SPOOLING_ACTIVE_TIMEOUT		( 1 * 1000)
#define WAITFOR_SPOOLING_NOTACTIVE_TIMEOUT	(60 * 1000)
#define WAITFOR_TICKET_PRESENT_TIMEOUT		( 5 * 1000)



/*

  checkStatus:  
  this function proces the printer status buffer and show error or interestig status

*/


#define MAX_GETSTATUS_RETRY_COUNT	10

unsigned long RetryGetPrinterStatus(unsigned char* bufferRecv,const unsigned long dwSize,unsigned long* dwRead) {
	unsigned long result=SUCCESS;
	int counter;

	for (counter=0; counter<MAX_GETSTATUS_RETRY_COUNT; counter++) {
		result=GetPrinterStatus(bufferRecv,dwSize,dwRead);

		if ( result == SUCCESS )
			break;

		usleep(10 * 1000);
	}

	return result;
}

/* *
 * return values:
 * 0 -> go on printing
 * -1 -> abort, error
 *  1 -> timeout waiting for extraction
 * */
int PageToPage_StatusCheck(int timeout, int pageCount) {

    struct tm* Localtime;
    unsigned long dwRead = 0;
    struct timeval SecMicrosec;  
    unsigned long dwResult = 0;  
    unsigned char buffer[READ_BUFFER_SIZE];
    unsigned long start_time, current_time;
    int status;
    unsigned long loopTimeout;
    unsigned char spooling, ticket_present;
    int extraction_timeout;

    extraction_timeout = timeout * 1000;


  // initialize th buffer
    bzero(buffer, READ_BUFFER_SIZE);

	status = STATUS_IDLE;
    dwResult = RetryGetPrinterStatus(buffer, READ_BUFFER_SIZE, &dwRead);
    if (dwResult != SUCCESS) {
        return -1;
    }

    // show hex printer response
    gettimeofday(&SecMicrosec, NULL);
    current_time = ((unsigned long)SecMicrosec.tv_sec * 1000) + ((unsigned long)SecMicrosec.tv_usec / 1000) ;
    Localtime = localtime(&(SecMicrosec.tv_sec));

    spooling = buffer[3] & 0x4;
    ticket_present = buffer[2] & 0x20;

    if ( pageCount ==  1 ) {
        status = STATUS_CHECK_FOR_EXTRACTION;
        loopTimeout= extraction_timeout;

    } else if ( spooling ) {
        status = STATUS_SPOOLING_ACTIVE;
        loopTimeout= WAITFOR_SPOOLING_NOTACTIVE_TIMEOUT;

    } else {
        status = STATUS_WAITFOR_SPOOLING_ACTIVE;
        loopTimeout= WAITFOR_SPOOLING_ACTIVE_TIMEOUT;
    }

    gettimeofday(&SecMicrosec, NULL);
    start_time = ((unsigned long)SecMicrosec.tv_sec * 1000) + ((unsigned long)SecMicrosec.tv_usec / 1000) ;


    //syslog(LOG_ERR, "P2P first status=%d, spooling=%d ticket_present=%d\n", status, spooling, ticket_present);

    // read 1 minutes the printer status
    while (1) {
        dwResult = RetryGetPrinterStatus(buffer, READ_BUFFER_SIZE, &dwRead);
        if (dwResult != SUCCESS) {
            return -1;
        }

        spooling = buffer[3] & 0x4;
        ticket_present = buffer[2] & 0x20; 

        // show hex printer response
        gettimeofday(&SecMicrosec, NULL);
        current_time = ((unsigned long)SecMicrosec.tv_sec * 1000) + ((unsigned long)SecMicrosec.tv_usec / 1000) ;
        Localtime = localtime(&(SecMicrosec.tv_sec));
        switch (status) {
            case STATUS_WAITFOR_SPOOLING_ACTIVE:
                if ( spooling ) {
                    status = STATUS_SPOOLING_ACTIVE;
                    loopTimeout = WAITFOR_SPOOLING_NOTACTIVE_TIMEOUT;
                    start_time = current_time;
                }
                break;

            case STATUS_SPOOLING_ACTIVE:	
                status=STATUS_WAITFOR_SPOOLING_NOTACTIVE;

            case STATUS_WAITFOR_SPOOLING_NOTACTIVE:
                if ( !spooling ) {
                    status = STATUS_WAITFOR_TICKET_PRESENT;
                    loopTimeout = WAITFOR_TICKET_PRESENT_TIMEOUT;
                    start_time = current_time;
                }
                break;

            case STATUS_WAITFOR_TICKET_PRESENT:
                if ( ticket_present ) {
                    status = STATUS_CHECK_FOR_EXTRACTION;
                    loopTimeout = extraction_timeout;//http://localhost:631/jobs/
                    start_time = current_time;
                }
                break;

            case STATUS_CHECK_FOR_EXTRACTION:
                if ( !ticket_present )
                    status = STATUS_EXTRACTED;
                break;

            default:
                status = STATUS_IDLE;
                break;
        }

        if ( (status == STATUS_EXTRACTED) || ( status == STATUS_IDLE) )
            break;


        if ( (current_time - start_time) > loopTimeout ) {
            /* TIMEOUT*/
            if ( status ==  STATUS_WAITFOR_SPOOLING_ACTIVE) {
                status = STATUS_CHECK_FOR_EXTRACTION;
                loopTimeout = extraction_timeout;
                start_time = current_time;

            } else if ( status ==  STATUS_WAITFOR_TICKET_PRESENT) {
                status = STATUS_CHECK_FOR_EXTRACTION;
                loopTimeout = extraction_timeout;
                start_time = current_time;

            } else
                break;
        }

        // sleep 100 msec
        usleep(100 * 1000);
    }

    // check closeUSBDevice result if is not ok exit and show error number see the file ComErrors.h
    if (dwResult != SUCCESS) {
        return -1;
    }
        
    if (status == STATUS_EXTRACTED) {
        return 0; 
    }

    return 1;
}

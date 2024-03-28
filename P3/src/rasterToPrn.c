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

#define MIN(x,y) (((x) < (y)) ? (x) : (y))

#define DEBUG_SYSLOG 1

#define PRINT_WIDTH 80 //line width (mm or bytes)

#define PRINT_MAX_HEIGHT 3336 //cups points

// PrinterSetUp structure
struct PrinterSetUp {
    float pageWidth;
    float pageHeight;
    int printSpeed;
    int bitsPerPixel;
    int cutterMode;
    int darkness;
    int cashDrawer
};

struct command {
    int    length;
    char* command;
};

char localBuf[2000];


// print quality commands
static const struct command PrintSpeedCommand [3] = {
    {5, (char[5]){0x1B,'*','o','0','Q'}},        // 0 High Quality
    {5, (char[5]){0x1B,'*','o','1','Q'}},        // 0 Normal
    {5, (char[5]){0x1B,'*','o','2','Q'}}         // 1 high speed
};

// print density commands
static const struct command Darkness [5] = {
    {5, (char[5]){0x1B, '&', 'k', '2', 'W'}},          // 0 -25 %
    {5, (char[5]){0x1B, '&', 'k', '3', 'W'}},          // 1 -12.5 %
    {5, (char[5]){0x1B, '&', 'k', '4', 'W'}},          // 2 0 %
    {5, (char[5]){0x1B, '&', 'k', '5', 'W'}},          // 3 +12.5 %
    {5, (char[5]){0x1B, '&', 'k', '6', 'W'}},          // 4 +25 %
};

// page cut type commands
static const struct command CutCommand [5] = {
    {5,(char[5]){0x0A, 0x0A, 0x0A, 0x0A, 0x0A}},    // 0 no cut
    {2,(char[2]){0x1B, 0x6D}}, 		                // 1 Partial cut end page
    {2,(char[2]){0x1B, 0x6D}}		                // 2 Partial cut end  job
};


// document resolution
static const struct command DocumentResolution = {
    7, (char[7]){0x1B,'*','t','2','0','0','R'}
};


// compression commands
static const struct command Compression [2] = {
    {5, (char[5]){0x1B,'*','b','2','M'}},   // FE_RLE compression
    {5, (char[5]){0x1B,'*','b','0','M'}}    // NO compression
};
 
  
// begin page command
static const struct command BeginPage = {
    11,(char[11]){0x18, 0x1D, 0xF6, 0x1D, 0xFF, 'j', '1', 0x01, 0xB8, 0x1B, '2'}
};
 
// end page command 
static const struct command endPage = {
    4, (char[4]){0x1B, '*', 'r', 'B'}   //1B *rB
};

// end page feed
static const struct command endPageFeed = {
    5,(char[5]){0x1B, '(', 'v', 0x08, 0x00}
};

// end document command
static const struct command endDoc = {
    11, (char[11]){0x1B, '*', 't', '2', '0', '0', 'R', 0x1D, 'P', 0x00, 0x00}   
};

// ignore text: prevents the printer from mistakenly printing text during a graphic print
static const struct command ignoreText [2] = {
    {5, (char[5]){0x1B, '*', 'r', '0', 'D'}},   // ON
    {5, (char[5]){0x1B, '*', 'r', '0', 'd'}}    // OFF
};

// define NoReset command
static const struct command NoReset = {
    6, (char[6]){0x1D, 0xFF, 0x3C, 0x31, 0x01, 0x8A}
};

// define cash drawer commands
static const struct command CashDrawer = {
    5, (char[5]){0x1B, 'p', '0', 0x80, 0x80}    // open cash drawer
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
    settings->cutterMode = FindOption("CutterMode", ppd);
    settings->cashDrawer = FindOption("CashDrawer", ppd);
    
#ifdef DEBUG_SYSLOG
	syslog(LOG_NOTICE,"settings->printSpeed = %d\n",settings->printSpeed);
	syslog(LOG_NOTICE,"settings->darkness = %d\n",settings->darkness);
	syslog(LOG_NOTICE,"settings->bitsPerPixel = %d\n",settings->bitsPerPixel);
	syslog(LOG_NOTICE,"settings->cashDrawer = %d\n",settings->cashDrawer);
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

    // Read the ppd settings and put them in the settings struct
    initializeSettings(argv[5], &settings);

    // IGNORE TEXT -> ON
    SendCommand(ignoreText[0]);
    
    // SET PRINT SPEED
    if(settings.printSpeed > -1)
        SendCommand(PrintSpeedCommand[settings.printSpeed]);

    // SET PRINT DENSITY
    if(settings.darkness > -1 && settings.darkness < 10) // 10 means Don't set
        SendCommand(Darkness[settings.darkness]);

    
    // Read the CUPS raster file
    rasterFileStruct = cupsRasterOpen(cupsRasterFileDescriptor, CUPS_RASTER_READ);
    
    // SEND BEING PAGE COMMAND
   	SendCommand(BeginPage);

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

    	// send document resolution
    	SendCommand(DocumentResolution);

        if( settings.cashDrawer == 1 )
            SendCommand(CashDrawer);

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

            // is this necessary?
            //bytesPerLine = MIN(PRINT_WIDTH, bytesPerLine);

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

        // END PAGE COMMAND
        SendCommand(endPage);

        // END PAGE FEED
        SendCommand(endPageFeed);	

        // SEND CUT COMMAND
	    if (settings.cutterMode == 1 || settings.cutterMode == 2) {
		    SendCommand(CutCommand[settings.cutterMode]);
		}

        // OPEN THE CASH DRAWER
        if( settings.cashDrawer == 2 )
            SendCommand(CashDrawer);

    }

    // close the cups raster document
    cupsRasterClose(rasterFileStruct);

    if (cupsRasterFileDescriptor != 0)
        close(cupsRasterFileDescriptor);
    
    // SEND END DOCUMENT
    SendCommand(endDoc);

    // IGNORE TEXT -> OFF
    SendCommand(ignoreText[1]);

	
    // SEND CUT COMMAND
	if (settings.cutterMode == 3 || settings.cutterMode == 4) {
		SendCommand(CutCommand[settings.cutterMode]);
	}

    // OPEN THE CASH DRAWER
    if( settings.cashDrawer == 3 )
        SendCommand(CashDrawer);
        

    #ifdef DEBUG_SYSLOG
        syslog(LOG_NOTICE,"main stop\n");
        closelog();
    #endif

    return (currentPageIndex == 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}




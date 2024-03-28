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

// PrinterSetUp structure
struct PrinterSetUp {
    float pageWidth;
    float pageHeight;
    int printSpeed;
    int bitsPerPixel;
    int cutterMode;
    int tearOffDistance;
    int darkness;
    int notchAlignment;
    int recoveryMode;
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
static const struct command CutCommand [3] = {
    {6,(char[6]){0x1C, 0xC0, '3', 0x1C, 0xC0, '0'}},    // NoCut
    {6,(char[6]){0x1C, 0xC0, '1', 0x1C, 0xC0, '0'}},    // TotalCutEndPage
    {6,(char[6]){0x1C, 0xC0, '1', 0x1C, 0xC0, '0'}}     // TotalCutEndDoc
                                                        // Tear off
};


// document resolution
static const struct command DocumentResolution = {
    7, (char[7]){0x1B,'*','t','2','0','0','R'}
};

// compression commands
static const struct command Compression [2] = {
    {5, (char[5]){0x1B,'*','b','1','M'}},   // FE_RLE compression
    {5, (char[5]){0x1B,'*','b','0','M'}}    // NO compression
};
 
  
// begin page command
static const struct command BeginPage = {
    8, (char[8]){0x1c, '<', 'E', 'P', 'O', 'S', '>', 0x18}
};
 
// end page command 
static const struct command EndPage = {
    4, (char[4]){0x1B, '*', 'r', 'B'}
};

// end document command
static const struct command EndDoc = {
    3, (char[3]){0x1D, 'e', 0x10}
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

// recovery modes commands           
static const struct command RecoveryModeCommand [4] = {
    {3, (char[3]){0x1C, 0xC1, 0x00}},           // No_Recovery 
    {3, (char[3]){0x1C, 0xC1, 0x0F}},           // Normal_Recovery
    {3, (char[3]){0x1C, 0xC1, 0x18}},           // Total_Recovery 
    {3, (char[3]){0x1C, 0xC1, 0xFF}}            // None 
};


// tear off distance command
static const struct command TearOffDistanceCommand [11] = {
    {10, (char[10]){0x1C, 0xC0, 'T', 0x2b, 0x1C, 0xC0, '9', 0x1C, 0xC0, 'L'}},   // +0mm
    {10, (char[10]){0x1C, 0xC0, 'T', 0x2d, 0x1C, 0xC0, '9', 0x1C, 0xC0, 'L'}},   // +2mm
    {10, (char[10]){0x1C, 0xC0, 'T', 0x2f, 0x1C, 0xC0, '9', 0x1C, 0xC0, 'L'}},   // +4mm
    {10, (char[10]){0x1C, 0xC0, 'T', 0x31, 0x1C, 0xC0, '9', 0x1C, 0xC0, 'L'}},   // +6mm
    {10, (char[10]){0x1C, 0xC0, 'T', 0x33, 0x1C, 0xC0, '9', 0x1C, 0xC0, 'L'}},   // +8mm
    {10, (char[10]){0x1C, 0xC0, 'T', 0x35, 0x1C, 0xC0, '9', 0x1C, 0xC0, 'L'}},   // +10mm
    {10, (char[10]){0x1C, 0xC0, 'T', 0x37, 0x1C, 0xC0, '9', 0x1C, 0xC0, 'L'}},   // +12mm
    {10, (char[10]){0x1C, 0xC0, 'T', 0x39, 0x1C, 0xC0, '9', 0x1C, 0xC0, 'L'}},   // +14mm
    {10, (char[10]){0x1C, 0xC0, 'T', 0x3b, 0x1C, 0xC0, '9', 0x1C, 0xC0, 'L'}},   // +16mm
    {10, (char[10]){0x1C, 0xC0, 'T', 0x3d, 0x1C, 0xC0, '9', 0x1C, 0xC0, 'L'}},   // +18mm
    {10, (char[10]){0x1C, 0xC0, 'T', 0x3f, 0x1C, 0xC0, '9', 0x1C, 0xC0, 'L'}}    // +20mm
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
    settings->tearOffDistance = FindOption("TearOffDistance", ppd);
    settings->notchAlignment = FindOption("NotchAlignment", ppd);
    settings->recoveryMode = FindOption("RecoveryMode", ppd);
    
#ifdef DEBUG_SYSLOG
	syslog(LOG_NOTICE,"settings->printSpeed = %d\n",settings->printSpeed);
	syslog(LOG_NOTICE,"settings->darkness = %d\n",settings->darkness);
	syslog(LOG_NOTICE,"settings->bitsPerPixel = %d\n",settings->bitsPerPixel);
	syslog(LOG_NOTICE,"settings->cutterMode = %d\n",settings->cutterMode);
	syslog(LOG_NOTICE,"settings->tearOffDistance = %d\n",settings->tearOffDistance);
	syslog(LOG_NOTICE,"settings->recoveryMode = %d\n",settings->recoveryMode);
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
    SendCommand(IgnoreText[0]);
    
    // SET PRINT SPEED
    if(settings.printSpeed > -1)
        SendCommand(PrintSpeedCommand[settings.printSpeed]);

    // SET PRINT DENSITY
    if(settings.darkness > -1 && settings.darkness < 10) // 10 means Don't set
        SendCommand(Darkness[settings.darkness]);

    // SEND TEAROFF COMMAND
    if( settings.cutterMode == 3 )
        SendCommand(TearOffDistanceCommand[settings.tearOffDistance]);

    // SEND RECOVERY MODE COMMAND
        SendCommand(RecoveryModeCommand[settings.recoveryMode]);
    
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

        // ALIGN TO NOTCH AT THE BEGINNING
        if( settings.notchAlignment == 1 )
            SendCommand(NotchAlignmentCommand[0]);

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

        // END PAGE COMMAND
        SendCommand(EndPage);
        
        // ALIGN TO NOTCH
        if (settings.notchAlignment == 1)
            SendCommand(NotchAlignmentCommand[1]);	

        // SEND CUT COMMAND
	    if (settings.cutterMode == 1) {
		    SendCommand(CutCommand[settings.cutterMode]);
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

	
    // SEND CUT COMMAND
	if (settings.cutterMode == 2) {
		SendCommand(CutCommand[settings.cutterMode]);
	}

    #ifdef DEBUG_SYSLOG
        syslog(LOG_NOTICE,"main stop\n");
        closelog();
    #endif

    return (currentPageIndex == 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}




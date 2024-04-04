/*
 Copyright (C) 2005 Custom Engineering S.p.A
*/

// include headers

#include <cups/cups.h>
#include <cups/ppd.h>
#include <cups/raster.h>
#include <stdlib.h>
#include <fcntl.h>

#define MIN(x,y) (((x) < (y)) ? (x) : (y))
// PrinterSetUp structure

struct PrinterSetUp
{
    float pageWidth;
    float pageHeight;
    int printQuality;
    int pageCutType;
    int cashDrawer;
    int printDensity;
    int logoBegin;
    int logoEnd;
    int beep;
    int blackMark;
};


struct command
{
    int    length;
    char* command;
};

char localBuf[2000];
char *lpSrc,*lpTgt;
char aux;
int bytesRem;
int cnt;
int old;
int bytecnt;


// define KUBE print quality commands
static const struct command PrintQuality [3] =
{{5,(char[5]){0x1B,'*','o','0','Q'}},           // 0 high quality
 {5,(char[5]){0x1B,'*','o','1','Q'}},           // 1 Normal
 {5,(char[5]){0x1B,'*','o','2','Q'}}};          // 2 high speed

// define KUBE print density commands
static const struct command PrintDensity [9] =
{{5,(char[5]){0x1B,'&','k','0','W'}},           // 0 -50 %
 {5,(char[5]){0x1B,'&','k','1','W'}},           // 1 -37 %
 {5,(char[5]){0x1B,'&','k','2','W'}},           // 2 -25 %
 {5,(char[5]){0x1B,'&','k','3','W'}},           // 3 -12 %
 {5,(char[5]){0x1B,'&','k','4','W'}},           // 4 0 %
 {5,(char[5]){0x1B,'&','k','5','W'}},           // 5 +12 %
 {5,(char[5]){0x1B,'&','k','6','W'}},           // 6 +25 %
 {5,(char[5]){0x1B,'&','k','7','W'}},           // 7 +37 %
 {5,(char[5]){0x1B,'&','k','8','W'}}};          // 8 +50 %

// define cash drawer commands
static const struct command CashDrawer =
{5,(char[5]){0x1B,'p','0',0x80,0x80}};               // open cash drawer
//{5,(char[5]){0x1B,'p','0',0xFF,0xFF}};               // open cash drawer

// define page cut type commands

static const struct command CutCommand [3] =
{{3,(char[3]){0x1C,0xC0,'3'}}, // 0 no cut
 {3,(char[3]){0x1C,0xC0,'2'}}, // 1 partial cut
 {3,(char[3]){0x1C,0xC0,'1'}}};// 2 Total cut

// define document resolution
static const struct command DocumentResolution =
{7,(char[7]){0x1B,0x2A,0x74,0x32,0x30,0x34,0x52}};

// define begin page
static const struct command BeginPage =
{8,(char[8]){0x18,0x1D,0xFF,'j',0x01,0xB8,0x1B,'2'}};

// define end page command
static const struct command endPage =
{4,(char[4]){0x1b,'*','r','B'}};

//one feed more
static const struct command endPage_feed =
{4,(char[4]){0x0A,0x0A,0x0A,0x0A}};

static const struct command endPage_cut =
{3,(char[3]){0X1C,0xC0,'0'}};

static const struct command endDoc =
{9,(char[9]){0x1D,0xFF,'j',0x00,0xB7,0x1D,'P',0x00,0x00}};


static const struct command LogoCommand [8]=
{{7,(char[7]){0x1B,0xFA,0x01,0x00,0x00,0x00,0xE7}},
 {7,(char[7]){0x1B,0xFA,0x01,0x00,0xE8,0x00,0xE7}},
 {7,(char[7]){0x1B,0xFA,0x01,0x01,0xBF,0x00,0xE7}},
 {7,(char[7]){0x1B,0xFA,0x01,0x02,0x96,0x00,0xE7}},
 {7,(char[7]){0x1B,0xFA,0x02,0x00,0x00,0x00,0xE7}},
 {7,(char[7]){0x1B,0xFA,0x02,0x00,0xE8,0x00,0xE7}},
 {7,(char[7]){0x1B,0xFA,0x02,0x01,0xBF,0x00,0xE7}},
 {7,(char[7]){0x1B,0xFA,0x02,0x02,0x96,0x00,0xE7}}};


// define Compression command
static const struct command Compression [2] =
{{5,(char[5]){0x1B,0x2A,0x62,0x31,0x4D}},                 // begin compression
 {5,(char[5]){0x1B,0x2A,0x62,0x30,0x4D}}};                // end compression

 // define beep command
static const struct command BeepCommand [2] =
{{3,(char[3]){0x1C,0xC0,'6'}},                 // beep off
 {3,(char[3]){0x1C,0xC0,'4'}}};                // beep on


 // define blackMark command begin page
static const struct command BlackMark_BP [3] =
{{0,(char[2]){0x00,0x00}},          /* empty command */           
 {2,(char[2]){0x1D,0xF6}},                                        
 {0,(char[2]){0x00,0x00}}};         /* empty command */           

 // define blackMark command end page
static const struct command BlackMark_EP [3] =
{{0,(char[2]){0x00,0x00}},          /* empty command */           
 {0,(char[2]){0x00,0x00}},          /* empty command */           
 {2,(char[2]){0x1D,0xF6}}};                    



/*------------------------------------------------------------------------------------------------

	SendCommand:
	this function send a specified command, based on command structure, to the printer

------------------------------------------------------------------------------------------------*/

void SendCommand(struct command output)
{
    int i;
    for ( i = 0; i < output.length; i++)
      putchar(output.command[i]);
}

/*------------------------------------------------------------------------------------------------

	FindOption:
	this function scan ppd file and search a specified choice and return the result
	of the selection

------------------------------------------------------------------------------------------------*/

int FindOption(const char * choiceName, ppd_file_t * ppd)
{
    ppd_choice_t * choice;
    ppd_option_t * option;

    choice = ppdFindMarkedChoice(ppd, choiceName);
    if (choice == NULL)
    {
        if ((option = ppdFindOption(ppd, choiceName))          == NULL) return -1;
        if ((choice = ppdFindChoice(option,option->defchoice)) == NULL) return -1;
    }

    return atoi(choice->choice);
}

// per adesso la lascio ai box, mi sembra inutile

void getPageWidthPageHeight(ppd_file_t * ppd, struct PrinterSetUp * settings)
{
    ppd_choice_t * choice;
    ppd_option_t * option;

    char width[20];
    int widthIdx;

    char height[20];
    int heightIdx;

    char * pageSize;
    int idx;

    int state;

    choice = ppdFindMarkedChoice(ppd, "PageSize");
    if (choice == NULL)
    {
        option = ppdFindOption(ppd, "PageSize");
        choice = ppdFindChoice(option,option->defchoice);
    }

    widthIdx = 0;
    memset(width, 0x00, sizeof(width));

    heightIdx = 0;
    memset(height, 0x00, sizeof(height));

    pageSize = choice->choice;
    idx = 0;

    state = 0; // 0 = init, 1 = width, 2 = height, 3 = complete, 4 = fail

    while (pageSize[idx] != 0x00)
    {
        if (state == 0)
        {
            if (pageSize[idx] == 'X')
            {
                state = 1;

                idx++;
                continue;
            }
        }
        else if (state == 1)
        {
            if ((pageSize[idx] >= '0') && (pageSize[idx] <= '9'))
            {
                width[widthIdx++] = pageSize[idx];

                idx++;
                continue;
            }
            else if (pageSize[idx] == 'D')
            {
                width[widthIdx++] = '.';

                idx++;
                continue;
            }
            else if (pageSize[idx] == 'M')
            {
                idx++;
                continue;
            }
            else if (pageSize[idx] == 'Y')
            {
                state = 2;

                idx++;
                continue;
            }
        }
        else if (state == 2)
        {
            if ((pageSize[idx] >= '0') && (pageSize[idx] <= '9'))
            {
                height[heightIdx++] = pageSize[idx];

                idx++;
                continue;
            }
            else if (pageSize[idx] == 'D')
            {
                height[heightIdx++] = '.';

                idx++;
                continue;
            }
            else if (pageSize[idx] == 'M')
            {
                state = 3;
                break;
            }
        }

        state = 4;
        break;
    }


    if (state == 3)
    {
        settings->pageWidth = atof(width);
        settings->pageHeight = atof(height);
    }
    else
    {
        settings->pageWidth = 0;
        settings->pageHeight = 0;
    }
}


/*------------------------------------------------------------------------------------------------

	initializeSettings:
	this function read the ppd choices and set the settings structure

------------------------------------------------------------------------------------------------*/

void initializeSettings(char * commandLineOptionSettings, struct PrinterSetUp * settings)
{
    ppd_file_t *    ppd         = NULL;
    cups_option_t * options     = NULL;
    int             numOptions  = 0;

    ppd = ppdOpenFile(getenv("PPD"));

    ppdMarkDefaults(ppd);

    numOptions = cupsParseOptions(commandLineOptionSettings, 0, &options);
    if ((numOptions != 0) && (options != NULL))
    {
        cupsMarkOptions(ppd, numOptions, options);

        cupsFreeOptions(numOptions, options);
    }

    memset(settings, 0x00, sizeof(struct PrinterSetUp));

    settings->printQuality = FindOption("PrinterQuality", ppd);
    settings->pageCutType = FindOption("PageCutType", ppd);
    settings->cashDrawer = FindOption("CashDrawer", ppd);
    settings->printDensity = FindOption("PrintDensity", ppd);
    settings->logoBegin = FindOption("LogoBeginDoc", ppd);
    settings->logoEnd = FindOption("LogoEndDoc", ppd);
    settings->beep = FindOption("Beep", ppd);
    settings->blackMark = FindOption("BlackMark", ppd);
    //getPageWidthPageHeight(ppd, settings);

    ppdClose(ppd);
}

/*------------------------------------------------------------------------------------------------

	SendWithCompression:
	this function compress a raster document linee and send it to the printer

------------------------------------------------------------------------------------------------*/

void SendWithCompression(unsigned char *bmp,int size)
{
   int i;
   printf("\x1B\x2A\x62%d\x57",size);

   bytesRem=size;
   lpSrc=bmp;
   lpTgt=localBuf;
   old=0xFF00;
   bytecnt=0;

   while (bytesRem>0)
   {
     if ((int)((char*)lpSrc-(char*)bmp) <size)
       aux=*lpSrc++;
     else
     {
       bytesRem=bytecnt;
       aux=(char)(old-1);
     }
     if (old == (int)aux)
     {
       bytecnt++;
       if (bytecnt >= 0x3F)
       {
         *lpTgt++=(char)(0XC0 | (bytecnt & 0x003F));
         *lpTgt++=(char)old;
          bytesRem-=bytecnt;
          bytecnt=0;
          old=0xFF00;
       }
     }
     else
     {
       if (bytecnt >1)
       {
         *lpTgt++=(char)(0XC0 | (bytecnt & 0x003F));
         *lpTgt++=(char)old;
         bytesRem-=bytecnt;
         bytecnt=1;
         old=(int)aux;
       }
       else if ((old & 0x00C0) == 0x00C0)
       {
         *lpTgt++=0xC1;
         *lpTgt++=(char)old;
         bytesRem--;
         bytecnt=1;
         old=(int)aux;
       }
       else if (bytecnt==1)
       {
         *lpTgt++=(char)old;
         bytesRem--;
         bytecnt=1;
         old=(int)aux;
       }
       else
       {
         bytecnt=1;
         old=(int)aux;
       }
     }
    }
    // send compressed linee
    cnt = (int)((char*)lpTgt - (char*)localBuf);
   // write(fd,localBuf,cnt);
    for (i=0;i<cnt;i++)
    {
    	putchar(localBuf[i]);
    }
}


/*----------------------------------------------------------------------------------------------------

	MAIN

-----------------------------------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int fd = 0; // CUPS raster file descriptor
    int page = 0; // Current page
    int y = 0; // Vertical position in the raster page
    int i = 0; // counter
    int scanLineBlank           = 0;        // Set to 1 is the entire scan line is blank (no black pixels)
    int lastBlackPixel          = 0;        // Position of the last byte containing one or more black pixels in the scan line
    int numBlankScanLines       = 0;        // Number of scanlines that were entirely black
    int bytesPerLine;

    cups_raster_t * ras = NULL; // CUPS raster file
    cups_page_header_t  header; // CUPS page header

    unsigned char *rasterData = NULL; // Pointer to raster data buffer
    unsigned char *originalRasterDataPtr = NULL; // Copy of rasterData
    struct PrinterSetUp    settings; // Configuration settings

   // putchar('C');
    // check the filter arguments

    // if argc is different than 6 and 7 return failure
    if (argc < 6 || argc > 7)
    {
        return EXIT_FAILURE;
    }

    if (argc == 7)
    {
        // open the raster file passed from CUPS

        if ((fd = open(argv[6], O_RDONLY)) == -1)
        {
            // error open raster file
            perror("ERROR: Unable to open raster file - ");

            // return failure
            return EXIT_FAILURE;
        }
    }


    // read the printer settings by PDD file
    initializeSettings(argv[5], &settings);


    // set the printer quality setting
    SendCommand(PrintQuality[settings.printQuality]);

    SendCommand(PrintDensity[settings.printDensity]);

    // cut command
    SendCommand(CutCommand[settings.pageCutType]);

   if (settings.cashDrawer==1)
	SendCommand(CashDrawer);

    SendCommand(BeepCommand[settings.beep]);

    SendCommand(BeginPage);
    // read the CUPS raster file
    ras = cupsRasterOpen(fd, CUPS_RASTER_READ);

    page = 0;

    while (cupsRasterReadHeader(ras, &header))
    {

	if ((header.cupsHeight == 0) || (header.cupsBytesPerLine == 0))
        {
            break;
        }

        if (rasterData == NULL)
        {
            rasterData = malloc(header.cupsBytesPerLine);
            if (rasterData == NULL)
            {
                 if (originalRasterDataPtr   != NULL) free(originalRasterDataPtr);
                   cupsRasterClose(ras);

                 if (fd != 0)
                   close(fd);

                 return EXIT_FAILURE;

            }
            originalRasterDataPtr = rasterData;
        }

        // send begin page document
        putchar(0x18);

        SendCommand(BlackMark_BP[settings.blackMark]);

	if (header.cupsHeight<3000)
	{
		  printf("\x1B\x26\x6C%dP",header.cupsHeight);
	}
	else
	{
		printf("\x1D\x75\x1B\x26\x6C");
		//printf("4676R");
		printf("%dR", header.cupsHeight);
        }
	// begin compression mode
	SendCommand(DocumentResolution);

//	printf("\033*p0xY");
	printf("\033*p0Y");

        page++;

        SendCommand(Compression[0]);
	numBlankScanLines = 0;

	if (settings.logoBegin!=0)
	  SendCommand(LogoCommand[settings.logoBegin-1]);

        // scan the CUPS raster file for heach document page


        for (y = 0; y < header.cupsHeight; y ++)
        {
            if (cupsRasterReadPixels(ras, rasterData, header.cupsBytesPerLine) < 1)
            {
                break;
            }

	    bytesPerLine = MIN(80, header.cupsBytesPerLine);
            // scan the blank line
            for (i = bytesPerLine/*header.cupsBytesPerLine*/ - 1; i >= 0; i--)
            {
                if (((char) *(rasterData + i)) != ((char) 0x00))
                {
                    break;
                }
            }

            if (i == -1)
            {
                scanLineBlank = 1;
                numBlankScanLines++;
            }
            else
            {
                lastBlackPixel = i + 1;
                scanLineBlank = 0;
            }


	    if (scanLineBlank == 0)
            {
	        if (numBlankScanLines > 0)
                {
//                  printf("\033*p+%dY",numBlankScanLines);
                  printf("\033*p%dY",y);
                  numBlankScanLines = 0;
                }

                SendWithCompression(rasterData,bytesPerLine/*header.cupsBytesPerLine*/);
            }

            rasterData = originalRasterDataPtr;
        }

        // end compression mode
        SendCommand(Compression[1]);

        if (settings.logoEnd!=0)
            SendCommand(LogoCommand[settings.logoEnd-1]);

        // end page
        SendCommand(endPage);


       	// feed paper at end page
       	SendCommand(endPage_feed);

        SendCommand(BlackMark_EP[settings.blackMark]);

        // cut at end page
        SendCommand(endPage_cut);

    	if (settings.cashDrawer==2)
		SendCommand(CashDrawer);

	// the same for cash drawer
    }

    SendCommand(endDoc);

    if (settings.cashDrawer==3)
		SendCommand(CashDrawer);


    if (originalRasterDataPtr   != NULL) free(originalRasterDataPtr);
      cupsRasterClose(ras);

    if (fd != 0)
        close(fd);

    return (page == 0)?EXIT_FAILURE:EXIT_SUCCESS;
}

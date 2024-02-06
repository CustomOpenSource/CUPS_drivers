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

#include <stdlib.h>
#include <string.h>
#include "dithering.h"

/*------------------------------------------------------------------------------------------------

	Dithering

------------------------------------------------------------------------------------------------*/
int DitherRaw1ImageWithFloydSteinberg(
    unsigned char* pBufferRaw8, 
    int dwBufferRaw24Size, 
    int w, 
    int h, 
    unsigned char** pBufferRaw1, 
    unsigned long* pdwBufferRaw1Size
){

	int dwNumBlackPixel = 0;

	int nRowBytesSRC = w;
	int nRowBytesDST = w  / 8;
	if (w % 8)
		nRowBytesDST++;

	//Create out buffer
	int dwBufferDSTSize = nRowBytesDST * h;
	unsigned char* pBufferDST = malloc(dwBufferDSTSize);
	memset(pBufferDST, 0, dwBufferDSTSize);

	int i;
	
	int *AccErr = malloc(w * 2 * 4);
	for (i = 0; i<(w * 2); i++)
		AccErr[i] = 0;

	// force threshold to 128
	int th = 128;
	//LibLog.WriteI("Threshold: " + thresh);

	unsigned char* pSRC;
	unsigned char* pDST;

	int y;
	int xsrc, xdst;
	unsigned char bMask = 0x80;
	unsigned char b;
	unsigned char bGrey;
	
	double dValue;
	double dError;

	for (y = 0; y < h; y++){

		//Identify the line position for SRC and DST
		pSRC = &pBufferRaw8[nRowBytesSRC * y];
		pDST = &pBufferDST[nRowBytesDST * y];

		xsrc = 0;
		xdst = 0;
		b = 0;
		bMask = 0x80;

		if (y>=1)
			xsrc = 0;

		//Swap the 2° line with the first
		for (i = 0; i < w; i++){
			AccErr[i] = AccErr[i + w];
			AccErr[i + w] = 0;
		}

		for (xsrc = 0; xsrc < w; xsrc++){
			//Get the Grey
			bGrey = 0xFF - pSRC[xsrc];
			dValue = bGrey + AccErr[xsrc];		

			//Check with threshold (if is BLACK)
			if (dValue <= th){
				//Set the Bit
				b |= bMask;

				dwNumBlackPixel++;

				//calc the error
				dError = dValue;
			}
			//WHITE
			else {
				//calc the error
				dError = dValue - 0xFF;
			}

			bMask >>= 1;

			//When the Byte is full, write it
			if (bMask == 0) {
				//Save it
				if (b != 0)
					pDST[xdst] = b;
				xdst++;

				bMask = 0x80;
				b = 0;
			}

			
			// Valore a DX
			if (xsrc < w - 1)
				AccErr[xsrc + 1] += (int)(dError * 0.4375);

			//Valore in basso
			if (y < h - 1)
				AccErr[xsrc + w] += (int)(dError * 0.3125);

			//Valore in basso a DX
			if ((xsrc < w - 1) && (y < h - 1))
				AccErr[xsrc + 1 + w] += (int)(dError * 0.0625);
			//Valore a SX in basso
			if (xsrc > 0)
				AccErr[xsrc - 1 + w] += (int)(dError * 0.1875);
		}

		//If i have 1 byte pending...
		if (b != 0) {
			//Save it
			pDST[xdst++] = b;
		}
	}

	//Save the Values
	*pBufferRaw1 = pBufferDST;
	*pdwBufferRaw1Size = dwBufferDSTSize;

	return dwNumBlackPixel;
}
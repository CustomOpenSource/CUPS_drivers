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


#include "compression.h"

#define RLE_FIXED_VALUE 0xC0
#define RLE_PACKET_SIZE_LIMIT 0x3F // 0xFF-0xC0

#define FE_RLE_PACKET_SIZE_LIMIT 0xFF

/*******************************************************************
@brief compress data received with RLE or FE_RLE algorithm

It uses the passed function as argument to compress data
It compress the data received in the input array store the result in the output array.
It reads the input array with a limit provided with "length" parameter
It doesn't check the size of the the outpur array

@param in input array with uncompressed data
@param length the length of data contained in the "in" parameter
@param out output array where the result of the compression will be stored
@param compress_func the compress function to use (RLE_write_compress or FE_RLE_write_compress)

@return the numbers of bytes put in the output array by compression algorithm
*******************************************************************/
int scan_and_compress(unsigned char *in, int length, unsigned char *out, unsigned char * (compress_func)(unsigned char *, unsigned char, int));

/*******************************************************************
@brief write out received data in the specificed array with RLE algorithm specific syntax

It writes on the specified array the data to represent "count" times of "value" with the RLE notation
It returns a pointer to the address memory immediately next to the last written by the function

@param ptr_out array where write the result
@param value the value of uncompressed data
@param count the counter of uncompressed data

@return the pointer of the memory address immediately after the last one written by the function
*******************************************************************/
unsigned char * RLE_write_compress(unsigned char *ptr_out, unsigned char value, int count);

/*******************************************************************
@brief write out received data in the specificed array with FE RLE algorithm specific syntax

It writes on the specified array the data to represent "count" times of "value" with the FE RLE notation
It returns a pointer to the address memory immediately next to the last written by the function

@param ptr_out array where write the result
@param value the value of uncompressed data
@param count the counter of uncompressed data

@return the pointer of the memory address immediately after the last one written by the function
*******************************************************************/
unsigned char * FE_RLE_write_compress(unsigned char *ptr_out, unsigned char value, int count);

int RLE_compress(unsigned char *in, int length, unsigned char *out){

	return scan_and_compress(in, length, out, RLE_write_compress);
    
}

unsigned char * RLE_write_compress(unsigned char *ptr_out, unsigned char value, int count){

	if(count > RLE_PACKET_SIZE_LIMIT){
		// es. FF -> 01 FF
	
		int round_count = count / RLE_PACKET_SIZE_LIMIT;
		int round_rest = count % RLE_PACKET_SIZE_LIMIT;
		
		while(round_count > 0){
			*ptr_out = 0xFF;
			*(ptr_out + 1) = value;
			ptr_out += 2;
			round_count--;
		}

		if(round_rest == 1){
			*ptr_out = value;
			ptr_out ++;
		}
		else if(round_rest > 1){
			*ptr_out = RLE_FIXED_VALUE + round_rest;
			*(ptr_out + 1) = value;
			ptr_out += 2;
		}
		
	}
	else{
	
		if(value >= RLE_FIXED_VALUE || count > 1){
			// es. FF -> C1 FF
			// es. A0 A0 -> (C0+2) A0 -> C2 A0
			*ptr_out = RLE_FIXED_VALUE + count;
			*(ptr_out + 1) = value;
			ptr_out += 2;
		}
		else{
			// es. A0 -> A0 
			*ptr_out = value;
			ptr_out ++;
		}
	
	}
			
	return ptr_out;
}

int FE_RLE_compress(unsigned char *in, int length, unsigned char *out){

	return scan_and_compress(in, length, out, FE_RLE_write_compress);
    
}

unsigned char * FE_RLE_write_compress(unsigned char *ptr_out, unsigned char value, int count){

	if(count > 1){
	
		if(count > FE_RLE_PACKET_SIZE_LIMIT){
		
			int round_count = count / FE_RLE_PACKET_SIZE_LIMIT;
			int round_rest = count % FE_RLE_PACKET_SIZE_LIMIT;
			
			while(round_count > 0){
				*ptr_out = value;
				*(ptr_out + 1) = value;
				*(ptr_out + 2) = FE_RLE_PACKET_SIZE_LIMIT;
				ptr_out += 3;
				round_count--;
			}
			
			if(round_rest == 1){
				*ptr_out = value;
				ptr_out ++;
			}
			else if(round_rest > 1){
				*ptr_out = value;
				*(ptr_out + 1) = value;
				*(ptr_out + 2) = round_rest;
				ptr_out += 3;
			}
		}
		else{			
			*ptr_out = value;
			*(ptr_out + 1) = value;
			*(ptr_out + 2) = count;
			ptr_out += 3;
		}
	}
	else{
		*ptr_out = value;
		ptr_out += 1;
	}
	return ptr_out;
}

int scan_and_compress(unsigned char * in, int length, unsigned char*out, unsigned char * (compress_func)(unsigned char *, unsigned char, int)){

	if(length < 1)
        return 0;
        
    int counter = 0;

	unsigned char *ptr_out = out;

    unsigned char *ptr_save = in;
    unsigned char *ptr_cursor = in;
    
    do{
    
        if(*ptr_save == *ptr_cursor)
        	counter++;
        else{
    		ptr_out = compress_func(ptr_out, *ptr_save, counter);
    		ptr_save = ptr_cursor;
        	counter = 1;
        }
        
        ptr_cursor++;
        length--;
        
        if(length==0)
        	ptr_out = compress_func(ptr_out, *ptr_save, counter);
        
    }while(length > 0);
    
    return ptr_out - out;
    
}

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


#ifndef COMPRESSION_H
#define COMPRESSION_H

/*******************************************************************
@brief compress data received with RLE algorithm

It compress the data received in the input array store the result in the output array.
It reads the input array with a limit provided with "length" parameter
It doesn't check the size of the the outpur array

@param in input array with uncompressed data
@param length the length of data contained in the "in" parameter
@param out output array where the result of the compression will be stored

@return the numbers of bytes putted in the output array by compression algorithm
*******************************************************************/
int RLE_compress(unsigned char *in, int length, unsigned char *out);

/*******************************************************************
@brief compress data received with FE_RLE algorithm

It compress the data received in the input array store the result in the output array.
It reads the input array with a limit provided with "length" parameter
It doesn't check the size of the the outpur array

@param in input array with uncompressed data
@param length the length of data contained in the "in" parameter
@param out output array where the result of the compression will be stored

@return the numbers of bytes putted in the output array by compression algorithm
*******************************************************************/
int FE_RLE_compress(unsigned char *in, int length, unsigned char *out);

#endif

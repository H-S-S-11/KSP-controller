#include "krpc_connection.h"


#include <windows.h>

#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <conio.h>
#include <algorithm> 

#include "ftd2xx.h"
#include "LibFT4222.h";

Controller::Controller() {
	

}

void Controller::update() {
	
	

}


void Controller::output_buffer_builder() {
	
	uint16 output_swapped = reverse_byte_order(output);
	uint16 smallest_byte = (output_swapped & 0x00FF);
	uint16 largest_byte = (output_swapped & 0xFF00);
	largest_byte = largest_byte >> 8;
	
	sendBuffer.clear();

	sendBuffer.push_back((uint8)smallest_byte);
	sendBuffer.push_back((uint8)largest_byte);
}

void Controller::input_builder() {
	uint16 mostSignificantByte = (uint16)recieveBuffer[0];
	uint16 leastSignificantByte = (uint16)recieveBuffer[1];
	mostSignificantByte = mostSignificantByte << 8;

	input = (mostSignificantByte | leastSignificantByte);
}

uint16 Controller::reverse_byte_order(uint16 x) //Puts the first 8 bits in the second eight bits and vice versa
{
	return (0xFF00 & (x << 8)) + (0x00FF & (x >> 8));
}


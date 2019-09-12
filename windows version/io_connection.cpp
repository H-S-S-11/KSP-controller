/*
 * @file spi_slave_test_master_side.cpp
 *
 * @author FTDI, changes: Harry Snell, Sen
 * @date 24/10/2018
 *
 * Copyright 2011 Future Technology Devices International Limited
 * Company Confidential
 *
 * Revision History:
 * 1.0 - initial version
 * 1.1 - spi slave with protocol and ack function
  */

//------------------------------------------------------------------------------
#include <windows.h>


#include "krpc_connection.h"

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <conio.h>
#include <algorithm>



//------------------------------------------------------------------------------
// include FTDI libraries
//
#include "ftd2xx.h"
#include "LibFT4222.h"


#define USER_WRITE_REQ      0x4a
#define USER_READ_REQ       0x4b
#define BUS_SIZE 0x2
#define SPI_SLAVE_HEADER_SIZE    7


//------------------------------------------------------------------------------


std::vector< FT_DEVICE_LIST_INFO_NODE > g_FTAllDevList;
std::vector< FT_DEVICE_LIST_INFO_NODE > g_FT4222DevList;

inline std::string DeviceFlagToString(DWORD flags)
{
	std::string msg;
	msg += (flags & 0x1) ? "DEVICE_OPEN" : "DEVICE_CLOSED";
	msg += ", ";
	msg += (flags & 0x2) ? "High-speed USB" : "Full-speed USB";
	return msg;
}

void ListFtUsbDevices()
{
	FT_STATUS ftStatus = 0;

	DWORD numOfDevices = 0;
	ftStatus = FT_CreateDeviceInfoList(&numOfDevices);

	for (DWORD iDev = 0; iDev < numOfDevices; ++iDev)
	{
		FT_DEVICE_LIST_INFO_NODE devInfo;
		memset(&devInfo, 0, sizeof(devInfo));

		ftStatus = FT_GetDeviceInfoDetail(iDev, &devInfo.Flags, &devInfo.Type, &devInfo.ID, &devInfo.LocId,
			devInfo.SerialNumber,
			devInfo.Description,
			&devInfo.ftHandle);

		if (FT_OK == ftStatus)
		{
			printf("Dev %d:\n", iDev);
			printf("  Flags= 0x%x, (%s)\n", devInfo.Flags, DeviceFlagToString(devInfo.Flags).c_str());
			printf("  Type= 0x%x\n", devInfo.Type);
			printf("  ID= 0x%x\n", devInfo.ID);
			printf("  LocId= 0x%x\n", devInfo.LocId);
			printf("  SerialNumber= %s\n", devInfo.SerialNumber);
			printf("  Description= %s\n", devInfo.Description);
			printf("  ftHandle= 0x%x\n", devInfo.ftHandle);

			const std::string desc = devInfo.Description;
			g_FTAllDevList.push_back(devInfo);

			if (desc == "FT4222" || desc == "FT4222 A")
			{
				g_FT4222DevList.push_back(devInfo);
			}
		}
	}
}


//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------

int main(int argc, char const *argv[])
{
	//Setup
	
	FT_HANDLE SPI_handle = NULL;
	FT_STATUS ftStatus;
	FT_STATUS ft4222_status;
		
	
	ListFtUsbDevices();
	if (g_FT4222DevList.empty()) {
		printf("No FT4222 device is found!\n");
		return 0;
	}

	int SPI_num = 0;
	for (int idx = 0; idx < 10; idx++)
	{
		//printf("select dev num(0~%d) as spi master\n", g_FTAllDevList.size() - 1);

		//SPI_num = _getch();
		
		SPI_num = 0;	//when using single board

		//SPI_num = SPI_num - '0';
		if (SPI_num >= 0 && SPI_num < g_FTAllDevList.size())
		{
			break;
		}
		else
		{
			printf("input error , please input again\n");
		}

	}

	
	ftStatus = FT_Open(SPI_num, &SPI_handle);
	if (FT_OK != ftStatus)
	{
		printf("Open a FT4222 device failed!\n");
		return 0;
	}
	//Set default Read and Write timeout 1 sec
	ftStatus = FT_SetTimeouts(SPI_handle, 1000, 1000);
	if (FT_OK != ftStatus)
	{
		printf("FT_SetTimeouts failed!\n");
		return 0;
	}

	ftStatus = FT4222_SetClock(SPI_handle, SYS_CLK_48);
	if (FT_OK != ftStatus)
	{
		printf("Set clock failed!\n");
		return 0;
	}


	// set latency to 1
	ftStatus = FT_SetLatencyTimer(SPI_handle, 1);
	if (FT_OK != ftStatus)
	{
		printf("FT_SetLatencyTimerfailed!\n");
		return 0;
	}

	ftStatus = FT_SetUSBParameters(SPI_handle, 4 * 1024, 0);
	if (FT_OK != ftStatus)
	{
		printf("FT_SetUSBParameters failed!\n");
		return 0;
	}



	//Initialise as master
	ft4222_status = FT4222_SPIMaster_Init(SPI_handle, SPI_IO_SINGLE, CLK_DIV_2, CLK_IDLE_HIGH, CLK_LEADING, 0x01);
	
	if (FT4222_OK != ft4222_status)
	{
		printf("Init FT4222 as SPI master device failed!\n");
		return 0;
	}

	ft4222_status = FT4222_SPI_SetDrivingStrength(SPI_handle, DS_8MA, DS_8MA, DS_8MA);
	if (FT4222_OK != ft4222_status)
	{
		printf("FT4222_SPI_SetDrivingStrength failed!\n");
		return 0;
	}


	std::vector<unsigned char> sendBuf;
	std::vector<unsigned char> recieve_buffer;

	recieve_buffer.resize(2);
	
	Controller ksp_control;
	ksp_control.output = 0x0000;
	bool run = true;
	ksp_control.recieveBuffer.resize(2);
	
	while (run){
		
		ksp_control.update();



		uint16 sizeTransferred;
		ksp_control.output_buffer_builder();
		sendBuf = ksp_control.sendBuffer;
		
		ft4222_status = FT4222_SPIMaster_SingleReadWrite(SPI_handle, &recieve_buffer[0], &sendBuf[0], sendBuf.size(), &sizeTransferred, true);	
		
		ksp_control.recieveBuffer.resize(2);
		ksp_control.recieveBuffer = recieve_buffer;
		ksp_control.input_builder();

		//printf("Sending 0x%x \n", ksp_control.output);
		//printf("recieved %b \n", ksp_control.input);

		


		//Checks if write failed
		if (FT4222_OK != ft4222_status)
		{
			printf("spi master write single failed %x!\n", ft4222_status);
			Sleep(10000);
			return 0;
		}
				
	
	}

    FT4222_UnInitialize(SPI_handle);
    FT_Close(SPI_handle);
	
	printf("press enter to close\n");
	int ch = std::cin.get();	//pause until enter pressed
    return 0;
}

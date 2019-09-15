#include <iostream>
#include <iomanip>
#include <tuple>
#include <krpc.hpp>
#include <krpc/services/krpc.hpp>
#include <krpc/services/space_center.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h> 

#include "ftd2xx.h"
#include "libft4222.h"

//Controller class which handles kRPC
class Controller
{
	public:
	
	//interface with ft4222
	uint16 input;
	uint16 output;
	std::vector<unsigned char> send_buffer;
	std::vector<unsigned char> recieve_buffer;
	
	//ksp variables
	krpc::Client conn;
	
	//krpc::services::SpaceCenter sc;

	krpc::services::SpaceCenter::Vessel vessel;
	krpc::services::SpaceCenter::Control control;
	krpc::services::SpaceCenter::Resources total_resources;
	
	//output related variables
	float max_liquid_fuel;
	float fuel_fraction;
	uint16 fuel_int;
	
	float total_liquid_fuel;


	//input related variables
	bool SAS_state;
	bool game_SAS_state;

	bool RCS_state;
	bool game_RCS_state;

	bool gear_state;
	bool game_gear_state;

	bool brakes_state;
	bool game_brakes_state;

	bool stage_trigger = true;
	bool previous_stage_trigger = true;
	bool stage_momentary_switch_reset = false;
	uint16 s_m_s_r_int = 0;

	bool abort_trigger = true;
	bool previous_abort_trigger = true;
	bool abort_momentary_switch_reset = false;
	uint16 a_m_s_r_int = 0;

	Controller()
	{
	
	//Set up kRPC

  	conn = krpc::connect();
  	krpc::services::KRPC krpc(&conn);
	std::cout << "Connected to kRPC server version " << krpc.get_status().version() << std::endl;
	
	krpc::services::SpaceCenter sc(&conn);
	
	vessel = sc.active_vessel();
	control = vessel.control();
	total_resources = vessel.resources();
	

	max_liquid_fuel = total_resources.max("LiquidFuel");

	recieve_buffer.resize(2);		

	}
	


	uint16 reverse_byte_order(uint16 x)
	{
		return (0xFF00 & (x << 8)) + (0x00FF & (x >> 8));
	}
	
	void output_buffer_builder()
	{
	uint16 output_swapped = reverse_byte_order(output);
	uint16 smallest_byte = (output_swapped & 0x00FF);
	uint16 largest_byte = (output_swapped & 0xFF00);
	largest_byte = largest_byte >> 8;
	
	send_buffer.clear();

	send_buffer.push_back((uint8)smallest_byte);
	send_buffer.push_back((uint8)largest_byte);
	}	
	
	void input_builder()
	{
	uint16 mostSignificantByte = (uint16)recieve_buffer[0];
	uint16 leastSignificantByte = (uint16)recieve_buffer[1];
	mostSignificantByte = mostSignificantByte << 8;

	input = (mostSignificantByte | leastSignificantByte);
	}



	void update()
	{
		//Outputs
		output = 0;

		//Fuel display 1
		fuel_fraction = (total_liquid_fuel/max_liquid_fuel);
		fuel_fraction = ceil(7*fuel_fraction);
		fuel_int = static_cast<uint16>(fuel_fraction);
		//std::cout << fuel_fraction << "\n";				
		output= output | (fuel_int << 0); //bitshift shown for consistency

		stage_momentary_switch_reset = false;
		abort_momentary_switch_reset = false;
		
		//Inputs

		//SAS switch
		SAS_state = (input & 0b0000000000000001);
		if (game_SAS_state != SAS_state){
		control.set_sas(SAS_state);
		}

		//RCS switch
		RCS_state = (input & 0b000000000000010);
		if (game_RCS_state != RCS_state){
		control.set_rcs(RCS_state);
		}

		//gear switch
		gear_state = (input & 0b0000000000000100);
		if (game_gear_state != gear_state){
		control.set_gear(gear_state);
		}
		
		//brakes switch
		brakes_state = (input & 0b0000000000001000);
		if (game_brakes_state != brakes_state){
		control.set_brakes(brakes_state);
		}

		//Staging button
		previous_stage_trigger = stage_trigger;		
		stage_trigger = (input & 0b0000000000010000);
		//std::cout << previous_stage_trigger << " " << stage_trigger << "\n";
		if (stage_trigger){
			stage_momentary_switch_reset = true;
			if (not previous_stage_trigger){
				//std::cout << "staging\n";
				control.activate_next_stage();
			}
		}

		//Abort button
		previous_abort_trigger = abort_trigger;		
		abort_trigger = (input & 0b0000000000100000);
		if (abort_trigger){
			abort_momentary_switch_reset = true;
			if (not previous_abort_trigger){
				//std::cout << "abort\n";
				control.set_abort(true);
			}
		}




		//Momentary switch resets
		s_m_s_r_int = static_cast<uint16>(stage_momentary_switch_reset);
		output= output | (s_m_s_r_int << 10);

		//Momentary switch resets
		a_m_s_r_int = static_cast<uint16>(abort_momentary_switch_reset);
		output= output | (a_m_s_r_int << 11);
		
		

	}

};

//main

int main() {
	
	//setup of ft4222
	FT_STATUS            ft_status;
   	FT_HANDLE            ft_handle = (FT_HANDLE)NULL;
    	FT4222_STATUS        ft4222_status;
    	FT4222_Version       ft4222Version;

    	ft_status = FT_Open(0, &ft_handle);
   	if (ft_status != FT_OK)
   	{
        printf("FT_Open failed (error %d)\n", 
    	    (int)ft_status);
    	    return 0;
    	}

	
	//Set default Read and Write timeout 1 sec
	ft_status = FT_SetTimeouts(ft_handle, 1000, 1000);
	if (FT_OK != ft_status)
	{
		printf("FT_SetTimeouts failed!\n");
		return 0;
	}

	ft_status = FT4222_SetClock(ft_handle, SYS_CLK_48);
	if (FT_OK != ft_status)
	{
		printf("Set clock failed!\n");
		return 0;
	}


	// set latency to 1
	ft_status = FT_SetLatencyTimer(ft_handle, 1);
	if (FT_OK != ft_status)
	{
		printf("FT_SetLatencyTimerfailed!\n");
		return 0;
	}

	ft_status = FT_SetUSBParameters(ft_handle, 4 * 1024, 0);
	if (FT_OK != ft_status)
	{
		printf("FT_SetUSBParameters failed!\n");
		return 0;
	}

	//Initialise as master
	ft4222_status = FT4222_SPIMaster_Init(ft_handle, SPI_IO_SINGLE, CLK_DIV_2, CLK_IDLE_HIGH, CLK_LEADING, 0x01);
	
	if (FT4222_OK != ft4222_status)
	{
		printf("Init FT4222 as SPI master device failed!\n");
		return 0;
	}

	ft4222_status = FT4222_SPI_SetDrivingStrength(ft_handle, DS_8MA, DS_8MA, DS_8MA);
	if (FT4222_OK != ft4222_status)
	{
		printf("FT4222_SPI_SetDrivingStrength failed!\n");
		return 0;
	}
	
	


	//Set up variables for main loop	
	uint16 sizeTransferred;
	std::vector<unsigned char> send_buffer;
	std::vector<unsigned char> recieve_buffer;

	recieve_buffer.resize(2);

	Controller ksp_control;
	
	//Start required streams
	auto conn = krpc::connect();
	krpc::services::SpaceCenter sc(&conn);
  	auto vessel = sc.active_vessel();
	
	auto total_liquid_fuel_stream = vessel.resources().amount_stream("LiquidFuel");
	auto SAS_state_stream = vessel.control().sas_stream();
	auto RCS_state_stream = vessel.control().rcs_stream();
	auto gear_state_stream = vessel.control().gear_stream();
	auto brakes_state_stream = vessel.control().brakes_stream();


	//Flush latches and any inputs

	ksp_control.output = 0x3800;

	for (int flushes = 0; flushes<2; flushes++){
	
	ksp_control.output_buffer_builder();
	send_buffer = ksp_control.send_buffer;

	
	ft4222_status = FT4222_SPIMaster_SingleReadWrite(ft_handle, &recieve_buffer[0], &send_buffer[0], send_buffer.size(), 				&sizeTransferred, true);
	}


	//Main loop
	while (true){
	
	//Get data from streams
	ksp_control.total_liquid_fuel = total_liquid_fuel_stream();	
	ksp_control.game_SAS_state = SAS_state_stream();
	ksp_control.game_RCS_state = RCS_state_stream();
	ksp_control.game_gear_state = gear_state_stream();
	ksp_control.game_brakes_state = brakes_state_stream();


	ksp_control.update();

	ksp_control.output_buffer_builder();
	send_buffer = ksp_control.send_buffer;

	
	ft4222_status = FT4222_SPIMaster_SingleReadWrite(ft_handle, &recieve_buffer[0], &send_buffer[0], send_buffer.size(), 				&sizeTransferred, true);

	ksp_control.recieve_buffer.resize(2);
	ksp_control.recieve_buffer = recieve_buffer;
	ksp_control.input_builder();

	//printf("Sending 0x%x \n", ksp_control.output);
	//printf("recieved 0x%x \n", ksp_control.input);
	//sleep(1);
	
	//Checks if write failed
	if (FT4222_OK != ft4222_status)
	{
		printf("spi master write single failed %x!\n", ft4222_status);
		return 0;
	}
	

	}
	return 0;

}




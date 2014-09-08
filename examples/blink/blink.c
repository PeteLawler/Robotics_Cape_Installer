/*
Copyright (c) 2014, James Strawson
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer. 
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies, 
either expressed or implied, of the FreeBSD Project.
*/

// Button and LED tester for the Robotics Cape
// Pressing either button makes an LED blink
// Hold the start button or ctrl-c to exit cleanly
// James Strawson - 2013

#include <robotics_cape.h>

// If the user holds the start button , exit cleanly
int on_pause_press(){
	printf("pressed start\n");
	setGRN(HIGH);
	int i=0;
	do{
		usleep(100000);
		if(get_pause_button_state() == LOW){
			return 0; //user let go before time-out
		}
		i++;
	}while(i<20);
	//user held the button down long enough, exit cleanly
	set_state(EXITING);
	return 0;
}

int on_pause_release(){
	printf("released start\n");
	setGRN(LOW);
	return 0;
}
int on_mode_press(){
	printf("pressed select\n");
	setRED(HIGH);
	return 0;
}
int on_mode_release(){
	setRED(LOW);
	printf("released select\n");
	return 0;
}


int main(){
	initialize_cape();
	
	printf("\nPress buttons to toggle lights\n");
	printf("Press both buttons to exit cleanly\n");
	
	//Assign your own functions to be called when events occur
	set_pause_pressed_func(&on_pause_press);
	set_pause_unpressed_func(&on_pause_release);
	set_mode_pressed_func(&on_mode_press);
	set_mode_unpressed_func(&on_mode_release);
	
	//run forever till the program state changes
	while(get_state() != EXITING){
		setGRN(HIGH);
		if(get_pause_button_state() == LOW){
			setRED(LOW);
		}
		usleep(500000);
		
		if(get_mode_button_state() == LOW){
			setGRN(LOW);
		}
		setRED(HIGH);
		usleep(500000);
	}
	
	cleanup_cape();
	return 0;
}



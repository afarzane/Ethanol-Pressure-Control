#include <iostream>
#include <wiringPi.h>
#include <string>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "include/latchFuncts.h"
#include "include/sensorBatchStructs.h"
#include <csignal>
#include <unistd.h>

using namespace std;

int shmid = shmget((key_t)123456780, sizeof(sensorBatch), 0666);
sensorBatch* shared_mem = (sensorBatch*) shmat(shmid, NULL, 0);
 
void signalHandler(int signum){
	latch(SOV1, 0);
	latch(SOV3, 0);
	exit(0);
}

int main(int argc, char** argv){
	
	bool pressurized = true; // New variable to check if chamber is pressurized
	int threshold = 10; // Threshold value for the feedback loop to maintain pressure (Needs to be discussed with fluids team)
	float currPress = 0;
    
    signal(SIGINT, signalHandler);
    if (argc != 2){
		cout<<"Improper arguments\nGo like: ./pressEth [TARGET PSI VALUE]"<<endl;
		exit(0);
    }

    float targetPress = atof(argv[1]);
    if (targetPress >= MAX_PRESS_UPPER_RANGE){
		cout<<"Target pressure too high, must be below: "<<MAX_PRESS_UPPER_RANGE<<", aborting"<<endl;
		exit(0);
    }

    latchSetup(shared_mem);
    cout<<"Starting eth press to "<<targetPress<<" psi"<<endl;
    latch(SOV1, 0);
    latch(SOV3, 0);
    usleep(1000000);
    // float currPress = shared_mem->PT_states[PT1];

	// ------------------------- Old code involving solenoid valves with no feedback ------------------------- //
    // if(currPress <= targetPress){
	// latch(SOV1,1);
	// while(currPress <= targetPress){
	// 	currPress = shared_mem->PT_states[PT1];
	// 	cout<<"Current pressure: "<<currPress<<endl;
	// 	usleep(25000);
	// }
	// latch(SOV1,0);
	// latch(SOV3,0);
    // }
    // else if(currPress > targetPress){
	// latch(SOV3,1);
	// while(currPress > targetPress){
	// 	currPress = shared_mem->PT_states[PT1];
	// 	cout<<"Current pressure: "<<currPress<<endl;
	// 	usleep(25000);
	// }
	// latch(SOV1,0);
	// latch(SOV3,0);
    // }
    // cout << "Exited successfully, last read Eth PSI at " << currPress << " psi" << endl;

	// ------------------------- Dummy variable feedback testing ------------------------- //
	// int targetPress = 400;
	// int threshold = 10;
	// int currPress = 0;
	// int counter = 0;
	// while(true){
	// 	//currPress = shared_mem->PT_states[PT1];
	// 	if(pressurized == false){
	// 		currPress++;
	// 		if(currPress == targetPress) pressurized = true;
	// 	}else{
	// 		if(counter == 100){
	// 			if(currPress < targetPress - threshold){
	// 				pressurized = false;
	// 			}else if(currPress > targetPress + threshold){
	// 				//latch(SOV3, 1);s
	// 				currPress--; // Opening vent SOV to release pressure
	// 			}else{
	// 				//latch(SOV1,0);
	// 				//latch(SOV3,0);
	// 				currPress--; // Leak causing pressure to decrease
	// 			}
	// 			counter = 0;
	// 		}
	// 		counter++;
	// 	}
	// 	cout << "Current pressure: "<< currPress << endl;
	// }

	// ----------------------------------------- New Code---------------------------------------------- //
	// bool ethanolValveOpen = false;
	// bool ventOpen = false;

	/*Notes for myself:
		For 'shared_mem->currentStates[i]'...
		- Ethanol Valve: i = 6
		- Ethanol Chamber Vent: i = 7
	*/ 

	bool printCount = false; // Print out the pressure of the chamber until pressurized to avoid a messy terminal
	
	int ethValve = 6;
	int ethVent = 7;

	while (true) { // Run this code until you wish to stop process with Ctrl + C
	
		currPress = shared_mem->PT_states[PT1]; // Check current pressure of chamber through sensors

		if (!pressurized) { // When chamber is not pressurized....
			if (currPress >= targetPress) { // Check to see if chamber is pressurized
				if (shared_mem->currentStates[ethValve] == 1) { // Check current state of ethanol chamber's valve and see if open (=1)
					cout << "Pressure stabilized. ";
					latch(SOV1, 0); // Close Ethanol valve
					// ethanolValveOpen = false;
				}
				if (shared_mem->currentStates[ethVent] == 1) { // Check current state of ethanol chamber's vent and see if open (=1)
					cout << "Pressure stabilized. ";
					latch(SOV3, 0); // Close vent
					// ventOpen = false;
				}
				pressurized = true; // Continue with feedback check in the next if statement
				printCount = true;
			}
		} else {
			if (currPress < targetPress - threshold) { // Check if pressure is lower than target pressure
				cout << "Pressure too low... Releasing Ethanol. ";
				if (shared_mem->currentStates[ethValve] == 0) {
					latch(SOV1, 1); // Open Ethanol valves to pressurize
					// ethanolValveOpen = true;
				}
				if (shared_mem->currentStates[ethVent] == 1) {
					cout << "Pressure stabilized. ";
					latch(SOV3, 0); // Close vent
					// ventOpen = false;
				}
				pressurized = false;
				printCount = false;

			} else if (currPress > targetPress + threshold) {
				cout << "Pressure too high... Opening vents to decrease pressure. ";
				if (shared_mem->currentStates[ethValve] == 1) {
					cout << "Pressure stabilized. ";
					latch(SOV1, 0); // Close Ethanol valve
					// ethanolValveOpen = false;
				}
				if (shared_mem->currentStates[ethVent] == 0) {
					latch(SOV3, 1); // Vent opening to release pressure
					// ventOpen = true;
				}
			} else {
				cout << "Pressure stabilized. ";
				if (shared_mem->currentStates[ethValve] == 1) {
					latch(SOV1, 0); // Close Ethanol valve
					// ethanolValveOpen = false;
				}
				if (shared_mem->currentStates[ethVent] == 1) {
					latch(SOV3, 0); // Close vent
					// ventOpen = false;
				}
				pressurized = true;
			}
		}
		
		if(!printCount){cout << "Current pressure: " << currPress << endl;}	// Print out the current pressure
		usleep(25000);
	}
	
    return 0;
}
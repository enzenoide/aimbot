#include <Windows.h> // include necessary Windows API headers for process and memory manipulation
#include <sstream> // Include for string stream operations
#include <iostream>//include for input/output operations
#include <math.h>// include for mathematical operations
#include <vector>//include for dynamic array operations
#include <algorithm>//include for algorithms like sort, find, etc.
#include <TlHelp32.h>//include for taking snapshots of the processes and modules
#include <iomanip>//include for text alignment and formatting


//Documentation: https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes

#define Number_Pad_9 0x69
#define F6 0x75
#define Right_mouse 0x02

//Define PI for angle calculations
const float PI = 3.14159265358979323846f;

//Flag to toggle debug output,by default debug menu is disabled, set to true to enable debug output
bool b_debugEnabled = false;

//Initializing its process ID of 'ac_client.exe'
uintptr_t uint_PID = 0;

//Initializing the base address of 'ac_client.exe' in memory
uintptr_t uint_ac_client_exe = 0;

//Initializing the handle to the target process
HANDLE h_process = 0;

//Initializing the number of entities inthe game
int i_NumberOfEntities = 0;


//Memory offsets for reading player and entity game data
const uintptr_t uint_Player_Base= 0x17E0A8;
const uintptr_t uint_EntityPlayer_Base = 0x18AC04; 
const uintptr_t uint_EntityLoopDistance = 0x4; //How many bytes each entity is apart with each other in memory
const uintptr_t uint_PlayerCount = 0x18AC0C;
const uintptr_t uint_GameState = 0x18ABF8; 
const uintptr_t uint_Eye_Position = 0x4; //Aiming at Eye for headshots
const uintptr_t uint_Rotation = 0x28;// Yaw and pitch
const uintptr_t uint_Health = 0xEC;
const uintptr_t uint_Name = 0x204;
const uintptr_t uint_Team = 0x30C;




//Function to get the process ID of the target game by its executable name
uintptr_t GetProcessIdentifier(const wchar_t* exeName) {
	//Structure to hold process information
	PROCESSENTRY32 processEntry = { 0 };

	//Create Snapshot of all processes in the system
	HANDLE Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	//Check if the snapshot was created successfully
	if (!Snapshot)
		return 0;

	//Set size of the process entry structure before using it
	processEntry.dwSize = sizeof(processEntry);

	//Get the first process in the snapshot
	if (!Process32First(Snapshot, &processEntry))
	{
		CloseHandle(Snapshot);
		return 0;
	}

	//Loop through the processes in the snapshot to find the target process by its executable name
	do {
		//compare the executable name of the current process with the target executable name
		if (!wcscmp(processEntry.szExeFile, exeName))
		{
			CloseHandle(Snapshot);
			return processEntry.th32ProcessID;
		}
	}while (Process32Next(Snapshot, &processEntry));

	//clean up and close the snapshot handle if no match found
	CloseHandle(Snapshot);
	return 0;
}
//Function to get the base address of a specific module (DLL OR EXE) in the target process
uintptr_t GetModuleBaseAddress(uintptr_t PID, const wchar_t* moduleName) {

	//Variable to store the base address of the module
	uintptr_t moduleBaseAddress = 0;

	// Take a snapshot of the modules loaded in the target process
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, PID);

	//Cherck if the snapshot was created successfully
	if (hSnap != INVALID_HANDLE_VALUE) {
		//Create a MODULEENTRY32 structure to hold module information
		MODULEENTRY32 moduleEntry;

		moduleEntry.dwSize = sizeof(moduleEntry);

		//Check if we can get the first module in the snapshot and loop through the modules to find the target module by its name
		if (Module32First(hSnap, &moduleEntry)) {
			do {
				//Compare the current module name with the target module name
				if (!wcscmp(moduleEntry.szModule, moduleName)) {

					//If a match is found, store the base address of the module and break the loop
					moduleBaseAddress = (uintptr_t)moduleEntry.modBaseAddr;
					break;
				}
			}
			//move to the next module in the snapshot
			while (Module32Next(hSnap, &moduleEntry));
		}
		CloseHandle(hSnap);

		//Return the base address of the module if found, otherwise return 0
		return moduleBaseAddress;
	}
}


//Structure to represent the local player
struct s_Player {

	uintptr_t uint_LocalPlayerPointer; // Pointer to the local player structure in memory
	int i_GameState; // Variable to store the current game state (e.g., in-game, menu, etc.)
	int i_Team; // Team identifier
	int i_Health; // Health points of the player
	float f_Position[3]; // Array to store the player's position in 3D space (x, y, z)
	char c_name[20];  // Player's name, assuming a maximum of 20 characters


	//Read varios pieces of player data using memory offsets
	void ReadMemory() {
		ReadProcessMemory(h_process, (LPCVOID)(uint_ac_client_exe + uint_Player_Base), &uint_LocalPlayerPointer, sizeof(uint_LocalPlayerPointer), NULL);
		ReadProcessMemory(h_process, (LPCVOID)(uint_LocalPlayerPointer + uint_Team), &i_Team, sizeof(i_Team), NULL);
		ReadProcessMemory(h_process, (LPCVOID)(uint_LocalPlayerPointer + uint_Health), &i_Health, sizeof(i_Health), NULL);
		ReadProcessMemory(h_process, (LPCVOID)(uint_LocalPlayerPointer + uint_Eye_Position), &f_Position, sizeof(f_Position), NULL);
		ReadProcessMemory(h_process, (LPCVOID)(uint_LocalPlayerPointer + uint_Name), &c_name, sizeof(c_name), NULL);

		ReadProcessMemory(h_process, (LPCVOID)(uint_ac_client_exe + uint_PlayerCount), &i_NumberOfEntities, sizeof(i_NumberOfEntities), NULL);
		// Decrease the entity count by 1 to exclude the local player from the entity list
		--i_NumberOfEntities;

		ReadProcessMemory(h_process, (LPCVOID)(uint_LocalPlayerPointer + uint_GameState), &i_GameState, sizeof(i_GameState), NULL);

	}
}Player;


void DebugCalculateAngle() {
	Player.ReadMemory();

	float Enemy[3] = {55,67,10};// med kit coordinates
	float Angles[2] = { 0,0 };
	
	//calculate the difference in position between the player and the enemy in each axis (x, y, z)
	// This will give us our absolute position, as if the player is at the origin (0, 0, 0) and the enemy is at (f_DeltaX, f_DeltaY, f_DeltaZ)
	float f_DeltaX = (Enemy[0] - Player.f_Position[0]);
	float f_DeltaY = (Enemy[1] - Player.f_Position[1]);
	float f_DeltaZ = (Enemy[2] - Player.f_Position[2]);


	//Calculate the hypotenuse (distance) between the player and the enemy using the Pythagorean theorem in 3D space
	float f_Hypotenuse = sqrt(f_DeltaX * f_DeltaX + f_DeltaY * f_DeltaY + f_DeltaZ * f_DeltaZ);

	/*
			arctan(inverse tangent): opposite / adjacent
			arsin(inverse sine): opposite / hypotenuse
			arcos(inverse cosine): adjacent / hypotenuse
	*/

	//Calculate yaw(vertical angle, left and right) using arctangent2 function
	//Why we use atan2f... atanf(y/x) holds back some information it only assumes that the input case came from quadrantes I or IV, only 2 quadrants
	//atan2f(y,x) resolves the correct angles, from all 4 quadrants
	//atan2f(y,x) represents an angle in a right triangle, y is the "opposite side" the side not connected to the angle , 
	// x is the "adjacent side" the side connected to the angle, and the angle is the angle between the adjacent side and the hypotenuse
	//so the function could be written as atan2f(opposite, adjacent) or atan2f(f_DeltaY, f_DeltaX) in our case,
	//because we want to calculate the angle between the player and the enemy in the horizontal plane (x and y axis)

	Angles[0] = (atan2f(f_DeltaY, f_DeltaX));// yaw in radians
	//Angles[0] = (atanf(f_DeltaY, f_DeltaX)) //Doesnt work if the DeltaX is less than 0
	//Angles[0] = (asinf(f_DeltaY / f_Hypotenuse)) //As DeltaX gets closer to 0 it becomes less accurate and drifts away
	//Angles[0] = (acosf(f_DeltaX / f_Hypotenuse)) //Not very accurate at all

	Angles[0] *= (180.0f / PI); // Convert radians to degrees

	//// Fix for atan DeltaX being less than 0
	// if(f_DeltaX <= 0){ Angles[0] += 100.0f}

	Angles[0] += 90.0f;

	//Calculate pitch(horizontal angle, up and down) using arcsine function
	//The sine of the pitch angle is the c-component divided by the hypotenuse
	Angles[1] = (asinf(f_DeltaZ / f_Hypotenuse)); // Pitch in radian.... asinf(opposite/hypotenyuse)
	//Angles[1] = (atan2f(f_DeltaZ,f_hypotenuse))// Not as accurate as asinf
	//Angles[1] = (atanf(f_DeltaZ / f_Hypotenuse)) //Not as accurate as asinf
	//Angles[1] = (acosf(f_DeltaZ / f_Hypotenuse)) //Only works when deltaY is 0 

	Angles[1] *= (180.0f / PI); // Convert radians to degrees


	system("CLS");
	std::cout << "f_DeltaX: " << f_DeltaX << std::endl;
	std::cout << "f_DeltaY: " << f_DeltaY << std::endl;
	std::cout << "f_DeltaZ: " << f_DeltaZ << std::endl;
	std::cout << "f_Hypotenuse: " << f_Hypotenuse << std::endl;
	std::cout << "Yaw: " << Angles[0] << std::endl;
	std::cout << "pitch: " << Angles[1] << std::endl;
	std::cout << "x = "<< Player.f_Position[0] << std::endl;
	std::cout << "y = " << Player.f_Position[1] << std::endl;
	std::cout << "z = " << Player.f_Position[2] << std::endl;
	std::cout << "LocalPlayerPtr: " << std::hex << Player.uint_LocalPlayerPointer << std::endl;


	WriteProcessMemory(h_process, (BYTE*)(Player.uint_LocalPlayerPointer + uint_Rotation), &Angles, sizeof(Angles), NULL);//Writes both yaw and pitch

	float test[3];
	ReadProcessMemory(h_process, (LPCVOID)(Player.uint_LocalPlayerPointer + 0x4), &test, sizeof(test), NULL);

	std::cout << "Raw X: " << test[0] << std::endl;
	std::cout << "Raw Y: " << test[1] << std::endl;
	std::cout << "Raw Z: " << test[2] << std::endl;

	float x, y, z;

	ReadProcessMemory(h_process, (LPCVOID)(Player.uint_LocalPlayerPointer + 0x4), &x, sizeof(float), NULL);
	ReadProcessMemory(h_process, (LPCVOID)(Player.uint_LocalPlayerPointer + 0x8), &y, sizeof(float), NULL);
	ReadProcessMemory(h_process, (LPCVOID)(Player.uint_LocalPlayerPointer + 0xC), &z, sizeof(float), NULL);

	std::cout << "X: " << x << std::endl;
	std::cout << "Y: " << y << std::endl;
	std::cout << "Z: " << z << std::endl;
}

int main() {

	//Get the process ID of the target game by its executable name
	uint_PID = GetProcessIdentifier(L"ac_client.exe");

	//Get the address of 'ac_client.exe' in memory
	uint_ac_client_exe = GetModuleBaseAddress(uint_PID, L"ac_client.exe");

	//Open handle to the game process with full access
	h_process = OpenProcess(PROCESS_ALL_ACCESS, NULL, uint_PID);

	printf("Aimbot always on.... hold right mouse button to aim at the med kit\n");

	//Bring the game window to the foreground to ensure it receives input and the aimbot works correctly
	SetForegroundWindow(FindWindowA(0, "AssaultCube"));

	while (!GetAsyncKeyState(F6)) {
		if (!GetAsyncKeyState(Right_mouse)) {// If the right mouse button is not pressed, skip the aimbot calculations and continue to the next iteration of the loop
			//Aimbot()
			DebugCalculateAngle();
			Sleep(1); // Sleep for a short duration to reduce CPU usage and prevent the loop from running too fast
		}
	}
}
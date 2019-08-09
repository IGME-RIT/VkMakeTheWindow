/*
Copyright 2019
Original authors: Niko Procopi
Written under the supervision of David I. Schwartz, Ph.D., and
supported by a professional development seed grant from the B. Thomas
Golisano College of Computing & Information Sciences
(https://www.rit.edu/gccis) at the Rochester Institute of Technology.
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.
<http://www.gnu.org/licenses/>.

Special Thanks to Exzap from Team Cemu,
he gave me advice on how to optimize Vulkan
graphics, he is working on a Wii U emulator
that utilizes Vulkan, see more at http://cemu.info
*/

#include "Demo.h"

#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <signal.h>
#include <vector>
#include "Main.h"

// This boolean keeps track of how many times we have executed the
// "prepare()" function. If we have never used the function before
// then we know we are initializing the program for the first time,
// if the boolean is false, then we know the program has already
// been initialized
bool firstInit = true;

void Demo::prepare_console()
{
	// This line is commented out,
	// if you uncomment this "freopen" line,
	// it will redirect all text from the console
	// window into a text file. Then, nothing will
	// print to the console window, but it will all
	// be saved to a file

	// freopen("output.txt", "a+", stdout);

	// If you leave that line commented out ^^
	// then everything will print to the console
	// window, just like normal

	// Create Console Window
	
	// Allocate memory for the console
	AllocConsole();
	
	// Attatch the console to this process,
	// so that printf statements from this 
	// process go into the console
	AttachConsole(GetCurrentProcessId());

	// looks confusing, but we never need to change it,
	// basically this redirects all printf, scanf,
	// cout, and cin to go to the console, rather than
	// going where it goes by default, which is nowhere
	FILE *stream;
	freopen_s(&stream, "conin$", "r", stdin);
	freopen_s(&stream, "conout$", "w", stdout);
	freopen_s(&stream, "conout$", "w", stderr);
	SetConsoleTitle(TEXT("Console window"));

	// Adjust Console Window
	// This is completely optional

	// Get the console window
	HWND console = GetConsoleWindow();

	// Move the window to position (0, 0),
	// resize the window to be 640 x 360 (+40).
	// The +40 is to account for the title bar
	MoveWindow(console, 0, 0, 640, 360 + 40, TRUE);
}

void Demo::prepare_window()
{
	// Make the title of the screen "Loading"
	// while the program loads
	strncpy(name, "Hello World", APP_NAME_STR_LEN);

	WNDCLASSEX win_class = {};

	// Initialize the window class structure:
	// The most important detail here is that we
	// give it the WndProc function from main.cpp,
	// so that the function can handle our window.
	// Everything else here sets the icon, the title,
	// the cursor, thing like that
	win_class.cbSize = sizeof(WNDCLASSEX);
	win_class.style = CS_HREDRAW | CS_VREDRAW;
	win_class.lpfnWndProc = WndProc;
	win_class.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	win_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	win_class.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	win_class.lpszMenuName = NULL;
	win_class.lpszClassName = name;
	win_class.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

	// RegisterClassEx will do waht it sounds like,
	// it registers the WNDCLASSEX structure, so that
	// we can use win_class to make a window

	// If the function fails to register win_class
	// then give an error
	if (!RegisterClassEx(&win_class))
	{
		// It didn't work, so try to give a useful error:
		printf("Unexpected error trying to start the application!\n");
		fflush(stdout);
		exit(1);
	}

	// Create window with the registered class:
	RECT wr = { 0, 0, width, height };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

	// We will now create our window
	// with the function "CreateWindowEx"
	// Win32 and DirectX 11 have some functions
	// with a huge ton of parameters. What 
	// Vulkan does for all functions, and what
	// DirectX 11 sometimes does, is organize
	// all parameters into a structure, and then
	// pass the structure as one parameter
	window = CreateWindowEx(0,
		name,            // class name
		name,            // app name
		WS_OVERLAPPEDWINDOW |  // window style
		WS_VISIBLE | WS_SYSMENU,

		// The position (0,0) is the top-left
		// corner of the screen, which is where
		// the command prompt is, and the command 
		// prompt is 640 pixels wide, so lets put
		// the Vulkan window right next to the console window
		640, 0,				 // (x,y) position
		wr.right - wr.left,  // width
		wr.bottom - wr.top,  // height
		NULL,                // handle to parent, no parent exists

		// This would give you "File" "Edit" "Help", etc
		NULL,                // handle to menu, no menu exists
		(HINSTANCE)0,		 // no hInstance
		NULL);               // no extra parameters

	// if we failed to make the window
	// then give an error that the window failed
	if (!window)
	{
		// It didn't work, so try to give a useful error:
		printf("Cannot create a window in which to draw!\n");
		fflush(stdout);
		exit(1);
	}

	// Window client area size must be at least 1 pixel high, to prevent crash.
	minsize.x = GetSystemMetrics(SM_CXMINTRACK);
	minsize.y = GetSystemMetrics(SM_CYMINTRACK) + 1;
}

void Demo::prepare()
{
	// We will be calling prepare() multiple times.
	// Some Vulkan assets only need to be created once, like
	// the instance, the device, and the queue (i'll explain those soon),
	// while some things need to be destroyed and rebuilt, like the 
	// swapchain images (I'll explain those soon).

	// We keep track of a variable called firstInit, to determine
	// if we have run the prepare() function before. "firstInit"
	// is initialized as true at the top of Demo.cpp, and it is
	// set to false after the first initialization is done

	if (firstInit)
	{
		// Validation will tell us if our Vulkan code is correct.
		// Sometimes, our code will execute the way we want it to,
		// but just becasue the code runs, does not mean the code
		// is correct. If you've ever used HTML, you may have used
		// and HTML validator, where your website might look correct,
		// but the validator will tell you that the code is wrong

		// If you run in Debug mode, no Validator text will appear,
		// text from Validator will only appear if you are in Release Mode

		// During development, this is a great tool. However, when it is
		// time to release a software or game, you don't want this running
		// in the background because it will continue constantly checking for errors
		// even if there are no errors. So, when you want to release a software or
		// game, simply set this to false.
		validate = true;

		// During development, it is good to have a console window.
		// You can read errors, and write printf statements.
		// However, if you want to release a software or game, you may
		// not want a console window. Simply comment out this line to 
		// disable the console window.
		prepare_console();

		// set the width and height of the window
		// literally set this to whatever you want
		width = 640;
		height = 360;

		// build the window with the Win32 API. This will look similar to how
		// a window is created in a DirectX 11/12 engine, and we will use the
		// WndProc from main.cpp to create the window
		prepare_window();
	}
}

Demo::Demo()
{
	// Welcome to the Demo constructor
	// The Demo class will handle the majority
	// of our code in this tutorial

	// Please look at Demo.h, 
	// where you can see a list of every variable that
	// we will use in the Demo class. Every variable
	// in this class will be fully explained while
	// we move through the code

	// The first thing we do is initalize the scene
	prepare();
}

Demo::~Demo()
{
}

// TaskManager.cpp
// Copyright 2000-01, Sony Online Entertainment Inc., all rights reserved. 
// Author: Justin Randall

//-----------------------------------------------------------------------

#include "FirstTaskManager.h"
#include "Console.h"
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <curses.h>
#ifdef _WIN32
#include <conio.h>  // For Windows console handling
#endif

//-----------------------------------------------------------------------

// Buffer mode management for non-blocking input
struct SetBufferMode
{
	SetBufferMode();
	~SetBufferMode() = default;  // Default destructor, no special handling needed
};

SetBufferMode::SetBufferMode()
{
	// Set stdin to unbuffered to allow immediate character input
	setvbuf(stdin, nullptr, _IONBF, 0);
}

//-----------------------------------------------------------------------

// Helper function to handle non-blocking character input for Unix-like systems
char getNextCharUnix()
{
	fd_set rfds;
	struct timeval tv;
	char result = 0;

	FD_ZERO(&rfds);                // Clear the file descriptor set
	FD_SET(STDIN_FILENO, &rfds);    // Add stdin (keyboard) to the set

	tv.tv_sec = 0;                  // Non-blocking, set to 0 seconds
	tv.tv_usec = 0;                 // Non-blocking, set to 0 microseconds

	// Use select() to check if input is available without blocking
	int ready = select(1, &rfds, nullptr, nullptr, &tv);
	if (ready > 0)                  // If input is ready, get the character
	{
		result = static_cast<char>(getchar());
		// Ensure valid input is captured
		if (result < 1) {
			result = 0;
		}
	}

	return result;
}

#ifdef _WIN32
// Helper function for Windows to handle non-blocking character input
char getNextCharWindows()
{
	char result = 0;
	if (_kbhit())                   // Check if a key has been pressed
	{
		result = static_cast<char>(_getche());  // Get and echo the pressed key
	}
	return result;
}
#endif

//-----------------------------------------------------------------------

// Unified getNextChar method that works on both Unix-like systems and Windows
const char Console::getNextChar()
{
	static SetBufferMode setBufferMode;  // Set unbuffered mode for stdin

#ifdef _WIN32
	return getNextCharWindows();
#else
	return getNextCharUnix();
#endif
}

//-----------------------------------------------------------------------



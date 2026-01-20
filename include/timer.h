#pragma once

#include <chrono>

class Timer
{
public:
	void start() 
	{
		startTime = std::chrono::high_resolution_clock::now();
		running = true;
	}

	void stop()
	{
		endTime = std::chrono::high_resolution_clock::now();
		running = false;
	}

	double elapsedMilliseconds()
	{
		if (running)
		{
			endTime = std::chrono::high_resolution_clock::now();
		}
		std::chrono::duration<double, std::milli> duration = endTime - startTime;
		return duration.count();
	}

	double elapsedSeconds()
	{
		return elapsedMilliseconds() / 1000.0;
	}

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
	std::chrono::time_point<std::chrono::high_resolution_clock> endTime;
	bool running = false;
};
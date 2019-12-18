//: # `reader.cpp` ([source](../appskeleton/reader.cpp))
#include "reader.h"
#include <chrono>
#include <thread>


Reader::Reader(int32_t device_param) : counter(device_param) {
	// Here we would connect to the device
}

Reader::~Reader() noexcept {
	// Close the connection
}

bool Reader::getData(std::vector<int32_t> &buffer) {
	// Acquire some data and return it
	for (auto &elem : buffer) elem = counter++;
	std::this_thread::sleep_for(std::chrono::milliseconds(buffer.size() * 100));
	return true;
}

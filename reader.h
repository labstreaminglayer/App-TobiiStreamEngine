//: # `reader.h` ([source](../appskeleton/reader.h))
#pragma once

#include <cstdint>
#include <vector>

//: Most recording device APIs provide some sort of handle to a device and
//: functions to query the state, read data and put it in a buffer etc.
//:
//: This is a very simple example to demonstrate how to integrate it with LSL.
//: The provided functions are:
//:
//: - the constructor
//: - the destructor
//: - `getData` with the buffer as one output parameter and the status as return value
//: - `getStatus` to check if everything's ok
class Reader {
public:
	explicit Reader(int32_t device_param);
	~Reader() noexcept;
	bool getData(std::vector<int32_t> &buffer);
	bool getStatus() { return true; }


private:
	int32_t counter;
};

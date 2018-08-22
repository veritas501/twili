#pragma once

#include<vector>

#include "RemoteObject.hpp"

namespace twili {
namespace twib {

class ITwibPipeReader {
 public:
	ITwibPipeReader(RemoteObject obj);

	std::vector<uint8_t> ReadSync();
 private:
	RemoteObject obj;
};

} // namespace twib
} // namespace twili
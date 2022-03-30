//
// Twili - Homebrew debug monitor for the Nintendo Switch
// Copyright (C) 2018 misson20000 <xenotoad@xenotoad.net>
//
// This file is part of Twili.
//
// Twili is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Twili is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Twili.  If not, see <http://www.gnu.org/licenses/>.
//

#pragma once

#include<condition_variable>
#include<mutex>

#include "platform/platform.hpp"
#include "platform/EventLoop.hpp"
#include "Buffer.hpp"

namespace twili {
namespace twib {
namespace tool {
namespace gdb {

class GdbConnection {
 public:
	GdbConnection(platform::File &&input_file, platform::File &&output_file);

	// NULL pointer means no message.
	// Entire buffer should be consumed before calling this again.
	util::Buffer *Process(bool &interrupted);

	std::mutex mutex;
	std::condition_variable error_condvar;
	
	bool error_flag = false;

	static uint8_t DecodeHexByte(char *hex);
	static uint8_t DecodeHexNybble(char hex);
	static void DecodeWithSeparator(uint64_t &out, char sep, util::Buffer &packet);
	static void DecodeWithSeparator(std::vector<uint8_t> &out, char sep, util::Buffer &packet);
	static void Decode(uint64_t &out, util::Buffer &packet);
	static void Decode(std::vector<uint8_t> &out, util::Buffer &packet);
	static void Decode(util::Buffer &out, util::Buffer &packet);
	static char EncodeHexNybble(uint8_t n);
	static void Encode(uint64_t n, size_t size, util::Buffer &dest);
	static void Encode(uint8_t *p, size_t size, util::Buffer &dest);
	static void Encode(std::string &string, util::Buffer &dest);
	static void Encode(std::string &&string, util::Buffer &dest);
	
	class InputMember : public platform::EventLoop::FileMember {
	 public:
		InputMember(GdbConnection &connection, platform::File &&File);

		virtual bool WantsRead() override;
		virtual void SignalRead() override;
		virtual void SignalError() override;
	 private:
		virtual platform::File &GetFile() final override;
		
		platform::File file;
		GdbConnection &connection;
	} in_member;

	void Respond(util::Buffer &buffer);
	void RespondEmpty();
	void RespondError(int no);
	void RespondOk();
	void SignalError();
	void StartNoAckMode();
	
 private:
	platform::File out_file;

	util::Buffer in_buffer;
	util::Buffer message_buffer;
	util::Buffer out_buffer;
	enum class State {
		WAITING_PACKET_OPEN,
		READING_PACKET_DATA,
		ESCAPE_CHARACTER,
		CHECKSUM_0,
		CHECKSUM_1
	} state = State::WAITING_PACKET_OPEN;

	uint8_t checksum = 0;
	char checksum_hex[2];
	bool ack_enabled = true; // this starts enabled
};

} // namespace gdb
} // namespace tool
} // namespace twib
} // namespace twili

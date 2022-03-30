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

#include "GdbConnection.hpp"
#include "common/Logger.hpp"

namespace twili {
namespace twib {
namespace tool {
namespace gdb {

GdbConnection::GdbConnection(
	platform::File &&input_file,
	platform::File &&output_file) :
	in_member(*this, std::move(input_file)),
	out_file(std::move(output_file)) {
}

util::Buffer *GdbConnection::Process(bool &interrupted) {
	std::unique_lock<std::mutex> lock(mutex);
	char ch;
	interrupted = false;
	while(in_buffer.Read(ch)) {
		switch(state) {
		case State::WAITING_PACKET_OPEN:
			if(ch == '+') {
				// ignore ack
				break;
			}
			if(ch == 0x03) {
				// break
				interrupted = true;
				return nullptr;
			}
			if(ch != '$') {
				LogMessage(Error, "packet opened with bad character %c", ch);
				SignalError();
				return nullptr;
			}
			message_buffer.Clear();
			checksum = 0;
			state = State::READING_PACKET_DATA;
			break;
		case State::READING_PACKET_DATA:
			if(ch == '#') {
				state = State::CHECKSUM_0;
				break;
			}
			checksum+= ch;
			if(ch == '}') {
				state = State::ESCAPE_CHARACTER;
				break;
			}
			message_buffer.Write(ch);
			break;
		case State::ESCAPE_CHARACTER:
			checksum+= ch;
			message_buffer.Write(ch ^ 0x20);
			state = State::READING_PACKET_DATA;
			break;
		case State::CHECKSUM_0:
			checksum_hex[0] = ch;
			state = State::CHECKSUM_1;
			break;
		case State::CHECKSUM_1:
			checksum_hex[1] = ch;
			state = State::WAITING_PACKET_OPEN;

			if(DecodeHexByte(checksum_hex) != checksum) {
				if(ack_enabled) {
					char msg = '-';
					write(out_file.fd, &msg, sizeof(msg)); // request retransmission
				} else {
					LogMessage(Error, "checksum does not match");
					SignalError();
					return nullptr;
				}
			} else {
				if(ack_enabled) {
					char msg = '+';
					write(out_file.fd, &msg, sizeof(msg)); // ok
				}
				return &message_buffer;
			}
			break;
		}
	}
	return nullptr;
}

void GdbConnection::Respond(util::Buffer &buffer) {
	std::unique_lock<std::mutex> lock(mutex);
	out_buffer.Write('$');
	uint8_t checksum = 0;
	char ch;
	while(buffer.Read(ch)) {
		if(ch == '#' || ch == '$' || ch == '}' || ch == '*') {
			out_buffer.Write('}');
			checksum+= '}';
			ch^= 0x20;
		}
		out_buffer.Write(ch);
		checksum+= ch;
	}
	out_buffer.Write('#');
	Encode(checksum, 1, out_buffer);

	while(out_buffer.ReadAvailable()) {
		ssize_t r = write(out_file.fd, (char*) out_buffer.Read(), out_buffer.ReadAvailable());
		if(r < 0) {
			SignalError();
			return;
		}
		if(r > 0) {
			out_buffer.MarkRead(r);
		}
	}
}

void GdbConnection::RespondEmpty() {
	static util::Buffer empty;
	Respond(empty);
}

void GdbConnection::RespondError(int no) {
	util::Buffer buf;
	char resp[4];
	snprintf(resp, sizeof(resp), "E%02x", no & 0xff);
	buf.Write((const char *)resp);
	Respond(buf);
}

void GdbConnection::RespondOk() {
	util::Buffer ok;
	ok.Write('O');
	ok.Write('K');
	Respond(ok);
}

void GdbConnection::StartNoAckMode() {
	ack_enabled = false;
}

GdbConnection::InputMember::InputMember(GdbConnection &connection, platform::File &&file) :
	connection(connection),
	file(std::move(file)) {
}

bool GdbConnection::InputMember::WantsRead() {
	return true;
}

void GdbConnection::InputMember::SignalRead() {
	std::tuple<uint8_t*, size_t> target = connection.in_buffer.Reserve(8192);
	ssize_t r = read(file.fd, (char*) std::get<0>(target), std::get<1>(target));
	if(r <= 0) {
		SignalError();
	} else {
		LogMessage(Debug, "gdb connection got 0x%lx bytes in", r);
		connection.in_buffer.MarkWritten(r);
	}
}

platform::File &GdbConnection::InputMember::GetFile() {
	return file;
}

void GdbConnection::InputMember::SignalError() {
	connection.SignalError();
}

void GdbConnection::SignalError() {
	LogMessage(Error, "gdb connection error");
	error_flag = true;
	error_condvar.notify_all();
}

uint8_t GdbConnection::DecodeHexNybble(char n) {
	if(n >= '0' && n <= '9') {
		return n - '0';
	}
	if(n >= 'a' && n <= 'f') {
		return n - 'a' + 0xa;
	}
	if(n >= 'A' && n <= 'F') {
		return n - 'A' + 0xa;
	}

	LogMessage(Error, "invalid nybble (%c)", n);
	return 0;
}

uint8_t GdbConnection::DecodeHexByte(char *h) {
	return (DecodeHexNybble(h[0]) << 4) | DecodeHexNybble(h[1]);
}

void GdbConnection::DecodeWithSeparator(uint64_t &out, char sep, util::Buffer &packet) {
	out = 0;
	while(packet.ReadAvailable() && packet.Read()[0] != sep) {
		out<<= 4;
		out|= DecodeHexNybble(packet.Read()[0]);
		packet.MarkRead(1); // consume
	}
	if(packet.ReadAvailable()) {
		packet.MarkRead(1); // consume separator
	}
}

void GdbConnection::DecodeWithSeparator(std::vector<uint8_t> &out, char sep, util::Buffer &packet) {
	out.reserve(packet.ReadAvailable()/2);
	while(packet.ReadAvailable() && packet.Read()[0] != sep) {
		uint8_t b = DecodeHexNybble(packet.Read()[0]) << 4;
		packet.MarkRead(1); // consume
		if(!packet.ReadAvailable()) {
			LogMessage(Error, "unexpectedly odd number of nybbles");
			return;
		}
		b|= DecodeHexNybble(packet.Read()[0]);
		packet.MarkRead(1); // consume

		out.push_back(b);
	}
	if(packet.ReadAvailable()) {
		packet.MarkRead(1); // consume separator
	}
}

void GdbConnection::Decode(uint64_t &out, util::Buffer &packet) {
	out = 0;
	while(packet.ReadAvailable()) {
		out<<= 4;
		out|= DecodeHexNybble(packet.Read()[0]);
		packet.MarkRead(1); // consume
	}
}

void GdbConnection::Decode(std::vector<uint8_t> &out, util::Buffer &packet) {
	out.reserve(packet.ReadAvailable()/2);
	while(packet.ReadAvailable()) {
		uint8_t b = DecodeHexNybble(packet.Read()[0]) << 4;
		packet.MarkRead(1); // consume
		if(!packet.ReadAvailable()) {
			LogMessage(Error, "unexpectedly odd number of nybbles");
			return;
		}
		b|= DecodeHexNybble(packet.Read()[0]);
		packet.MarkRead(1); // consume

		out.push_back(b);
	}
}

void GdbConnection::Decode(util::Buffer &out, util::Buffer &packet) {
	std::tuple<uint8_t*, size_t> r = out.Reserve(packet.ReadAvailable()/2);
	uint8_t *dest = std::get<0>(r);
	while(packet.ReadAvailable()) {
		uint8_t b = DecodeHexNybble(packet.Read()[0]) << 4;
		packet.MarkRead(1); // consume
		if(!packet.ReadAvailable()) {
			LogMessage(Error, "unexpectedly odd number of nybbles");
			return;
		}
		b|= DecodeHexNybble(packet.Read()[0]);
		packet.MarkRead(1); // consume

		*(dest++) = b;
		out.MarkWritten(1);
	}
}

char GdbConnection::EncodeHexNybble(uint8_t n) {
	if(n < 0xa) {
		return '0' + n;
	} else {
		// Note to any gdb stub implementors looking at my code:
		// It is very important that this is lowercase, because
		// if a packet's response is hex data and it begins with
		// a 0xE nybble, encoding as 'E' will cause gdb to think
		// it's an error reply, whereas encoding as 'e' will have
		// the desired behaviour.

		// This has been fixed in newer versions of gdb:
		// https://sourceware.org/bugzilla/show_bug.cgi?id=9665
		return 'a' + n - 0xa;
	}
}

void GdbConnection::Encode(uint64_t n, size_t size, util::Buffer &out_buffer) {
	for(int bit_offset = size == 0 ? 64 : size * 8; bit_offset > 0; bit_offset-= 8) {
		if(size != 0 || ((n >> (bit_offset - 8)) & 0xff) != 0) { // skip zero bytes if size == 0
			out_buffer.Write(EncodeHexNybble((n >> (bit_offset - 4)) & 0xf));
			out_buffer.Write(EncodeHexNybble((n >> (bit_offset - 8)) & 0xf));
		}
	}
}

void GdbConnection::Encode(uint8_t *p, size_t size, util::Buffer &out_buffer) {
	for(size_t i = 0; i < size; i++) {
		uint8_t n = p[i];
		out_buffer.Write(EncodeHexNybble((n >> 4) & 0xf));
		out_buffer.Write(EncodeHexNybble((n >> 0) & 0xf));
	}
}

void GdbConnection::Encode(std::string &string, util::Buffer &out_buffer) {
	Encode((uint8_t*) string.data(), string.size(), out_buffer);
}

void GdbConnection::Encode(std::string &&string, util::Buffer &out_buffer) {
	Encode((uint8_t*) string.data(), string.size(), out_buffer);
}

} // namespace gdb
} // namespace tool
} // namespace twib
} // namespace twili

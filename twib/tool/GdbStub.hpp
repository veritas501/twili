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

#include<optional>
#include<unordered_map>

#include "GdbConnection.hpp"
#include "interfaces/ITwibDeviceInterface.hpp"
#include "interfaces/ITwibDebugger.hpp"

namespace twili {
namespace twib {
namespace tool {
namespace gdb {

class GdbStub {
 public:
	GdbStub(ITwibDeviceInterface &itdi);
	~GdbStub();
	
	void Run();

	inline static uint64_t ShiftPid(uint64_t pid) {
		if(pid == 0) {
			return 512;
		}
		return pid;
	}
	
	inline static uint64_t UnshiftPid(uint64_t pid) {
		if(pid == 512) {
			return 0;
		}
		return pid;
	}
	
	class Query {
	 public:
		Query(GdbStub &stub, std::string field, void (GdbStub::*visitor)(util::Buffer&), bool should_advertise=true, char separator=':');

		const GdbStub &stub;
		const std::string field;
		void (GdbStub::*const visitor)(util::Buffer&);
		const bool should_advertise;
		const char separator;
	};

	class XferObject {
	 public:
		virtual void Read(std::string annex, size_t offset, size_t length) = 0;
		virtual void Write(std::string annex, size_t offset, util::Buffer &data) = 0;
		virtual bool AdvertiseRead();
		virtual bool AdvertiseWrite();
	};

	class ReadOnlyStringXferObject : public XferObject {
	 public:
		ReadOnlyStringXferObject(GdbStub &stub, std::string (GdbStub::*generator)());
		virtual void Read(std::string annex, size_t offset, size_t length) override;
		virtual void Write(std::string annex, size_t offset, util::Buffer &data) override;
		virtual bool AdvertiseRead() override;
	 private:
		GdbStub &stub;
		std::string (GdbStub::*const generator)();
	};

	class Process;
	
	class Thread {
	 public:
		Thread(Process &process, uint64_t thread_id, uint64_t tls_addr);
		ThreadContext GetRegisters();
		void SetRegisters(const ThreadContext &regs);
		Process &process;
		uint64_t thread_id = 0;
		uint64_t tls_addr = 0;
	};

	class Process {
	 public:
		Process(uint64_t pid, ITwibDebugger debugger);
		bool IngestEvents(GdbStub &stub); // returns whether process is stopped
		std::string BuildLibraryList();
		uint64_t pid;
		ITwibDebugger debugger;
		std::map<uint64_t, Thread> threads;
		std::vector<uint64_t> running_thread_ids;
		std::shared_ptr<bool> has_events;
		bool running = false;
	};
	
	Thread *current_thread = nullptr;
	std::map<uint64_t, Process> attached_processes;
	
	void AddFeature(std::string feature);
	void AddGettableQuery(Query query);
	void AddSettableQuery(Query query);
	void AddMultiletterHandler(std::string name, void (GdbStub::*handler)(util::Buffer&));
	void AddXferObject(std::string name, XferObject &ob);
	
	std::string stop_reason = "W00";
	bool waiting_for_stop = false;
	bool has_async_wait = false;
	bool multiprocess_enabled = false;

	void Stop();
	
 private:
	ITwibDeviceInterface &itdi;
	GdbConnection connection;
	class Logic : public platform::EventLoop::Logic {
	 public:
		Logic(GdbStub &stub);
		virtual void Prepare(platform::EventLoop &loop) override;
	 private:
		GdbStub &stub;
	} logic;
	platform::EventLoop loop;

	std::list<std::string> features;
	std::unordered_map<std::string, Query> gettable_queries;
	std::unordered_map<std::string, Query> settable_queries;
	std::unordered_map<std::string, void (GdbStub::*)(util::Buffer&)> multiletter_handlers;
	std::unordered_map<std::string, XferObject&> xfer_objects;

	struct {
		bool valid = false;
		std::map<uint64_t, Process>::iterator process_iterator;
		std::map<uint64_t, Thread>::iterator thread_iterator;
	} get_thread_info;

	// utilities
	void ReadThreadId(util::Buffer &buffer, int64_t &pid, int64_t &thread_id);
	
	// packets
	void HandleGeneralGetQuery(util::Buffer &packet);
	void HandleGeneralSetQuery(util::Buffer &packet);
	void HandleIsThreadAlive(util::Buffer &packet);
	void HandleMultiletterPacket(util::Buffer &packet);
	void HandleGetStopReason();
	void HandleDetach(util::Buffer &packet);
	void HandleReadGeneralRegisters();
	void HandleWriteGeneralRegisters(util::Buffer &packet);
	void HandleSetCurrentThread(util::Buffer &packet);
	void HandleReadMemory(util::Buffer &packet);
	void HandleWriteMemory(util::Buffer &packet);
	
	// multiletter packets
	void HandleVAttach(util::Buffer &packet);
	void HandleVContQuery(util::Buffer &packet);
	void HandleVCont(util::Buffer &packet);
	void HandleVFile(util::Buffer &packet);
	
	// get queries
	void QueryGetSupported(util::Buffer &packet);
	void QueryGetCurrentThread(util::Buffer &packet);
	void QueryGetFThreadInfo(util::Buffer &packet);
	void QueryGetSThreadInfo(util::Buffer &packet);
	void QueryGetThreadExtraInfo(util::Buffer &packet);
	void QueryGetOffsets(util::Buffer &packet);
	void QueryGetRemoteCommand(util::Buffer &packet);
	void QueryXfer(util::Buffer &packet);
	
	// set queries
	void QuerySetStartNoAckMode(util::Buffer &packet);
	void QuerySetThreadEvents(util::Buffer &packet);

	// xfer objects
	std::string XferReadLibraries();
	ReadOnlyStringXferObject xfer_libraries;
	
	bool thread_events_enabled = false;

	int fake_mappings_fd = -1;
	std::string fake_mappings_buffer;
};

} // namespace gdb
} // namespace tool
} // namespace twib
} // namespace twili

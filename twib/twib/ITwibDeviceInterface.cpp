#include "ITwibDeviceInterface.hpp"

#include "Protocol.hpp"
#include "Logger.hpp"

namespace twili {
namespace twib {

ITwibDeviceInterface::ITwibDeviceInterface(std::shared_ptr<RemoteObject> obj) : obj(obj) {
}

ITwibProcessMonitor ITwibDeviceInterface::CreateMonitoredProcess(std::string type) {
	Response rs = obj->SendSyncRequest(protocol::ITwibDeviceInterface::Command::CREATE_MONITORED_PROCESS, std::vector<uint8_t>(type.begin(), type.end()));
	return ITwibProcessMonitor(rs.objects[0]);
}

void ITwibDeviceInterface::Reboot() {
	obj->SendSyncRequest(protocol::ITwibDeviceInterface::Command::REBOOT);
}

std::vector<uint8_t> ITwibDeviceInterface::CoreDump(uint64_t process_id) {
    uint8_t *process_id_bytes = (uint8_t*) &process_id;
    return obj->SendSyncRequest(protocol::ITwibDeviceInterface::Command::COREDUMP, std::vector<uint8_t>(process_id_bytes, process_id_bytes + sizeof(process_id))).payload;
}

void ITwibDeviceInterface::Terminate(uint64_t process_id) {
	uint8_t *process_id_bytes = (uint8_t*) &process_id;
	obj->SendSyncRequest(protocol::ITwibDeviceInterface::Command::TERMINATE, std::vector<uint8_t>(process_id_bytes, process_id_bytes + sizeof(process_id)));
}

std::vector<ProcessListEntry> ITwibDeviceInterface::ListProcesses() {
	Response rs = obj->SendSyncRequest(protocol::ITwibDeviceInterface::Command::LIST_PROCESSES);
	ProcessListEntry *first = (ProcessListEntry*) rs.payload.data();
	return std::vector<ProcessListEntry>(first, first + (rs.payload.size() / sizeof(ProcessListEntry)));
}

msgpack11::MsgPack ITwibDeviceInterface::Identify() {
	Response rs = obj->SendSyncRequest(protocol::ITwibDeviceInterface::Command::IDENTIFY);
	std::string err;
	return msgpack11::MsgPack::parse(std::string(rs.payload.begin(), rs.payload.end()), err);
}

std::vector<std::string> ITwibDeviceInterface::ListNamedPipes() {
	Response rs = obj->SendSyncRequest(protocol::ITwibDeviceInterface::Command::LIST_NAMED_PIPES);
	uint32_t count = *(uint32_t*) rs.payload.data();
	std::vector<std::string> names;
	
	size_t pos = 4;
	for(uint32_t i = 0; i < count; i++) {
		uint32_t size = *(uint32_t*) (rs.payload.data() + pos);
		pos+= 4;
		names.emplace_back(rs.payload.data() + pos, rs.payload.data() + pos + size);
		pos+= size;
	}

	return names;
}

ITwibPipeReader ITwibDeviceInterface::OpenNamedPipe(std::string name) {
	Response rs = obj->SendSyncRequest(protocol::ITwibDeviceInterface::Command::OPEN_NAMED_PIPE, std::vector<uint8_t>(name.begin(), name.end()));
	if(rs.payload.size() < sizeof(uint32_t)) {
		LogMessage(Fatal, "response size invalid");
		exit(1);
	}
	return rs.objects[*(uint32_t*) rs.payload.data()];
}

msgpack11::MsgPack ITwibDeviceInterface::GetMemoryInfo() {
	Response rs = obj->SendSyncRequest(protocol::ITwibDeviceInterface::Command::GET_MEMORY_INFO);
	std::string err;
	return msgpack11::MsgPack::parse(std::string(rs.payload.begin(), rs.payload.end()), err);
}

} // namespace twib
} // namespace twili

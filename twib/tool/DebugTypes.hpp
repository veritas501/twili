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

namespace twili {
namespace twib {
namespace nx {

using PageInfo = uint32_t;

#ifndef BIT
#define BIT(n) (1U<<(n))
#endif

// copied from https://github.com/switchbrew/libnx/blob/master/nx/include/switch/kernel/svc.h
/// Memory type enumeration (lower 8 bits of \ref MemoryState)
typedef enum {
    MemType_Unmapped = 0x00,            ///< Unmapped memory.
    MemType_Io = 0x01,                  ///< Mapped by kernel capability parsing in svcCreateProcess.
    MemType_Normal = 0x02,              ///< Mapped by kernel capability parsing in svcCreateProcess.
    MemType_CodeStatic = 0x03,          ///< Mapped during svcCreateProcess.
    MemType_CodeMutable = 0x04,         ///< Transition from MemType_CodeStatic performed by svcSetProcessMemoryPermission.
    MemType_Heap = 0x05,                ///< Mapped using svcSetHeapSize.
    MemType_SharedMem = 0x06,           ///< Mapped using svcMapSharedMemory.
    MemType_WeirdSharedMem = 0x07,      ///< Mapped using svcMapMemory.
    MemType_ModuleCodeStatic = 0x08,    ///< Mapped using svcMapProcessCodeMemory.
    MemType_ModuleCodeMutable = 0x09,   ///< Transition from MemType_ModuleCodeStatic performed by svcSetProcessMemoryPermission.
    MemType_IpcBuffer0 = 0x0A,          ///< IPC buffers with descriptor flags=0.
    MemType_MappedMemory = 0x0B,        ///< Mapped using svcMapMemory.
    MemType_ThreadLocal = 0x0C,         ///< Mapped during svcCreateThread.
    MemType_TransferMemIsolated = 0x0D, ///< Mapped using svcMapTransferMemory when the owning process has perm=0.
    MemType_TransferMem = 0x0E,         ///< Mapped using svcMapTransferMemory when the owning process has perm!=0.
    MemType_ProcessMem = 0x0F,          ///< Mapped using svcMapProcessMemory.
    MemType_Reserved = 0x10,            ///< Reserved.
    MemType_IpcBuffer1 = 0x11,          ///< IPC buffers with descriptor flags=1.
    MemType_IpcBuffer3 = 0x12,          ///< IPC buffers with descriptor flags=3.
    MemType_KernelStack = 0x13,         ///< Mapped in kernel during svcCreateThread.
    MemType_JitReadOnly = 0x14,         ///< Mapped in kernel during svcMapJitMemory.
    MemType_JitWritable = 0x15,         ///< Mapped in kernel during svcMapJitMemory.
} MemoryType;

/// Memory attribute bitmasks.
typedef enum {
    MemAttr_IsBorrowed = BIT(0),     ///< Is borrowed memory.
    MemAttr_IsIpcMapped = BIT(1),    ///< Is IPC mapped (when IpcRefCount > 0).
    MemAttr_IsDeviceMapped = BIT(2), ///< Is device mapped (when DeviceRefCount > 0).
    MemAttr_IsUncached = BIT(3),     ///< Is uncached.
} MemoryAttribute;

/// Memory permission bitmasks.
typedef enum {
    Perm_None = 0,             ///< No permissions.
    Perm_R = BIT(0),           ///< Read permission.
    Perm_W = BIT(1),           ///< Write permission.
    Perm_X = BIT(2),           ///< Execute permission.
    Perm_Rw = Perm_R | Perm_W, ///< Read/write permissions.
    Perm_Rx = Perm_R | Perm_X, ///< Read/execute permissions.
    Perm_DontCare = BIT(28),   ///< Don't care
} Permission;

struct MemoryInfo {
    uint64_t base_addr;        ///< Base address.
    uint64_t size;             ///< Size.
    uint32_t memory_type;      ///< Memory type (see lower 8 bits of \ref MemoryState).
    uint32_t memory_attribute; ///< Memory attributes (see \ref MemoryAttribute).
    uint32_t permission;       ///< Memory permissions (see \ref Permission).
    uint32_t device_ref_count; ///< Device reference count.
    uint32_t ipc_ref_count;    ///< IPC reference count.
    uint32_t padding;          ///< Padding.
};

struct DebugEvent {
	enum class EventType : uint32_t {
		AttachProcess = 0,
		AttachThread = 1,
		ExitProcess = 2,
		ExitThread = 3,
		Exception = 4,
	};
	enum class ExitType : uint64_t {
		PausedThread = 0,
		RunningThread = 1,
		ExitedProcess = 2,
		TerminatedProcess = 3,
	};
	enum class ExceptionType : uint32_t {
		Trap = 0,
		InstructionAbort = 1,
		DataAbortMisc = 2,
		PcSpAlignmentFault = 3,
		DebuggerAttached = 4,
		BreakPoint = 5,
		UserBreak = 6,
		DebuggerBreak = 7,
		BadSvcId = 8,
		SError = 9,
	};

	EventType event_type;
	uint32_t flags;
	uint64_t thread_id;
	union {
		struct {
			uint64_t title_id;
			uint64_t process_id;
			char process_name[12];
			uint32_t mmu_flags;
			uint64_t user_exception_context_addr; // [5.0.0+]
		} attach_process;
		struct {
			uint64_t thread_id;
			uint64_t tls_pointer;
			uint64_t entrypoint;
		} attach_thread;
		struct {
			ExitType type;
		} exit;
		struct {
			ExceptionType exception_type;
			uint64_t fault_register;
			union {
				struct {
					uint32_t opcode;
				} undefined_instruction;
				struct {
					uint32_t is_watchpoint;
				} breakpoint;
				struct {
					uint32_t info0;
					uint64_t info1;
					uint64_t info2;
				} user_break;
				struct {
					uint32_t svc_id;
				} bad_svc_id;
			};
		} exception;
		uint8_t padding[0x80]; // not sure how large this actually needs to be, but let's be safe.
	};
};

struct ThreadContext {
	union {
		uint64_t regs[100];
		struct {
			uint64_t x[31];
			uint64_t sp, pc;
		};
	};
};

struct LoadedModuleInfo {
	uint8_t build_id[0x20];
	uint64_t base_addr;
	uint64_t size;
};

} // namespace nx
} // namespace tiwb
} // namespace twili

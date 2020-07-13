#include <shlobj.h>
#include <processthreadsapi.h>

#include "xbyak/xbyak.h"

#include "skse64/GameData.h"
#include "skse64/PluginAPI.h"

#include "skse64_common/skse_version.h"

#include "skse64_common/SafeWrite.h"
#include "skse64_common/BranchTrampoline.h"

#define DLL_VERSION_MAJOR 0
#define DLL_VERSION_MINOR 1
#define DLL_VERSION_PATCH 0
#define DLL_VERSION_STR "0.1.0"
#define DLL_NAME_STR "more-archives"

static char	returned_string_buffer[512 * 2] = {0};

RelocAddr<uintptr_t> ini_enter_offset(0xD38164);
RelocAddr<uintptr_t> ini_return_offset(0xD38196);

typedef struct ini_loading_code_s : Xbyak::CodeGenerator {
	ini_loading_code_s(void *buffer) : Xbyak::CodeGenerator(4096, buffer) {
		Xbyak::Label return_label;
		Xbyak::Label function_label;

		// lpDefault
		// mov r8, qword ptr ds : [rdi + 8]
		mov(r8, qword[rdi + 0x8]);

		// lpFilename
		// lea rax, qword ptr ds : [rbx + 8]
		lea(rax, qword[rbx + 0x8]);
		// mov qword ptr ss : [rsp + 28], rax
		mov(qword[rsp + 0x28], rax);

		// lpReturnedString
		// lea r9, qword ptr ss : [rbp + AC0] (modified)
		//lea(r9, ptr[returned_string_buffer]);
		mov(r9, (uintptr_t)&returned_string_buffer);

		// lpKeyName
		// lea rdx, qword ptr ss : [rbp - 80]
		lea(rdx, qword[rbp - 0x80]);

		// nSize
		// mov dword ptr ss : [rsp + 20], 200
		mov(dword[rsp + 0x20], sizeof(returned_string_buffer));

		// lpAppName
		// lea rcx, qword ptr ss : [rsp + 40]
		lea(rcx, qword[rsp + 0x40]);

		// GetPrivateProfileStringA call
		// call qword ptr ds : [<&GetPrivateProfileStringA>]
		call(ptr[rip + function_label]);

		// lpReturnedStringreturned by reference
		// lea rdx, qword ptr ss : [rbp + AC0] (modified)
		//lea(rdx, ptr[returned_string_buffer]);
		mov(rdx, (uintptr_t)&returned_string_buffer);

		L(function_label);
		dq((uintptr_t)&GetPrivateProfileStringA);

		// Return 
		jmp(ptr[rip + return_label]);
		L(return_label);
		dq(ini_return_offset.GetUIntPtr());
	}
} ini_loading_code_t;

extern "C" {

	bool SKSEPlugin_Load(const SKSEInterface *skse)
	{
		PluginHandle plugin = skse->GetPluginHandle();

#if _DEBUG
		// Wait for debugger (taken from SKSE)
		while (!IsDebuggerPresent()) {
			Sleep(10);
		}
		Sleep(1000 * 2);
#endif

		// Ensure instruction cache is flushed
		FlushInstructionCache(GetCurrentProcess(), NULL, NULL);

		_MESSAGE("%s.dll %s (SKSE %s)", DLL_NAME_STR, DLL_VERSION_STR, CURRENT_RELEASE_SKSE_STR);

		if (!g_localTrampoline.Create(1024 * 64, nullptr)) {
			_ERROR("could not create codegen buffer. this is fatal. skipping remainder of init process.");
			return false;
		}

		if (!g_branchTrampoline.Create(1024 * 64)) {
			_ERROR("could not create branch trampoline. this is fatal. skipping remainder of init process.");
			return false;
		}

		_MESSAGE("patching INI loading code");
		{
			try {
				void *code_buffer = g_localTrampoline.StartAlloc();
				ini_loading_code_t code(code_buffer);
				g_localTrampoline.EndAlloc(code.getCurr());
				g_branchTrampoline.Write6Branch(ini_enter_offset.GetUIntPtr(), uintptr_t(code.getCode()));
			} catch (const std::runtime_error& e) {
				_ERROR("Runtime exception: %s", e.what());
			} catch (const std::exception& e) {
				_ERROR("Exception: %s", e.what());
			} catch (...) {
				_ERROR("Unknown exception");
			}
		}
		_MESSAGE("patched INI loading code");

#if _DEBUG
		DebugBreak();
#endif

		return true;
	}

	bool SKSEPlugin_Query(const SKSEInterface *skse, PluginInfo *info)
	{
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim Special Edition\\SKSE\\" DLL_NAME_STR ".log");
		gLog.SetLogLevel(IDebugLog::kLevel_DebugMessage);
		gLog.SetPrintLevel(IDebugLog::kLevel_Error);

		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = DLL_NAME_STR;
		info->version = DLL_VERSION_MAJOR;
		
		if (skse->isEditor != 0) {
			_MESSAGE("loaded in editor, skip");
			return false;
		}

		if (skse->runtimeVersion != CURRENT_RELEASE_RUNTIME) {
			_MESSAGE("unsupported runtime version %08X", skse->runtimeVersion);
			return false;
		}

		return true;
	}

}


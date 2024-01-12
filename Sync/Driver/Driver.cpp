#include <ntifs.h>
#include <ntddk.h>
#include <IntSafe.h>
#include <ntimage.h>

#define print(fmt, ...) DbgPrintEx(0, 0, fmt, ##__VA_ARGS__)
//#define print(fmt, ...)

template <typename str_type, typename str_type_2>
__forceinline bool crt_strcmp(str_type str, str_type_2 in_str, bool two)
{
	if (!str || !in_str)
		return false;

	wchar_t c1, c2;
#define to_lower(c_char) ((c_char >= 'A' && c_char <= 'Z') ? (c_char + 32) : c_char)

	do
	{
		c1 = *str++; c2 = *in_str++;
		c1 = to_lower(c1); c2 = to_lower(c2);

		if (!c1 && (two ? !c2 : 1))
			return true;

	} while (c1 == c2);


	return false;
}

PEPROCESS GetProcess(const wchar_t* process_name)
{
	CHAR image_name[15];
	PEPROCESS sys_process = PsInitialSystemProcess;
	PEPROCESS cur_entry = sys_process;

	do
	{
		RtlCopyMemory((PVOID)(&image_name), (PVOID)((uintptr_t)cur_entry + 0x5A8), sizeof(image_name));

		if (crt_strcmp(image_name, process_name, true))
		{
			UINT32 active_threads;
			RtlCopyMemory((PVOID)&active_threads, (PVOID)((uintptr_t)cur_entry + 0x5F0), sizeof(active_threads));

			if (active_threads)
				return cur_entry;
		}

		PLIST_ENTRY list = (PLIST_ENTRY)((uintptr_t)(cur_entry)+0x448);
		cur_entry = (PEPROCESS)((uintptr_t)list->Flink - 0x448);

	} while (cur_entry != sys_process);

	return 0;
}

namespace consts {
	namespace comm {
		const wchar_t* registryKeyPath = L"\\Registry\\Machine\\SOFTWARE\\Vasie";

		const wchar_t* registryBufferName = L"Buffer";
		const wchar_t* registryPIDName = L"PID";

		enum Type {
			Read, Write, Base,
		};

		struct Request {
			UINT64 Magic;

			enum Type Type;

			UINT64 OutBuffer;
			UINT64 InBuffer;

			UINT32 TargetPID;

			SIZE_T SizeBuffer;

			UINT8 Completed;
			UINT8 Running;
		};
	}
}

namespace registry {
	template <typename T>
	NTSTATUS GetValueFromRegistry(const wchar_t* keyPath, const wchar_t* valueName, T& resultValue)
	{
		OBJECT_ATTRIBUTES objectAttributes;
		UNICODE_STRING unicodeKeyPath, unicodeValueName;
		HANDLE hKey;
		KEY_VALUE_PARTIAL_INFORMATION* keyValueInfo;
		ULONG bufferSize;
		NTSTATUS status;

		RtlInitUnicodeString(&unicodeKeyPath, keyPath);
		RtlInitUnicodeString(&unicodeValueName, valueName);

		InitializeObjectAttributes(&objectAttributes, &unicodeKeyPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

		status = ZwOpenKey(&hKey, KEY_READ, &objectAttributes);

		if (!NT_SUCCESS(status))
		{
			print("Error opening the key, status: %p\n", status);
			return status;
		}

		bufferSize = sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(T);
		keyValueInfo = (KEY_VALUE_PARTIAL_INFORMATION*)ExAllocatePool(NonPagedPool, bufferSize);

		if (!keyValueInfo)
		{
			ZwClose(hKey);
			print("Allocation failure, status: %p\n", status);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		status = ZwQueryValueKey(hKey, &unicodeValueName, KeyValuePartialInformation, keyValueInfo, bufferSize, &bufferSize);

		if (NT_SUCCESS(status))
		{
			if (keyValueInfo->DataLength == sizeof(T))
			{
				resultValue = *(T*)keyValueInfo->Data;
			}
			else
			{
				print("Incorrect registry value type or size, status: %p\n", status);
				status = STATUS_UNSUCCESSFUL;
			}
		}
		else
		{
			print("Unknown failure, status: %p\n", status);
		}

		ExFreePool(keyValueInfo);
		ZwClose(hKey);

		return status;
	}
}

EXTERN_C NTSTATUS NTAPI MmCopyVirtualMemory
(
	PEPROCESS SourceProcess,
	PVOID SourceAddress,
	PEPROCESS TargetProcess,
	PVOID TargetAddress,
	SIZE_T BufferSize,
	KPROCESSOR_MODE PreviousMode,
	PSIZE_T ReturnSize
);

EXTERN_C NTKERNELAPI PVOID PsGetProcessSectionBaseAddress
(
	__in PEPROCESS Process
);

namespace communication {
	void* Buffer = nullptr;
	UINT32 PID = 0;
	PEPROCESS ClientProcess{};

	void Set(consts::comm::Request* Request) {
		SIZE_T ReturnSize{};
		NTSTATUS Status{};

		Status = MmCopyVirtualMemory(IoGetCurrentProcess(), Request, ClientProcess, Buffer, sizeof(consts::comm::Request), 0, &ReturnSize);
		if (!NT_SUCCESS(Status))
		{
			print("MmCopyVirtualMemory failure, status: %p\n", Status);
			return;
		}
	}
	consts::comm::Request Get() {
		consts::comm::Request Request{};
		NTSTATUS Status{};
		SIZE_T ReturnSize{};

		Status = MmCopyVirtualMemory(ClientProcess, Buffer, IoGetCurrentProcess(), &Request, sizeof(consts::comm::Request), 0, &ReturnSize);
		if (!NT_SUCCESS(Status))
		{
			print("MmCopyVirtualMemory failure, status: %p\n", Status);
			return Request;
		}
		return Request;
	}
}

void Sleep(int mSecs)
{
	LARGE_INTEGER delayInterval;
	delayInterval.QuadPart = -(LONGLONG)(mSecs * 10000);

	KeDelayExecutionThread(KernelMode, FALSE, &delayInterval);
}

VOID CommThread() {
	print("CommThread entry\n");

	while (true) {
		Sleep(1);

		consts::comm::Request Request = communication::Get();

		if (Request.Magic != 0x78593765 || !Request.Running)
		{
			print("Exit\n");
			break;
		}
	
		if (Request.Completed)
			continue;
	
		PEPROCESS TargetProcess{};
		NTSTATUS Status = PsLookupProcessByProcessId((HANDLE)Request.TargetPID, &TargetProcess);
		if (!NT_SUCCESS(Status))
		{
			print("PsLookupProcessByProcessId failure, status: %p\n", Status);
			continue; //TODo add error messages
		}

		switch (Request.Type)
		{
			case consts::comm::Type::Read:
			{
				Status = MmCopyVirtualMemory(
					TargetProcess, 
					(void*)Request.InBuffer, 
					communication::ClientProcess,
					(void*)Request.OutBuffer,
					Request.SizeBuffer, 
					KernelMode,
					&Request.SizeBuffer
				);
				if (!NT_SUCCESS(Status))
				{
					print("Read failure, status: %p\n", Status);
					continue;
				}

				Request.OutBuffer = Request.SizeBuffer;
	
				break;
			}

			case consts::comm::Type::Write:
			{
				Status = MmCopyVirtualMemory(
					communication::ClientProcess,
					(void*)Request.OutBuffer,
					TargetProcess,
					(void*)Request.InBuffer,
					Request.SizeBuffer,
					KernelMode,
					&Request.SizeBuffer
				);
				if (!NT_SUCCESS(Status))
				{
					print("Write failure, status: %p\n", Status);
					continue;
				}

				Request.OutBuffer = Request.SizeBuffer;

				break;
			}

			case consts::comm::Type::Base:
			{
				Request.OutBuffer = (UINT64)PsGetProcessSectionBaseAddress(TargetProcess);
				
				break;
			}

			default:
			{

				
	
				break;
			}
		}

		Request.Completed = true;
		communication::Set(&Request);
	}

	PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS CustomDriverEntry(
	_In_ PDRIVER_OBJECT  kdmapperParam1,
	_In_ PUNICODE_STRING kdmapperParam2
)
{
	UNREFERENCED_PARAMETER(kdmapperParam1);
	UNREFERENCED_PARAMETER(kdmapperParam2);
	
	print("Driver entry\n");

	UINT64 CommBuffer = { 0 };

	if (!NT_SUCCESS(registry::GetValueFromRegistry(
		consts::comm::registryKeyPath,
		consts::comm::registryBufferName,
		CommBuffer
	)))
	{
		return 1;
	}
	communication::Buffer = (void*)CommBuffer;
	if (!communication::Buffer)
		return 1;

	print("Buffer: %p\n", communication::Buffer);

	UINT32 PID = { 0 };

	if (!NT_SUCCESS(registry::GetValueFromRegistry(
		consts::comm::registryKeyPath,
		consts::comm::registryPIDName,
		PID
	)))
	{
		return 1;
	}
	communication::PID = PID;
	if (!communication::PID)
		return 1;

	print("PID: %i\n", communication::PID);

	NTSTATUS Status = PsLookupProcessByProcessId((HANDLE)PID, &communication::ClientProcess);
	if (!NT_SUCCESS(Status))
	{
		print("PsLookupProcessByProcessId failure, status: %p\n", Status);
		return STATUS_FAILED_DRIVER_ENTRY;
	}

	HANDLE ThreadHandle{};
	PsCreateSystemThread(&ThreadHandle, 0, 0, 0, 0, (PKSTART_ROUTINE)CommThread, 0);

	return STATUS_SUCCESS;
}
#pragma once

class Driver {
    inline static void* CommBuffer = nullptr;
    inline static std::uint32_t TargetPID;

    enum Type {
        Read, Write, Base
    };

    struct Request {
        std::uint64_t Magic;

        enum Type Type;

        std::uint64_t OutBuffer;
        std::uint64_t InBuffer;

        std::uint32_t TargetPID;

        std::size_t SizeBuffer;

        std::uint8_t Completed;
        std::uint8_t Running;
    };

public:
    static bool InitComm(std::uint32_t TargetProcessID) {
        CommBuffer = VirtualAlloc(
            nullptr,
            sizeof(Request),
            MEM_COMMIT,
            PAGE_READWRITE
        );
        if (!CommBuffer) {
            return false;
        }

        RtlZeroMemory(CommBuffer, sizeof(Request));

        if (!CreateValueInRegistry(
            L"SOFTWARE\\Vasie",
            L"Buffer",
            reinterpret_cast<std::uint64_t>(CommBuffer)
        )) {
            return false;
        }

        if (!Create32BitValueInRegistry(
            L"SOFTWARE\\Vasie",
            L"PID",
            GetCurrentProcessId()
        )) {
            return false;
        }

        TargetPID = TargetProcessID;

        Request Req{};

        std::memset(&Req, 0, sizeof(Request));

        Req.Magic = 0x78593765;
        Req.Running = true;

        std::memcpy(CommBuffer, &Req, sizeof(Request));

        system("Mapper.exe Driver.sys");

        return true;
    }
    static std::uint64_t GetBase() {
        Request Req{};

        std::memset(&Req, 0, sizeof(Request));

        Req.Magic = 0x78593765;
        Req.Type = Type::Base;
        Req.TargetPID = TargetPID;
        Req.Running = true;
        Req.Completed = false;

        std::memcpy(CommBuffer, &Req, sizeof(Request));

        while (true)
        {
            std::memcpy(&Req, CommBuffer, sizeof(Request));

            if (Req.Completed)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return Req.OutBuffer;
    }
    template<typename T>
    static T ReadMem(std::uint64_t Address, std::size_t Size = sizeof(T)) {
        T Buffer{};

        Request Req{};

        std::memset(&Req, 0, sizeof(Request));

        Req.Magic = 0x78593765;
        Req.Type = Type::Read;

        Req.TargetPID = TargetPID;

        Req.InBuffer = Address;
        Req.OutBuffer = (std::uint64_t)&Buffer;
        Req.SizeBuffer = Size;

        Req.Running = true;
        Req.Completed = false;

        std::memcpy(CommBuffer, &Req, sizeof(Request));

        while (true)
        {
            std::memcpy(&Req, CommBuffer, sizeof(Request));

            if (Req.Completed)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return Buffer;
    }
    template<typename T>
    static bool WriteMem(std::uint64_t Address, T Buffer, std::size_t Size = sizeof(T)) {
        Request Req{};

        std::memset(&Req, 0, sizeof(Request));

        Req.Magic = 0x78593765;
        Req.Type = Type::Write;

        Req.TargetPID = TargetPID;

        Req.InBuffer = Address;
        Req.OutBuffer = (std::uint64_t)&Buffer;
        Req.SizeBuffer = Size;

        Req.Running = true;
        Req.Completed = false;

        std::memcpy(CommBuffer, &Req, sizeof(Request));

        while (true)
        {
            std::memcpy(&Req, CommBuffer, sizeof(Request));

            if (Req.Completed)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return true;
    }
};
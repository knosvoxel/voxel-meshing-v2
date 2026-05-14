#pragma once

#include <cstdint>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <psapi.h>
#pragma comment(lib, "psapi.lib")

#include <dxgi1_4.h>
#pragma comment(lib, "dxgi.lib")

struct MemorySnapshot
{
    uint64_t ramWorkingSetBytes = 0;
    uint64_t ramCommittedBytes = 0;
    uint64_t vramUsedBytes = 0;
    uint64_t vramBudgetBytes = 0;

    double ramWorkingSetMB()  const { return ramWorkingSetBytes / (1024.0 * 1024.0); }
    double ramCommittedMB()   const { return ramCommittedBytes / (1024.0 * 1024.0); }
    double vramUsedMB()       const { return vramUsedBytes / (1024.0 * 1024.0); }
    double vramBudgetMB()     const { return vramBudgetBytes / (1024.0 * 1024.0); }

    static std::string csvHeader()
    {
        return "ram_working_set_mb,ram_committed_mb,vram_used_mb,vram_budget_mb";
    }

    std::string toCsvRow() const
    {
        return std::to_string(ramWorkingSetMB()) + "," +
            std::to_string(ramCommittedMB()) + "," +
            std::to_string(vramUsedMB()) + "," +
            std::to_string(vramBudgetMB());
    }
};

inline IDXGIAdapter3* createDXGIAdapter()
{
    IDXGIFactory4* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&factory)))
        return nullptr;

    IDXGIAdapter1* adapter1 = nullptr;
    factory->EnumAdapters1(0, &adapter1);  // index 0 = primary GPU
    factory->Release();

    if (!adapter1) return nullptr;

    IDXGIAdapter3* adapter3 = nullptr;
    adapter1->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&adapter3);
    adapter1->Release();

    return adapter3;
}

inline MemorySnapshot queryMemory(IDXGIAdapter3* dxgiAdapter)
{
    MemorySnapshot snap;

    // RAM
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
        sizeof(pmc)))
    {
        snap.ramWorkingSetBytes = pmc.WorkingSetSize;
        snap.ramCommittedBytes = pmc.PrivateUsage;
    }

    // VRAM
    if (dxgiAdapter)
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO info{};
        if (SUCCEEDED(dxgiAdapter->QueryVideoMemoryInfo(
            0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
        {
            snap.vramUsedBytes = info.CurrentUsage;
            snap.vramBudgetBytes = info.Budget;
        }
    }

    return snap;
}
// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "desk.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hinst = hModule;
        break;
    case DLL_PROCESS_DETACH:
        IsolationAwareCleanup();
        break;
    }
    return TRUE;
}


/**
 * @file main.cpp
 * @brief Application entry point.
 *
 * Creates the Game instance and runs the message loop. Key-down events
 * are forwarded to the StateStack via Game's MsgProc override so state
 * transitions (Title → Menu → Game ↔ Pause) are event-driven.
 */

#include "Game.hpp"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    try
    {
        Game theApp(hInstance);
        if (!theApp.Initialize())
            return 0;
        return theApp.Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace IndraApcInterop
{

constexpr const char *kPluginName = "Indra APC";
constexpr const char *kDisplayName = "Indra APC Overlay";
constexpr const char *kDrawCommand = ".indra on";
constexpr const char *kUndrawCommand = ".indra off";

inline void SendKeyInput(WORD vk, bool keyUp)
{
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    if (keyUp) input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(input));
}

inline void SendText(const char *text)
{
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); p && *p; ++p)
    {
        SHORT key = VkKeyScanA(static_cast<char>(*p));
        if (key == -1) continue;

        BYTE vk = LOBYTE(key);
        BYTE shift = HIBYTE(key);

        if (shift & 1) SendKeyInput(VK_SHIFT, false);
        if (shift & 2) SendKeyInput(VK_CONTROL, false);
        if (shift & 4) SendKeyInput(VK_MENU, false);

        SendKeyInput(vk, false);
        SendKeyInput(vk, true);

        if (shift & 4) SendKeyInput(VK_MENU, true);
        if (shift & 2) SendKeyInput(VK_CONTROL, true);
        if (shift & 1) SendKeyInput(VK_SHIFT, true);
    }
}

inline bool SendEuroScopeCommand(const char *command)
{
    HWND window = GetActiveWindow();
    if (!window) window = GetForegroundWindow();
    if (!window) return false;

    SetForegroundWindow(window);
    SendText(command);
    SendKeyInput(VK_RETURN, false);
    SendKeyInput(VK_RETURN, true);
    return true;
}

inline bool Draw()
{
    return SendEuroScopeCommand(kDrawCommand);
}

template <typename Ignored>
inline bool Draw(Ignored *)
{
    return Draw();
}

inline bool Undraw()
{
    return SendEuroScopeCommand(kUndrawCommand);
}

template <typename Ignored>
inline bool Undraw(Ignored *)
{
    return Undraw();
}

inline bool SetDrawn(bool drawn)
{
    return drawn ? Draw() : Undraw();
}

template <typename Ignored>
inline bool SetDrawn(Ignored *, bool drawn)
{
    return SetDrawn(drawn);
}

} // namespace IndraApcInterop

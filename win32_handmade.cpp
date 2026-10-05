/*
  TODO(gaurang): THIS IS NOT A FINAL PLATFORM LAYER!!!

  - Saved game locations
  - Getting a handle to our own executable file
  - Asset loading path
  - Threading (launch a thread)
  - Raw Input (support for multiple keyboards)
  - ClipCursor() (for multimonitor support)
  - Fullscreen support
  - WM_SETCURSOR (control cursor visibility)
  - QueryCancelAutoplay
  - WM_ACTIVATEAPP (for when we are not the active application)
  - Blit speed improvements (BitBlt)
  - Hardware Acceleration (OpenGL or Direct3D or both?)
  - GetKeyboardLayout (international WASD support)

  Just a partial list of stuff!!
*/

#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "winmm")

#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>
#include <xinput.h>
#include <mmeapi.h>
#include <dsound.h>

#include "handmade_platform.h"
#include "handmade_intrinsics.h"

function
PLATFORM_ALLOCATE_MEMORY(Win32AllocateMemory)
{
    u8 *Result = (u8 *)VirtualAlloc(0, Size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    Assert(Result);
    return(Result);
}

function
PLATFORM_DEALLOCATE_MEMORY(Win32DeallocateMemory)
{
    VirtualFree(Memory, 0, MEM_RELEASE);
}

function
PLATFORM_FILE_ERROR(Win32FileError)
{
#if HANDMADE_INTERNAL
    OutputDebugStringA("WIN32 FILE ERROR: ");
    OutputDebugStringA(Message);
    OutputDebugStringA("\n");
#endif
    Handle->HasErrors = true;
}

global char *Files[2] = { "test2.hha", "test1.hha" };
function
PLATFORM_GET_ALL_FILES_OF_TYPE_BEGIN(Win32GetAllFilesOfTypeBegin)
{
    platform_file_group Result = {};

    Result.FileCount = 2;

    return(Result);
}

function
PLATFORM_GET_ALL_FILES_OF_TYPE_END(Win32GetAllFilesOfTypeEnd)
{
}

function
PLATFORM_OPEN_FILE(Win32OpenFile)
{
    platform_file_handle *Result = (platform_file_handle *)
        VirtualAlloc(0, sizeof(platform_file_handle), MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);;

    HANDLE Handle = CreateFileA(Files[FileIndex],
                                GENERIC_READ,
                                FILE_SHARE_READ,
                                0,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                0);
    if(Handle != INVALID_HANDLE_VALUE)
    {
        Result->Platform = (void *)Handle;
    }
    else
    {
        Win32FileError(Result, "Create file handle failed.");
    }

    return(Result);
}

function
PLATFORM_READ_DATA_FROM_FILE(Win32ReadDataFromFile)
{
    HANDLE Handle = (HANDLE)Source->Platform;
    if(PlatformNoFileErrors(Source))
    {
        OVERLAPPED Overlapped = {};
        Overlapped.Offset = SafeTruncateToUInt32(Offset);
        DWORD BytesRead = 0;
        DWORD BytesToRead = SafeTruncateToUInt32(Size);
        if(ReadFile(Handle, Dest, BytesToRead, &BytesRead, &Overlapped) &&
           (BytesToRead == BytesRead))
        {
            // NOTE(gaurang): File read succeeded!
        }
        else
        {
            Win32FileError(Source, "Read file failed.");
        }
    }
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN  pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

#define XINPUT_GET_STATE(Name) DWORD Name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef XINPUT_GET_STATE(xinput_get_state);
XINPUT_GET_STATE(XInputGetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global xinput_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define XINPUT_SET_STATE(name) DWORD name(DWORD , XINPUT_VIBRATION *)
typedef XINPUT_SET_STATE(xinput_set_state);
XINPUT_SET_STATE(XInputSetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global xinput_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

struct platform_work_queue_entry
{
    platform_work_queue_callback *Callback;
    void *Data;
};

struct platform_work_queue
{
    u32 volatile EntryCompletionGoal;
    u32 volatile EntryCompletionCount;
    s32 volatile EntryIndex;
    s32 volatile EntryCount;
    HANDLE SemaphoreHandle;

    platform_work_queue_entry Entries[256];
};

// TODO(gaurang): These are globals for now.
global b32 GlobalRunning;
global WINDOWPLACEMENT GlobalWindowPlacement = { sizeof(GlobalWindowPlacement) };
global game_button GlobalToggleFullscreenButton = {};
platform_work_queue HighPriorityQueue = {};
platform_work_queue LowPriorityQueue = {};

function void
Win32ToggleFullscreen(HWND Window)
{
    // NOTE(gaurang): This follows Raymond Chen's prescription for
    // fullscreen toggling, see:
    // https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353

    DWORD Style = GetWindowLongA(Window, GWL_STYLE);
    if (Style & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
        if (GetWindowPlacement(Window, &GlobalWindowPlacement) &&
            GetMonitorInfo(MonitorFromWindow(Window,
                                             MONITOR_DEFAULTTOPRIMARY), &MonitorInfo))
        {
            SetWindowLong(Window, GWL_STYLE,
                          Style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(Window, HWND_TOP,
                         MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                         MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                         MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowLong(Window, GWL_STYLE,
                      Style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(Window, &GlobalWindowPlacement);
        SetWindowPos(Window, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

#if HANDMADE_INTERNAL
function void
HandleDebugCycleCounters(game_memory *Memory)
{
    OutputDebugStringA("DEBUG CYCLE COUNTS:\n");
    for(s32 Index = 0;
        Index < DebugCycleCounter_Count;
        ++Index)
    {
        debug_cycle_counter *Counter = Memory->DebugCycleCounters + Index;

        if(Counter->HitCount)
        {
            char Buffer[256];
            snprintf(Buffer, sizeof(Buffer), "%d: %I64ucy %uh %I64ucy/h\n",
                     Index,
                     Counter->CycleCount,
                     Counter->HitCount,
                     Counter->CycleCount / Counter->HitCount);
            OutputDebugStringA(Buffer);

            Counter->HitCount = 0;
            Counter->CycleCount = 0;
        }
    }
}
#endif

function void
Win32AddEntry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data)
{
    platform_work_queue_entry *Entry = Queue->Entries + Queue->EntryCount;
    Entry->Callback = Callback;
    Entry->Data = Data;

    CompletePastWritesBeforeFutureWrites;

    Queue->EntryCount = (Queue->EntryCount + 1) % ArrayCount(Queue->Entries);
    Queue->EntryCompletionGoal++;

    ReleaseSemaphore(Queue->SemaphoreHandle, 1, 0);
}

function b32
PlatformCompleteNextWorkEntry(platform_work_queue *Queue)
{
    b32 ShouldSleep = false;

    s32 Index = Queue->EntryIndex;
    s32 NextIndex = (Index + 1) % ArrayCount(Queue->Entries);
    if(Index != Queue->EntryCount)
    {
        if(InterlockedCompareExchange((LONG volatile *)&Queue->EntryIndex, NextIndex, Index) == Index)
        {
            platform_work_queue_entry *Entry = Queue->Entries + Index;
            Entry->Callback(Entry->Data);

            InterlockedIncrement((LONG volatile *)&Queue->EntryCompletionCount);
        }
    }
    else
    {
        ShouldSleep = true;
    }

    return(ShouldSleep);
}

function void
Win32CompleteAllWork(platform_work_queue *Queue)
{
    // TODO(gaurang): This fails if EntryCompletionCount overflow!
    while(Queue->EntryCompletionGoal != Queue->EntryCompletionCount)
    {
        PlatformCompleteNextWorkEntry(Queue);
    }

    Assert(Queue->EntryIndex == Queue->EntryCount);
}

typedef GAME_UPDATE_AND_RENDER(game_update_and_render);
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub)
{
}

typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesStub)
{
}

struct win32_game_code
{
    HMODULE Library;
    u64 Timestamp;
    game_update_and_render *UpdateAndRender;
    game_get_sound_samples *GetSoundSamples;
};

function void
Win32UnloadGameCode(win32_game_code *Game)
{
    if(Game->Library)
    {
        if(!FreeLibrary(Game->Library))
        {
            // TODO(gaurang): Diagnostics.
            InvalidCodePath;
        }
    }
    Game->Library = 0;
    Game->Timestamp = 0;
    Game->UpdateAndRender = GameUpdateAndRenderStub;
    Game->GetSoundSamples = GameGetSoundSamplesStub;
}

function b32
Win32HotReloadGameCode(win32_game_code *Game)
{
    b32 ExecutableReloaded = false;

    char *FileName = "handmade.dll";
    char *WorkingCopyName = "handmade_loaded.dll";

    WIN32_FILE_ATTRIBUTE_DATA FileAttributeData = { 0 };
    if(GetFileAttributesExA(FileName, GetFileExInfoStandard, &FileAttributeData))
    {
        ULARGE_INTEGER FileTime = {};
        FileTime.HighPart = FileAttributeData.ftLastWriteTime.dwHighDateTime;
        FileTime.LowPart = FileAttributeData.ftLastWriteTime.dwLowDateTime;
        u64 Timestamp = FileTime.QuadPart;

        if(Game->Timestamp < Timestamp)
        {
            Win32CompleteAllWork(&HighPriorityQueue);
            Win32CompleteAllWork(&LowPriorityQueue);
            Win32UnloadGameCode(Game);
            if(CopyFileA(FileName, WorkingCopyName, false))
            {
                Game->Timestamp = Timestamp;
                Game->Library = LoadLibraryA(WorkingCopyName);
                if(Game->Library)
                {
                    Game->UpdateAndRender = (game_update_and_render *)GetProcAddress(Game->Library, "GameUpdateAndRender");
                    if(!Game->UpdateAndRender)
                    {
                        // TODO(gaurang): Diagnostics.
                        InvalidCodePath;
                    }

                    Game->GetSoundSamples = (game_get_sound_samples *)GetProcAddress(Game->Library, "GameGetSoundSamples");
                    if(!Game->GetSoundSamples)
                    {
                        // TODO(gaurang): Diagnostics.
                        InvalidCodePath;
                    }

                    ExecutableReloaded = true;
                }
                else
                {
                    // TODO(gaurang): Diagnostics.
                    InvalidCodePath;
                }
            }
            else
            {
                // TODO(gaurang): Diagnostics.
                InvalidCodePath;
            }
        }
    }
    else
    {
        // TODO(gaurang): Diagnostics.
        InvalidCodePath;
    }

    if(!Game->UpdateAndRender)
    {
        Game->UpdateAndRender = GameUpdateAndRenderStub;
        InvalidCodePath;
    }
    if(!Game->GetSoundSamples)
    {
        Game->GetSoundSamples = GameGetSoundSamplesStub;
        InvalidCodePath;
    }

    return(ExecutableReloaded);
}

struct win32_audio
{
    // NOTE(gaurang): 16-bit audio, dual channel.
    DWORD SampleRate;
    DWORD BytesPerSample;
    DWORD BufferSize;
    DWORD FilledCursor;
    DWORD LatencyBytes;
    LPDIRECTSOUNDBUFFER DirectSoundBuffer;
};

global win32_audio
Win32InitDSound(HWND Window, DWORD SampleRate, DWORD LatencyBytes)
{
    win32_audio Result = { 0 };

    HMODULE Library = LoadLibraryA("dsound.dll");
    if(Library)
    {
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(Library, "DirectSoundCreate");
        if(DirectSoundCreate)
        {
            LPDIRECTSOUND DirectSound;
            if(DirectSoundCreate(0, &DirectSound, 0) == DS_OK)
            {
                if(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY) == DS_OK)
                {
                    DSBUFFERDESC BufferDescription = { 0 };
                    BufferDescription.dwSize = sizeof(BufferDescription);
                    BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                    // BufferDescription.dwBufferBytes = ;
                    // BufferDescription.lpwfxFormat;
                    BufferDescription.guid3DAlgorithm = DS3DALG_DEFAULT;

                    LPDIRECTSOUNDBUFFER PrimaryBuffer;
                    if(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0) == DS_OK)
                    {
                        WAVEFORMATEX WaveFormat = { 0 };
                        WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
                        WaveFormat.nChannels = 2;
                        WaveFormat.nSamplesPerSec = SampleRate;
                        WaveFormat.wBitsPerSample = 16;
                        WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample)/8;
                        WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;

                        // TODO(gaurang): Should we remove DSBCAPS_GLOBALFOCUS in release build.
                        BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_GLOBALFOCUS;
                        BufferDescription.dwBufferBytes = LatencyBytes*100;
                        BufferDescription.lpwfxFormat = &WaveFormat;

                        LPDIRECTSOUNDBUFFER SecondaryBuffer;
                        if(DirectSound->CreateSoundBuffer(&BufferDescription, &SecondaryBuffer, 0) == DS_OK)
                        {
                            if(SecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING) == DS_OK)
                            {
                                // NOTE(gaurang): DSound is initialized.
                                Result.SampleRate = WaveFormat.nSamplesPerSec;
                                Result.BytesPerSample = WaveFormat.nBlockAlign;
                                Result.LatencyBytes = LatencyBytes;
                                Result.BufferSize = BufferDescription.dwBufferBytes;
                                Result.DirectSoundBuffer = SecondaryBuffer;
                            }
                            else
                            {
                                // TODO(gaurang): Diagnostics.
                                InvalidCodePath;
                            }
                        }
                        else
                        {
                            // TODO(gaurang): Diagnostics.
                            InvalidCodePath;
                        }
                    }
                    else
                    {
                        // TODO(gaurang): Diagnostics.
                        InvalidCodePath;
                    }
                }
                else
                {
                    // TODO(gaurang): Diagnostics.
                    InvalidCodePath;
                }
            }
            else
            {
                // TODO(gaurang): Diagnostics.
                InvalidCodePath;
            }
        }
        else
        {
            // TODO(gaurang): Diagnostics.
            InvalidCodePath;
        }
    }
    else
    {
        // TODO(gaurang): Diagnostics.
        InvalidCodePath;
    }

    return(Result);
}

function r32
Win32ProcessXInputStick(SHORT Value, SHORT Deadzone)
{
    r32 Result = 0;

    if(Value < -Deadzone)
    {
        r32 ScalingFactor = (r32)(32768 - Deadzone);
        Result = (r32)(Value + Deadzone) / ScalingFactor;
    }
    else if(Value > Deadzone)
    {
        r32 ScalingFactor = (r32)(32767 - Deadzone);
        Result = (r32)(Value - Deadzone) / ScalingFactor;
    }

    return(Result);
}

function void
Win32UpdateGameButton(game_button *Button, b32 Down)
{
    Button->Pressed = !Button->Down && Down;
    Button->Released = Button->Down && !Down;
    Button->Down = Down;
}

function debug_read_file
DEBUGWin32ReadEntireFile(char *FileName)
{
    debug_read_file Result = {};

    HANDLE Handle = CreateFileA(FileName,
                                GENERIC_READ,
                                FILE_SHARE_READ,
                                0,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                0);
    if(Handle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize = {};
        if(GetFileSizeEx(Handle, &FileSize))
        {
            if(!FileSize.HighPart)
            {
                void *FileMemory = VirtualAlloc(0, FileSize.LowPart,
                                                MEM_COMMIT|MEM_RESERVE,
                                                PAGE_READWRITE);
                if(FileMemory)
                {
                    DWORD BytesRead = 0;
                    if(ReadFile(Handle, FileMemory, FileSize.LowPart, &BytesRead, 0) &&
                       (FileSize.LowPart == BytesRead))
                    {
                        Result.Count = FileSize.LowPart;
                        Result.Data = (u8 *)FileMemory;
                    }
                    else
                    {
                        // TODO(gaurang): Diagnostics.
                        if(VirtualFree(FileMemory, 0, MEM_RELEASE))
                        {
                            // NOTE(gaurang): Memory freed successfully.
                        }
                        else
                        {
                            // TODO(gaurang): Diagnostics
                            InvalidCodePath;
                        }

                        InvalidCodePath;
                    }
                }
                else
                {
                    // TODO(gaurang): Diagnostics.
                    InvalidCodePath;
                }
            }
            else
            {
                // TODO(gaurang): Diagnostics.
                InvalidCodePath;
            }
        }
        else
        {
            // TODO(gaurang): Diagnostics.
            InvalidCodePath;
        }

        if(CloseHandle(Handle))
        {
            // NOTE(gaurang): File handle closed successfully.
        }
        else
        {
            // TODO(gaurang): Diagnostics.
            InvalidCodePath;
        }
    }
    else
    {
        // TODO(gaurang): Diagnostics.
        InvalidCodePath;
    }

    return(Result);
}

function void
DEBUGWin32WriteEntireFile(char *FileName, void *Data, u32 Count)
{
    HANDLE Handle = CreateFileA(FileName,
                                GENERIC_WRITE,
                                0,
                                0,
                                OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                0);
    if(Handle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten = 0;
        if(WriteFile(Handle, Data, Count, &BytesWritten, 0) &&
           (BytesWritten == Count))
        {
            // NOTE(gaurang): File written successfully.
        }
        else
        {
            // TODO(gaurang): Diagnostics.
            InvalidCodePath;
        }

        if(CloseHandle(Handle))
        {
            // NOTE(gaurang): File handle closed successfully.
        }
        else
        {
            // TODO(gaurang): Diagnostics.
            InvalidCodePath;
        }
    }
    else
    {
        // TODO(gaurang): Diagnostics.
        InvalidCodePath;
    }
}

function void
DEBUGWin32FreeFileMemory(debug_read_file *File)
{
    if(VirtualFree(File->Data, 0, MEM_RELEASE))
    {
        // NOTE(gaurang): Memory freed successfully.
    }
    else
    {
        // TODO(gaurang): Diagnostics
        InvalidCodePath;
    }
    File->Count = 0;
    File->Data = 0;
}

function void
Win32InitializeXInput(void)
{
    HMODULE Library = LoadLibraryA("xinput1_4.dll");
    if(Library)
    {
        XInputGetState = (xinput_get_state *)GetProcAddress(Library, "XInputGetState");
        if(!XInputGetState)
        {
            // TODO(gaurang): Diagnostics.
            XInputGetState = XInputGetStateStub;

            InvalidCodePath;
        }

        XInputSetState = (xinput_set_state *)GetProcAddress(Library, "XInputSetState");
        if(!XInputSetState)
        {
            // TODO(gaurang): Diagnostics.
            XInputSetState = XInputSetStateStub;

            InvalidCodePath;
        }
    }
    else
    {
        // TODO(gaurang): Diagnostics.
        XInputGetState = XInputGetStateStub;
        XInputSetState = XInputSetStateStub;

        InvalidCodePath;
    }
}

function LRESULT CALLBACK
Win32WindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_CLOSE:
        {
            // TODO(gaurang): Handle this with a message to the user?
            GlobalRunning = false;
        } break;
        case WM_DESTROY:
        {
            // TODO(gaurang): Handle this as an error - recreate window?
            GlobalRunning = false;
        } break;
        case WM_PAINT:
        {
            if(ValidateRect(Window, 0))
            {
                // NOTE(gaurang): The entire client area is validated successfully.
            }
            else
            {
                // TODO(gaurang): Diagnostics.
                InvalidCodePath;
            }
        } break;
        default:
        {
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        } break;
    }

    return(Result);
}

//
//
//

function DWORD WINAPI
Win32ThreadProc(LPVOID Parameter)
{
    platform_work_queue *Queue = (platform_work_queue *)Parameter;
    for(;;)
    {
        b32 ShouldSleep = PlatformCompleteNextWorkEntry(Queue);
        if(ShouldSleep)
        {
            WaitForSingleObject(Queue->SemaphoreHandle, INFINITE);
        }
    }
}

function void
Win32MakeQueue(platform_work_queue *Queue, s32 ThreadCount)
{
    s32 InitialCount = 0;
    Queue->SemaphoreHandle = CreateSemaphoreA(0, 0, ThreadCount, 0);

    for(s32 ThreadIndex = 0;
        ThreadIndex < ThreadCount;
        ++ThreadIndex)
    {
        HANDLE ThreadHandle = CreateThread(0, 0, Win32ThreadProc, (platform_work_queue *)Queue, 0, 0);
        CloseHandle(ThreadHandle);  // NOTE(gaurang): This doesn't kill the thread, it just release the handle.
    }
}

int WINAPI
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR CommandLine, int ShowCode)
{
    Win32MakeQueue(&HighPriorityQueue, 2);
    Win32MakeQueue(&LowPriorityQueue, 1);

    Win32InitializeXInput();

    if(timeBeginPeriod(1) == TIMERR_NOCANDO)
    {
        // TODO(gaurang): Diagnostics.
        InvalidCodePath;
    }

    WNDCLASSEXA WindowClass = {};
    WindowClass.cbSize = sizeof(WindowClass);
    WindowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32WindowCallback;
    WindowClass.hInstance = Instance;
    // TODO(gaurang): WindowClass.hIcon = ;
    WindowClass.hCursor = LoadCursorA(0, IDC_ARROW); // TODO(gaurang): Handle this correctly.
    WindowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";
    // TODO(gaurang): WindowClass.hIconSm = ;
    if(RegisterClassExA(&WindowClass))
    {
        DWORD WindowStyle = WS_OVERLAPPEDWINDOW|WS_VISIBLE;

        win32_game_code Game = { 0 };

        game_video GameVideo = {};
        GameVideo.Width = 540*2;
        GameVideo.Height = 540*2;
        GameVideo.BytesPerPixel = 4;
        GameVideo.Pitch = GameVideo.BytesPerPixel * GameVideo.Width;
        Assert(GameVideo.Pitch >= 0);
        memory_index GameVideoMemorySize = (memory_index)GameVideo.Height*(memory_index)GameVideo.Pitch;
        GameVideo.Memory = (u8 *)VirtualAlloc(0, GameVideoMemorySize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);

        BITMAPINFO BitmapInfo = {};
        BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
        BitmapInfo.bmiHeader.biWidth = GameVideo.Width;
        BitmapInfo.bmiHeader.biHeight = GameVideo.Height;
        BitmapInfo.bmiHeader.biPlanes = 1;
        BitmapInfo.bmiHeader.biBitCount = SafeTruncateToS16(GameVideo.BytesPerPixel)*8;
        BitmapInfo.bmiHeader.biCompression = BI_RGB;

        RECT WindowRect = {};
        WindowRect.left = 0;
        WindowRect.top = 0;
        WindowRect.right = GameVideo.Width;
        WindowRect.bottom = GameVideo.Height;
        if(!AdjustWindowRect(&WindowRect, WindowStyle, 0))
        {
            // TODO(gaurang): Diagnostics.
            InvalidCodePath;
        }
        WindowRect.right -= WindowRect.left;
        WindowRect.bottom -= WindowRect.top;
        WindowRect.left = WindowRect.top = 0;

        HWND Window = CreateWindowA(WindowClass.lpszClassName, "Handmade Hero", WindowStyle, WindowRect.left, WindowRect.top, WindowRect.right, WindowRect.bottom, 0, 0, Instance, 0);
        if(Window)
        {
            HDC DeviceContext = GetDC(Window);
            if(DeviceContext)
            {
                int MonitorRefreshRate = GetDeviceCaps(DeviceContext, VREFRESH);
                if(MonitorRefreshRate < 60)
                {
                    MonitorRefreshRate = 60;

                    // TODO(gaurang): Diagnostics.
                    InvalidCodePath;
                }
                // MonitorRefreshRate /= 2;

                //
                // STUDY: IMPORTANT: TODO(gaurang): Write a descriptive
                // comment and test it's validity.
                //
                // Safety Bytes: 5760
                // Granularity: 3840
                //
                win32_audio Win32Audio = Win32InitDSound(Window, 48000, 16384);

                game_memory GameMemory = {};
                GameMemory.PermanentStorageSize = Megabytes(64);
                GameMemory.TransientStorageSize = Gigabytes(8);
                memory_index GameMemorySize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
                LPVOID BaseAddress = 0;
#if HANDMADE_INTERNAL
                BaseAddress = (LPVOID)Terabytes(1);
#endif
                GameMemory.PermanentStorage = (u8 *)VirtualAlloc(BaseAddress, GameMemorySize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
                GameMemory.TransientStorage = GameMemory.PermanentStorage + GameMemory.PermanentStorageSize;

                GameMemory.HighPriorityQueue = &HighPriorityQueue;
                GameMemory.LowPriorityQueue = &LowPriorityQueue;
                GameMemory.PlatformAPI.AddEntry = Win32AddEntry;
                GameMemory.PlatformAPI.CompleteAllWork = Win32CompleteAllWork;

                GameMemory.PlatformAPI.DebugReadEntireFile = DEBUGWin32ReadEntireFile;
                GameMemory.PlatformAPI.DebugWriteEntireFile = DEBUGWin32WriteEntireFile;
                GameMemory.PlatformAPI.DebugFreeFileMemory = DEBUGWin32FreeFileMemory;

                GameMemory.PlatformAPI.GetAllFilesOfTypeBegin = Win32GetAllFilesOfTypeBegin;
                GameMemory.PlatformAPI.GetAllFilesOfTypeEnd = Win32GetAllFilesOfTypeEnd;
                GameMemory.PlatformAPI.OpenFile = Win32OpenFile;
                GameMemory.PlatformAPI.ReadDataFromFile = Win32ReadDataFromFile;
                GameMemory.PlatformAPI.FileError = Win32FileError;

                GameMemory.PlatformAPI.AllocateMemory = Win32AllocateMemory;
                GameMemory.PlatformAPI.DeallocateMemory = Win32DeallocateMemory;

                game_audio GameAudio = {};
                GameAudio.SampleRate = Win32Audio.SampleRate;
                s32 MaxPossibleOverrun = 2*8*sizeof(s16); // TODO(gaurang): Remove this?
                memory_index SampleMemorySize = Win32Audio.LatencyBytes + MaxPossibleOverrun;
                GameAudio.Samples = (s16 *)VirtualAlloc(0,
                                                        SampleMemorySize,
                                                        MEM_COMMIT|MEM_RESERVE,
                                                        PAGE_READWRITE);

                game_input GameInput = {};
                GameInput.dtForFrame = 1.0f/(r32)MonitorRefreshRate;
                GameInput.ControllerCount = XUSER_MAX_COUNT;
                game_controller Controllers[XUSER_MAX_COUNT];
                GameInput.Controllers = Controllers;

                u64 OldCycleCount = 0;
                u64 NewCycleCount = 0;
                LARGE_INTEGER NewPerformanceCount = { 0 };
                LARGE_INTEGER OldPerformanceCount = { 0 };
                LARGE_INTEGER PerformanceFrequency = { 0 };
                QueryPerformanceFrequency(&PerformanceFrequency);

                GlobalRunning = true;
                while(GlobalRunning)
                {
                    MSG Message;
                    while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
                    {
                        TranslateMessage(&Message);
                        DispatchMessageA(&Message);
                    }

                    Win32HotReloadGameCode(&Game);

                    // TODO(gaurang): Should we poll this more frequently.
                    for(DWORD ControllerIndex = 0;
                        ControllerIndex < XUSER_MAX_COUNT;
                        ++ControllerIndex)
                    {
                        XINPUT_STATE ControllerState = {};
                        if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                        {
                            // NOTE(gaurang): This controller is plugged in.
                            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                            game_controller *Controller = GameInput.Controllers + ControllerIndex;

                            Win32UpdateGameButton(&Controller->MoveUp, Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                            Win32UpdateGameButton(&Controller->MoveDown, Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                            Win32UpdateGameButton(&Controller->MoveLeft, Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                            Win32UpdateGameButton(&Controller->MoveRight, Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

                            Win32UpdateGameButton(&Controller->ActionUp, Pad->wButtons & XINPUT_GAMEPAD_Y);
                            Win32UpdateGameButton(&Controller->ActionDown, Pad->wButtons & XINPUT_GAMEPAD_A);
                            Win32UpdateGameButton(&Controller->ActionLeft, Pad->wButtons & XINPUT_GAMEPAD_X);
                            Win32UpdateGameButton(&Controller->ActionRight, Pad->wButtons & XINPUT_GAMEPAD_B);

                            Win32UpdateGameButton(&Controller->LeftShoulder, Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                            Win32UpdateGameButton(&Controller->RightShoulder, Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);

                            Win32UpdateGameButton(&Controller->Back, Pad->wButtons & XINPUT_GAMEPAD_BACK);
                            Win32UpdateGameButton(&Controller->Start, Pad->wButtons & XINPUT_GAMEPAD_START);

                            Controller->StickX = Win32ProcessXInputStick(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            Controller->StickY = Win32ProcessXInputStick(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                        }
                        else
                        {
                            // NOTE(gaurang): This controller is not available.
                        }
                    }

                    BYTE Keyboard[256];
                    if(GetKeyboardState(Keyboard))
                    {
                        game_controller *Controller = GameInput.Controllers + 1;

                        Win32UpdateGameButton(&Controller->MoveUp, Keyboard['W'] >> 7);
                        Win32UpdateGameButton(&Controller->MoveDown, Keyboard['S'] >> 7);
                        Win32UpdateGameButton(&Controller->MoveLeft, Keyboard['A'] >> 7);
                        Win32UpdateGameButton(&Controller->MoveRight, Keyboard['D'] >> 7);

                        Win32UpdateGameButton(&Controller->ActionUp, Keyboard[VK_UP] >> 7);
                        Win32UpdateGameButton(&Controller->ActionDown, Keyboard[VK_DOWN] >> 7);
                        Win32UpdateGameButton(&Controller->ActionLeft, Keyboard[VK_LEFT] >> 7);
                        Win32UpdateGameButton(&Controller->ActionRight, Keyboard[VK_RIGHT] >> 7);

                        Win32UpdateGameButton(&Controller->LeftShoulder, Keyboard['Q'] >> 7);
                        Win32UpdateGameButton(&Controller->RightShoulder, Keyboard['E'] >> 7);

                        Win32UpdateGameButton(&Controller->Back, Keyboard[VK_ESCAPE] >> 7);
                        Win32UpdateGameButton(&Controller->Start, Keyboard[VK_RETURN] >> 7);

                        POINT MouseP;
                        if(GetCursorPos(&MouseP))
                        {
                            if(ScreenToClient(Window, &MouseP))
                            {
                                GameInput.Mouse.X = MouseP.x;
                                GameInput.Mouse.Y = MouseP.y;
                            }
                            else
                            {
                                // TODO(gaurang): Diagnostics.
                                InvalidCodePath;
                            }
                        }
                        else
                        {
                            // TODO(gaurang): Diagnostics.
                            InvalidCodePath;
                        }
                        Win32UpdateGameButton(&GameInput.Mouse.Left, Keyboard[VK_LBUTTON] >> 7);
                        Win32UpdateGameButton(&GameInput.Mouse.Right, Keyboard[VK_RBUTTON] >> 7);

                        Win32UpdateGameButton(&GlobalToggleFullscreenButton, Keyboard[VK_F11] >> 7);
                        if(GlobalToggleFullscreenButton.Pressed)
                        {
                            Win32ToggleFullscreen(Window);
                        }
                    }
                    else
                    {
                        // TODO(gaurang): Diagnostics.
                        InvalidCodePath;
                    }

                    Game.UpdateAndRender(&GameMemory, &GameVideo, &GameInput);
                    HandleDebugCycleCounters(&GameMemory);

                    if(Win32Audio.DirectSoundBuffer)
                    {
                        DWORD SafeCursor = 0;
                        if(Win32Audio.DirectSoundBuffer->GetCurrentPosition(0, &SafeCursor) == DS_OK)
                        {
                            DWORD BytesToFill = 0;

                            DWORD BytesRemaining;
                            if(SafeCursor <= Win32Audio.FilledCursor)
                            {
                                BytesRemaining = Win32Audio.FilledCursor - SafeCursor;
                            }
                            else
                            {
                                BytesRemaining = Win32Audio.FilledCursor + (Win32Audio.BufferSize - SafeCursor);
                            }

                            if(BytesRemaining <= Win32Audio.LatencyBytes)
                            {
                                BytesToFill = Win32Audio.LatencyBytes - BytesRemaining;
                            }
                            else
                            {
                                BytesToFill = Win32Audio.LatencyBytes;
                                Win32Audio.FilledCursor = SafeCursor;
                            }

                            GameAudio.SampleCount = Align8(BytesToFill / Win32Audio.BytesPerSample);
                            BytesToFill = GameAudio.SampleCount*Win32Audio.BytesPerSample;
                            Game.GetSoundSamples(&GameMemory, &GameAudio);

                            if(BytesToFill)
                            {
                                VOID *Region1;
                                DWORD Region1Size;
                                VOID *Region2;
                                DWORD Region2Size;
                                if(Win32Audio.DirectSoundBuffer->Lock(Win32Audio.FilledCursor, BytesToFill,
                                                                      &Region1, &Region1Size,
                                                                      &Region2, &Region2Size,
                                                                      0) == DS_OK)
                                {
                                    s16 *SampleOut = GameAudio.Samples;
                                    {
                                        s16 *Sample = (s16 *)Region1;
                                        s32 SampleCount = Region1Size / Win32Audio.BytesPerSample;
                                        for(s32 SampleIndex = 0;
                                            SampleIndex < SampleCount;
                                            ++SampleIndex)
                                        {
                                            *Sample++ = *SampleOut++;
                                            *Sample++ = *SampleOut++;
                                        }
                                    }

                                    {
                                        s16 *Sample = (s16 *)Region2;
                                        s32 SampleCount = Region2Size / Win32Audio.BytesPerSample;
                                        for(s32 SampleIndex = 0;
                                            SampleIndex < SampleCount;
                                            ++SampleIndex)
                                        {
                                            *Sample++ = *SampleOut++;
                                            *Sample++ = *SampleOut++;
                                        }
                                    }

                                    if(Win32Audio.DirectSoundBuffer->Unlock(Region1, Region1Size,
                                                                            Region2, Region2Size) == DS_OK)
                                    {
                                        // NOTE(gaurang): DirectSoundBuffer Unlocked Successfully.
                                    }
                                    else
                                    {
                                        // TODO(gaurang): Diagnostics.
                                        InvalidCodePath;
                                    }
                                }
                                else
                                {
                                    // TODO(gaurang): Diagnostics.
                                    InvalidCodePath;
                                }
                            }

                            Win32Audio.FilledCursor += BytesToFill;
                            Win32Audio.FilledCursor %= Win32Audio.BufferSize;
                        }
                        else
                        {
                            // TODO(gaurang): Diagnostics.
                            InvalidCodePath;
                        }
                    }

                    NewCycleCount = __rdtsc();
                    u64 CycleElapsed = NewCycleCount - OldCycleCount;
                    OldCycleCount = NewCycleCount;
                    r64 MegaCyclesElapsed = (r64)CycleElapsed / (1000.0*1000.0);
                    r64 SecondsElapsed = 0.0;
                    do
                    {
                        QueryPerformanceCounter(&NewPerformanceCount);
                        s64 CounterElapsed = NewPerformanceCount.QuadPart - OldPerformanceCount.QuadPart;
                        SecondsElapsed = (r64)CounterElapsed / (r64)PerformanceFrequency.QuadPart;
                    } while(SecondsElapsed < GameInput.dtForFrame);
                    OldPerformanceCount = NewPerformanceCount;
#if 1
                    char Buffer[256];
                    snprintf(Buffer, sizeof(Buffer), "SecondsPerFrame: %g, MCPF: %g\n", SecondsElapsed, MegaCyclesElapsed);
                    OutputDebugStringA(Buffer);
#endif

#if 0
                    Assert(GameVideo.Pitch > 0);
                    u8 *Source = GameVideo.Memory + GameVideo.Pitch;
                    u8 *Dest = GameVideo.Memory + GameVideo.Width*GameVideo.BytesPerPixel;
                    s32 Size = GameVideo.Width*GameVideo.BytesPerPixel;
                    for(s32 Y = 0;
                        Y < (GameVideo.Height - 1);
                        ++Y)
                    {
                        memcpy(Dest, Source, Size);
                        Dest += Size;
                        Source += GameVideo.Pitch;
                    }
#endif

                    RECT ClientRect;
                    if(!GetClientRect(Window, &ClientRect))
                    {
                        InvalidCodePath;
                    }
                    s32 ClientWidth = ClientRect.right - ClientRect.left;
                    s32 ClientHeight = ClientRect.bottom - ClientRect.top;

                    if((ClientWidth >= 2*GameVideo.Width) &&
                       (ClientHeight >= 2*GameVideo.Height))
                    {
                        ClientWidth = 2*GameVideo.Width;
                        ClientHeight = 2*GameVideo.Height;
                    }
                    else
                    {
                        ClientWidth = GameVideo.Width;
                        ClientHeight = GameVideo.Height;
                    }

                    if(!StretchDIBits(DeviceContext,
                                      0, 0, ClientWidth, ClientHeight, // NOTE(gaurang): Destination
                                      0, 0, GameVideo.Width, GameVideo.Height, // NOTE(gaurang): Source
                                      (VOID *)GameVideo.Memory, &BitmapInfo,
                                      DIB_RGB_COLORS, SRCCOPY))
                    {
                        // TODO(gaurang): Diagnostics
                        InvalidCodePath;
                    }
                }
            }
            else
            {
                // TODO(gaurang): Diagnostics
                InvalidCodePath;
            }
        }
        else
        {
            // TODO(gaurang): Diagnostics
            InvalidCodePath;
        }
    }
    else
    {
        // TODO(gaurang): Diagnostics
        InvalidCodePath;
    }

    return(0);
}

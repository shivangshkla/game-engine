/*
  NOTE(gaurang):

  HANDMADE_INTERNAL:
    0 - Build for public release
    1 - Build for developer only

  HANDMADE_SLOW:
    0 - Slow code is not allowed.
    1 - Slow code is allowed.
*/

//
// NOTE(gaurang): Compilers
//

#ifndef COMPILER_MSVC
#define COMPILER_MSVC 0
#endif

#ifndef COMPILER_LLVM
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
// TODO(gaurang): More compilers!
#undef COMPILER_LLVM
#define COMPILER LLVM 1
#endif
#endif

//
// NOTE(gaurang): Types
//

#define function static
#define local_persist static
#define global static

#include <stdint.h>
#include <float.h>

typedef int8_t s08;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u08;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int32_t b32;

typedef float r32;
typedef float f32;
typedef double r64;

typedef uintptr_t umm;
typedef intptr_t smm;
typedef size_t memory_index;

#define S32Min INT32_MIN
#define S32Max INT32_MAX
#define U32Max INT32_MAX
#define S16Max INT16_MAX
#define R32Max FLT_MAX
#define F32Max FLT_MAX

#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))

#define Kilobytes(Value) ((memory_index)1024 * (memory_index)(Value))
#define Megabytes(Value) ((memory_index)1024 * Kilobytes(Value))
#define Gigabytes(Value) ((memory_index)1024 * Megabytes(Value))
#define Terabytes(Value) ((memory_index)1024 * Gigabytes(Value))

#define Minimum(A, B) ((A) < (B) ? (A) : (B))
#define Maximum(A, B) ((A) > (B) ? (A) : (B))

#if HANDMADE_SLOW
#define Assert(Expression) do { if(!(Expression)) { *(int *)0 = 0; } } while(0);
#else
#define Assert(Expression)
#endif

#define InvalidCodePath Assert(!"InvalidCodePath");
#define InvalidDefaultCase default: { InvalidCodePath; } break;

#define Pi32 3.141592f
#define Tau32 6.283185f

#define AlignPow2(Value, Alignment) (((Value) + ((Alignment)-1)) & ~((Alignment-1)))
#define Align4(Value) (((Value) + 3) & ~3)
#define Align8(Value) (((Value) + 7) & ~7)
#define Align16(Value) (((Value) + 15) & ~15)

function s16
SafeTruncateToS16(s32 Value)
{
    Assert(Value <= S16Max);
    s16 Result = (s16)Value;
    return(Result);
}

function u32
SafeTruncateToUInt32(s64 Value)
{
    Assert(Value <= U32Max);
    u32 Result = (u32)Value;
    return(Result);
}

#if HANDMADE_INTERNAL
/*
  IMPORTANT(gaurang):

  These are NOT for doing anything in the shipping game - they are
  blocking and the write doesn't protect against lost data!
*/
struct debug_read_file
{
	u32 Count;
	u8 *Data;
};
#define DEBUG_PLATFORM_READ_ENTIRE_FILE(Name) debug_read_file Name(char *FileName)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);
#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(Name) void Name(char *FileName, void *Contents, u32 ContentSize)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);
#define DEBUG_PLATFORM_FREE_FILE_MEMORY(Name) void Name(debug_read_file *File)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

struct debug_cycle_counter
{
    u32 HitCount;
    u64 CycleCount;
};

enum
{
    DebugCycleCounter_GameUpdateAndRender,
    DebugCycleCounter_RenderGroupToOutput,
    DebugCycleCounter_DrawRectangleQuickly,
    DebugCycleCounter_ProcessPixel,
    DebugCycleCounter_Count
};

struct game_memory;

global game_memory *DebugGlobalMemory;

#if COMPILER_MSVC
#define BEGIN_TIMED_BLOCK(ID) u64 StartCycleCount_##ID = __rdtsc();
#define END_TIMED_BLOCK(ID) DebugGlobalMemory->DebugCycleCounters[DebugCycleCounter_##ID].CycleCount += __rdtsc() - StartCycleCount_##ID; ++DebugGlobalMemory->DebugCycleCounters[DebugCycleCounter_##ID].HitCount;
#define END_TIMED_BLOCK_COUNTED(ID, Count) DebugGlobalMemory->DebugCycleCounters[DebugCycleCounter_##ID].CycleCount += __rdtsc() - StartCycleCount_##ID; DebugGlobalMemory->DebugCycleCounters[DebugCycleCounter_##ID].HitCount += (Count);
#else
#endif

#endif

struct platform_work_queue;

#define PLATFORM_WORK_QUEUE_CALLBACK(Name) void Name(void *Data)
typedef PLATFORM_WORK_QUEUE_CALLBACK(platform_work_queue_callback);

typedef void platform_add_entry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data);
typedef void platform_complete_all_work(platform_work_queue *Queue);


struct platform_file_handle
{
    b32 HasErrors;
    void *Platform;
};

struct platform_file_group
{
    s32  FileCount;
    void *Platform;
};

enum platform_file_type
{
    PlatformFileType_AssetFile,
    PlatformFileType_SavedGameFile,

    PlatformFileType_Count
};

#define PLATFORM_GET_ALL_FILES_OF_TYPE_BEGIN(name) platform_file_group name(platform_file_type Type)
typedef PLATFORM_GET_ALL_FILES_OF_TYPE_BEGIN(platform_get_all_files_of_type_begin);

#define PLATFORM_GET_ALL_FILES_OF_TYPE_END(name) void name(platform_file_group FileGroup)
typedef PLATFORM_GET_ALL_FILES_OF_TYPE_END(platform_get_all_files_of_type_end);

#define PLATFORM_OPEN_FILE(name) platform_file_handle *name(platform_file_group FileGroup, s32 FileIndex)
typedef PLATFORM_OPEN_FILE(platform_open_file);

#define PLATFORM_READ_DATA_FROM_FILE(name) void name(platform_file_handle *Source, s64 Offset, s64 Size, void *Dest)
typedef PLATFORM_READ_DATA_FROM_FILE(platform_read_data_from_file);

#define PLATFORM_FILE_ERROR(name) void name(platform_file_handle *Handle, char *Message)
typedef PLATFORM_FILE_ERROR(platform_file_error);

#define PLATFORM_ALLOCATE_MEMORY(name) u8 *name(smm Size)
typedef PLATFORM_ALLOCATE_MEMORY(platform_allocate_memory);

#define PLATFORM_DEALLOCATE_MEMORY(name) void name(u8 *Memory)
typedef PLATFORM_DEALLOCATE_MEMORY(platform_deallocate_memory);

#define PlatformNoFileErrors(Handle) (!(Handle)->HasErrors)

struct platform_api
{
    platform_add_entry *AddEntry;
    platform_complete_all_work *CompleteAllWork;

    platform_get_all_files_of_type_begin *GetAllFilesOfTypeBegin;
    platform_get_all_files_of_type_end *GetAllFilesOfTypeEnd;
    platform_open_file *OpenFile;
    platform_read_data_from_file *ReadDataFromFile;
    platform_file_error *FileError;

    platform_allocate_memory *AllocateMemory;
    platform_deallocate_memory *DeallocateMemory;

    debug_platform_read_entire_file *DebugReadEntireFile;
    debug_platform_write_entire_file *DebugWriteEntireFile;
    debug_platform_free_file_memory *DebugFreeFileMemory;
};

struct game_memory
{
    memory_index PermanentStorageSize;
    u8 *PermanentStorage;
    memory_index TransientStorageSize;
    u8 *TransientStorage;

    platform_work_queue *HighPriorityQueue;
    platform_work_queue *LowPriorityQueue;

    platform_api PlatformAPI;

    debug_cycle_counter DebugCycleCounters[DebugCycleCounter_Count];
};

struct game_video
{
    s32 Width;
    s32 Height;
    s32 BytesPerPixel;
    s32 Pitch;
    u8 *Memory;
};

struct game_audio
{
    s32 SampleRate;
    s32 SampleCount;
    // IMPORTANT(gaurang): Sample memory must be padded up to a multiple of 4.
    s16 *Samples;
};

struct game_button
{
    b32 Down;
    b32 Pressed;
    b32 Released;
};

struct game_controller
{
    game_button MoveUp;
    game_button MoveDown;
    game_button MoveLeft;
    game_button MoveRight;

    game_button ActionUp;
    game_button ActionDown;
    game_button ActionLeft;
    game_button ActionRight;

    game_button LeftShoulder;
    game_button RightShoulder;

    game_button Back;
    game_button Start;

    r32 StickX;
    r32 StickY;
};

struct game_input
{
    r32 dtForFrame;
    s32 ControllerCount;
    game_controller *Controllers;
    struct
    {
        s32 X;
        s32 Y;
        game_button Left;
        game_button Right;
    } Mouse;
};


//
// NOTE(gaurang): Services that the platform layer provides to the game.
//

//
// NOTE(gaurang): Services that the game provides to the platform layer.
//

#define GAME_UPDATE_AND_RENDER(name) void name(game_memory *Memory, game_video *Video, game_input *Input)
#define GAME_GET_SOUND_SAMPLES(name) void name(game_memory *Memory, game_audio *Audio)

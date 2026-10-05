/*
  TODO(gaurang):

  - Flush all thread queues before reloading DLL
  ARCHITECTURE EXPLORATION
  - Metagame / save game?
    - Persistent unlocks?
    - Do we allow saved games? Probably yes, just only for "pausing"
    * Continuous save for crash recovery?
  - Implement multiple sim region per frame
    - Per-entity clocking
    - Sim region merging? For multiple players?
  - Collision detection
    - Entry/Exit?
    - What's the plan for robustness / shape redefinition?
  - AI
    - Rudimentary monster behaviour example
    * pathfinding
    - AI "storage"
  - rudimentary world gen (no quality, just "what sort of things" we do)
    - Placement of background things
    - Connectivity?
    - Non-overlapping?
  * Animation, should probably lead into rendering
    - Skeletal animation
    - particle system
  - Audio
    - Sound effect triggers
    - Ambient sounds
    - Music
  - Debug code
    - Logging
    - Diagramming
    - Switches? Sliders?

  PRODUCTION
  - Rendering
  - Game
    - Entity system
    - world generation
*/

struct memory_arena
{
    memory_index Used;
    memory_index Size;
    u8 *Base;
    s32 TempCount;
};

function void
InitializeArena(memory_arena *Arena, memory_index Size, u8 *Base)
{
    Arena->Used = 0;
    Arena->Size = Size;
    Arena->Base = Base;
    Arena->TempCount = 0;
}

function memory_index
GetAlignmentOffset(memory_arena *Arena, memory_index Alignment)
{
    memory_index Base = (memory_index)(Arena->Base + Arena->Used);

    Assert(!(Alignment & 1));
    memory_index AlignmentOffset = 0;
    memory_index AlignmentMask = (Alignment - 1);
    if(Base & AlignmentMask)
    {
        AlignmentOffset = Alignment - (Base & AlignmentMask);
    }

    return(AlignmentOffset);
}

function memory_index
GetArenaSizeRemaining(memory_arena *Arena, memory_index Alignment = 4)
{
    memory_index AlignmentOffset = GetAlignmentOffset(Arena, Alignment);
    memory_index Result = Arena->Size - (Arena->Used + AlignmentOffset);
    return(Result);
}

#define PushStruct(Arena, type, ...) ((type *)PushSize_((Arena), sizeof(type), __VA_ARGS__))
#define PushArray(Arena, Count, type, ...) ((type *)PushSize_((Arena), sizeof(type)*(Count), __VA_ARGS__))
#define PushSize(Arena, Size, ...) (PushSize_((Arena), (Size), __VA_ARGS__))
function u8 *
PushSize_(memory_arena *Arena, memory_index Size, memory_index Alignment = 4)
{
    u8 *Result = Arena->Base + Arena->Used;

    memory_index AlignmentOffset = GetAlignmentOffset(Arena, Alignment);

    Result += AlignmentOffset;
    Arena->Used += AlignmentOffset;

    Assert((Arena->Used + Size) <= Arena->Size);
    Arena->Used += Size;
    return(Result);
}

#define ZeroStruct(Instance) (ZeroSize(sizeof(Instance), &(Instance)))
function void
ZeroSize(memory_index Size, void *Pointer)
{
    u8 *Byte = (u8 *)Pointer;
    while(Size--)
    {
        *Byte++ = 0;
    }
}

function void
Copy(smm Size, u8 *Source, u8 *Dest)
{
    while(Size--)
    {
        *Dest++ = *Source++;
    }
}

function void
SubArena(memory_arena *Result, memory_arena *Arena, memory_index Size, memory_index Alignment = 16)
{
    Result->Size = Size;
    Result->Base = PushSize(Arena, Size, Alignment);
    Result->TempCount = 0;
    Result->Used = 0;
}

// NOTE(gaurang): This is not for production use, this is probably
// something only needed for testing.
function char *
PushString(memory_arena *Arena, char *Src)
{
    s32 Size = 1;
    for(char *At = Src;
        *At;
        ++At)
    {
        ++Size;
    }

    char *Dest = (char *)PushSize(Arena, Size);
    for(s32 CharIndex = 0;
        CharIndex < Size;
        ++CharIndex)
    {
        Dest[CharIndex] = Src[CharIndex];
    }

    return(Dest);
}

struct temporary_memory
{
    memory_arena *Arena;
    memory_index Used;
};

function temporary_memory
BeginTemporaryMemory(memory_arena *Arena)
{
    temporary_memory Result;

    Result.Arena = Arena;
    Result.Used = Arena->Used;

    Arena->TempCount++;

    return(Result);
}

function void
EndTemporaryMemory(temporary_memory TempMem)
{
    memory_arena *Arena = TempMem.Arena;
    Assert(Arena->Used >= TempMem.Used);
    Arena->Used = TempMem.Used;
    Assert(Arena->TempCount > 0);
    Arena->TempCount--;
}

function void
CheckArena(memory_arena *Arena)
{
    Assert(Arena->TempCount == 0);
}

#include "handmade_math.h"
#include "handmade_random.h"
#include "handmade_render_group.h"
#include "handmade_file_formats.h"
#include "handmade_asset.h"
#include "handmade_world.h"
#include "handmade_sim_region.h"
#include "handmade_audio.h"

struct controller_request
{
    v3 ddP;
    v3 dSwordP;
};

struct particle_cell
{
    f32 Density;
    v3 VelocityTimesDensity;
};

struct particle
{
    v3 P;
    v3 dP;
    v3 ddP;
    v4 dColor;
    v4 Color;
    pixmap_id PixmapID;
};

struct game_state
{
    s32 PlayerStoredIndex;
    controller_request ControllerRequest;

    memory_arena WorldArena;
    world *World;

    game_audio_state AudioState;
    playing_sound *Music;

    random_series EffectsEntropy; // NOTE(gaurang): This is entropy that doesn't effect the gameplay

    s32 ParticleCount;
    particle Particles[512];
#define PARTICLE_CELL_DIM 32
    particle_cell ParticleCells[PARTICLE_CELL_DIM][PARTICLE_CELL_DIM];

    b32 IsInitialized;
};

struct ground_buffer
{
    b32 IsValid;
    world_position P;
    pixmap Pixmap;
};

struct task_with_memory
{
    b32 IsBeingUsed;
    temporary_memory GroundMemory;
    memory_arena Arena;
};

struct transient_state
{
    memory_arena TranArena;

    task_with_memory Tasks[4];

    s32 GroundBufferCount;
    ground_buffer *GroundBuffers;

    game_assets *Assets;

    b32 IsInitialized;
};

function task_with_memory *
BeginTaskWithMemory(transient_state *State)
{
    task_with_memory *Found = 0;

    for(s32 Index = 0;
        Index < ArrayCount(State->Tasks);
        ++Index)
    {
        task_with_memory *Task = State->Tasks + Index;
        if(!Task->IsBeingUsed)
        {
            Found = Task;

            Task->IsBeingUsed = true;
            Task->GroundMemory = BeginTemporaryMemory(&Task->Arena);

            break;
        }
    }

    return(Found);
}

function void
EndTaskWithMemory(task_with_memory *Task)
{
    EndTemporaryMemory(Task->GroundMemory);

    CompletePastWritesBeforeFutureWrites;

    Task->IsBeingUsed = false;
}

global platform_work_queue *HighPriorityQueue;
global platform_work_queue *LowPriorityQueue;
global platform_api Platform;

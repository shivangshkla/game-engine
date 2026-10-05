struct memory_arena
{
    smm Used;
    smm Size;
    u8 *Base;

    s32x TempCount;
};

function void
InitializeArena(memory_arena *Arena, smm Size, u8 *Base)
{
    Arena->Used = 0;
    Arena->Size = Size;
    Arena->Base = Base;
}

#define PushStruct(Arena, type, ...) ((type *)PushSize_((Arena), sizeof(type)))
#define PushArray(Arena, Count, type, ...) ((type *)PushSize_((Arena), sizeof(type)*(Count)))
#define PushSize(Arena, Size, ...) (PushSize_((Arena), (Size)))

function u8 *
PushSize_(memory_arena *Arena, smm Size)
{
    u8 *Result = Arena->Base + Arena->Used;

    Assert((Arena->Used + Size) <= Arena->Size);
    Arena->Used += Size;

    return(Result);
}

#define ZeroStruct(Instance) (ZeroSize(sizeof(Instance), &(Instance)))
#define ZeroArray(Count, Base) (ZeroSize((Count)*sizeof((Base)[0]), (Base)))
function void
ZeroSize(smm Size, u8 *Base)
{
    u8 *Byte = Base;
    while(Size--)
    {
        *Byte++ = 0;
    }
}

struct temporary_memory
{
    memory_arena *Arena;
    smm Used;
    s32x Index;
};

function temporary_memory
BeginTemporaryMemory(memory_arena *Arena)
{
    temporary_memory Result;

    Result.Arena = Arena;
    Result.Used = Arena->Used;
    Result.Index = Arena->TempCount++;

    return(Result);
}

function void
EndTemporaryMemory(temporary_memory Temp)
{
    Temp.Arena->TempCount--;
    Assert(Temp.Arena->TempCount >= 0);
    Assert(Temp.Index == Temp.Arena->TempCount);

    Temp.Arena->Used = Temp.Used;
}

function void
CheckArena(memory_arena *Arena)
{
    Assert(Arena->TempCount == 0);
}

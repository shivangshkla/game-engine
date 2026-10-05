#include "handmade_platform.h"
#include "handmade_intrinsics.h"

#include "handmade.h"

#include "handmade_asset.cpp"
#include "handmade_render_group.cpp"
#include "handmade_world.cpp"
#include "handmade_sim_region.cpp"
#include "handmade_audio.cpp"

struct hero_pixmaps
{
    pixmap_id Head;
    pixmap_id Cape;
    pixmap_id Torso;
};

function void
DrawHitPoints(render_group *Group, entity *Entity, v3 Base)
{
    v2 HealthDim = V2(0.2f, 0.2f);
    r32 SpacingX = 1.5f*HealthDim.X;
    r32 FirstX = -0.5f*(Entity->HitPointMax-1)*SpacingX;
    for(s32 HealthIndex = 0;
        HealthIndex < Entity->HitPointMax;
        ++HealthIndex)
    {
        v4 Color = V4(1.0f, 0.0f, 0.0f, 1.0f);
        if(HealthIndex >= Entity->HitPointCount)
        {
            Color.R *= 0.4f;
            Color.G *= 0.3f;
            Color.B *= 0.3f;
        }
        PushRect(Group, Base + V3(FirstX+HealthIndex*SpacingX, -0.25f, 0.0f), HealthDim, Color);
    }
}

function void
ClearPixmap(pixmap *Pixmap)
{
    memory_index PixmapMemorySize = Pixmap->Height*Pixmap->Pitch;
    if(Pixmap->Memory)
    {
        ZeroSize(PixmapMemorySize, Pixmap->Memory);
    }
}

function pixmap
MakeEmptyPixmap(memory_arena *Arena, s32 Width, s32 Height, b32 ClearToZero = true)
{
    pixmap Result = {};

    Result.Width = Width;
    Result.Height = Height;
    Result.AlignPercentage = V2(0.5f, 0.5f);
    Result.WidthOverHeight = SafeRatio0((r32)Result.Width, (r32)Result.Height);
    Result.BytesPerPixel = 4;
    Result.Pitch = Result.Width*Result.BytesPerPixel;

    memory_index PixmapMemorySize = Result.Height*Result.Pitch;
    Result.Memory = PushSize(Arena, PixmapMemorySize);
    if(ClearToZero)
    {
        ClearPixmap(&Result);
    }

    return(Result);
}

struct draw_ground_chunk_work
{
    render_group *Group;
    pixmap *Output;
    task_with_memory *Task;
};

PLATFORM_WORK_QUEUE_CALLBACK(DrawGroundChunkWork)
{
    draw_ground_chunk_work *Work = (draw_ground_chunk_work *)Data;

    Work->Output->Width -= 1;
    Work->Output->Height -= 1;
    RenderGroupToOutput(Work->Group, Work->Output, {0, 0, Work->Output->Width, Work->Output->Height});
    Work->Output->Width += 1;
    Work->Output->Height += 1;

    EndTaskWithMemory(Work->Task);
}

function void
DrawGroundChunk(game_state *GameState, ground_buffer *Buffer, world_position WorldP, transient_state *TranState)
{
    task_with_memory *Task = BeginTaskWithMemory(TranState);
    if(Task)
    {
        draw_ground_chunk_work *Work = PushStruct(&Task->Arena, draw_ground_chunk_work);

        // TODO(gaurang): Safe cast from memory_uint to u32?
        render_group *Group = AllocateRenderGroup(&Task->Arena,
                                                  (u32)GetArenaSizeRemaining(&Task->Arena),
                                                  TranState->Assets);
        Orthographic(Group, 1.0f, 1.0f);

        Clear(Group, V4(1.0f, 0.0f, 1.0f, 1.0f));

        for(s32 OffsetY = -1;
            OffsetY <= 1;
            ++OffsetY)
        {
            for(s32 OffsetX = -1;
                OffsetX <= 1;
                ++OffsetX)
            {
                // TODO(gaurang): Look into wang hashing or some other spatial
                // seed generation "thing"!
                s32 ChunkX = WorldP.ChunkX + OffsetX;
                s32 ChunkY = WorldP.ChunkY + OffsetY;
                s32 ChunkZ = WorldP.ChunkZ;
                random_series Series = RandomSeed(191*ChunkX + 73*ChunkY + 53*ChunkZ);

                u32 RandomNumberIndex = 0;
                for(s32 Index = 0;
                    Index < 50;
                    ++Index)
                {
                    pixmap_id Stamp;
                    if(RandomChoice(&Series, 2))
                    {
                         Stamp = GetRandomPixmapFrom(TranState->Assets, Asset_Grass, &Series);
                    }
                    else
                    {
                        Stamp = GetRandomPixmapFrom(TranState->Assets, Asset_Ground, &Series);
                    }

                    v2 Offset = V2(RandomBilateral(&Series), RandomBilateral(&Series));
                    v3 P = V3(Offset, 0.0f);
                    P += V3((r32)OffsetX, (r32)OffsetY, 0.0f);

                    PushPixmap(Group, P, 0.5f, Stamp);
                }
            }
        }

        for(s32 OffsetY = -1;
            OffsetY <= 1;
            ++OffsetY)
        {
            for(s32 OffsetX = -1;
                OffsetX <= 1;
                ++OffsetX)
            {
                // TODO(gaurang): Look into wang hashing or some other spatial
                // seed generation "thing"!
                s32 ChunkX = WorldP.ChunkX + OffsetX;
                s32 ChunkY = WorldP.ChunkY + OffsetY;
                s32 ChunkZ = WorldP.ChunkZ;
                random_series Series = RandomSeed(191*ChunkX + 73*ChunkY + 53*ChunkZ);

                for(s32 Index = 0;
                    Index < 25;
                    ++Index)
                {
                    pixmap_id Stamp = GetRandomPixmapFrom(TranState->Assets, Asset_Tuft, &Series);

                    v2 Offset = V2(RandomBilateral(&Series), RandomBilateral(&Series));
                    v3 P = V3(Offset, 0.0f);
                    P += V3((r32)OffsetX, (r32)OffsetY, 0.0f);

                    PushPixmap(Group, P, 0.05f, Stamp);
                }
            }
        }

        if(AllResourcesPresent(Group))
        {
            Buffer->P = WorldP;
            Buffer->IsValid = true;

            Work->Group = Group;
            Work->Output = &Buffer->Pixmap;
            Work->Task = Task;

            Platform.AddEntry(LowPriorityQueue, DrawGroundChunkWork, (void *)Work);
        }
        else
        {
            EndTaskWithMemory(Task);
        }
    }
}

global render_group *DEBUGRenderGroup;
global f32 LeftEdge;
global f32 AtY;
global f32 FontScale;

function void
DebugReset(void)
{
    FontScale = 0.0005f;
    Orthographic(DEBUGRenderGroup, 1.0f, 1.0f);
    AtY = 0.5f - 80*FontScale;
    LeftEdge = -0.5f + FontScale;
}

function void
DEBUGTextLine(char *String)
{
    asset_vector MatchVector = {};
    asset_vector WeightVector = {};
    WeightVector.E[Tag_UnicodeCodepoint] = 1.0f;

    font_id FontID = BestMatchFontID(DEBUGRenderGroup->Assets, Asset_Font, &MatchVector, &WeightVector);
    asset_slot *Font = GetFontSlot(DEBUGRenderGroup->Assets, FontID);

    if(Font)
    {
        u32 PrevCodePoint = 0;
        f32 AtX = LeftEdge;
        for(char *At = String;
            *At;
            ++At)
        {
            u32 CodePoint = *At;
            f32 AdvanceX = FontScale*GetHorizontalAdvanceForPair(Font, PrevCodePoint, CodePoint);
            AtX += AdvanceX;

            f32 CharDim = FontScale*2.0f;
            if(CodePoint != ' ')
            {
                pixmap_id PixmapID = GetPixmapIDForGlyph(Font, CodePoint);
                hha_pixmap *PixmapInfo = GetPixmapInfo(DEBUGRenderGroup->Assets, PixmapID);
                PushPixmap(DEBUGRenderGroup, V3(AtX, AtY, 0), FontScale*PixmapInfo->Dim[1], PixmapID, V4(1, 1, 1, 1));
            }

            PrevCodePoint = CodePoint;
        }

        AtY -= GetLineAdvanceFor(Font)*FontScale;
    }
}

function void
OverlayCycleCounter(game_memory *Memory)
{
    char *NameTable[] = {
        "GameUpdateAndRender",
        "RenderGroupToOutput",
        "DrawRectangleQuickly",
        "ProcessPixel",
    };
    DEBUGTextLine("DEBUG CYCLE COUNTS:");
    for(s32 Index = 0;
        Index < DebugCycleCounter_Count;
        ++Index)
    {
        debug_cycle_counter *Counter = Memory->DebugCycleCounters + Index;

        if(Counter->HitCount)
        {
#if 0
            char Buffer[256];
            snprintf(Buffer, sizeof(Buffer), "%d: %I64ucy %uh %I64ucy/h\n",
                     Index,
                     Counter->CycleCount,
                     Counter->HitCount,
                     Counter->CycleCount / Counter->HitCount);
            OutputDebugStringA(Buffer);
#else
            DEBUGTextLine(NameTable[Index]);
#endif
            Counter->HitCount = 0;
            Counter->CycleCount = 0;
        }
    }
}

GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    DebugGlobalMemory = Memory;

    HighPriorityQueue = Memory->HighPriorityQueue;
    LowPriorityQueue = Memory->LowPriorityQueue;
    Platform = Memory->PlatformAPI;

    BEGIN_TIMED_BLOCK(GameUpdateAndRender);

    r32 GroundBufferWidth = 4;
    r32 GroundBufferHeight = 4;

    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if(!GameState->IsInitialized)
    {
        InitializeArena(&GameState->WorldArena,
                        Memory->PermanentStorageSize - sizeof(game_state),
                        Memory->PermanentStorage + sizeof(game_state));

        InitializeAudioState(&GameState->AudioState, &GameState->WorldArena);

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world *World = GameState->World;

        // NOTE(gaurang): Reserve entity slot 0 for the null entity.
        World->StoredEntityCount++;

        World->TileSideInMeters = 1.0f;
        World->ChunkSideInMeters = V3((r32)GroundBufferWidth,
                                      (r32)GroundBufferHeight,
                                      2.5f*World->TileSideInMeters);

        World->SwordCollision = MakeSimpleGroundedCollision(GameState, V3(0.75f, 0.5f, 0.5f));
        World->StairCollision = MakeSimpleGroundedCollision(GameState, V3(World->TileSideInMeters, 2.0f*World->TileSideInMeters, 2.0f*World->ChunkSideInMeters.Z));
        World->HeroCollision = MakeSimpleGroundedCollision(GameState, V3(0.75f, 0.5f, 1.2f));
        World->MonsterCollision = MakeSimpleGroundedCollision(GameState, V3(0.75f, 0.5f, 0.5f));
        World->WallCollision = MakeSimpleGroundedCollision(GameState, V3(World->TileSideInMeters, World->TileSideInMeters, World->ChunkSideInMeters.Z));
        World->FamiliarCollision = MakeSimpleGroundedCollision(GameState, V3(0.75f, 0.5f, 0.5f));
        World->StandardRoomCollision = MakeSimpleGroundedCollision(GameState, V3(17*World->TileSideInMeters,
                                                                                 9*World->TileSideInMeters,
                                                                                 0.9f*World->ChunkSideInMeters.Z));

        s32 ScreenWidth = 17;
        s32 ScreenHeight = 9;
        s32 ScreenX = 0;
        s32 ScreenY = 0;

        b32 DoorLeft = false;
        b32 DoorRight = false;
        b32 DoorTop = false;
        b32 DoorBottom = false;
        b32 DoorUp = false;
        b32 DoorDown = false;
        s32 Z = 0;

        random_series Series = RandomSeed(120);
        for(s32 ScreenIndex = 0;
            ScreenIndex < 64;
            ++ScreenIndex)
        {
            s32 Choice;
            if(DoorUp || DoorDown)
            {
                Choice = RandomChoice(&Series, 2);
            }
            else
            {
                Choice = RandomChoice(&Series, 3);
            }

            b32 CreatedZDoor = false;
            if(Choice == 2)
            {
                CreatedZDoor = true;
                if(Z == 0)
                {
                    DoorUp = true;
                }
                else
                {
                    DoorDown = true;
                }
            }
            else if(Choice == 1)
            {
                DoorRight = true;
            }
            else
            {
                DoorTop = true;
            }

            AddStandardRoom(&GameState->WorldArena, World,
                            ScreenX*ScreenWidth, ScreenY*ScreenHeight, Z,
                            (1 + ScreenX)*ScreenWidth, (1 + ScreenY)*ScreenHeight, Z + 1);

            for(s32 TileY = 0;
                TileY < ScreenHeight;
                ++TileY)
            {
                for(s32 TileX = 0;
                    TileX < ScreenWidth;
                    ++TileX)
                {
                    s32 X = ScreenX*ScreenWidth + TileX;
                    s32 Y = ScreenY*ScreenHeight + TileY;
                    s32 TileValue = 1;

                    if(TileX == 0)
                    {
                        if(!DoorLeft)
                        {
                            TileValue = 2;
                        }
                        if(TileY != (ScreenHeight / 2))
                        {
                            TileValue = 2;
                        }
                    }
                    if(TileX == (ScreenWidth-1))
                    {
                        if(!DoorRight)
                        {
                            TileValue = 2;
                        }
                        if(TileY != (ScreenHeight / 2))
                        {
                            TileValue = 2;
                        }
                    }

                    if(TileY == 0)
                    {
                        if(!DoorBottom)
                        {
                            TileValue = 2;
                        }
                        if(TileX != (ScreenWidth / 2))
                        {
                            TileValue = 2;
                        }
                    }
                    if(TileY == (ScreenHeight-1))
                    {
                        if(!DoorTop)
                        {
                            TileValue = 2;
                        }
                        if(TileX != (ScreenWidth / 2))
                        {
                            TileValue = 2;
                        }
                    }

                    if((TileX == 12) && (TileY == 5))
                    {
                        if(DoorUp)
                        {
                            TileValue = 3;
                        }
                        if(DoorDown)
                        {
                            TileValue = 4;
                        }
                    }

                    if(TileValue == 2)
                    {
                        AddWall(&GameState->WorldArena, World, X, Y, Z);
                    }
                    if(TileValue == 3)
                    {
                        AddStair(&GameState->WorldArena, World, X, Y, Z);
                    }
                }
            }

            DoorLeft = DoorRight;
            DoorBottom = DoorTop;

            if(CreatedZDoor)
            {
                DoorDown = !DoorDown;
                DoorUp = !DoorUp;
            }
            else
            {
                DoorUp = false;
                DoorDown = false;
            }

            DoorRight = false;
            DoorTop = false;

            if(Choice == 2)
            {
                if(Z == 0)
                {
                    Z = 1;
                }
                else
                {
                    Z = 0;
                }
            }
            else if(Choice == 1)
            {
                ScreenX += 1;
            }
            else
            {
                ScreenY += 1;
            }
        }

        GameState->PlayerStoredIndex = AddHero(&GameState->WorldArena, World, 3, 5, 0);
        AddMonster(&GameState->WorldArena, World, 13, 6, 0);
        s32 FamiliarIndex = AddFamiliar(&GameState->WorldArena, World, 5, 3, 0);
        AddCollisionRule(&GameState->WorldArena, GameState->World, FamiliarIndex, GameState->PlayerStoredIndex, true);

        GameState->EffectsEntropy = RandomSeed(1234);

        GameState->IsInitialized = true;
    }

    transient_state *TranState = (transient_state *)Memory->TransientStorage;
    if(!TranState->IsInitialized)
    {
        InitializeArena(&TranState->TranArena,
                        Memory->TransientStorageSize - sizeof(transient_state),
                        Memory->TransientStorage + sizeof(transient_state));

        for(s32 Index = 0;
            Index < ArrayCount(TranState->Tasks);
            ++Index)
        {
            task_with_memory *Task = TranState->Tasks + Index;

            Task->IsBeingUsed = false;
            SubArena(&Task->Arena, &TranState->TranArena, Megabytes(1));
        }

        TranState->Assets = AllocateAssets(&TranState->TranArena, Megabytes(22), TranState);

        DEBUGRenderGroup = AllocateRenderGroup(&TranState->TranArena, Megabytes(16), TranState->Assets);

        TranState->GroundBufferCount = 64;
        TranState->GroundBuffers = PushArray(&TranState->TranArena, TranState->GroundBufferCount, ground_buffer);
        for(s32 Index = 0;
            Index < TranState->GroundBufferCount;
            ++Index)
        {
            ground_buffer *GroundBuffer = TranState->GroundBuffers + Index;

            GroundBuffer->IsValid = false;
            GroundBuffer->Pixmap = MakeEmptyPixmap(&TranState->TranArena, (s32)GroundBufferWidth*60+1, (s32)GroundBufferHeight*60+1, false);
        }

        // GameState->Music = PlaySound(&GameState->AudioState, GetFirstSoundID(TranState->Assets, Asset_Music));

        TranState->IsInitialized = true;
    }

    temporary_memory RenderMemory = BeginTemporaryMemory(&TranState->TranArena);
    render_group *RenderGroup = AllocateRenderGroup(&TranState->TranArena, Megabytes(4), TranState->Assets);
    // Perspective(RenderGroup, 5.0f, 0.2f, 4.0f/3.0f*0.4f, 0.4f);
    Perspective(RenderGroup, 5.0f, 0.2f, 0.4f, 0.4f);
    Clear(RenderGroup, V4(0.3f, 0.3f, 0.3f, 1.0f));

    world *World = GameState->World;

    rectangle2 ScreenBounds = GetCameraRectangleAtTarget(RenderGroup);

    world_position RegionOrigin = World->StoredEntities[GameState->PlayerStoredIndex].P;

    // NOTE(gaurang): This seems to be the ground level. If we remove
    // this, you can get out of stairs inbetween two layers, if we set
    // this to 0, we snap to the lower floor. Remember, SimRegion is
    // always snapped to the Z = 0.0f. TODO(gaurang): Should we move
    // this into begin sim? All this just to handle the ground level
    // yikes!
    RegionOrigin.Offset_.Z = 0.0f;

    rectangle3 RegionBounds = RectMinMax(V3(ScreenBounds.Min, -0.25f*World->ChunkSideInMeters.Z), V3(ScreenBounds.Max, 0.25f*World->ChunkSideInMeters.Z));
    RegionBounds = AddRadiusTo(RegionBounds, V3(0, 0, 1.0f*World->ChunkSideInMeters.Z));

#if 1
    world_position Min = Offset(World, RegionOrigin, RegionBounds.Min);
    world_position Max = Offset(World, RegionOrigin, RegionBounds.Max);

#if 0
    for(s32 ChunkZ = Min.ChunkZ;
        ChunkZ <= Max.ChunkZ;
        ++ChunkZ)
#else
        s32 ChunkZ = 0;
#endif
    {
        for(s32 ChunkY = Min.ChunkY-1;
            ChunkY <= Max.ChunkY+1;
            ++ChunkY)
        {
            for(s32 ChunkX = Min.ChunkX-1;
                ChunkX <= Max.ChunkX+1;
                ++ChunkX)
            {
                world_position WorldP = { 0 };
                WorldP.ChunkX = ChunkX;
                WorldP.ChunkY = ChunkY;
                WorldP.ChunkZ = ChunkZ;
                WorldP.Offset_.XY = 0.5f*World->ChunkSideInMeters.XY;

                r32 FurthestBufferDistanceSq = 0.0f;
                ground_buffer *GroundBuffer = 0;
                for(s32 Index = 0;
                    Index < TranState->GroundBufferCount;
                    ++Index)
                {
                    ground_buffer *Buffer = TranState->GroundBuffers + Index;
                    if((Buffer->P.ChunkX == WorldP.ChunkX) &&
                       (Buffer->P.ChunkY == WorldP.ChunkY) &&
                       (Buffer->P.ChunkZ == WorldP.ChunkZ))
                    {
                        if(Buffer->IsValid)
                        {
                            GroundBuffer = 0;
                            break;
                        }
                    }

                    // TODO(gaurang): Test this when you have a
                    // sophisticated renderer and asynchronous ground
                    // chunk filling.
                    if(Buffer->IsValid)
                    {
                        r32 DistanceSq = LengthSq(Subtract(World, Buffer->P, RegionOrigin).XY);
                        if(DistanceSq > FurthestBufferDistanceSq)
                        {
                            FurthestBufferDistanceSq = DistanceSq;
                            GroundBuffer = Buffer;
                        }
                    }
                    else
                    {
                        FurthestBufferDistanceSq = R32Max;
                        GroundBuffer = Buffer;
                    }
                }

                if(GroundBuffer)
                {
                    DrawGroundChunk(GameState, GroundBuffer, WorldP, TranState);
                }

                v3 P = Subtract(World, WorldP, RegionOrigin);
                v2 Dim = World->ChunkSideInMeters.XY;

                // PushPixmap(RenderGroup, P, &TranState->GroundBuffers->Pixmap);
                // PushRectOutline(RenderGroup, P, Dim, V4(1.0f, 1.0f, 0.0f, 1.0f));
            }
        }
    }
#endif
#if 0
    v2 MusicVolume;
    MusicVolume.Y = SafeRatio0((f32)Input->Mouse.X, (f32)Video->Width);
    MusicVolume.X = 1 - MusicVolume.Y;
    ChangeVolume(&GameState->AudioState, GameState->Music, 0.1f*MusicVolume, 0.01f);
#endif

    // TODO(gaurang): This should be grouped with the RegionBounds
    // initialization, but we are currently using it as camera bounds,
    // for rendering ground chunks.
    v3 RegionBoundsExpansion = V3(10, 10, 0);
    RegionBounds = AddRadiusTo(RegionBounds, RegionBoundsExpansion);

    temporary_memory SimMemory = BeginTemporaryMemory(&TranState->TranArena);
    sim_region *Region = BeginSim(&TranState->TranArena, World, RegionOrigin, RegionBounds, Input->dtForFrame);

    s32 PlayerEntityIndex = 0;
    for(s32 ControllerIndex = 1;
        ControllerIndex < Input->ControllerCount;
        ++ControllerIndex)
    {
        game_controller *Controller = Input->Controllers + ControllerIndex;

        for(s32 EntityIndex = 1;
            EntityIndex < Region->EntityCount;
            ++EntityIndex)
        {
            entity *Entity = Region->Entities + EntityIndex;

            if(Entity->Type == EntityType_Hero)
            {
                if(!PlayerEntityIndex)
                {
                    PlayerEntityIndex = EntityIndex;
                }
                if(!Entity->ControllerIndex)
                {
                    Entity->ControllerIndex = ControllerIndex;
                }
                if(Entity->ControllerIndex == ControllerIndex)
                {
                    v2 ddP = { 0 };
                    if(Controller->MoveUp.Down)
                    {
                        ddP.Y = 1.0f;
                        Entity->FacingDirection = 0.25f;
                    }
                    if(Controller->MoveDown.Down)
                    {
                        ddP.Y = -1.0f;
                        Entity->FacingDirection = 0.75f;
                    }
                    if(Controller->MoveLeft.Down)
                    {
                        ddP.X = -1.0f;
                        Entity->FacingDirection = 0.5f;
                    }
                    if(Controller->MoveRight.Down)
                    {
                        ddP.X = 1.0f;
                        Entity->FacingDirection = 0;
                    }

                    v2 dSwordP = { 0 };
                    if(Controller->ActionUp.Pressed)
                    {
                        // dSwordP.Y = 1.0f;
                        ChangeVolume(&GameState->AudioState, GameState->Music, V2(0.1f, 0.1f), 10.0f);
                    }
                    if(Controller->ActionDown.Pressed)
                    {
                        // dSwordP.Y = -1.0f;
                        ChangeVolume(&GameState->AudioState, GameState->Music, V2(0.0f, 0.0f), 10.0f);
                    }
                    if(Controller->ActionLeft.Pressed)
                    {
                        // dSwordP.X = -1.0f;
                        ChangeVolume(&GameState->AudioState, GameState->Music, V2(0.1f, 0.0f), 10.0f);
                    }
                    if(Controller->ActionRight.Pressed)
                    {
                        // dSwordP.X = 1.0f;
                        ChangeVolume(&GameState->AudioState, GameState->Music, V2(0.0f, 0.1f), 10.0f);
                    }

                    GameState->ControllerRequest.ddP = V3(ddP, 0);
                    GameState->ControllerRequest.dSwordP = V3(dSwordP, 0);
                    break;
                }
            }
        }
    }

    //
    // NOTE(gaurang): Render
    //

    v3 CameraP = Region->Entities[PlayerEntityIndex].P;

    for(s32 Index = 0;
        Index < TranState->GroundBufferCount;
        ++Index)
    {
        ground_buffer *GroundBuffer = TranState->GroundBuffers + Index;

        if(GroundBuffer->IsValid)
        {
            v3 P = Subtract(World, GroundBuffer->P, RegionOrigin);
            P = P - CameraP;
            PushPixmap(RenderGroup, P, (r32)GroundBufferHeight, &GroundBuffer->Pixmap);
        }
    }

    for(s32 EntityIndex = 1;
        EntityIndex < Region->EntityCount;
        ++EntityIndex)
    {
        entity *Entity = Region->Entities + EntityIndex;
        RenderGroup->Transform.Offset = Entity->P - CameraP;

        asset_vector MatchVector = {};
        MatchVector.E[Tag_FacingDirection] = Entity->FacingDirection;
        asset_vector WeightVector = {};
        WeightVector.E[Tag_FacingDirection] = Entity->FacingDirection;

        hero_pixmaps HeroPixmaps = {};
        HeroPixmaps.Head = BestMatchPixmapID(TranState->Assets, Asset_Head, &MatchVector, &WeightVector);
        HeroPixmaps.Cape = BestMatchPixmapID(TranState->Assets, Asset_Cape, &MatchVector, &WeightVector);
        HeroPixmaps.Torso = BestMatchPixmapID(TranState->Assets, Asset_Torso, &MatchVector, &WeightVector);

        if(Entity->Updatable)
        {
            move_spec MoveSpec = { 0 };
            MoveSpec.UnitMaxAccelVector = true;
            MoveSpec.Acceleration = 0.0f;
            MoveSpec.Drag = 0.0f;
            v3 ddP = { 0 };

            switch(Entity->Type)
            {
                case EntityType_Hero:
                {
                    MoveSpec.Acceleration = 50.0f;
                    MoveSpec.Drag = 8.0f;
                    ddP = GameState->ControllerRequest.ddP;

                    v3 dSwordP = GameState->ControllerRequest.dSwordP;
                    if((dSwordP.X != 0) || (dSwordP.Y != 0) || (dSwordP.Z != 0))
                    {
                        v3 SwordP = Entity->P+dSwordP+V3(0, 0, 0.5f);
                        r32 SwordSpeed = 10.0;
                        if(!Entity->Sword)
                        {
                            entity *Sword = NewEntity(&GameState->WorldArena, World, Region, SwordP, World->SwordCollision);
                            Sword->CollisionGroup = World->SwordCollision;
                            Sword->Type = EntityType_Sword;
                            Sword->dP = Entity->dP + dSwordP*SwordSpeed;
                            Sword->DistanceLimit = 3.0f;
                            Entity->Sword = Sword;
                            AddCollisionRule(&GameState->WorldArena, World, Sword->StoredIndex, Entity->StoredIndex, false);
                            AddFlags(Sword, EntityFlag_Movable|EntityFlag_Collides);
                        }
                        else if(IsSet(Entity->Sword, EntityFlag_NonSpatial))
                        {
                            ClearFlags(Entity->Sword, EntityFlag_NonSpatial);
                            Entity->Sword->P = SwordP;
                            Entity->Sword->dP = Entity->dP + dSwordP*SwordSpeed;
                            Entity->Sword->DistanceLimit = 3.0f;
                        }
                    }

                    for(s32 VolumeIndex = 0;
                        VolumeIndex < Entity->CollisionGroup->VolumeCount;
                        ++VolumeIndex)
                    {
                        entity_collision_volume *Volume = Entity->CollisionGroup->Volumes + VolumeIndex;
                        PushPixmap(RenderGroup, V3(0, 0, 0), 1.5f*1.2f, GetFirstPixmapID(TranState->Assets, Asset_Shadow));
                        PushPixmap(RenderGroup, V3(0, 0, 0), 1.5f*1.2f, HeroPixmaps.Head);
                        PushPixmap(RenderGroup, V3(0, 0, 0), 1.5f*1.2f, HeroPixmaps.Torso);
                        PushPixmap(RenderGroup, V3(0, 0, 0), 1.5f*1.2f, HeroPixmaps.Cape);
                        // PushRect(RenderGroup, Volume->OffsetP, Volume->Dimension.Xy, V4(1.0f, 1.0f, 0.0f, 1.0f));
                        DrawHitPoints(RenderGroup, Entity, Volume->OffsetP);
                    }
                } break;
                case EntityType_Familiar:
                {
                    entity *ClosestHero = 0;
                    r32 ClosestHeroDistanceSquared = Square(10.0f);
                    for(s32 TestEntityIndex = 1;
                        TestEntityIndex < Region->EntityCount;
                        ++TestEntityIndex)
                    {
                        entity *Target = Region->Entities + TestEntityIndex;

                        if(Target->Type == EntityType_Hero)
                        {
                            r32 DistanceSquared = LengthSq(Target->P - Entity->P);
                            if(DistanceSquared < ClosestHeroDistanceSquared)
                            {
                                ClosestHero = Target;
                                ClosestHeroDistanceSquared = DistanceSquared;
                            }
                        }
                    }

                    r32 Acceleration = 0.0f;
                    if(ClosestHero && (ClosestHeroDistanceSquared > 5.0f))
                    {
                        //Acceleration = 30.0f;
                        r32 OneOverLength = 1.0f / SquareRoot(ClosestHeroDistanceSquared);
                        ddP = Acceleration * OneOverLength * (ClosestHero->P - Entity->P);
                    }

                    MoveSpec.Acceleration = Acceleration;
                    MoveSpec.Drag = 8.0f;

                    Entity->tBob += Input->dtForFrame;
                    if(Entity->tBob > 2.0f*Pi32)
                    {
                        Entity->tBob -= 2.0f*Pi32;
                    }
                    for(s32 VolumeIndex = 0;
                        VolumeIndex < Entity->CollisionGroup->VolumeCount;
                        ++VolumeIndex)
                    {
                        entity_collision_volume *Volume = Entity->CollisionGroup->Volumes + VolumeIndex;
                        PushPixmap(RenderGroup, V3(0, 0, 0), 1.5f, GetFirstPixmapID(TranState->Assets, Asset_Shadow));
                        PushPixmap(RenderGroup, V3(0, 0, 0), 1.5f, HeroPixmaps.Head);
                        // PushRect(RenderGroup, Volume->OffsetP, Volume->Dimension.Xy, V4(1.0f, 1.0f, 0.0f, 1.0f));
                    }

                    {
                        for(s32 ParticleSpawnIndex = 0;
                            ParticleSpawnIndex < 1;
                            ++ParticleSpawnIndex)
                        {
                            particle *Particle = GameState->Particles + GameState->ParticleCount++;
                            GameState->ParticleCount %= ArrayCount(GameState->Particles);

                            Particle->P = V3(RandomBetween(&GameState->EffectsEntropy, -0.05f, 0.05f), 0, 0);
                            Particle->dP = V3(RandomBetween(&GameState->EffectsEntropy, -0.01f, 0.01f), 7.0f*RandomBetween(&GameState->EffectsEntropy, 0.7f, 1.0f), 0.0f);
                            Particle->ddP = V3(0.0f, -9.8f, 0.0f);
                            Particle->Color = V4(RandomBetween(&GameState->EffectsEntropy, 0.75f, 1.0f),
                                                 RandomBetween(&GameState->EffectsEntropy, 0.75f, 1.0f),
                                                 RandomBetween(&GameState->EffectsEntropy, 0.75f, 1.0f),
                                                 1.0f);
                            Particle->dColor = V4(0, 0, 0, -0.25f);

                            MatchVector = {};
                            WeightVector = {};
                            char Gaurang[] = "NOTHINGS";
                            MatchVector.E[Tag_UnicodeCodepoint] = Gaurang[RandomChoice(&GameState->EffectsEntropy, ArrayCount(Gaurang)-1)];
                            WeightVector.E[Tag_UnicodeCodepoint] = 1.0f;
                            Particle->PixmapID = BestMatchPixmapID(TranState->Assets, Asset_Font, &MatchVector, &WeightVector);
                            // Particle->PixmapID = GetRandomPixmapFrom(TranState->Assets, Asset_Font, &GameState->EffectsEntropy);
                        }

                        ZeroSize(sizeof(GameState->ParticleCells), GameState->ParticleCells);

                        f32 GridScale = 0.25f;
                        v3 GridOrigin = V3(-0.5f*PARTICLE_CELL_DIM*GridScale, 0.0f, 0.0f);
                        for(s32 ParticleIndex = 0;
                            ParticleIndex < ArrayCount(GameState->Particles);
                            ++ParticleIndex)
                        {
                            particle *Particle = GameState->Particles + ParticleIndex;

                            v3 P = (1.0f/GridScale)*(Particle->P - GridOrigin);

                            s32 X = TruncateReal32ToInt32(P.X);
                            s32 Y = TruncateReal32ToInt32(P.Y);

                            if(X < 0) { X = 0; }
                            if(X >= PARTICLE_CELL_DIM) { X = PARTICLE_CELL_DIM - 1; }
                            if(Y < 0) { Y = 0; }
                            if(Y >= PARTICLE_CELL_DIM) { Y = PARTICLE_CELL_DIM - 1; }

                            particle_cell *Cell = &GameState->ParticleCells[Y][X];

                            f32 Density = Particle->Color.A;
                            Cell->Density += Density;
                            Cell->VelocityTimesDensity += Density*Particle->dP;
                        }
#if 0
                        for(s32 Y = 0;
                            Y < PARTICLE_CELL_DIM;
                            ++Y)
                        {
                            for(s32 X = 0;
                                X < PARTICLE_CELL_DIM;
                                ++X)
                            {
                                particle_cell *Cell = &GameState->ParticleCells[Y][X];

                                f32 Alpha = Clamp01(0.1f*Cell->Density);
                                PushRect(RenderGroup, GridScale*V3((f32)X, (f32)Y, 0) + GridOrigin, GridScale*V2(1.0f, 1.0f), V4(Alpha, Alpha, Alpha, 1.0f));
                            }
                        }
#endif
                        // NOTE(gaurang): Particle systme test.
                        for(s32 ParticleIndex = 0;
                            ParticleIndex < ArrayCount(GameState->Particles);
                            ++ParticleIndex)
                        {
                            particle *Particle = GameState->Particles + ParticleIndex;

                            v3 P = (1.0f/GridScale)*(Particle->P - GridOrigin);

                            s32 X = TruncateReal32ToInt32(P.X);
                            s32 Y = TruncateReal32ToInt32(P.Y);

                            if(X < 1) { X = 1; }
                            if(X > (PARTICLE_CELL_DIM-2)) { X = PARTICLE_CELL_DIM - 2; }
                            if(Y < 1) { Y = 1; }
                            if(Y > (PARTICLE_CELL_DIM-2)) { Y = PARTICLE_CELL_DIM - 2; }

                            particle_cell *CellCenter = &GameState->ParticleCells[Y][X];
                            particle_cell *CellUp = &GameState->ParticleCells[Y+1][X];
                            particle_cell *CellDown = &GameState->ParticleCells[Y-1][X];
                            particle_cell *CellLeft = &GameState->ParticleCells[Y][X-1];
                            particle_cell *CellRight = &GameState->ParticleCells[Y][X+1];

                            v3 Dispersion = {};

                            f32 DC = 1.0f;
                            Dispersion += DC*(CellCenter->Density - CellLeft->Density)*V3(-1.0f, 0.0f, 0.0f);
                            Dispersion += DC*(CellCenter->Density - CellRight->Density)*V3(1.0f, 0.0f, 0.0f);
                            Dispersion += DC*(CellCenter->Density - CellDown->Density)*V3(0.0f, -1.0f, 0.0f);
                            Dispersion += DC*(CellCenter->Density - CellUp->Density)*V3(0.0f, 1.0f, 0.0f);

                            v3 ddP_ = Particle->ddP + Dispersion;

                            // NOTE(gaurang): Simulate particle forward in time.
                            Particle->P += 0.5f*Square(Input->dtForFrame)*ddP_ + Input->dtForFrame*Particle->dP;
                            Particle->dP += Input->dtForFrame*ddP_;
                            Particle->Color += Input->dtForFrame*Particle->dColor;
                            v4 Color = Clamp01(Particle->Color);

                            if(Particle->P.Y < 0.0f)
                            {
                                f32 CoefficientOfRestitution = 0.3f;
                                f32 CoefficientOfFriction = 0.7f;
                                Particle->P.Y = -Particle->P.Y;
                                Particle->dP.Y = -CoefficientOfRestitution*Particle->dP.Y;
                                Particle->dP.X = CoefficientOfFriction*Particle->dP.X;
                            }

                            if(Color.A > 0.9f)
                            {
                                Color.A = 0.9f*Clamp01MapToRange(1.0f, Color.A, 0.9f);
                            }

                            // NOTE(gaurang): Render the particle.
                            // PushPixmap(RenderGroup, Particle->P, 1.0f, { GetFirstPixmapID(TranState->Assets, Asset_Head).Value + 3 }, Color);
                            PushPixmap(RenderGroup, Particle->P, 0.2f, Particle->PixmapID, Color);
                        }
                    }
                } break;
                case EntityType_Monster:
                {
                    for(s32 VolumeIndex = 0;
                        VolumeIndex < Entity->CollisionGroup->VolumeCount;
                        ++VolumeIndex)
                    {
                        entity_collision_volume *Volume = Entity->CollisionGroup->Volumes + VolumeIndex;
                        PushPixmap(RenderGroup, V3(0, 0, 0), 5.0f, GetFirstPixmapID(TranState->Assets, Asset_Shadow));
                        PushPixmap(RenderGroup, V3(0, 0, 0), 5.0f, HeroPixmaps.Torso);
                        // PushRect(RenderGroup, Volume->OffsetP, Volume->Dimension.Xy, V4(1.0f, 1.0f, 0.0f, 1.0f));
                        DrawHitPoints(RenderGroup, Entity, V3(0, 0, 0));
                    }
                } break;
                case EntityType_Sword:
                {
                    if(Entity->DistanceLimit == 0.0f)
                    {
                        Entity->dP = V3(0, 0, 0);
                        AddFlags(Entity, EntityFlag_NonSpatial);
                        ClearCollisionRule(World, Entity->StoredIndex);
                    }

                    PushPixmap(RenderGroup, V3(0, 0, 0), 0.5f, GetFirstPixmapID(TranState->Assets, Asset_Shadow));
                    PushPixmap(RenderGroup, V3(0, 0, 0.0f), 0.5f, GetFirstPixmapID(TranState->Assets, Asset_Sword));
                } break;
                case EntityType_Wall:
                {
                    for(s32 VolumeIndex = 0;
                        VolumeIndex < Entity->CollisionGroup->VolumeCount;
                        ++VolumeIndex)
                    {
                        entity_collision_volume *Volume = Entity->CollisionGroup->Volumes + VolumeIndex;
                        PushPixmap(RenderGroup, V3(0, 0, 0), 1.5f, GetFirstPixmapID(TranState->Assets, Asset_Tree));
                        // PushRect(RenderGroup, Volume->OffsetP, Volume->Dimension.Xy, V4(1.0f, 1.0f, 0.0f, 1.0f));
                    }
                } break;
                case EntityType_Space:
                {
#if 1
                    for(s32 VolumeIndex = 0;
                        VolumeIndex < Entity->CollisionGroup->VolumeCount;
                        ++VolumeIndex)
                    {
                        entity_collision_volume *Volume = Entity->CollisionGroup->Volumes + VolumeIndex;
                        // PushRectOutline(RenderGroup, V3(0, 0, 0), Volume->Dimension.XY, V4(0.0f, 0.5f, 1.0f, 1.0f));
                    }
#endif
                } break;
                case EntityType_Stair:
                {
                    for(s32 VolumeIndex = 0;
                        VolumeIndex < Entity->CollisionGroup->VolumeCount;
                        ++VolumeIndex)
                    {
                        entity_collision_volume *Volume = Entity->CollisionGroup->Volumes + VolumeIndex;
                        PushRect(RenderGroup, V3(0, 0, 0), Entity->WalkableDimension, V4(1.0f, 1.0f, 0.4f, 1.0f));
                        PushRect(RenderGroup, V3(0, 0, Entity->WalkableHeight), Entity->WalkableDimension, V4(1.0f, 0.6f, 0.4f, 1.0f));
                    }
                } break;
                default:
                {
                    InvalidCodePath;
                };
            }

            if(IsSet(Entity, EntityFlag_Movable))
            {
                MoveEntity(GameState, Region, EntityIndex, Input->dtForFrame, &MoveSpec, ddP);
            }
        }
    }

#if 0
    RenderGroup->Transform.Offset = V3(0, 0, 0);

    PushRectOutline(RenderGroup, V3(0, 0, 0), GetDim(ToRectangleXY(Region->UpdatableBounds)), V4(0.0f, 1.0f, 0.0f, 1.0f));
    PushRectOutline(RenderGroup, V3(0, 0, 0), GetDim(ToRectangleXY(Region->Bounds)), V4(1.0f, 0.0f, 0.0f, 1.0f));
    PushRectOutline(RenderGroup, V3(0, 0, 0), GetDim(ScreenBounds), V4(1.0f, 1.0f, 0.0f, 1.0f));
#endif

    EndSim(&GameState->WorldArena, World, Region);
    EndTemporaryMemory(SimMemory);

    pixmap Target;
    Target.Width = Video->Width;
    Target.Height = Video->Height;
    Target.BytesPerPixel = Video->BytesPerPixel;
    Target.Pitch = Video->Pitch;
    Target.Memory = Video->Memory;
    TiledRenderGroupToOutput(HighPriorityQueue, RenderGroup, &Target);

    EndTemporaryMemory(RenderMemory);

    CheckArena(&GameState->WorldArena);
    CheckArena(&TranState->TranArena);

    EvictAssetsAsNecessary(TranState->Assets);

    END_TIMED_BLOCK(GameUpdateAndRender);

    DebugReset();
    OverlayCycleCounter(Memory);

    TiledRenderGroupToOutput(HighPriorityQueue, DEBUGRenderGroup, &Target);
    DEBUGRenderGroup->PushBufferSize = 0;
}

GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    transient_state *TranState = (transient_state *)Memory->TransientStorage;

    OutputPlayingSounds(Audio, &GameState->AudioState, TranState->Assets, &TranState->TranArena);
}

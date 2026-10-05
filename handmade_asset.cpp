/*
  TODO(gaurang):

  - What stuff is allowed to be execute in parallel?
    - A background thread should not have the ability to spawn more background threads.
      - This implies that every routine that uses background tasks should have a Stall flag.

  - Our task routines are not thread safe.

  - Our Renderer is not thread safe.
    - MarkAssetSlotAsRecentlyUsed/AddAssetSlot/RemoveAssetSlot/etc are not thread safe.

  - If the least recently used asset is queued, we are going to stall
    the entire thing until it is loaded. Should we have the ability to
    kill work tasks?

  - EvictAssetsAsNecessary must be called after every rendergroup is
    finished working. Essentially, any background rendering processes
    will crash. (Lock?)

  - If we end up implementing a locking mechanism, then
    EvictAssetsAsNecessary can just be called inside
    AcquireAssetMemory when it runs out of memory.
*/

function u8 *
AcquireAssetMemory(game_assets *Assets, smm Size)
{
    u8 *Result = Platform.AllocateMemory(Size);
    Assets->TotalMemoryUsed += Size;
    return(Result);
}

function void
ReleaseAssetMemory(game_assets *Assets, smm Size, u8 *Memory)
{
    Platform.DeallocateMemory(Memory);
    Assets->TotalMemoryUsed -= Size;
}

function void
RemoveAssetSlotFromUsageList(game_assets *Assets, asset_slot *Slot)
{
    Slot->Prev->Next = Slot->Next;
    Slot->Next->Prev = Slot->Prev;

    Slot->Next = 0;
    Slot->Prev = 0;
}

function void
AddAssetSlotAsRecentlyUsed(game_assets *Assets, asset_slot *Slot)
{
    Assert(Slot->State != AssetState_Unloaded);
    asset_slot *Sentinel = &Assets->AssetSlotUsageSentinel;
    Slot->Prev = Sentinel;
    Slot->Next = Sentinel->Next;

    Slot->Next->Prev = Slot;
    Slot->Prev->Next = Slot;
}

function void
MarkAssetSlotAsRecentlyUsed(game_assets *Assets, asset_slot *Slot)
{
    Assert(Slot->State != AssetState_Unloaded);

    RemoveAssetSlotFromUsageList(Assets, Slot);

    asset_slot *Sentinel = &Assets->AssetSlotUsageSentinel;
    Slot->Prev = Sentinel;
    Slot->Next = Sentinel->Next;

    Slot->Next->Prev = Slot;
    Slot->Prev->Next = Slot;
}

function asset_slot *
GetLeastRecentlyUsedAsset(game_assets *Assets)
{
    asset_slot *Result = Assets->AssetSlotUsageSentinel.Prev;
    return(Result);
}

function void
EvictAsset(game_assets *Assets, asset_slot *Slot)
{
    Assert(Slot->State != AssetState_Unloaded);

    if(Slot->State == AssetState_Loaded)
    {
        ReleaseAssetMemory(Assets, Slot->MemorySize, Slot->Memory);
        RemoveAssetSlotFromUsageList(Assets, Slot);
        Slot->State = AssetState_Unloaded;
    }
    else
    {
        Assert(Slot->State == AssetState_Queued);
        InvalidCodePath;
    }
}

function void
EvictAssetsAsNecessary(game_assets *Assets)
{
    while(Assets->TotalMemoryUsed > Assets->TargetMemoryUsed)
    {
        asset_slot *Slot = GetLeastRecentlyUsedAsset(Assets);
        Assert(Slot);
        if(Slot != &Assets->AssetSlotUsageSentinel)
        {
            EvictAsset(Assets, Slot);
        }
        else
        {
            InvalidCodePath;
            break;
        }
    }
}

function platform_file_handle *
GetFileHandleFor(game_assets *Assets, s32 FileIndex)
{
    Assert(FileIndex < Assets->FileCount);
    platform_file_handle *Handle = Assets->Files[FileIndex].Handle;
    return(Handle);
}

struct load_asset_work
{
    asset_slot *Slot;
    platform_file_handle *Handle;

    s64 Offset;
    s64 Size;
    u8 *Destination;

    task_with_memory *Task;
    asset_state FinalState;
};

function void
LoadAssetWorkDirectly()
{
    Assert(!"NotImplemented");  // NOTE(gaurang): Day 166 - [00:12:42]
}

PLATFORM_WORK_QUEUE_CALLBACK(DoLoadAssetWork)
{
    load_asset_work *Work = (load_asset_work *)Data;

    // TODO(gaurang): Should we mark the asset as unloaded if
    // file-stream is down, and then at a separate place we can reset
    // the busted file-streams.
    if(PlatformNoFileErrors(Work->Handle))
    {
        Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Destination);
    }

    CompletePastWritesBeforeFutureWrites;
    Work->Slot->State = Work->FinalState;

    EndTaskWithMemory(Work->Task);
}

function void
LoadPixmap(game_assets *Assets, pixmap_id ID)
{
    if(ID.Value)
    {
        asset_slot *Slot = Assets->Slots + ID.Value;

        if(AtomicCompareExchangeInt32((s32 volatile *)&Slot->State, AssetState_Queued, AssetState_Unloaded) == AssetState_Unloaded)
        {
            task_with_memory *Task = BeginTaskWithMemory(Assets->TranState_);
            if(Task)
            {
                hha_pixmap *Source = &Assets->Assets[ID.Value].Pixmap;
                pixmap *Dest = &Slot->Pixmap;

                Dest->AlignPercentage = V2(Source->AlignPercentage[0], Source->AlignPercentage[1]);
                Dest->WidthOverHeight = (f32)Source->Dim[0] / (f32)Source->Dim[1];
                Dest->Width = Source->Dim[0];
                Dest->Height = Source->Dim[1];
                Dest->BytesPerPixel = 4;
                Dest->Pitch = Dest->Width * Dest->BytesPerPixel;

                Slot->MemorySize = Dest->Pitch*Dest->Height;
                Slot->Memory = AcquireAssetMemory(Assets, Slot->MemorySize);// PushSize(&Assets->Arena, Size);
                Dest->Memory = Slot->Memory;

                load_asset_work *Work = PushStruct(&Task->Arena, load_asset_work);
                Work->Destination = Dest->Memory;
                Work->Handle = GetFileHandleFor(Assets, Slot->FileIndex);
                Work->Offset = Assets->Assets[ID.Value].DataOffset;
                Work->Size = Slot->MemorySize;
                Work->Slot = Slot;
                Work->Task = Task;
                Work->FinalState = AssetState_Loaded;

                Platform.AddEntry(LowPriorityQueue, DoLoadAssetWork, (void *)Work);

                // TODO(gaurang): Should this happen before addEntry?
                AddAssetSlotAsRecentlyUsed(Assets, Slot);
            }
            else
            {
                Slot->State = AssetState_Unloaded;
            }
        }
        else
        {
            MarkAssetSlotAsRecentlyUsed(Assets, Slot);
        }
    }
}

function void
LoadSound(game_assets *Assets, sound_id ID)
{
    if(ID.Value)
    {
        asset_slot *Slot = Assets->Slots + ID.Value;

        if(AtomicCompareExchangeInt32((s32 volatile *)&Slot->State, AssetState_Queued, AssetState_Unloaded) == AssetState_Unloaded)
        {
            task_with_memory *Task = BeginTaskWithMemory(Assets->TranState_);
            if(Task)
            {
                hha_sound *Source = &Assets->Assets[ID.Value].Sound;
                sound *Dest = &Slot->Sound;

                Dest->SampleCount = Source->SampleCount;
                Dest->ChannelCount = Source->ChannelCount;

                Slot->MemorySize = Dest->SampleCount*Dest->ChannelCount*sizeof(s16);
                Slot->Memory = AcquireAssetMemory(Assets, Slot->MemorySize); //PushSize(&Assets->Arena, Size);
                u8 *SampleData = Slot->Memory;
                for(s32 Index = 0;
                    Index < Dest->ChannelCount;
                    ++Index)
                {
                    Dest->Samples[Index] = (s16 *)(SampleData + Dest->SampleCount*sizeof(s16)*Index);
                }

                load_asset_work *Work = PushStruct(&Task->Arena, load_asset_work);
                Work->Destination = SampleData;
                Work->Handle = GetFileHandleFor(Assets, Slot->FileIndex);
                Work->Offset = Assets->Assets[ID.Value].DataOffset;
                Work->Size = Slot->MemorySize;
                Work->Slot = Slot;
                Work->Task = Task;
                Work->FinalState = AssetState_Loaded;

                Platform.AddEntry(LowPriorityQueue, DoLoadAssetWork, (void *)Work);

                // TODO(gaurang): Should this happen before addEntry?
                AddAssetSlotAsRecentlyUsed(Assets, Slot);
            }
            else
            {
                Slot->State = AssetState_Unloaded;
            }
        }
        else
        {
            MarkAssetSlotAsRecentlyUsed(Assets, Slot);
        }
    }
}

function void
LoadFont(game_assets *Assets, pixmap_id ID)
{
    if(ID.Value)
    {
        asset_slot *Slot = Assets->Slots + ID.Value;

        if(AtomicCompareExchangeInt32((s32 volatile *)&Slot->State, AssetState_Queued, AssetState_Unloaded) == AssetState_Unloaded)
        {
            task_with_memory *Task = BeginTaskWithMemory(Assets->TranState_);
            if(Task)
            {
                hha_font *Source = &Assets->Assets[ID.Value].Font;
                font *Dest = &Slot->Font;

                Dest->CodepointCount = Source->CodepointCount;
                Dest->LineAdvance = Source->LineAdvance;

                Slot->MemorySize = Dest->CodepointCount*(sizeof(pixmap_id)*sizeof(f32));
                Slot->Memory = AcquireAssetMemory(Assets, Slot->MemorySize);// PushSize(&Assets->Arena, Size);
                Dest->Codepoints = (pixmap_id *)Slot->Memory;
                Dest->HorizontalAdvance = (f32 *)(Dest->Codepoints + Dest->CodepointCount);

                load_asset_work *Work = PushStruct(&Task->Arena, load_asset_work);
                Work->Destination = Slot->Memory;
                Work->Handle = GetFileHandleFor(Assets, Slot->FileIndex);
                Work->Offset = Assets->Assets[ID.Value].DataOffset;
                Work->Size = Slot->MemorySize;
                Work->Slot = Slot;
                Work->Task = Task;
                Work->FinalState = AssetState_Loaded;

                Platform.AddEntry(LowPriorityQueue, DoLoadAssetWork, (void *)Work);

                // TODO(gaurang): Should this happen before addEntry?
                AddAssetSlotAsRecentlyUsed(Assets, Slot);
            }
            else
            {
                Slot->State = AssetState_Unloaded;
            }
        }
        else
        {
            MarkAssetSlotAsRecentlyUsed(Assets, Slot);
        }
    }
}

function s32
BestMatchAsset(game_assets *Assets, asset_type_id TypeID, asset_vector *MatchVector, asset_vector *WeightVector)
{
    f32 BestDiff = F32Max;
    s32 BestIndex = 0;

    asset_type *Type = Assets->AssetTypes + TypeID;
    for(s32 AssetIndex = Type->FirstAssetIndex;
        AssetIndex < Type->OnePastLastAssetIndex;
        ++AssetIndex)
    {
        hha_asset *Asset = Assets->Assets + AssetIndex;

        f32 TotalWeightedDiff = 0.0f;
        for(s32 TagIndex = Asset->FirstTagIndex;
            TagIndex < Asset->OnePastLastTagIndex;
            ++TagIndex)
        {
            hha_tag *Tag = Assets->Tags + TagIndex;
            f32 Difference = MatchVector->E[Tag->ID] - Tag->Value;
            f32 Weighted = WeightVector->E[Tag->ID]*AbsoluteValue(Difference);
            TotalWeightedDiff += Weighted;
        }

        if(BestDiff > TotalWeightedDiff)
        {
            BestDiff = TotalWeightedDiff;
            BestIndex = AssetIndex;
        }
    }

    return(BestIndex);
}

function pixmap_id
BestMatchPixmapID(game_assets *Assets, asset_type_id TypeID, asset_vector *MatchVector, asset_vector *WeightVector)
{
    pixmap_id Result = { BestMatchAsset(Assets, TypeID, MatchVector, WeightVector) };
    return(Result);
}

function font_id
BestMatchFontID(game_assets *Assets, asset_type_id TypeID, asset_vector *MatchVector, asset_vector *WeightVector)
{
    font_id Result = { BestMatchAsset(Assets, TypeID, MatchVector, WeightVector) };
    return(Result);
}

function s32
GetFirstSlotID(game_assets *Assets, asset_type_id TypeID)
{
    s32 Result = 0;

    asset_type *Type = Assets->AssetTypes + TypeID;
    if(Type->FirstAssetIndex < Type->OnePastLastAssetIndex)
    {
        hha_asset *Asset = Assets->Assets + Type->FirstAssetIndex;
        Result = Type->FirstAssetIndex;
    }

    return(Result);
}

function pixmap_id
GetFirstPixmapID(game_assets *Assets, asset_type_id TypeID)
{
    pixmap_id Result = { GetFirstSlotID(Assets, TypeID) };
    return(Result);
}

function sound_id
GetFirstSoundID(game_assets *Assets, asset_type_id TypeID)
{
    sound_id Result = { GetFirstSlotID(Assets, TypeID) };
    return(Result);
}

function s32
RandomAssetFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series)
{
    s32 Result = 0;

    asset_type *Type = Assets->AssetTypes + TypeID;
    if(Type->FirstAssetIndex < Type->OnePastLastAssetIndex)
    {
        s32 AssetIndex = Type->FirstAssetIndex + RandomChoice(Series, Type->OnePastLastAssetIndex - Type->FirstAssetIndex);
        Result = AssetIndex;
    }

    return(Result);
}

function pixmap_id
GetRandomPixmapFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series)
{
    pixmap_id Result = { RandomAssetFrom(Assets, TypeID, Series) };
    return(Result);
}

function sound_id
GetRandomSoundFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series)
{
    sound_id Result = { RandomAssetFrom(Assets, TypeID, Series) };
    return(Result);
}

function game_assets *
AllocateAssets(memory_arena *Arena, smm Size, transient_state *TranState_)
{
    game_assets* Assets = PushStruct(Arena, game_assets);
    SubArena(&Assets->Arena, Arena, Size);
    Assets->TranState_ = TranState_;

    Assets->TargetMemoryUsed = Size;
    Assets->TotalMemoryUsed = 0;
    Assets->AssetSlotUsageSentinel.Next = &Assets->AssetSlotUsageSentinel;
    Assets->AssetSlotUsageSentinel.Prev = &Assets->AssetSlotUsageSentinel;

    Assets->TagCount = 1;
    Assets->AssetCount = 1;

    {
        platform_file_group FileGroup = Platform.GetAllFilesOfTypeBegin(PlatformFileType_AssetFile);
        Assets->FileCount = FileGroup.FileCount;
        Assets->Files = PushArray(&Assets->Arena, Assets->FileCount, asset_file);
        for(s32 FileIndex = 0;
            FileIndex < Assets->FileCount;
            ++FileIndex)
        {
            asset_file *File = Assets->Files + FileIndex;

            File->TagBase = Assets->TagCount;

            File->Handle = Platform.OpenFile(FileGroup, FileIndex);
            Platform.ReadDataFromFile(File->Handle, 0, sizeof(File->Header), &File->Header);

            hha_header *Header = &File->Header;

            File->AssetTypeArray = PushArray(&Assets->Arena, Header->AssetTypeCount, hha_asset_type);
            Platform.ReadDataFromFile(File->Handle, Header->AssetTypes, Header->AssetTypeCount*sizeof(hha_asset_type), File->AssetTypeArray);

            if(Header->MagicValue != HHA_MAGIC_VALUE)
            {
                Platform.FileError(File->Handle, "HHA file has an invalid magic value.");
            }
            if(Header->Version != HHA_VERSION)
            {
                Platform.FileError(File->Handle, "HHA file is of a later version.");
            }

            if(PlatformNoFileErrors(File->Handle))
            {
                Assets->TagCount += Header->TagCount-1;
                Assets->AssetCount += Header->AssetCount-1;
            }
            else
            {
                // TODO(gaurang): Eventually, have some way of notifying users of bogus files?
                InvalidCodePath;
            }
        }
        Platform.GetAllFilesOfTypeEnd(FileGroup);
    }

    Assets->Tags = PushArray(&Assets->Arena, Assets->TagCount, hha_tag);
    Assets->Assets = PushArray(&Assets->Arena, Assets->AssetCount, hha_asset);
    Assets->Slots = PushArray(&Assets->Arena, Assets->AssetCount, asset_slot);

    for(s32 FileIndex = 0;
        FileIndex < Assets->FileCount;
        ++FileIndex)
    {
        asset_file *File = Assets->Files + FileIndex;
        if(PlatformNoFileErrors(File->Handle))
        {
            smm TagArraySize = sizeof(hha_tag)*(File->Header.TagCount - 1);
            Platform.ReadDataFromFile(File->Handle, File->Header.Tags + sizeof(hha_tag), TagArraySize, Assets->Tags + File->TagBase);
        }
    }

    s32 AssetCount = 1;
    for(s32 DestTypeID = 1;
        DestTypeID < Asset_Count;
        ++DestTypeID)
    {
        asset_type *Dest = Assets->AssetTypes + DestTypeID;
        Dest->FirstAssetIndex = AssetCount;

        for(s32 FileIndex = 0;
            FileIndex < Assets->FileCount;
            ++FileIndex)
        {
            asset_file *File = Assets->Files + FileIndex;
            if(PlatformNoFileErrors(File->Handle))
            {
                for(s32 SourceIndex = 1;
                    SourceIndex < File->Header.AssetTypeCount;
                    ++SourceIndex)
                {
                    hha_asset_type *Source = File->AssetTypeArray + SourceIndex;
                    if(Source->TypeID == DestTypeID)
                    {
                        s32 AssetCountForType = (Source->OnePastLastAssetIndex -
                                                 Source->FirstAssetIndex);

                        Platform.ReadDataFromFile(File->Handle,
                                                 File->Header.Assets + Source->FirstAssetIndex*sizeof(hha_asset),
                                                 AssetCountForType*sizeof(hha_asset),
                                                 Assets->Assets + AssetCount);

                        for(s32 AssetIndex = AssetCount;
                            AssetIndex < (AssetCountForType + AssetCount);
                            ++AssetIndex)
                        {
                            hha_asset *Asset = Assets->Assets + AssetIndex;
                            asset_slot *Slot = Assets->Slots + AssetIndex;
                            Slot->FileIndex = FileIndex;

                            if(Asset->FirstTagIndex == 0)
                            {
                                Asset->FirstTagIndex += 1;
                            }

                            Asset->FirstTagIndex += File->TagBase-1;
                            Asset->OnePastLastTagIndex += File->TagBase-1;
                        }
                        AssetCount += AssetCountForType;
                    }
                }
            }
        }

        Dest->OnePastLastAssetIndex = AssetCount;
    }

    return(Assets);
}

function asset_slot *
GetPixmapSlot(game_assets *Assets, pixmap_id ID)
{
    asset_slot *Result = 0;
    if(ID.Value < Assets->AssetCount)
    {
        asset_slot *Slot = Assets->Slots + ID.Value;
        if(Slot->State == AssetState_Loaded)
        {
            Result = Assets->Slots + ID.Value;
            MarkAssetSlotAsRecentlyUsed(Assets, Result);
        }
    }
    return(Result);
}

function asset_slot *
GetSoundSlot(game_assets *Assets, sound_id ID)
{
    asset_slot *Result = 0;
    if(ID.Value < Assets->AssetCount)
    {
        asset_slot *Slot = Assets->Slots + ID.Value;
        if(Slot->State == AssetState_Loaded)
        {
            Result = Assets->Slots + ID.Value;
            MarkAssetSlotAsRecentlyUsed(Assets, Result);
        }
    }
    return(Result);
}

function asset_slot *
GetFontSlot(game_assets *Assets, font_id ID)
{
    asset_slot *Result = 0;
    if(ID.Value < Assets->AssetCount)
    {
        asset_slot *Slot = Assets->Slots + ID.Value;
        if(Slot->State == AssetState_Loaded)
        {
            Result = Assets->Slots + ID.Value;
            MarkAssetSlotAsRecentlyUsed(Assets, Result);
        }
    }
    return(Result);
}

function hha_pixmap *
GetPixmapInfo(game_assets *Assets, pixmap_id ID)
{
    Assert(ID.Value < Assets->AssetCount);
    hha_pixmap *Result = &Assets->Assets[ID.Value].Pixmap;
    return(Result);
}

function u32
GetClampedCodepoint(asset_slot *Slot, u32 Codepoint)
{
    if(Codepoint >= Slot->Font.CodepointCount)
    {
        Codepoint = 0;
    }
    return(Codepoint);
}

function f32
GetHorizontalAdvanceForPair(asset_slot *Slot, u32 Prev, u32 Current)
{
    Prev = GetClampedCodepoint(Slot, Prev);
    Current = GetClampedCodepoint(Slot, Current);
    f32 Result = Slot->Font.HorizontalAdvance[Prev*Slot->Font.CodepointCount + Current];
    return(Result);
}

function pixmap_id
GetPixmapIDForGlyph(asset_slot *Slot, u32 Codepoint)
{
    pixmap_id Result = Slot->Font.Codepoints[GetClampedCodepoint(Slot, Codepoint)];

    return(Result);
}

function f32
GetLineAdvanceFor(asset_slot *Slot)
{
    f32 Result = Slot->Font.LineAdvance;
    return(Result);
}

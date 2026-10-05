struct pixmap_id
{
    s32 Value;
};

struct sound_id
{
    s32 Value;
};

struct font_id
{
    s32 Value;
};

struct sound
{
    s32 ChannelCount;
    s32 SampleCount;
    s16 *Samples[2];
};

struct font
{
    u32 CodepointCount;
    pixmap_id *Codepoints;
    f32 *HorizontalAdvance;
    f32 LineAdvance;
};

enum asset_state
{
    AssetState_Unloaded,
    AssetState_Queued,
    AssetState_Loaded,
};

struct asset_slot
{
    asset_state State;
    s32 FileIndex;
    union
    {
        pixmap Pixmap;
        sound Sound;
        font Font;
    };

    smm MemorySize;
    u8 *Memory;

    // NOTE(gaurang): Used for computing LeastRecentlyUsedAsset.
    asset_slot *Prev;
    asset_slot *Next;
};

struct asset_vector
{
    f32 E[Tag_Count];
};

struct asset_type
{
    s32 FirstAssetIndex;
    s32 OnePastLastAssetIndex;
};

struct transient_state;

struct asset_file
{
    platform_file_handle *Handle;
    hha_header Header;
    hha_asset_type *AssetTypeArray;
    s32 TagBase;
};

struct game_assets
{
    asset_slot AssetSlotUsageSentinel;

    s32 FileCount;
    asset_file *Files;

    s32 TagCount;
    hha_tag *Tags;

    s32 AssetCount;
    hha_asset *Assets;

    asset_type AssetTypes[Asset_Count];

    asset_slot *Slots;

    smm TargetMemoryUsed;
    smm TotalMemoryUsed;
    transient_state *TranState_;
    memory_arena Arena;
};

function asset_slot *
GetPixmapSlot(game_assets *Assets, pixmap_id ID);
function asset_slot *
GetSoundSlot(game_assets *Assets, sound_id ID);
function void
LoadPixmap(game_assets *Assets, pixmap_id ID);
function void
LoadSound(game_assets *Assets, sound_id ID);
function void
PrefetchPixmap(game_assets *Assets, pixmap_id ID) { LoadPixmap(Assets, ID); }
function void
PrefetchSound(game_assets *Assets, sound_id ID) { LoadSound(Assets, ID); }

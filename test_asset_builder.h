struct pixmap_id
{
    s32 Value;
};

struct sound_id
{
    s32 Value;
};

enum asset_state
{
    AssetState_Unloaded,
    AssetState_Queued,
    AssetState_Loaded,
    AssetState_Locked,
};

struct asset_vector
{
    f32 E[Tag_Count];
};

enum asset_type
{
    AssetType_None,
    AssetType_Sound,
    AssetType_Pixmap,
    AssetType_Character,
};

struct asset
{
    char *FileName;
    char *FontName;
    asset_type Type;
    u32 Codepoint;
};

#define VERY_LARGE_NUMBER 4096

struct game_assets
{
    hha_asset_type *DebugAssetType;
    s32 AssetIndex;

    s32 TagCount;
    hha_tag Tags[VERY_LARGE_NUMBER];

    s32 AssetCount;
    asset AssetSources[VERY_LARGE_NUMBER];
    hha_asset Assets[VERY_LARGE_NUMBER];

    s32 AssetTypeCount;
    hha_asset_type AssetTypes[Asset_Count];
};

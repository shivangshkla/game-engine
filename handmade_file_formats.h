enum asset_tag_id
{
    Tag_Smoothness,
    Tag_Flatness,
    Tag_FacingDirection,
    Tag_UnicodeCodepoint,

    Tag_Count
};

enum asset_type_id
{
    Asset_None,

    Asset_Tree,
    Asset_Shadow,
    Asset_Sword,

    Asset_Grass,
    Asset_Ground,
    Asset_Tuft,

    Asset_Head,
    Asset_Cape,
    Asset_Torso,

    Asset_Font,

    Asset_Bloop,
    Asset_Drop,
    Asset_Glide,
    Asset_Music,
    Asset_Crack,
    Asset_Puhp,

    Asset_Count
};

#define HHA_CODE(a, b, c, d) ((a << 0) | (b << 8) | (c << 16) | (d << 24))

#pragma pack(push, 1)
struct hha_header
{
#define HHA_MAGIC_VALUE HHA_CODE('h', 'h', 'a', 'f')
    u32 MagicValue;

#define HHA_VERSION 0
    s32 Version;

    s32 TagCount;
    s32 AssetCount;
    s32 AssetTypeCount;

    s64 Tags; // [TagCount]hha_tag
    s64 Assets; // [AssetCount]hha_asset
    s64 AssetTypes; // [AssetTypeCount]hha_asset_type
};

struct hha_tag
{
    s32 ID;
    f32 Value;
};

struct hha_pixmap
{
    s32 Dim[2];
    f32 AlignPercentage[2];
    // NOTE(gaurang): Data is:
    // u32 Pixels[Dim[1]][Dim[0]];
};

struct hha_sound
{
    s32 SampleCount;
    s32 ChannelCount;

    // NOTE(gaurang): Data is:
    // s16 Channels[ChannelCount][SampleCount];
};

struct hha_font
{
    u32 CodepointCount;
    f32 LineAdvance;

    // NOTE(gaurang): Data is:
    // bitmap_id Codepoints[CodepointCount];
    // f32 HorizontalAdvance[CodepointCount][CodepointCount];
};

struct hha_asset
{
    s64 DataOffset;

    s32 FirstTagIndex;
    s32 OnePastLastTagIndex;

    union
    {
        hha_pixmap Pixmap;
        hha_sound Sound;
        hha_font Font;
    };
};

struct hha_asset_type
{
    s32 TypeID;
    s32 FirstAssetIndex;
    s32 OnePastLastAssetIndex;
};

#pragma pack(pop)

struct pixmap
{
    v2 AlignPercentage;
    r32 WidthOverHeight;

    s32 Width;
    s32 Height;
    s32 BytesPerPixel;
    s32 Pitch;
    u8 *Memory;
};

function v4
Unpack4x8(u32 Packed)
{
    v4 Result;

    Result =  V4((r32)((Packed >> 16) & 0xFF),
                 (r32)((Packed >> 8) & 0xFF),
                 (r32)((Packed >> 0) & 0xFF),
                 (r32)((Packed >> 24) & 0xFF));
    return(Result);
}

// NOTE(gaurang): Render group entry is a "compact discriminated union".
enum render_group_entry_type
{
    RenderGroupEntryType_render_group_entry_clear,
    RenderGroupEntryType_render_group_entry_pixmap,
    RenderGroupEntryType_render_group_entry_rectangle,
};

struct render_group_entry_header
{
    render_group_entry_type Type;
};

struct render_group_entry_clear
{
    v4 Color;
};

struct render_group_entry_pixmap
{
    v3 P;
    v2 Dim;
    pixmap *Pixmap;
    v4 Tint;
};

struct render_group_entry_rectangle
{
    v3 P;
    v2 Dim;
    v4 Color;
};

struct render_transform
{
    r32 FocalLength;
    r32 TargetDistanceFromCamera;

    r32 Height;
    r32 Width;

    v3 Offset;
};

struct game_assets;

struct render_group
{
    u32 MaxPushBufferSize;
    u32 PushBufferSize;
    u8 *PushBufferBase;

    game_assets *Assets;

    render_transform Transform;

    s32 MissingResourceCount;
};

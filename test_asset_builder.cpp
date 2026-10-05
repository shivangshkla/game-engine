#define _CRT_SECURE_NO_WARNINGS

#define USE_FONTS_ON_WINDOWS 1

#if USE_FONTS_ON_WINDOWS

#pragma comment(lib, "gdi32")

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#else

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#endif

#include <stdio.h>
#include <stdlib.h>

#include "handmade_platform.h"
#include "handmade_intrinsics.h"
#include "handmade_math.h"
#include "handmade_file_formats.h"
#include "test_asset_builder.h"

struct pixmap
{
    s32 Width;
    s32 Height;
    s32 BytesPerPixel;
    s32 Pitch;
    u8 *Memory;

    void *Free;
};

#pragma pack(push, 1)
struct bitmap
{
    struct
    {
        u16 Type;
        u32 Size;
        u16 Reserved1;
        u16 Reserved2;
        u32 OffBits;
    } FileHeader;

    struct
    {
        u32 Size;
        s32 Width;
        s32 Height;
        u16 Planes;
        u16 BitCount;
        u32 Compression;
        u32 SizeImage;
        s32 XPelsPerMeter;
        s32 YPelsPerMeter;
        u32 ClrUsed;
        u32 ClrImportant;
        u32 RedMask;
        u32 GreenMask;
        u32 BlueMask;
        u32 AlphaMask;
    } DataHeader;
};


struct wave_header
{
    u32 ID;
    u32 Size;
    u32 WaveID;
};

struct wave_chunk
{
    u32 ID;
    u32 Size;
};

struct wave_fmt
{
    u16 wFormatTag;
    u16 nChannels;
    u32 nSamplesPerSec;
    u32 nAvgBytesPerSec;
    u16 nBlockAlign;
    u16 wBitsPerSample;
    u16 cbSize;
    u16 wValidBitsPerSample;
    u32 dwChannelMask;
    u8 SubFormat[16];
};

#pragma pack(pop)

struct entire_file
{
    s64 Count;
    u8 *Data;
};

function entire_file
ReadEntireFile(char *FileName)
{
    entire_file Result = {};

    FILE *In = fopen(FileName, "rb"); Assert(In);

    _fseeki64(In, 0, SEEK_END);
    Result.Count = _ftelli64(In);
    _fseeki64(In, 0, SEEK_SET);
    Result.Data = (u8 *)calloc(Result.Count+64, 1); Assert(Result.Data);

    fread(Result.Data, Result.Count, 1, In);

    fclose(In);

    return(Result);
}

function void
AddAssetCharacter(game_assets *Assets, char *FileName, char *FontName, u32 Codepoint, f32 AlignPercentageX = 0.5f, f32 AlignPercentageY = 0.5f)
{
    s32 AssetIndex = Assets->AssetIndex = Assets->DebugAssetType->OnePastLastAssetIndex++;
    asset *Source = Assets->AssetSources + AssetIndex;
    hha_asset *Dest = Assets->Assets + AssetIndex;

    Dest->FirstTagIndex = Assets->TagCount;
    Dest->OnePastLastTagIndex = Dest->FirstTagIndex;
    Source->Type = AssetType_Character;
    Source->FileName = FileName;
    Source->FontName = FontName;
    Source->Codepoint = Codepoint;
    Dest->Pixmap.AlignPercentage[0] = AlignPercentageX;
    Dest->Pixmap.AlignPercentage[1] = AlignPercentageY;
}

function pixmap
MakeEmptyPixmap(s32 Width, s32 Height, b32 ClearToZero = true)
{
    pixmap Result = {};

    Result.Width = Width;
    Result.Height = Height;
    Result.BytesPerPixel = 4;
    Result.Pitch = Result.Width*Result.BytesPerPixel;

    memory_index PixmapMemorySize = Result.Height*Result.Pitch;
    if(ClearToZero)
    {
        Result.Memory = (u8 *)calloc(PixmapMemorySize, 1);
    }
    else
    {
        Result.Memory = (u8 *)malloc(PixmapMemorySize);
    }

    Result.Free = Result.Memory;

    return(Result);
}

function pixmap
GetGlyphPixmap(char *FileName, char *FontName, u32 Codepoint, hha_asset *Asset)
{
    pixmap Result = {};

#if USE_FONTS_ON_WINDOWS
    local_persist TEXTMETRIC TextMetric = {};
    local_persist HDC DeviceContext = 0;
    if(!DeviceContext)
    {
        AddFontResourceExA(FileName, FR_PRIVATE, 0);
        int Height = 128;
        HFONT Font = CreateFontA(Height, 0, 0, 0,
                                 FW_NORMAL, // NOTE(gaurang): Weight
                                 FALSE, // NOTE(gaurang): Italic
                                 FALSE,  // NOTE(gaurang): Underline
                                 FALSE,  // NOTE(gaurang): StrikeOut
                                 DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS,
                                 ANTIALIASED_QUALITY,
                                 DEFAULT_PITCH|FF_DONTCARE,
                                 FontName);
        DeviceContext = CreateCompatibleDC(0);

        BITMAPINFO Info = {};

        Info.bmiHeader.biSize = sizeof(Info.bmiHeader);
        Info.bmiHeader.biWidth = 1024;
        Info.bmiHeader.biHeight = 1024;
        Info.bmiHeader.biPlanes = 1;
        Info.bmiHeader.biBitCount = 32;
        Info.bmiHeader.biCompression = BI_RGB;

        void *Bits;
        HBITMAP Bitmap = CreateDIBSection(DeviceContext, &Info, DIB_RGB_COLORS, &Bits, 0, 0);

        SelectObject(DeviceContext, Bitmap);
        SelectObject(DeviceContext, Font);
        SetBkColor(DeviceContext, RGB(0, 0, 0));

        GetTextMetrics(DeviceContext, &TextMetric);
    }

    char CheesePoint = (char)Codepoint;

    SIZE Size; GetTextExtentPoint32(DeviceContext, &CheesePoint, 1, &Size);

    int Width = Size.cx;
    int Height = Size.cy;

    // PatBlt(DeviceContext, 0, 0, Width, Height, BLACKNESS);

    SetTextColor(DeviceContext, RGB(255, 255, 255));
    TextOut(DeviceContext, 0, 0, &CheesePoint, 1);

    Result = MakeEmptyPixmap(Width+2, Height+2);

    u32 *DestRow = (u32 *)Result.Memory + (Result.Height-2)*Result.Width;
    for(s32 Y = 0;
        Y < Height;
        ++Y)
    {
        u32 *Dest = DestRow + 1;
        for(s32 X = 0;
            X < Width;
            ++X)
        {
            u32 G = GetPixel(DeviceContext, X, Y) & 0xFF;
            u32 A = G;
            *Dest++ = (A << 24) | (G << 16) | (G << 8) | (G << 0);
        }
        DestRow -= Result.Width;
    }

    Asset->Pixmap.AlignPercentage[0] = 1.0f / Result.Width;
    Asset->Pixmap.AlignPercentage[1] = (1.0f + TextMetric.tmDescent) / Result.Height;

#else
    entire_file TTFFile = ReadEntireFile(FileName);

    stbtt_fontinfo Font;

    stbtt_InitFont(&Font, TTFFile.Data, stbtt_GetFontOffsetForIndex(TTFFile.Data,0));

    f32 Scale = stbtt_ScaleForPixelHeight(&Font, 128.0f);
    int Ascent;
    int Descent;
    stbtt_GetFontVMetrics(&Font, &Ascent,&Descent,0);
    f32 Baseline = Descent*Scale;

    int Width, Height, OffsetX, OffsetY;
    u8 *MonoBitmap = stbtt_GetCodepointBitmap(&Font, 0, Scale,
                                              Codepoint, &Width, &Height, &OffsetX, &OffsetY);

    Result = MakeEmptyPixmap(Width, Height, false);

    u32 *Dest_ = (u32 *)Result.Memory;
    for(s32 Y = 0;
        Y < Result.Height;
        ++Y)
    {
        for(s32 X = 0;
            X < Result.Width;
            ++X)
        {
            u32 G = MonoBitmap[(Height - Y - 1)*Width + X];
            u32 A = 0xFF;//G;
            Dest_[Y*Result.Width + X] = (A << 24) | (G << 16) | (G << 8) | (G << 0);
        }
    }

    stbtt_FreeBitmap(MonoBitmap, 0);

    free(TTFFile.Data);

    Dest->Pixmap.AlignPercentage[0] = 1.0f / Result.Width;
    Dest->Pixmap.AlignPercentage[1] = -(1.0f + Baseline - 128) / Result.Height;

#endif

    return(Result);
}

function pixmap
LoadBMP(char *FileName)
{
    pixmap Result = {};

    entire_file File = ReadEntireFile(FileName);

    Result.Free = File.Data;

    bitmap *Bitmap = (bitmap *)File.Data;
    Assert(Bitmap->DataHeader.Height > 0);
    Assert(Bitmap->DataHeader.Compression == 3);
    Assert((Bitmap->DataHeader.BitCount / 8) == 4);

    u32 RedMask = Bitmap->DataHeader.RedMask;
    u32 GreenMask = Bitmap->DataHeader.GreenMask;
    u32 BlueMask = Bitmap->DataHeader.BlueMask;
    u32 AlphaMask = Bitmap->DataHeader.AlphaMask;

    bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
    bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
    bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
    bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);

    Assert(RedScan.Found && GreenScan.Found && BlueScan.Found && AlphaScan.Found);

    s32 RedShift = 16 - RedScan.Index;
    s32 GreenShift = 8 - GreenScan.Index;
    s32 BlueShift = 0 - BlueScan.Index;
    s32 AlphaShift = 24 - AlphaScan.Index;

    Result.Width = Bitmap->DataHeader.Width;
    Result.Height = Bitmap->DataHeader.Height;
    Result.BytesPerPixel = Bitmap->DataHeader.BitCount / 8;
    Result.Pitch = Result.Width * Result.BytesPerPixel;
    Result.Memory = File.Data + Bitmap->FileHeader.OffBits;

    u8 *Row = Result.Memory;
    for(s32 Y = 0;
        Y < Result.Height;
        ++Y)
    {
        u32 *Pixel = (u32 *)Row;
        for(s32 X = 0;
            X < Result.Width;
            ++X)
        {
            u32 Color32 = *Pixel;

#if 1
            v4 Texel = V4((r32)((Color32 >> RedScan.Index) & 0xFF),
                          (r32)((Color32 >> GreenScan.Index) & 0xFF),
                          (r32)((Color32 >> BlueScan.Index) & 0xFF),
                          (r32)((Color32 >> AlphaScan.Index) & 0xFF));

            Texel = SRGB255ToLinear1(Texel);

            Texel.RGB *= Texel.A;

            Texel = Linear1ToSRGB255(Texel);

            *Pixel++ = (((u32)(Texel.A + 0.5f) << 24) |
                        ((u32)(Texel.R + 0.5f) << 16) |
                        ((u32)(Texel.G + 0.5f) << 8) |
                        ((u32)(Texel.B + 0.5f) << 0));
#else
            *Pixel++ = (RotateLeft(Color32 & RedMask, RedShift) |
                        RotateLeft(Color32 & GreenMask, GreenShift) |
                        RotateLeft(Color32 & BlueMask, BlueShift) |
                        RotateLeft(Color32 & AlphaMask, AlphaShift));
#endif
        }
        Row += Result.Pitch;
    }

    return(Result);
}

#define RIFF_CODE(a, b, c, d) ((a << 0) | (b << 8) | (c << 16) | (d << 24))

enum
{
    WaveChunkID_fmt = RIFF_CODE('f', 'm', 't', ' '),
    WaveChunkID_data = RIFF_CODE('d', 'a', 't', 'a'),
    WaveChunkID_RIFF = RIFF_CODE('R', 'I', 'F', 'F'),
    WaveChunkID_WAVE = RIFF_CODE('W', 'A', 'V', 'E'),
};

struct riff_iterator
{
    wave_chunk *Chunk;
    u8 *At;
    u8 *Stop;
};

function riff_iterator
ParseChunkAt(u8 *At, u8 *Stop)
{
    riff_iterator Result = {};
    Result.At = At;
    Result.Stop = Stop;
    Result.Chunk = (wave_chunk *)Result.At;
    return(Result);
}

function b32
IsValid(riff_iterator Iterator)
{
    b32 Result = (Iterator.At < Iterator.Stop);
    return(Result);
}

function riff_iterator
NextChunk(riff_iterator Iterator)
{
    riff_iterator Result = Iterator;

    u32 Size = (Iterator.Chunk->Size + 1) & ~1;

    Result.At += sizeof(wave_chunk) + Size;
    Result.Chunk = (wave_chunk *)Result.At;

    return(Result);
}

function u32
GetChunkID(riff_iterator Iterator)
{
    u32 Result = Iterator.Chunk->ID;
    return(Result);
}

function u8 *
GetChunkData(riff_iterator Iterator)
{
    u8 *Result = Iterator.At + sizeof(wave_chunk);
    return(Result);
}

function s32
GetChunkDataSize(riff_iterator Iterator)
{
    s32 Result = Iterator.Chunk->Size;
    return(Result);
}

struct sound
{
    s32 ChannelCount;
    s32 SampleCount;
    s16 *Samples[2];

    void *Free;
};

function sound
LoadSound(char *FileName)
{
    sound Result = {};

    entire_file File = ReadEntireFile(FileName);

    Result.Free = File.Data;

    wave_header *Header = (wave_header *)File.Data;
    Assert(Header->ID == WaveChunkID_RIFF);
    Assert(Header->WaveID == WaveChunkID_WAVE);

    s32 ChannelCount = 0;
    s32 SampleDataSize = 0;
    s16 *SampleData = 0;

    for(riff_iterator Iterator = ParseChunkAt((u8 *)(Header + 1), (u8 *)(Header + 1) + Header->Size - 4);
        IsValid(Iterator);
        Iterator = NextChunk(Iterator))
    {
        switch(GetChunkID(Iterator))
        {
            case WaveChunkID_fmt:
            {
                wave_fmt *fmt = (wave_fmt *)GetChunkData(Iterator);
                Assert(fmt->wFormatTag == 1);
                Assert(fmt->nSamplesPerSec == 48000);
                Assert(fmt->wBitsPerSample == 16);
                Assert(fmt->nBlockAlign == (sizeof(u16)*fmt->nChannels));

                ChannelCount = fmt->nChannels;
            } break;

            case WaveChunkID_data:
            {
                SampleData = (s16 *)GetChunkData(Iterator);
                SampleDataSize = GetChunkDataSize(Iterator);
            } break;
        }
    }

    Assert(SampleDataSize > 0);
    Assert(ChannelCount);
    Assert(SampleData);

    s32 SampleCount = SampleDataSize / (ChannelCount*sizeof(u16));

    Result.ChannelCount = ChannelCount;

    if(ChannelCount == 1)
    {
        Result.Samples[0] = SampleData;
        Result.Samples[1] = 0;
    }
    else if(ChannelCount == 2)
    {
        Result.Samples[0] = SampleData;
        Result.Samples[1] = SampleData + SampleCount;

        for(s32 SampleIndex = 0;
            SampleIndex < SampleCount;
            ++SampleIndex)
        {
            s16 Src = SampleData[2*SampleIndex];
            SampleData[2*SampleIndex] = SampleData[SampleIndex];
            SampleData[SampleIndex] = Src;
        }
    }
    else
    {
        Assert(!"NotImplemented");
    }

    for(s32 ChannelIndex = 0;
        ChannelIndex < ChannelCount;
        ++ChannelIndex)
    {
        for(s32 SampleIndex = SampleCount;
            SampleIndex < (SampleCount + 8);
            ++SampleIndex)
        {
            Result.Samples[ChannelIndex][SampleIndex] = 0;
        }
    }

    Result.SampleCount = SampleCount;

    return(Result);
}

FILE *Out = 0;

function void
BeginAssetType(game_assets *Assets, asset_type_id ID)
{
    hha_asset_type *Type = Assets->DebugAssetType = Assets->AssetTypes + Assets->AssetTypeCount++;

    Type->TypeID = ID;
    Type->FirstAssetIndex = Assets->AssetCount;
    Type->OnePastLastAssetIndex = Type->FirstAssetIndex;
}

function void
AddAssetPixmap(game_assets *Assets, char *FileName, f32 AlignPercentageX = 0.5f, f32 AlignPercentageY = 0.5f)
{
    s32 AssetIndex = Assets->AssetIndex = Assets->DebugAssetType->OnePastLastAssetIndex++;
    asset *Source = Assets->AssetSources + AssetIndex;
    hha_asset *Dest = Assets->Assets + AssetIndex;

    Dest->FirstTagIndex = Assets->TagCount;
    Dest->OnePastLastTagIndex = Dest->FirstTagIndex;
    Source->Type = AssetType_Pixmap;
    Dest->Pixmap.AlignPercentage[0] = AlignPercentageX;
    Dest->Pixmap.AlignPercentage[1] = AlignPercentageY;
    Source->FileName = FileName;
}

function sound_id
AddAssetSound(game_assets *Assets, char *FileName)
{
    sound_id Result = { Assets->AssetIndex = Assets->DebugAssetType->OnePastLastAssetIndex++ };

    asset *Source = Assets->AssetSources + Result.Value;
    hha_asset *Dest = Assets->Assets + Result.Value;

    Dest->FirstTagIndex = Assets->TagCount;
    Dest->OnePastLastTagIndex = Dest->FirstTagIndex;

    Source->Type = AssetType_Sound;
    Source->FileName = FileName;

    return(Result);
}

function void
AddTag(game_assets *Assets, asset_tag_id TagID, f32 Value)
{
    hha_tag *Tag = Assets->Tags + Assets->Assets[Assets->AssetIndex].OnePastLastTagIndex++;
    Tag->ID = TagID;
    Tag->Value = Value;
    Assets->TagCount = Assets->Assets[Assets->AssetIndex].OnePastLastTagIndex;
}

function void
EndAssetType(game_assets *Assets)
{
    Assets->AssetCount = Assets->DebugAssetType->OnePastLastAssetIndex;
}

function void
WriteHHA(game_assets *Assets, char *FileName)
{
    Out = fopen(FileName, "wb");
    if(Out)
    {
        hha_header Header = {};
        Header.MagicValue = HHA_MAGIC_VALUE;
        Header.Version = HHA_VERSION;
        Header.TagCount = Assets->TagCount;
        Header.AssetCount = Assets->AssetCount;
        Header.AssetTypeCount = Assets->AssetTypeCount;

        s64 TagArraySize = Header.TagCount*sizeof(hha_tag);
        s64 AssetArraySize = Header.AssetCount*sizeof(hha_asset);
        s64 AssetTypeArraySize = Header.AssetTypeCount*sizeof(hha_asset_type);

        Header.Tags = sizeof(Header);
        Header.Assets = Header.Tags + TagArraySize;
        Header.AssetTypes = Header.Assets + AssetArraySize;

        fwrite(&Header, sizeof(Header), 1, Out);
        fwrite(Assets->Tags, TagArraySize, 1, Out);
        _fseeki64(Out, AssetArraySize, SEEK_CUR);
        fwrite(Assets->AssetTypes, AssetTypeArraySize, 1, Out);

        for(s32 AssetIndex = 1;
            AssetIndex < Header.AssetCount;
            ++AssetIndex)
        {
            asset *Source = Assets->AssetSources + AssetIndex;
            hha_asset *Dest = Assets->Assets + AssetIndex;

            if(AssetIndex == 25)
            {
                int a = 4;
            }

            switch(Source->Type)
            {
                case AssetType_Pixmap:
                {
                    pixmap Pixmap = LoadBMP(Source->FileName);
                    Assert(Pixmap.Pitch > 0);
                    Assert(Pixmap.Pitch == (Pixmap.Width*sizeof(u32)));

                    Dest->DataOffset = _ftelli64(Out);
                    Dest->Pixmap.Dim[0] = Pixmap.Width;
                    Dest->Pixmap.Dim[1] = Pixmap.Height;

                    fwrite(Pixmap.Memory, Pixmap.Pitch*Pixmap.Height, 1, Out);

                    Assert(Pixmap.Free);
                    free(Pixmap.Free);
                } break;

                case AssetType_Character:
                {
                    pixmap Result = GetGlyphPixmap(Source->FileName, Source->FontName, Source->Codepoint, Dest);

                    Assert(Result.Pitch > 0);
                    Assert(Result.Pitch == (Result.Width*sizeof(u32)));

                    Dest->DataOffset = _ftelli64(Out);
                    Dest->Pixmap.Dim[0] = Result.Width;
                    Dest->Pixmap.Dim[1] = Result.Height;

                    fwrite(Result.Memory, Result.Pitch*Result.Height, 1, Out);

                    Assert(Result.Free);
                    free(Result.Free);

                } break;

                case AssetType_Sound:
                {
                    sound Sound = LoadSound(Source->FileName);

                    Dest->DataOffset = _ftelli64(Out);
                    Dest->Sound.SampleCount = Sound.SampleCount;
                    Dest->Sound.ChannelCount = Sound.ChannelCount;

                    for(s32 Index = 0;
                        Index < Sound.ChannelCount;
                        ++Index)
                    {
                        fwrite(Sound.Samples[Index], Sound.SampleCount*sizeof(s16), 1, Out);
                    }

                    Assert(Sound.Free);
                    free(Sound.Free);

                } break;

                InvalidDefaultCase;
            }
        }

        _fseeki64(Out, Header.Assets, SEEK_SET);
        fwrite(Assets->Assets, AssetArraySize, 1, Out);

        fclose(Out);
    }
    else
    {
        printf("ERROR: Couldn't open file :(\n");
    }
}

int
main(int ArgCount, char **Args)
{
    game_assets Assets_ = {};

    game_assets *Assets = &Assets_;

    Assets->TagCount = 1;
    Assets->AssetCount = 1;
    Assets->AssetTypeCount = 1;

    BeginAssetType(Assets, Asset_Tree);
    AddAssetPixmap(Assets, "test2/tree02.bmp", 0.5f, 0.3301886916160584f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Shadow);
    AddAssetPixmap(Assets, "test/test_hero_shadow.bmp", 0.5f, 0.161290317773819f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Sword);
    AddAssetPixmap(Assets, "test2/rock03.bmp", 0.5f, 0.6875f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Grass);
    AddAssetPixmap(Assets, "test2/grass00.bmp", 0.5f, 0.5f);
    AddAssetPixmap(Assets, "test2/grass01.bmp", 0.5f, 0.5f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Ground);
    AddAssetPixmap(Assets, "test2/ground00.bmp", 0.5f, 0.5f);
    AddAssetPixmap(Assets, "test2/ground01.bmp", 0.5f, 0.5f);
    AddAssetPixmap(Assets, "test2/ground02.bmp", 0.5f, 0.5f);
    AddAssetPixmap(Assets, "test2/ground03.bmp", 0.5f, 0.5f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Tuft);
    AddAssetPixmap(Assets, "test2/tuft00.bmp", 0.5f, 0.5f);
    AddAssetPixmap(Assets, "test2/tuft01.bmp", 0.5f, 0.5f);
    AddAssetPixmap(Assets, "test2/tuft02.bmp", 0.5f, 0.5f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Head);
    AddAssetPixmap(Assets, "test/test_hero_right_head.bmp", 0.5f, 0.161290317773819f);
    AddTag(Assets, Tag_FacingDirection, 0.0f);
    AddAssetPixmap(Assets, "test/test_hero_back_head.bmp", 0.5f, 0.161290317773819f);
    AddTag(Assets, Tag_FacingDirection, 0.25f);
    AddAssetPixmap(Assets, "test/test_hero_left_head.bmp", 0.5f, 0.161290317773819f);
    AddTag(Assets, Tag_FacingDirection, 0.5f);
    AddAssetPixmap(Assets, "test/test_hero_front_head.bmp", 0.5f, 0.161290317773819f);
    AddTag(Assets, Tag_FacingDirection, 0.75f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Cape);
    AddAssetPixmap(Assets, "test/test_hero_right_cape.bmp", 0.5f, 0.161290317773819f);
    AddTag(Assets, Tag_FacingDirection, 0.0f);
    AddAssetPixmap(Assets, "test/test_hero_back_cape.bmp", 0.5f, 0.161290317773819f);
    AddTag(Assets, Tag_FacingDirection, 0.25f);
    AddAssetPixmap(Assets, "test/test_hero_left_cape.bmp", 0.5f, 0.161290317773819f);
    AddTag(Assets, Tag_FacingDirection, 0.5f);
    AddAssetPixmap(Assets, "test/test_hero_front_cape.bmp", 0.5f, 0.161290317773819f);
    AddTag(Assets, Tag_FacingDirection, 0.75f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Torso);
    AddAssetPixmap(Assets, "test/test_hero_right_torso.bmp", 0.5f, 0.161290317773819f);
    AddTag(Assets, Tag_FacingDirection, 0.0f);
    AddAssetPixmap(Assets, "test/test_hero_back_torso.bmp", 0.5f, 0.161290317773819f);
    AddTag(Assets, Tag_FacingDirection, 0.25f);
    AddAssetPixmap(Assets, "test/test_hero_left_torso.bmp", 0.5f, 0.161290317773819f);
    AddTag(Assets, Tag_FacingDirection, 0.5f);
    AddAssetPixmap(Assets, "test/test_hero_front_torso.bmp", 0.5f, 0.161290317773819f);
    AddTag(Assets, Tag_FacingDirection, 0.75f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Font);
    for(char Character = '!';
        Character <= '~';
        ++Character)
    {
        // AddAssetCharacter(Assets, "c:/Windows/Fonts/cour.ttf", "Courier New", Character);
        AddAssetCharacter(Assets, "test4/liberation.ttf", "Liberation Mono", Character);
        AddTag(Assets, Tag_UnicodeCodepoint, (f32)Character);
    }
    EndAssetType(Assets);

    WriteHHA(Assets, "test1.hha");

    Assets_ = {};

    Assets = &Assets_;

    Assets->TagCount = 1;
    Assets->AssetCount = 1;
    Assets->AssetTypeCount = 1;

    BeginAssetType(Assets, Asset_Bloop);
    AddAssetSound(Assets, "test3/bloop_00.wav");
    AddAssetSound(Assets, "test3/bloop_01.wav");
    AddAssetSound(Assets, "test3/bloop_02.wav");
    AddAssetSound(Assets, "test3/bloop_03.wav");
    AddAssetSound(Assets, "test3/bloop_04.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Drop);
    AddAssetSound(Assets, "test3/drop_00.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Glide);
    AddAssetSound(Assets, "test3/glide_00.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Music);
    AddAssetSound(Assets, "test3/blue.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Crack);
    AddAssetSound(Assets, "test3/crack_00.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Puhp);
    AddAssetSound(Assets, "test3/puhp_00.wav");
    AddAssetSound(Assets, "test3/puhp_01.wav");
    EndAssetType(Assets);

    WriteHHA(Assets, "test2.hha");

    return(0);
}

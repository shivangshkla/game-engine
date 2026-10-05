function void
Perspective(render_group *Group, r32 TargetDistanceFromCamera, r32 FocalLength, r32 Width, r32 Height)
{
    Group->Transform.FocalLength = FocalLength;
    Group->Transform.TargetDistanceFromCamera = 5.0f;
    Group->Transform.Width = Width;
    Group->Transform.Height = Height;
    Group->Transform.Offset = V3(0, 0, 0);
}

function void
Orthographic(render_group *Group, r32 Width, r32 Height)
{
    Group->Transform.FocalLength = 1.0f;
    Group->Transform.TargetDistanceFromCamera = 1.0f;
    Group->Transform.Width = Width;
    Group->Transform.Height = Height;
    Group->Transform.Offset = V3(0, 0, 0);
}

function render_group *
AllocateRenderGroup(memory_arena *Arena, u32 MaxPushBufferSize, game_assets *Assets)
{
    render_group *Result = PushStruct(Arena, render_group);

    // TODO(gaurang): This changes the meaning of MaxPushBufferSize
    // because it's also taking into account render_group. Do
    // something about it.
    Result->MaxPushBufferSize = MaxPushBufferSize - sizeof(render_group);
    Result->PushBufferSize = 0;
    Result->PushBufferBase = PushSize(Arena, Result->MaxPushBufferSize);
    Result->Assets = Assets;
    Result->MissingResourceCount = 0;

    // Result->Transform.TargetDistanceFromCamera = 30.0f;

    return(Result);
}

struct bilinear_sample
{
    u32 A, B, C, D;
};
function bilinear_sample
BilinearSample(pixmap *Texture, s32 X, s32 Y)
{
    bilinear_sample Result;

    u8 *TexelPtr = Texture->Memory + Texture->Pitch* Y + Texture->BytesPerPixel* X;
    Result.A = *(u32 *)(TexelPtr);
    Result.B = *(u32 *)(TexelPtr + Texture->BytesPerPixel);
    Result.C = *(u32 *)(TexelPtr + Texture->Pitch);
    Result.D = *(u32 *)(TexelPtr + Texture->Pitch + Texture->BytesPerPixel);

    return(Result);
}

function void
DrawPixmap(pixmap *Video, pixmap *Pixmap, r32 RealMinX, r32 RealMinY)
{
    s32 MinX = RoundReal32ToInt32(RealMinX);
    s32 MinY = RoundReal32ToInt32(RealMinY);
    s32 MaxX = RoundReal32ToInt32(RealMinX + Pixmap->Width);
    s32 MaxY = RoundReal32ToInt32(RealMinY + Pixmap->Height);

    s32 PixmapOffsetX = 0;
    s32 PixmapOffsetY = 0;
    if(MinX < 0)
    {
        PixmapOffsetX = -MinX;
        MinX = 0;
    }
    if(MinY < 0)
    {
        PixmapOffsetY = -MinY;
        MinY = 0;
    }
    if(MaxX > Video->Width)
    {
        MaxX = Video->Width;
    }
    if(MaxY > Video->Height)
    {
        MaxY = Video->Height;
    }

    u8 *SrcRow = Pixmap->Memory + Pixmap->Pitch*PixmapOffsetY + Pixmap->BytesPerPixel*PixmapOffsetX;
    u8 *DestRow = Video->Memory + Video->Pitch*MinY + Video->BytesPerPixel*MinX;
    for(s32 Y = MinY;
        Y < MaxY;
        ++Y)
    {
        u32 *Src = (u32 *)SrcRow;
        u32 *Dest = (u32 *)DestRow;
        for(s32 X = MinX;
            X < MaxX;
            ++X)
        {
            v4 Texel = V4((r32)((*Src >> 16) & 0xFF),
                          (r32)((*Src >> 8) & 0xFF),
                          (r32)((*Src >> 0) & 0xFF),
                          (r32)((*Src >> 24) & 0xFF));

            v4 D = V4((r32)((*Dest >> 16) & 0xFF),
                      (r32)((*Dest >> 8) & 0xFF),
                      (r32)((*Dest >> 0) & 0xFF),
                      (r32)((*Dest >> 24) & 0xFF));

            Texel = SRGB255ToLinear1(Texel);
            D = SRGB255ToLinear1(D);

            v4 Blended = (1.0f - Texel.A)*D + Texel;

            Blended = Linear1ToSRGB255(Blended);

            *Dest = (((u32)(Blended.A + 0.5f) << 24) |
                     ((u32)(Blended.R + 0.5f) << 16) |
                     ((u32)(Blended.G + 0.5f) << 8) |
                     ((u32)(Blended.B + 0.5f) << 0));

            ++Dest;
            ++Src;
        }
        DestRow += Video->Pitch;
        SrcRow += Pixmap->Pitch;
    }
}

function void
DrawRectangle(pixmap *Video, v2 Min, v2 Max, v4 Color, rectangle2i ClipRect)
{
    rectangle2i PixmapRect = { 0, 0, Video->Width, Video->Height };

    rectangle2i FillRect;
    FillRect.MinX = RoundReal32ToInt32(Min.X);
    FillRect.MinY = RoundReal32ToInt32(Min.Y);
    FillRect.MaxX = RoundReal32ToInt32(Max.X);
    FillRect.MaxY = RoundReal32ToInt32(Max.Y);

    FillRect = Intersect(Intersect(FillRect, ClipRect), PixmapRect);

    u32 Color32 = ((TruncateReal32ToUInt32(Color.A*255.0f) << 24) |
                   (TruncateReal32ToUInt32(Color.R*255.0f) << 16) |
                   (TruncateReal32ToUInt32(Color.G*255.0f) << 8) |
                   (TruncateReal32ToUInt32(Color.B*255.0f) << 0));

    u8 *Row = Video->Memory + Video->Pitch*FillRect.MinY + Video->BytesPerPixel*FillRect.MinX;
    for(s32 Y = FillRect.MinY;
        Y < FillRect.MaxY;
        ++Y)
    {
        u32 *Pixel = (u32 *)Row;
        for(s32 X = FillRect.MinX;
            X < FillRect.MaxX;
            ++X)
        {
            *Pixel++ = Color32;
        }
        Row += Video->Pitch;
    }
}

function void
DrawRectangleSlowly(pixmap *Video, v2 Origin, v2 XAxis, v2 YAxis, pixmap *Texture, v4 Tint)
{
    Tint.RGB *= Tint.A;

    r32 InvXAxisLengthSq = 1.0f / LengthSq(XAxis);
    r32 InvYAxisLengthSq = 1.0f / LengthSq(YAxis);

    v2 P[4] = { Origin, Origin+XAxis, Origin+XAxis+YAxis, Origin+YAxis };

    s32 WidthMax = Video->Width-1;
    s32 HeightMax = Video->Height-1;

    s32 MinX = WidthMax;
    s32 MinY = HeightMax;
    s32 MaxX = 0;
    s32 MaxY = 0;

    for(s32 Index = 0;
        Index < ArrayCount(P);
        ++Index)
    {
        v2 TestP = P[Index];

        s32 FloorX = FloorReal32ToInt32(TestP.X);
        s32 FloorY = FloorReal32ToInt32(TestP.Y);
        s32 CeilX = CeilReal32ToInt32(TestP.X);
        s32 CeilY = CeilReal32ToInt32(TestP.Y);

        if(MinX > FloorX) { MinX = FloorX; }
        if(MinY > FloorY) { MinY = FloorY; }
        if(MaxX < CeilX) { MaxX = CeilX; }
        if(MaxY < CeilY) { MaxY = CeilY; }
    }

    if(MinX < 0)
    {
        MinX = 0;
    }
    if(MinY < 0)
    {
        MinY = 0;
    }
    if(MaxX > WidthMax)
    {
        MaxX = WidthMax;
    }
    if(MaxY > HeightMax)
    {
        MaxY = HeightMax;
    }

    u8 *Row = Video->Memory + Video->Pitch*MinY + Video->BytesPerPixel*MinX;
    for(s32 Y = MinY;
        Y <= MaxY;
        ++Y)
    {
        u32 *Pixel = (u32 *)Row;
        for(s32 X = MinX;
            X <= MaxX;
            ++X)
        {
            v2 PixelP = V2i(X, Y);
            v2 D = PixelP - Origin;

            r32 Edge0 = Inner(D, -Perp(XAxis));
            r32 Edge1 = Inner(D - XAxis, -Perp(YAxis));
            r32 Edge2 = Inner(D - XAxis - YAxis, Perp(XAxis));
            r32 Edge3 = Inner(D - YAxis, Perp(YAxis));
            if((Edge0 <= 0) &&
               (Edge1 < 0) &&
               (Edge2 < 0) &&
               (Edge3 <= 0))
            {
                r32 U = InvXAxisLengthSq*Inner(D, XAxis);
                r32 V = InvYAxisLengthSq*Inner(D, YAxis);

                r32 tX = U*(Texture->Width - 1) + 0.0f;
                r32 tY = V*(Texture->Height - 1) + 0.0f;

                s32 iX = (s32)(tX + 0.0f);
                s32 iY = (s32)(tY + 0.0f);

                r32 fX = tX - iX;
                r32 fY = tY - iY;

                u8 *TexelPtr = Texture->Memory + Texture->Pitch*iY + Texture->BytesPerPixel*iX;
                u32 TexelA32 = *(u32 *)(TexelPtr);
                u32 TexelB32 = *(u32 *)(TexelPtr + Texture->BytesPerPixel);
                u32 TexelC32 = *(u32 *)(TexelPtr + Texture->Pitch);
                u32 TexelD32 = *(u32 *)(TexelPtr + Texture->Pitch + Texture->BytesPerPixel);

                v4 TexelA = V4((r32)((TexelA32 >> 16) & 0xFF),
                               (r32)((TexelA32 >> 8) & 0xFF),
                               (r32)((TexelA32 >> 0) & 0xFF),
                               (r32)((TexelA32 >> 24) & 0xFF));
                v4 TexelB = V4((r32)((TexelB32 >> 16) & 0xFF),
                               (r32)((TexelB32 >> 8) & 0xFF),
                               (r32)((TexelB32 >> 0) & 0xFF),
                               (r32)((TexelB32 >> 24) & 0xFF));
                v4 TexelC = V4((r32)((TexelC32 >> 16) & 0xFF),
                               (r32)((TexelC32 >> 8) & 0xFF),
                               (r32)((TexelC32 >> 0) & 0xFF),
                               (r32)((TexelC32 >> 24) & 0xFF));
                v4 TexelD = V4((r32)((TexelD32 >> 16) & 0xFF),
                               (r32)((TexelD32 >> 8) & 0xFF),
                               (r32)((TexelD32 >> 0) & 0xFF),
                               (r32)((TexelD32 >> 24) & 0xFF));

                // NOTE(gaurang): Convert from sRGB to "linear" brightness space.
                TexelA = SRGB255ToLinear1(TexelA);
                TexelB = SRGB255ToLinear1(TexelB);
                TexelC = SRGB255ToLinear1(TexelC);
                TexelD = SRGB255ToLinear1(TexelD);

                v4 Texel = Lerp(Lerp(TexelA, fX, TexelB),
                                fY,
                                Lerp(TexelC, fX, TexelD));

                u32 Dest32 = *Pixel;
                v4 Dest = V4((r32)((Dest32 >> 16) & 0xFF),
                             (r32)((Dest32 >> 8) & 0xFF),
                             (r32)((Dest32 >> 0) & 0xFF),
                             (r32)((Dest32 >> 24) & 0xFF));

                // NOTE(gaurang): Convert from sRGB to "linear" brightness space.
                Dest = SRGB255ToLinear1(Dest);

                Texel = Hadamard(Texel, Tint);

                v4 Blended = (1.0f - Texel.A)*Dest + Texel;

                // NOTE(gaurang): Convert from "linear" brightness space to sRGB.
                v4 Blended255 = Linear1ToSRGB255(Blended);

                *Pixel = (((u32)(Blended255.A + 0.5f) << 24) |
                          ((u32)(Blended255.R + 0.5f) << 16) |
                          ((u32)(Blended255.G + 0.5f) << 8) |
                          ((u32)(Blended255.B + 0.5f) << 0));
            }
            ++Pixel;
        }
        Row += Video->Pitch;
    }
}

function void
DrawRectangleQuickly(pixmap *Video,
                     v2 Origin, v2 XAxis, v2 YAxis, pixmap *Texture, v4 Tint,
                     rectangle2i ClipRect)
{
    //
    // NOTE(gaurang): STUDY: We could enhance our performance if we
    // use a texture format where a pixel chunk is stored
    // sequentially. But we need to change its format before blitting
    // it to the screen, which could cancel out our performance
    // gain. But we should be able to hit 1080p at 60Hz with
    // multithreading so we are fine as of right now.
    //
    BEGIN_TIMED_BLOCK(DrawRectangleQuickly);

    Tint.RGB *= Tint.A;

    r32 InvXAxisLengthSq = 1.0f / LengthSq(XAxis);
    r32 InvYAxisLengthSq = 1.0f / LengthSq(YAxis);

    v2 P[4] = { Origin, Origin+XAxis, Origin+XAxis+YAxis, Origin+YAxis };

    rectangle2i PixmapRect = { 0, 0, Video->Width, Video->Height };

    rectangle2i FillRect = InvertedInfinityRectangle();

    for(s32 Index = 0;
        Index < ArrayCount(P);
        ++Index)
    {
        v2 TestP = P[Index];

        s32 FloorX = FloorReal32ToInt32(TestP.X);
        s32 FloorY = FloorReal32ToInt32(TestP.Y);
        s32 CeilX = CeilReal32ToInt32(TestP.X) + 1;
        s32 CeilY = CeilReal32ToInt32(TestP.Y) + 1;

        if(FillRect.MinX > FloorX) { FillRect.MinX = FloorX; }
        if(FillRect.MinY > FloorY) { FillRect.MinY = FloorY; }
        if(FillRect.MaxX < CeilX) { FillRect.MaxX = CeilX; }
        if(FillRect.MaxY < CeilY) { FillRect.MaxY = CeilY; }
    }

    FillRect = Intersect(Intersect(FillRect, ClipRect), PixmapRect);

    v2 nXAxis = InvXAxisLengthSq*XAxis;
    v2 nYAxis = InvYAxisLengthSq*YAxis;
    r32 Inv255 = 1.0f/255.0f;

    __m128 OriginX_4x = _mm_set1_ps(Origin.X);
    __m128 OriginY_4x = _mm_set1_ps(Origin.Y);
    __m128 nXAxisX_4x = _mm_set1_ps(nXAxis.X);
    __m128 nXAxisY_4x = _mm_set1_ps(nXAxis.Y);
    __m128 nYAxisX_4x = _mm_set1_ps(nYAxis.X);
    __m128 nYAxisY_4x = _mm_set1_ps(nYAxis.Y);

    __m128 One = _mm_set1_ps(1.0f);
    __m128 Two = _mm_set1_ps(2.0f);
    __m128 Four = _mm_set1_ps(4.0f);
    __m128 Zero = _mm_set1_ps(0.0f);
    __m128 One255 = _mm_set1_ps(255.0f);
    __m128 Inv255_4x = _mm_set1_ps(Inv255);

    __m128i MaskFF = _mm_set1_epi32(0xFF);

    __m128 TintR = _mm_set1_ps(Tint.R);
    __m128 TintG = _mm_set1_ps(Tint.G);
    __m128 TintB = _mm_set1_ps(Tint.B);
    __m128 TintA = _mm_set1_ps(Tint.A);

    __m128 TexelAR = _mm_set1_ps(0.0f);
    __m128 TexelAG = _mm_set1_ps(0.0f);
    __m128 TexelAB = _mm_set1_ps(0.0f);
    __m128 TexelAA = _mm_set1_ps(0.0f);

    __m128 TexelBR = _mm_set1_ps(0.0f);
    __m128 TexelBG = _mm_set1_ps(0.0f);
    __m128 TexelBB = _mm_set1_ps(0.0f);
    __m128 TexelBA = _mm_set1_ps(0.0f);

    __m128 TexelCR = _mm_set1_ps(0.0f);
    __m128 TexelCG = _mm_set1_ps(0.0f);
    __m128 TexelCB = _mm_set1_ps(0.0f);
    __m128 TexelCA = _mm_set1_ps(0.0f);

    __m128 TexelDR = _mm_set1_ps(0.0f);
    __m128 TexelDG = _mm_set1_ps(0.0f);
    __m128 TexelDB = _mm_set1_ps(0.0f);
    __m128 TexelDA = _mm_set1_ps(0.0f);

    __m128 DestR = _mm_set1_ps(0.0f);
    __m128 DestG = _mm_set1_ps(0.0f);
    __m128 DestB = _mm_set1_ps(0.0f);
    __m128 DestA = _mm_set1_ps(0.0f);

    __m128 TextureWidthMinusTwo = _mm_set1_ps((r32)(Texture->Width - 2));
    __m128 TextureHeightMinusTwo = _mm_set1_ps((r32)(Texture->Height - 2));

    __m128 FillRectMaxX_4x = _mm_set1_ps((r32)FillRect.MaxX);

    __m128 PixelPY = _mm_set1_ps((r32)FillRect.MinY);
    PixelPY = _mm_sub_ps(PixelPY, OriginY_4x);

    u8 *TextureMemory = Texture->Memory;
    s32 TexturePitch = Texture->Pitch;
    s32 TextureBytesPerPixel = Texture->BytesPerPixel;

    __m128i TexturePitch_4x = _mm_set1_epi32(TexturePitch);
    __m128i TextureBytesPerPixel_4x = _mm_set1_epi32(TextureBytesPerPixel);

    BEGIN_TIMED_BLOCK(ProcessPixel);
    u8 *Row = Video->Memory + Video->Pitch*FillRect.MinY + Video->BytesPerPixel*FillRect.MinX;
    for(s32 Y = FillRect.MinY;
        Y < FillRect.MaxY;
        Y += 1)
    {
        __m128 X_4x = _mm_set_ps((r32)(FillRect.MinX + 3),
                                 (r32)(FillRect.MinX + 2),
                                 (r32)(FillRect.MinX + 1),
                                 (r32)(FillRect.MinX + 0));

        // TODO(gaurang): Try recomputing this inside the X loop every
        // iteration, when you have statistical profiling.
        __m128 PixelPX = _mm_sub_ps(X_4x, OriginX_4x);

        u32 *Col = (u32 *)Row;
        for(s32 X = FillRect.MinX;
            X < FillRect.MaxX;
            X += 4)
        {
            __m128 U = _mm_add_ps(_mm_mul_ps(PixelPX, nXAxisX_4x), _mm_mul_ps(PixelPY, nXAxisY_4x));
            __m128 V = _mm_add_ps(_mm_mul_ps(PixelPX, nYAxisX_4x), _mm_mul_ps(PixelPY, nYAxisY_4x));

            __m128 FillMask = _mm_and_ps(_mm_and_ps(_mm_cmpge_ps(U, Zero),
                                                    _mm_cmpge_ps(V, Zero)),
                                         _mm_and_ps(_mm_cmple_ps(U, One),
                                                    _mm_cmple_ps(V, One)));

            __m128 ClipMask = _mm_cmplt_ps(X_4x, FillRectMaxX_4x);

            __m128i WriteMask = _mm_castps_si128(_mm_and_ps(ClipMask, FillMask));

            U = _mm_min_ps(_mm_max_ps(U, Zero), One);
            V = _mm_min_ps(_mm_max_ps(V, Zero), One);

            __m128 tX = _mm_mul_ps(U, TextureWidthMinusTwo);
            __m128 tY = _mm_mul_ps(V, TextureHeightMinusTwo);

            __m128i iX = _mm_cvttps_epi32(tX);
            __m128i iY = _mm_cvttps_epi32(tY);

            __m128i TexelFetchIndex = _mm_add_epi32(_mm_mullo_epi32(TexturePitch_4x, iY),
                                                    _mm_mullo_epi32(TextureBytesPerPixel_4x, iX));

            __m128 fX = _mm_sub_ps(tX, _mm_cvtepi32_ps(iX));
            __m128 fY = _mm_sub_ps(tY, _mm_cvtepi32_ps(iY));

            __m128 ifX = _mm_sub_ps(One, fX);
            __m128 ifY = _mm_sub_ps(One, fY);

            __m128 l0 = _mm_mul_ps(ifY, ifX);
            __m128 l1 = _mm_mul_ps(ifY, fX);
            __m128 l2 = _mm_mul_ps(fY, ifX);
            __m128 l3 = _mm_mul_ps(fY, fX);

#define ANCIENT_PC 1
#if ANCIENT_PC
            __m128i Dest;
            Dest.m128i_u32[0] = WriteMask.m128i_u32[0] ? Col[0] : 0;
            Dest.m128i_u32[1] = WriteMask.m128i_u32[1] ? Col[1] : 0;
            Dest.m128i_u32[2] = WriteMask.m128i_u32[2] ? Col[2] : 0;
            Dest.m128i_u32[3] = WriteMask.m128i_u32[3] ? Col[3] : 0;
#else
            __m128i Dest = _mm_maskload_epi32((s32 *)Col, WriteMask);
#endif

            u8 *TexelPtr0 = TextureMemory + TexelFetchIndex.m128i_i32[0];
            u8 *TexelPtr1 = TextureMemory + TexelFetchIndex.m128i_i32[1];
            u8 *TexelPtr2 = TextureMemory + TexelFetchIndex.m128i_i32[2];
            u8 *TexelPtr3 = TextureMemory + TexelFetchIndex.m128i_i32[3];

            __m128i SampleA = _mm_setr_epi32(*(u32 *)(TexelPtr0),
                                             *(u32 *)(TexelPtr1),
                                             *(u32 *)(TexelPtr2),
                                             *(u32 *)(TexelPtr3));

            __m128i SampleB = _mm_setr_epi32(*(u32 *)(TexelPtr0 + TextureBytesPerPixel),
                                             *(u32 *)(TexelPtr1 + TextureBytesPerPixel),
                                             *(u32 *)(TexelPtr2 + TextureBytesPerPixel),
                                             *(u32 *)(TexelPtr3 + TextureBytesPerPixel));

            __m128i SampleC = _mm_setr_epi32(*(u32 *)(TexelPtr0 + TexturePitch),
                                             *(u32 *)(TexelPtr1 + TexturePitch),
                                             *(u32 *)(TexelPtr2 + TexturePitch),
                                             *(u32 *)(TexelPtr3 + TexturePitch));

            __m128i SampleD = _mm_setr_epi32(*(u32 *)(TexelPtr0 + TexturePitch + TextureBytesPerPixel),
                                             *(u32 *)(TexelPtr1 + TexturePitch + TextureBytesPerPixel),
                                             *(u32 *)(TexelPtr2 + TexturePitch + TextureBytesPerPixel),
                                             *(u32 *)(TexelPtr3 + TexturePitch + TextureBytesPerPixel));

            TexelAR = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleA, 16), MaskFF));
            TexelAG = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleA, 8), MaskFF));
            TexelAB = _mm_cvtepi32_ps(_mm_and_si128(SampleA, MaskFF));
            TexelAA = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleA, 24), MaskFF));

            TexelBR = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleB, 16), MaskFF));
            TexelBG = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleB, 8), MaskFF));
            TexelBB = _mm_cvtepi32_ps(_mm_and_si128(SampleB, MaskFF));
            TexelBA = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleB, 24), MaskFF));

            TexelCR = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleC, 16), MaskFF));
            TexelCG = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleC, 8), MaskFF));
            TexelCB = _mm_cvtepi32_ps(_mm_and_si128(SampleC, MaskFF));
            TexelCA = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleC, 24), MaskFF));

            TexelDR = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleD, 16), MaskFF));
            TexelDG = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleD, 8), MaskFF));
            TexelDB = _mm_cvtepi32_ps(_mm_and_si128(SampleD, MaskFF));
            TexelDA = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(SampleD, 24), MaskFF));

            DestR = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(Dest, 16), MaskFF));
            DestG = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(Dest, 8), MaskFF));
            DestB = _mm_cvtepi32_ps(_mm_and_si128(Dest, MaskFF));
            DestA = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(Dest, 24), MaskFF));

            // NOTE(gaurang): Convert from sRGB to "linear" brightness space.
            TexelAR = _mm_mul_ps(Inv255_4x, TexelAR);
            TexelAR = _mm_mul_ps(TexelAR, TexelAR);
            TexelAG = _mm_mul_ps(Inv255_4x, TexelAG);
            TexelAG = _mm_mul_ps(TexelAG, TexelAG);
            TexelAB = _mm_mul_ps(Inv255_4x, TexelAB);
            TexelAB = _mm_mul_ps(TexelAB, TexelAB);
            TexelAA = _mm_mul_ps(Inv255_4x, TexelAA);

            TexelBR = _mm_mul_ps(Inv255_4x, TexelBR);
            TexelBR = _mm_mul_ps(TexelBR, TexelBR);
            TexelBG = _mm_mul_ps(Inv255_4x, TexelBG);
            TexelBG = _mm_mul_ps(TexelBG, TexelBG);
            TexelBB = _mm_mul_ps(Inv255_4x, TexelBB);
            TexelBB = _mm_mul_ps(TexelBB, TexelBB);
            TexelBA = _mm_mul_ps(Inv255_4x, TexelBA);

            TexelCR = _mm_mul_ps(Inv255_4x, TexelCR);
            TexelCR = _mm_mul_ps(TexelCR, TexelCR);
            TexelCG = _mm_mul_ps(Inv255_4x, TexelCG);
            TexelCG = _mm_mul_ps(TexelCG, TexelCG);
            TexelCB = _mm_mul_ps(Inv255_4x, TexelCB);
            TexelCB = _mm_mul_ps(TexelCB, TexelCB);
            TexelCA = _mm_mul_ps(Inv255_4x, TexelCA);

            TexelDR = _mm_mul_ps(Inv255_4x, TexelDR);
            TexelDR = _mm_mul_ps(TexelDR, TexelDR);
            TexelDG = _mm_mul_ps(Inv255_4x, TexelDG);
            TexelDG = _mm_mul_ps(TexelDG, TexelDG);
            TexelDB = _mm_mul_ps(Inv255_4x, TexelDB);
            TexelDB = _mm_mul_ps(TexelDB, TexelDB);
            TexelDA = _mm_mul_ps(Inv255_4x, TexelDA);

            DestR = _mm_mul_ps(Inv255_4x, DestR);
            DestR = _mm_mul_ps(DestR, DestR);
            DestG = _mm_mul_ps(Inv255_4x, DestG);
            DestG = _mm_mul_ps(DestG, DestG);
            DestB = _mm_mul_ps(Inv255_4x, DestB);
            DestB = _mm_mul_ps(DestB, DestB);
            DestA = _mm_mul_ps(Inv255_4x, DestA);

            // NOTE(gaurang): Perform bilinear blend.
            __m128 TexelR = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAR), _mm_mul_ps(l1, TexelBR)),
                                          _mm_add_ps(_mm_mul_ps(l2, TexelCR), _mm_mul_ps(l3, TexelDR)));
            __m128 TexelG = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAG), _mm_mul_ps(l1, TexelBG)),
                                          _mm_add_ps(_mm_mul_ps(l2, TexelCG), _mm_mul_ps(l3, TexelDG)));
            __m128 TexelB = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAB), _mm_mul_ps(l1, TexelBB)),
                                          _mm_add_ps(_mm_mul_ps(l2, TexelCB), _mm_mul_ps(l3, TexelDB)));
            __m128 TexelA = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAA), _mm_mul_ps(l1, TexelBA)),
                                          _mm_add_ps(_mm_mul_ps(l2, TexelCA), _mm_mul_ps(l3, TexelDA)));

            // NOTE(gaurang): Apply the tint.
            TexelR = _mm_mul_ps(TexelR, TintR);
            TexelG = _mm_mul_ps(TexelG, TintG);
            TexelB = _mm_mul_ps(TexelB, TintB);
            TexelA = _mm_mul_ps(TexelA, TintA);

            // NOTE(gaurang): Alpha blending.
            __m128 OneMinusTexelA = _mm_sub_ps(One, TexelA);
            __m128 BlendedR = _mm_add_ps(_mm_mul_ps(OneMinusTexelA, DestR), TexelR);
            __m128 BlendedG = _mm_add_ps(_mm_mul_ps(OneMinusTexelA, DestG), TexelG);
            __m128 BlendedB = _mm_add_ps(_mm_mul_ps(OneMinusTexelA, DestB), TexelB);
            __m128 BlendedA = _mm_add_ps(_mm_mul_ps(OneMinusTexelA, DestA), TexelA);

            // NOTE(gaurang): Convert from "linear" brightness space to sRGB.
            BlendedR = _mm_mul_ps(One255, _mm_sqrt_ps(BlendedR));
            BlendedG = _mm_mul_ps(One255, _mm_sqrt_ps(BlendedG));
            BlendedB = _mm_mul_ps(One255, _mm_sqrt_ps(BlendedB));
            BlendedA = _mm_mul_ps(One255, BlendedA);

            __m128i IntR = _mm_cvtps_epi32(BlendedR);
            __m128i IntG = _mm_cvtps_epi32(BlendedG);
            __m128i IntB = _mm_cvtps_epi32(BlendedB);
            __m128i IntA = _mm_cvtps_epi32(BlendedA);

            __m128i Blended = _mm_or_si128(_mm_or_si128(IntB, _mm_slli_epi32(IntG, 8)),
                                           _mm_or_si128(_mm_slli_epi32(IntR, 16), _mm_slli_epi32(IntA, 24)));

#if ANCIENT_PC
            if(WriteMask.m128i_u32[0]) Col[0] = Blended.m128i_u32[0];
            if(WriteMask.m128i_u32[1]) Col[1] = Blended.m128i_u32[1];
            if(WriteMask.m128i_u32[2]) Col[2] = Blended.m128i_u32[2];
            if(WriteMask.m128i_u32[3]) Col[3] = Blended.m128i_u32[3];
#else
            _mm_maskstore_epi32((s32 *)Col, WriteMask, Blended);
#endif

            PixelPX = _mm_add_ps(PixelPX, Four);
            X_4x = _mm_add_ps(X_4x, Four);
            Col += 4;
        }

        PixelPY = _mm_add_ps(PixelPY, One);
        Row += 1*Video->Pitch;
    }

    s32 Width = (FillRect.MaxX - FillRect.MinX);
    s32 Height = (FillRect.MaxY - FillRect.MinY);
    s32 Area = 0;
    if((Width > 0) && (Height > 0))
    {
        Area = Width*Height;
    }

    END_TIMED_BLOCK_COUNTED(ProcessPixel, Area);
    END_TIMED_BLOCK(DrawRectangleQuickly);
}

#define PushRenderEntry(Group, type) ((type*)PushRenderElement_(Group, sizeof(type), RenderGroupEntryType_##type))
function u8 *
PushRenderElement_(render_group *Group, u32 Size, render_group_entry_type Type)
{
    u8 *Result = 0;

    Size += sizeof(render_group_entry_header);

    if((Group->PushBufferSize + Size) <= Group->MaxPushBufferSize)
    {
        render_group_entry_header *Header = (render_group_entry_header *)(Group->PushBufferBase + Group->PushBufferSize);
        Header->Type = Type;
        Result = (u8 *)Header + sizeof(*Header);
        Group->PushBufferSize += Size;
    }
    else
    {
        InvalidCodePath;
    }

    return(Result);
}

function b32
ApplyTransform(render_transform *Transform, v3 *P)
{
    b32 Valid = false;

    *P += Transform->Offset;

    v3 RawXY = V3((*P).XY, 1.0f);

    r32 DistanceToPZ = Transform->TargetDistanceFromCamera - (*P).Z;
    if(0) DistanceToPZ += 25.0f;
    r32 NearClipPlane = 0.1f;

    if(DistanceToPZ > NearClipPlane)
    {
        v3 ProjectedXY = (1.0f / DistanceToPZ) * Transform->FocalLength*RawXY;
        *P = ProjectedXY;
        Valid = true;
    }

    return(Valid);
}

function void
PushPixmap(render_group *Group, v3 P, r32 Height, pixmap *Pixmap, v4 Color = V4(1, 1, 1, 1))
{
    Assert(Pixmap);
    v2 Dim = V2(Height*Pixmap->WidthOverHeight, Height);
    v3 Align = V3(Hadamard(Pixmap->AlignPercentage, Dim), 0.0f);
    P = (P - Align);

    if(ApplyTransform(&Group->Transform, &P))
    {
        render_group_entry_pixmap *Entry = PushRenderEntry(Group, render_group_entry_pixmap);
        if(Entry)
        {
            Entry->P = P;
            Entry->Dim = P.Z * Dim;
            Entry->Pixmap = Pixmap;
            Entry->Tint = Color;
        }
    }
}

function void
PushPixmap(render_group *Group, v3 P, r32 Height, pixmap_id SlotID, v4 Color = V4(1, 1, 1, 1))
{
    asset_slot *Slot = GetPixmapSlot(Group->Assets, SlotID);
    if(Slot)
    {
        PushPixmap(Group, P, Height, &Slot->Pixmap, Color);
    }
    else
    {
        LoadPixmap(Group->Assets, SlotID);
        Group->MissingResourceCount++;
    }
}

function void
PushRect(render_group *Group, v3 P, v2 Dim, v4 Color)
{
    if(ApplyTransform(&Group->Transform, &P))
    {
        render_group_entry_rectangle *Entry = PushRenderEntry(Group, render_group_entry_rectangle);
        if(Entry)
        {
            Entry->P = P;
            Entry->Dim = P.Z * Dim;
            Entry->Color = Color;
        }
    }
}

function void
PushRectOutline(render_group *Group, v3 P, v2 Dimension, v4 Color, r32 Thickness = 0.115f)
{
    // NOTE(gaurang): Top & Bottom
    PushRect(Group, P + V3(0.0f, 0.5f*Dimension.Y, 0.0f), V2(Dimension.X, Thickness), Color);
    PushRect(Group, P - V3(0.0f, 0.5f*Dimension.Y, 0.0f), V2(Dimension.X, Thickness), Color);

    // NOTE(gaurang): Left & Right
    PushRect(Group, P - V3(0.5f*Dimension.X, 0.0f, 0.0f), V2(Thickness, Dimension.Y), Color);
    PushRect(Group, P + V3(0.5f*Dimension.X, 0.0f, 0.0f), V2(Thickness, Dimension.Y), Color);
}

function void
Clear(render_group *Group, v4 Color)
{
    render_group_entry_clear *Entry = PushRenderEntry(Group, render_group_entry_clear);
    if(Entry)
    {
        Entry->Color = Color;
    }
}

function void
RenderGroupToOutput(render_group *Group, pixmap *Output, rectangle2i ClipRect)
{
    BEGIN_TIMED_BLOCK(RenderGroupToOutput);

    v2 ScreenDim = V2i(Output->Width, Output->Height);
    v2 Origin = 0.5f*ScreenDim;

    r32 MetersToPixels = Output->Height / Group->Transform.Height;

    for(u32 BaseAddress = 0;
        BaseAddress < Group->PushBufferSize;
        )
    {
        render_group_entry_header *Header = (render_group_entry_header *)(Group->PushBufferBase + BaseAddress);
        u8 *Data = (u8 *)Header + sizeof(*Header);
        BaseAddress += sizeof(*Header);

        switch(Header->Type)
        {
            case RenderGroupEntryType_render_group_entry_clear:
            {
                render_group_entry_clear *Entry = (render_group_entry_clear *)Data;

                DrawRectangle(Output, V2(0, 0), V2i(Output->Width, Output->Height), Entry->Color, ClipRect);

                BaseAddress += sizeof(*Entry);
            } break;

            case RenderGroupEntryType_render_group_entry_pixmap:
            {
                render_group_entry_pixmap *Entry = (render_group_entry_pixmap *)Data;

                v3 P = Entry->P;
                v2 Dim = Entry->Dim;

                P.XY *= MetersToPixels;
                P.XY += Origin;

                Dim *= MetersToPixels;

                Assert(Entry->Pixmap);
                DrawRectangleQuickly(Output, P.XY, V2(Dim.X, 0.0f), V2(0.0f, Dim.Y), Entry->Pixmap, Entry->Tint, ClipRect);

                BaseAddress += sizeof(*Entry);
            } break;

            case RenderGroupEntryType_render_group_entry_rectangle:
            {
                render_group_entry_rectangle *Entry = (render_group_entry_rectangle *)Data;

                v3 P = Entry->P;
                v2 Dim = Entry->Dim;

                P.XY *= MetersToPixels;
                P.XY += Origin;

                Dim *= MetersToPixels;

                v2 Min = V2(P.X, P.Y);
                Min -= 0.5f*Dim;
                v2 Max = V2(P.X, P.Y);
                Max += 0.5f*Dim;
                DrawRectangle(Output, Min, Max, Entry->Color, ClipRect);

                BaseAddress += sizeof(*Entry);
            } break;

            InvalidDefaultCase;
        }
    }

    END_TIMED_BLOCK(RenderGroupToOutput);
}

struct tiled_render_work
{
    render_group *Group;
    pixmap *Output;
    rectangle2i ClipRect;
};

PLATFORM_WORK_QUEUE_CALLBACK(DoTiledRenderWork)
{
    tiled_render_work *Work = (tiled_render_work *)Data;
    RenderGroupToOutput(Work->Group, Work->Output, Work->ClipRect);
}

function void
TiledRenderGroupToOutput(platform_work_queue *Queue, render_group *Group, pixmap *Output)
{
    s32 const TileCountY = 4;
    s32 const TileCountX = 4;

    s32 TileWidth = Output->Width / TileCountX;
    s32 TileHeight = Output->Height / TileCountY;

    tiled_render_work WorkArray[TileCountY*TileCountX];
    for(s32 TileY = 0;
        TileY < TileCountY;
        ++TileY)
    {
        for(s32 TileX = 0;
            TileX < TileCountX;
            ++TileX)
        {
            tiled_render_work *Work = WorkArray + TileY*TileCountX + TileX;

            Work->Group = Group;
            Work->Output = Output;

            Work->ClipRect.MinX = TileX*TileWidth;
            Work->ClipRect.MinY = TileY*TileHeight;
            Work->ClipRect.MaxX = Work->ClipRect.MinX + TileWidth;
            Work->ClipRect.MaxY = Work->ClipRect.MinY + TileHeight;

            Platform.AddEntry(Queue, DoTiledRenderWork, (void *)Work);
        }
    }

    Platform.CompleteAllWork(Queue);
}

function v2
Unproject(render_group *Group, v2 ProjectedXY, r32 DistanceFromCamera)
{
    v2 Result = (DistanceFromCamera / Group->Transform.FocalLength) * ProjectedXY;
    return(Result);
}

function rectangle2
GetCameraRectangleAtDistance(render_group *Group, r32 DistanceFromCamera)
{
    v2 ProjectedXY = 0.5f*V2(Group->Transform.Width, Group->Transform.Height);
    v2 RawXY = Unproject(Group, ProjectedXY, DistanceFromCamera);
    rectangle2 Result = RectCenterHalfDim(V2(0, 0), RawXY);
    return(Result);
}

function rectangle2
GetCameraRectangleAtTarget(render_group *Group)
{
    rectangle2 Result = GetCameraRectangleAtDistance(Group, Group->Transform.TargetDistanceFromCamera);
    return(Result);
}

function b32
AllResourcesPresent(render_group *Group)
{
    b32 Result = (Group->MissingResourceCount == 0);
    return(Result);
}

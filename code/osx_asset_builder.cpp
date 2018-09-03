/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

#include "osx_asset_builder.h"

#define DATA_DIR "../data"
#define FONT_DIR "../fonts"
#define TEST_DIR "../data/test"
#define TEST2_DIR "../data/test2"
#define TEST3_DIR "../data/test3"


#pragma pack(push, 1)
struct bitmap_header
{
    uint16 FileType;
    uint32 FileSize;
    uint16 Reserved1;
    uint16 Reserved2;
    uint32 BitmapOffset;
    uint32 Size;
    int32 Width;
    int32 Height;
    uint16 Planes;
    uint16 BitsPerPixel;
    uint32 Compression;
    uint32 SizeOfBitmap;
    int32 HorzResolution;
    int32 VertResolution;
    uint32 ColorsUsed;
    uint32 ColorsImportant;

    uint32 RedMask;
    uint32 GreenMask;
    uint32 BlueMask;
};

struct WAVE_header
{
    uint32 RIFFID;
    uint32 Size;
    uint32 WAVEID;
};

#define RIFF_CODE(a, b, c, d) (((uint32)(a) << 0) | ((uint32)(b) << 8) | ((uint32)(c) << 16) | ((uint32)(d) << 24))
enum
{
    WAVE_ChunkID_fmt = RIFF_CODE('f', 'm', 't', ' '),
    WAVE_ChunkID_data = RIFF_CODE('d', 'a', 't', 'a'),
    WAVE_ChunkID_RIFF = RIFF_CODE('R', 'I', 'F', 'F'),
    WAVE_ChunkID_WAVE = RIFF_CODE('W', 'A', 'V', 'E'),
};
struct WAVE_chunk
{
    uint32 ID;
    uint32 Size;
};

struct WAVE_fmt
{
    uint16 wFormatTag;
    uint16 nChannels;
    uint32 nSamplesPerSec;
    uint32 nAvgBytesPerSec;
    uint16 nBlockAlign;
    uint16 wBitsPerSample;
    uint16 cbSize;
    uint16 wValidBitsPerSample;
    uint32 dwChannelMask;
    uint8 SubFormat[16];
};

#pragma pack(pop)

struct entire_file
{
    u32 ContentsSize;
    void *Contents;
};


entire_file
ReadEntireFile(char *FileName)
{
    entire_file Result = {};

    FILE *In = fopen(FileName, "rb");
    if(In)
    {
        fseek(In, 0, SEEK_END);
        Result.ContentsSize = ftell(In);
        fseek(In, 0, SEEK_SET);

        Result.Contents = malloc(Result.ContentsSize);
        fread(Result.Contents, Result.ContentsSize, 1, In);
        fclose(In);
    }
    else
    {
        printf("ERROR: Cannot open file %s.\n", FileName);
    }

    return(Result);
}


internal loaded_bitmap
LoadBMP(char *FileName)
{
    loaded_bitmap Result = {};

    entire_file ReadResult = ReadEntireFile(FileName);
    if(ReadResult.ContentsSize != 0)
    {
        Result.Free = ReadResult.Contents;

        bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
        uint32 *Pixels = (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
        Result.Memory = Pixels;
        Result.Width = Header->Width;
        Result.Height = Header->Height;

        Assert(Result.Height >= 0);
        Assert(Header->Compression == 3);

        // NOTE(casey): If you are using this generically for some reason,
        // please remember that BMP files CAN GO IN EITHER DIRECTION and
        // the height will be negative for top-down.
        // (Also, there can be compression, etc., etc... DON'T think this
        // is complete BMP loading code because it isn't!!)

        // NOTE(casey): Byte order in memory is determined by the Header itself,
        // so we have to read out the masks and convert the pixels ourselves.
        uint32 RedMask = Header->RedMask;
        uint32 GreenMask = Header->GreenMask;
        uint32 BlueMask = Header->BlueMask;
        uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

        bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
        bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
        bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
        bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);

        Assert(RedScan.Found);
        Assert(GreenScan.Found);
        Assert(BlueScan.Found);
        Assert(AlphaScan.Found);

        int32 RedShiftDown = (int32)RedScan.Index;
        int32 GreenShiftDown = (int32)GreenScan.Index;
        int32 BlueShiftDown = (int32)BlueScan.Index;
        int32 AlphaShiftDown = (int32)AlphaScan.Index;

        uint32 *SourceDest = Pixels;
        for(int32 Y = 0;
            Y < Header->Height;
            ++Y)
        {
            for(int32 X = 0;
                X < Header->Width;
                ++X)
            {
                uint32 C = *SourceDest;

                v4 Texel = {(real32)((C & RedMask) >> RedShiftDown),
                            (real32)((C & GreenMask) >> GreenShiftDown),
                            (real32)((C & BlueMask) >> BlueShiftDown),
                            (real32)((C & AlphaMask) >> AlphaShiftDown)};

                Texel = SRGB255ToLinear1(Texel);
#if 1
                Texel.rgb *= Texel.a;
#endif
                Texel = Linear1ToSRGB255(Texel);

                *SourceDest++ = (((uint32)(Texel.a + 0.5f) << 24) |
                                 ((uint32)(Texel.r + 0.5f) << 16) |
                                 ((uint32)(Texel.g + 0.5f) << 8) |
                                 ((uint32)(Texel.b + 0.5f) << 0));
            }
        }
    }

    Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;

#if 0
    Result.Memory = (uint8 *)Result.Memory + Result.Pitch*(Result.Height - 1);
    Result.Pitch = -Result.Pitch;
#endif

    return(Result);
}


internal loaded_font *
LoadFont(char *FileName, char *FontName, int PixelHeight)
{
    loaded_font *Font = (loaded_font *)malloc(sizeof(loaded_font));

#if USE_FONTS_FROM_WINDOWS

    AddFontResourceExA(FileName, FR_PRIVATE, 0);
    Font->Win32Handle = CreateFontA(PixelHeight, 0, 0, 0,
                                     FW_NORMAL, // NOTE(casey): Weight
                                     FALSE, // NOTE(casey): Italic
                                     FALSE, // NOTE(casey): Underline
                                     FALSE, // NOTE(casey): Strikeout
                                     DEFAULT_CHARSET,
                                     OUT_DEFAULT_PRECIS,
                                     CLIP_DEFAULT_PRECIS,
                                     ANTIALIASED_QUALITY,
                                     DEFAULT_PITCH|FF_DONTCARE,
                                     FontName);

    SelectObject(GlobalFontDeviceContext, Font->Win32Handle);
    GetTextMetrics(GlobalFontDeviceContext, &Font->TextMetric);

#else // STB TrueType

    entire_file TTFFile = ReadEntireFile(FileName);

    if(TTFFile.ContentsSize != 0)
    {
        stbtt_InitFont(&Font->FontInfo, (u8 *)TTFFile.Contents, stbtt_GetFontOffsetForIndex((u8 *)TTFFile.Contents, 0));
        Font->Scale = stbtt_ScaleForPixelHeight(&Font->FontInfo, PixelHeight);

        stbtt_GetFontVMetrics(&Font->FontInfo,
        					  &Font->FontMetric.Ascent,
        					  &Font->FontMetric.Descent,
        					  &Font->FontMetric.Leading);

        Font->FontMetric.Ascent *= Font->Scale;
        Font->FontMetric.Descent *= -Font->Scale;
        Font->FontMetric.Leading *= Font->Scale;
    }

#endif // USE_FONTS_FROM_WINDOWS

    Font->MinCodePoint = INT_MAX;
    Font->MaxCodePoint = 0;

    // NOTE(casey): 5k characters should be more than enough for _anybody_!
    Font->MaxGlyphCount = 5000;
    Font->GlyphCount = 0;

    u32 GlyphIndexFromCodePointSize = ONE_PAST_MAX_FONT_CODEPOINT*sizeof(u32);
    Font->GlyphIndexFromCodePoint = (u32 *)malloc(GlyphIndexFromCodePointSize);
    memset(Font->GlyphIndexFromCodePoint, 0, GlyphIndexFromCodePointSize);

    Font->Glyphs = (hha_font_glyph *)malloc(sizeof(hha_font_glyph)*Font->MaxGlyphCount);
    size_t HorizontalAdvanceSize = sizeof(r32)*Font->MaxGlyphCount*Font->MaxGlyphCount;
    Font->HorizontalAdvance = (r32 *)malloc(HorizontalAdvanceSize);
    memset(Font->HorizontalAdvance, 0, HorizontalAdvanceSize);

    Font->OnePastHighestCodepoint = 0;

    // NOTE(casey): Reserve space for the null glyph
    Font->GlyphCount = 1;
    Font->Glyphs[0].UnicodeCodePoint = 0;
    Font->Glyphs[0].BitmapID.Value = 0;

    return(Font);
}


internal void
FinalizeFontKerning(loaded_font *Font)
{

#if USE_FONTS_FROM_WINDOWS

    SelectObject(GlobalFontDeviceContext, Font->Win32Handle);

    DWORD KerningPairCount = GetKerningPairsW(GlobalFontDeviceContext, 0, 0);
    KERNINGPAIR *KerningPairs = (KERNINGPAIR *)malloc(KerningPairCount*sizeof(KERNINGPAIR));
    GetKerningPairsW(GlobalFontDeviceContext, KerningPairCount, KerningPairs);
    for(DWORD KerningPairIndex = 0;
        KerningPairIndex < KerningPairCount;
        ++KerningPairIndex)
    {
        KERNINGPAIR *Pair = KerningPairs + KerningPairIndex;
        if((Pair->wFirst < ONE_PAST_MAX_FONT_CODEPOINT) &&
           (Pair->wSecond < ONE_PAST_MAX_FONT_CODEPOINT))
        {
            u32 First = Font->GlyphIndexFromCodePoint[Pair->wFirst];
            u32 Second = Font->GlyphIndexFromCodePoint[Pair->wSecond];
            if((First != 0) && (Second != 0))
            {
                Font->HorizontalAdvance[First*Font->MaxGlyphCount + Second] += (r32)Pair->iKernAmount;
            }
        }
    }

    free(KerningPairs);

#else  // STB Truetype

    for(u32 FirstGlyphIndex = 1;
        FirstGlyphIndex < Font->GlyphCount;
        ++FirstGlyphIndex)
    {
        hha_font_glyph First = Font->Glyphs[FirstGlyphIndex];

        for(u32 SecondGlyphIndex = 1;
            SecondGlyphIndex < Font->GlyphCount;
            ++SecondGlyphIndex)
        {
            hha_font_glyph Second = Font->Glyphs[SecondGlyphIndex];
            r32 KernAdvance = Font->Scale * stbtt_GetCodepointKernAdvance(&Font->FontInfo,
                                                                          First.UnicodeCodePoint,
                                                                          Second.UnicodeCodePoint);

            Font->HorizontalAdvance[SecondGlyphIndex*Font->MaxGlyphCount + FirstGlyphIndex] += KernAdvance;
        }
    }

#endif // USE_FONTS_FROM_WINDOWS
}


internal void
FreeFont(loaded_font *Font)
{
    if(Font)
    {
#if USE_FONTS_FROM_WINDOWS
        DeleteObject(Font->Win32Handle);
#endif

        free(Font->Glyphs);
        free(Font->HorizontalAdvance);
        free(Font->GlyphIndexFromCodePoint);
        free(Font);
    }
}


#if USE_FONTS_FROM_WINDOWS
internal void
InitializeFontDC(void)
{
    GlobalFontDeviceContext = CreateCompatibleDC(GetDC(0));

    BITMAPINFO Info = {};
    Info.bmiHeader.biSize = sizeof(Info.bmiHeader);
    Info.bmiHeader.biWidth = MAX_FONT_WIDTH;
    Info.bmiHeader.biHeight = MAX_FONT_HEIGHT;
    Info.bmiHeader.biPlanes = 1;
    Info.bmiHeader.biBitCount = 32;
    Info.bmiHeader.biCompression = BI_RGB;
    Info.bmiHeader.biSizeImage = 0;
    Info.bmiHeader.biXPelsPerMeter = 0;
    Info.bmiHeader.biYPelsPerMeter = 0;
    Info.bmiHeader.biClrUsed = 0;
    Info.bmiHeader.biClrImportant = 0;
    HBITMAP Bitmap = CreateDIBSection(GlobalFontDeviceContext, &Info, DIB_RGB_COLORS, &GlobalFontBits, 0, 0);
    SelectObject(GlobalFontDeviceContext, Bitmap);
    SetBkColor(GlobalFontDeviceContext, RGB(0, 0, 0));
}
#endif


internal loaded_bitmap
LoadGlyphBitmap(loaded_font *Font, u32 CodePoint, hha_asset *Asset)
{
    loaded_bitmap Result = {};

    u32 GlyphIndex = Font->GlyphIndexFromCodePoint[CodePoint];

#if USE_FONTS_FROM_WINDOWS

    SelectObject(GlobalFontDeviceContext, Font->Win32Handle);

    memset(GlobalFontBits, 0x00, MAX_FONT_WIDTH*MAX_FONT_HEIGHT*sizeof(u32));

    wchar_t CheesePoint = (wchar_t)CodePoint;

    SIZE Size;
    GetTextExtentPoint32W(GlobalFontDeviceContext, &CheesePoint, 1, &Size);

    int PreStepX = 128;

    int BoundWidth = Size.cx + 2*PreStepX;
    if(BoundWidth > MAX_FONT_WIDTH)
    {
        BoundWidth = MAX_FONT_WIDTH;
    }
    int BoundHeight = Size.cy;
    if(BoundHeight > MAX_FONT_HEIGHT)
    {
        BoundHeight = MAX_FONT_HEIGHT;
    }

//    PatBlt(DeviceContext, 0, 0, Width, Height, BLACKNESS);
//    SetBkMode(DeviceContext, TRANSPARENT);
    SetTextColor(GlobalFontDeviceContext, RGB(255, 255, 255));
    TextOutW(GlobalFontDeviceContext, PreStepX, 0, &CheesePoint, 1);

    s32 MinX = 10000;
    s32 MinY = 10000;
    s32 MaxX = -10000;
    s32 MaxY = -10000;

    u32 *Row = (u32 *)GlobalFontBits + (MAX_FONT_HEIGHT - 1)*MAX_FONT_WIDTH;
    for(s32 Y = 0;
        Y < BoundHeight;
        ++Y)
    {
        u32 *Pixel = Row;
        for(s32 X = 0;
            X < BoundWidth;
            ++X)
        {
#if 0
            COLORREF RefPixel = GetPixel(GlobalFontDeviceContext, X, Y);
            Assert(RefPixel == *Pixel);
#endif
            if(*Pixel != 0)
            {
                if(MinX > X)
                {
                    MinX = X;
                }

                if(MinY > Y)
                {
                    MinY = Y;
                }

                if(MaxX < X)
                {
                    MaxX = X;
                }

                if(MaxY < Y)
                {
                    MaxY = Y;
                }
            }

            ++Pixel;
        }
        Row -= MAX_FONT_WIDTH;
    }

    r32 KerningChange = 0;
    if(MinX <= MaxX)
    {
        int Width = (MaxX - MinX) + 1;
        int Height = (MaxY - MinY) + 1;

        Result.Width = Width + 2;
        Result.Height = Height + 2;
        Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;
        Result.Memory = malloc(Result.Height*Result.Pitch);
        Result.Free = Result.Memory;

        memset(Result.Memory, 0, Result.Height*Result.Pitch);

        u8 *DestRow = (u8 *)Result.Memory + (Result.Height - 1 - 1)*Result.Pitch;
        u32 *SourceRow = (u32 *)GlobalFontBits + (MAX_FONT_HEIGHT - 1 - MinY)*MAX_FONT_WIDTH;
        for(s32 Y = MinY;
            Y <= MaxY;
            ++Y)
        {
            u32 *Source = (u32 *)SourceRow + MinX;
            u32 *Dest = (u32 *)DestRow + 1;
            for(s32 X = MinX;
                X <= MaxX;
                ++X)
            {
#if 0
                COLORREF Pixel = GetPixel(GlobalFontDeviceContext, X, Y);
                Assert(Pixel == *Source);
#else
                u32 Pixel = *Source;
#endif
                r32 Gray = (r32)(Pixel & 0xFF);
                v4 Texel = {255.0f, 255.0f, 255.0f, Gray};
                Texel = SRGB255ToLinear1(Texel);
                Texel.rgb *= Texel.a;
                Texel = Linear1ToSRGB255(Texel);

                *Dest++ = (((uint32)(Texel.a + 0.5f) << 24) |
                           ((uint32)(Texel.r + 0.5f) << 16) |
                           ((uint32)(Texel.g + 0.5f) << 8) |
                           ((uint32)(Texel.b + 0.5f) << 0));


                ++Source;
            }

            DestRow -= Result.Pitch;
            SourceRow -= MAX_FONT_WIDTH;
        }

        Asset->Bitmap.AlignPercentage[0] = (1.0f) / (r32)Result.Width;
        Asset->Bitmap.AlignPercentage[1] = (1.0f + (MaxY - (BoundHeight - Font->TextMetric.tmDescent))) / (r32)Result.Height;

        KerningChange = (r32)(MinX - PreStepX);
    }

#if 0
    ABC ThisABC;
    GetCharABCWidthsW(GlobalFontDeviceContext, CodePoint, CodePoint, &ThisABC);
    r32 CharAdvance = (r32)(ThisABC.abcA + ThisABC.abcB + ThisABC.abcC);
#else
    INT ThisWidth;
    GetCharWidth32W(GlobalFontDeviceContext, CodePoint, CodePoint, &ThisWidth);
    r32 CharAdvance = (r32)ThisWidth;
#endif

#else // STB TrueType

    int Width, Height, XOffset, YOffset;
    u8 *MonoBitmap = stbtt_GetCodepointBitmap(&Font->FontInfo, 0, Font->Scale, CodePoint,
                                              &Width, &Height, &XOffset, &YOffset);

    Result.Width = Width + 2;
    Result.Height = Height + 2;
    Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;
    Result.Memory = malloc(Result.Height*Result.Pitch);
    Result.Free = Result.Memory;

    memset(Result.Memory, 0, Result.Height * Result.Pitch);

    u8 *DestRow = (u8 *)Result.Memory + (Result.Height - 2) * Result.Pitch;
    u8 *Source = MonoBitmap;
    for(s32 Y = 0;
        Y < Height;
        ++Y)
    {
        u32 *Dest = ((u32 *)DestRow) + 1;
        for(s32 X = 0;
            X < Width;
            ++X)
        {
            u32 Pixel = *Source;
            r32 Gray = (r32)(Pixel & 0xFF);
            v4 Texel = {255.0f, 255.0f, 255.0f, Gray};
            Texel = SRGB255ToLinear1(Texel);
            Texel.rgb *= Texel.a;
            Texel = Linear1ToSRGB255(Texel);

			*Dest++ = (((u32)(Texel.a + 0.5f) << 24) |
					   ((u32)(Texel.r + 0.5f) << 16) |
					   ((u32)(Texel.g + 0.5f) << 8) |
					   ((u32)(Texel.b + 0.5f) << 0));

            ++Source;
        }

        DestRow -= Result.Pitch;
    }

    stbtt_FreeBitmap(MonoBitmap, 0);
    r32 VAlignAdjustment = Height + YOffset;

    Asset->Bitmap.AlignPercentage[0] = (1.0f) / (r32)Result.Width;
    Asset->Bitmap.AlignPercentage[1] = (1.0f + VAlignAdjustment) / (r32)Result.Height;

    r32 KerningChange = (r32)XOffset;

    s32 ThisWidth;
    stbtt_GetCodepointHMetrics(&Font->FontInfo, CodePoint, &ThisWidth, 0);
    r32 CharAdvance = Font->Scale * (r32)ThisWidth;
#endif

    for(u32 OtherGlyphIndex = 0;
        OtherGlyphIndex < Font->MaxGlyphCount;
        ++OtherGlyphIndex)
    {
        Font->HorizontalAdvance[GlyphIndex*Font->MaxGlyphCount + OtherGlyphIndex] += CharAdvance - KerningChange;
        if(OtherGlyphIndex != 0)
        {
            Font->HorizontalAdvance[OtherGlyphIndex*Font->MaxGlyphCount + GlyphIndex] += KerningChange;
        }
    }

    return(Result);
}

struct riff_iterator
{
    uint8 *At;
    uint8 *Stop;
};

inline riff_iterator
ParseChunkAt(void *At, void *Stop)
{
    riff_iterator Iter;

    Iter.At = (uint8 *)At;
    Iter.Stop = (uint8 *)Stop;

    return(Iter);
}

inline riff_iterator
NextChunk(riff_iterator Iter)
{
    WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
    uint32 Size = (Chunk->Size + 1) & ~1;
    Iter.At += sizeof(WAVE_chunk) + Size;

    return(Iter);
}

inline bool32
IsValid(riff_iterator Iter)
{
    bool32 Result = (Iter.At < Iter.Stop);

    return(Result);
}

inline void *
GetChunkData(riff_iterator Iter)
{
    void *Result = (Iter.At + sizeof(WAVE_chunk));

    return(Result);
}

inline uint32
GetType(riff_iterator Iter)
{
    WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
    uint32 Result = Chunk->ID;

    return(Result);
}

inline uint32
GetChunkDataSize(riff_iterator Iter)
{
    WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
    uint32 Result = Chunk->Size;

    return(Result);
}

struct loaded_sound
{
    uint32 SampleCount; // NOTE(casey): This is the sample count divided by 8
    uint32 ChannelCount;
    int16 *Samples[2];

    void *Free;
};

internal loaded_sound
LoadWAV(char *FileName, u32 SectionFirstSampleIndex, u32 SectionSampleCount)
{
    loaded_sound Result = {};

    entire_file ReadResult = ReadEntireFile(FileName);
    if(ReadResult.ContentsSize != 0)
    {
        Result.Free = ReadResult.Contents;

        WAVE_header *Header = (WAVE_header *)ReadResult.Contents;
        Assert(Header->RIFFID == WAVE_ChunkID_RIFF);
        Assert(Header->WAVEID == WAVE_ChunkID_WAVE);

        uint32 ChannelCount = 0;
        uint32 SampleDataSize = 0;
        int16 *SampleData = 0;
        for(riff_iterator Iter = ParseChunkAt(Header + 1, (uint8 *)(Header + 1) + Header->Size - 4);
            IsValid(Iter);
            Iter = NextChunk(Iter))
        {
            switch(GetType(Iter))
            {
                case WAVE_ChunkID_fmt:
                {
                    WAVE_fmt *fmt = (WAVE_fmt *)GetChunkData(Iter);
                    Assert(fmt->wFormatTag == 1); // NOTE(casey): Only support PCM
                    Assert(fmt->nSamplesPerSec == 48000);
                    Assert(fmt->wBitsPerSample == 16);
                    Assert(fmt->nBlockAlign == (sizeof(int16)*fmt->nChannels));
                    ChannelCount = fmt->nChannels;
                } break;

                case WAVE_ChunkID_data:
                {
                    SampleData = (int16 *)GetChunkData(Iter);
                    SampleDataSize = GetChunkDataSize(Iter);
                } break;
            }
        }

        Assert(ChannelCount && SampleData);

        Result.ChannelCount = ChannelCount;
        u32 SampleCount = SampleDataSize / (ChannelCount*sizeof(int16));
        if(ChannelCount == 1)
        {
            Result.Samples[0] = SampleData;
            Result.Samples[1] = 0;
        }
        else if(ChannelCount == 2)
        {
            Result.Samples[0] = SampleData;
            Result.Samples[1] = SampleData + SampleCount;

#if 0
            for(uint32 SampleIndex = 0;
                SampleIndex < SampleCount;
                ++SampleIndex)
            {
                SampleData[2*SampleIndex + 0] = (int16)SampleIndex;
                SampleData[2*SampleIndex + 1] = (int16)SampleIndex;
            }
#endif

            for(uint32 SampleIndex = 0;
                SampleIndex < SampleCount;
                ++SampleIndex)
            {
                int16 Source = SampleData[2*SampleIndex];
                SampleData[2*SampleIndex] = SampleData[SampleIndex];
                SampleData[SampleIndex] = Source;
            }
        }
        else
        {
            Assert(!"Invalid channel count in WAV file");
        }

        // TODO(casey): Load right channels!
        b32 AtEnd = true;
        Result.ChannelCount = 1;
        if(SectionSampleCount)
        {
            Assert((SectionFirstSampleIndex + SectionSampleCount) <= SampleCount);
            AtEnd = ((SectionFirstSampleIndex + SectionSampleCount) == SampleCount);
            SampleCount = SectionSampleCount;
            for(uint32 ChannelIndex = 0;
                ChannelIndex < Result.ChannelCount;
                ++ChannelIndex)
            {
                Result.Samples[ChannelIndex] += SectionFirstSampleIndex;
            }
        }

        if(AtEnd)
        {
            for(uint32 ChannelIndex = 0;
                ChannelIndex < Result.ChannelCount;
                ++ChannelIndex)
            {
                for(u32 SampleIndex = SampleCount;
                    SampleIndex < (SampleCount + 8);
                    ++SampleIndex)
                {
                    Result.Samples[ChannelIndex][SampleIndex] = 0;
                }
            }
        }

        Result.SampleCount = SampleCount;
    }

    return(Result);
}

internal void
BeginAssetType(game_assets *Assets, asset_type_id_v0 TypeID)
{
    Assert(Assets->DEBUGAssetType == 0);

    Assets->DEBUGAssetType = Assets->AssetTypes + TypeID;
    Assets->DEBUGAssetType->TypeID = TypeID;
    Assets->DEBUGAssetType->FirstAssetIndex = Assets->AssetCount;
    Assets->DEBUGAssetType->OnePastLastAssetIndex = Assets->DEBUGAssetType->FirstAssetIndex;
}

struct added_asset
{
    u32 ID;
    hha_asset *HHA;
    asset_source *Source;
};
internal added_asset
AddAsset(game_assets *Assets)
{
    Assert(Assets->DEBUGAssetType);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < ArrayCount(Assets->Assets));

    u32 Index = Assets->DEBUGAssetType->OnePastLastAssetIndex++;
    asset_source *Source = Assets->AssetSources + Index;
    hha_asset *HHA = Assets->Assets + Index;
    HHA->FirstTagIndex = Assets->TagCount;
    HHA->OnePastLastTagIndex = HHA->FirstTagIndex;

    Assets->AssetIndex = Index;

    added_asset Result;
    Result.ID = Index;
    Result.HHA = HHA;
    Result.Source = Source;
    return(Result);
}

internal bitmap_id
AddBitmapAsset(game_assets *Assets, char *FileName, r32 AlignPercentageX = 0.5f, r32 AlignPercentageY = 0.5f)
{
    added_asset Asset = AddAsset(Assets);
    Asset.HHA->Bitmap.AlignPercentage[0] = AlignPercentageX;
    Asset.HHA->Bitmap.AlignPercentage[1] = AlignPercentageY;
    Asset.Source->Type = AssetType_Bitmap;
    Asset.Source->Bitmap.FileName = FileName;

    bitmap_id Result = {Asset.ID};
    return(Result);
}

internal bitmap_id
AddCharacterAsset(game_assets *Assets, loaded_font *Font, u32 Codepoint)
{
    added_asset Asset = AddAsset(Assets);
    Asset.HHA->Bitmap.AlignPercentage[0] = 0.0f; // NOTE(casey): Set later by extraction
    Asset.HHA->Bitmap.AlignPercentage[1] = 0.0f; // NOTE(casey): Set later by extraction
    Asset.Source->Type = AssetType_FontGlyph;
    Asset.Source->Glyph.Font = Font;
    Asset.Source->Glyph.Codepoint = Codepoint;

    bitmap_id Result = {Asset.ID};

    Assert(Font->GlyphCount < Font->MaxGlyphCount);
    u32 GlyphIndex = Font->GlyphCount++;
    hha_font_glyph *Glyph = Font->Glyphs + GlyphIndex;
    Glyph->UnicodeCodePoint = Codepoint;
    Glyph->BitmapID = Result;
    Font->GlyphIndexFromCodePoint[Codepoint] = GlyphIndex;

    if(Font->OnePastHighestCodepoint <= Codepoint)
    {
        Font->OnePastHighestCodepoint = Codepoint + 1;
    }

    return(Result);
}

internal sound_id
AddSoundAsset(game_assets *Assets, char *FileName, u32 FirstSampleIndex = 0, u32 SampleCount = 0)
{
    added_asset Asset = AddAsset(Assets);
    Asset.HHA->Sound.SampleCount = SampleCount;
    Asset.HHA->Sound.Chain = HHASoundChain_None;
    Asset.Source->Type = AssetType_Sound;
    Asset.Source->Sound.FileName = FileName;
    Asset.Source->Sound.FirstSampleIndex = FirstSampleIndex;

    sound_id Result = {Asset.ID};
    return(Result);
}

internal font_id
AddFontAsset(game_assets *Assets, loaded_font *Font)
{
    added_asset Asset = AddAsset(Assets);
    Asset.HHA->Font.OnePastHighestCodepoint = Font->OnePastHighestCodepoint;
    Asset.HHA->Font.GlyphCount = Font->GlyphCount;
#if USE_FONTS_FROM_WINDOWS
    Asset.HHA->Font.AscenderHeight = (r32)Font->TextMetric.tmAscent;
    Asset.HHA->Font.DescenderHeight = (r32)Font->TextMetric.tmDescent;
    Asset.HHA->Font.ExternalLeading = (r32)Font->TextMetric.tmExternalLeading;
#else
    Asset.HHA->Font.AscenderHeight = Font->FontMetric.Ascent;
    Asset.HHA->Font.DescenderHeight = Font->FontMetric.Descent;
    Asset.HHA->Font.ExternalLeading = Font->FontMetric.Leading;
#endif
    Asset.Source->Type = AssetType_Font;
    Asset.Source->Font.Font = Font;

    font_id Result = {Asset.ID};
    return(Result);
}

internal void
AddTag(game_assets *Assets, asset_tag_id ID, real32 Value)
{
    Assert(Assets->AssetIndex);

    hha_asset *HHA = Assets->Assets + Assets->AssetIndex;
    ++HHA->OnePastLastTagIndex;
    hha_tag *Tag = Assets->Tags + Assets->TagCount++;

    Tag->ID = ID;
    Tag->Value = Value;
}

internal void
EndAssetType(game_assets *Assets)
{
    Assert(Assets->DEBUGAssetType);
    Assets->AssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
    Assets->DEBUGAssetType = 0;
    Assets->AssetIndex = 0;
}

internal void
WriteHHA(game_assets *Assets, char *FileName)
{
    FILE *Out = fopen(FileName, "wb");
    if(Out)
    {
        hha_header_v0 Header = {};
        Header.MagicValue = HHA_MAGIC_VALUE;
        Header.Version = HHA_VERSION;
        Header.TagCount = Assets->TagCount;
        Header.AssetTypeCount = Asset_Count; // TODO(casey): Do we really want to do this?  Sparseness!
        Header.AssetCount = Assets->AssetCount;

        u32 TagArraySize = Header.TagCount*sizeof(hha_tag);
        u32 AssetTypeArraySize = Header.AssetTypeCount*sizeof(hha_asset_type);
        u32 AssetArraySize = Header.AssetCount*sizeof(hha_asset);

        Header.Tags = sizeof(Header);
        Header.AssetTypes = Header.Tags + TagArraySize;
        Header.Assets = Header.AssetTypes + AssetTypeArraySize;

        fwrite(&Header, sizeof(Header), 1, Out);
        fwrite(Assets->Tags, TagArraySize, 1, Out);
        fwrite(Assets->AssetTypes, AssetTypeArraySize, 1, Out);
        fseek(Out, AssetArraySize, SEEK_CUR);
        for(u32 AssetIndex = 1;
            AssetIndex < Header.AssetCount;
            ++AssetIndex)
        {
            asset_source *Source = Assets->AssetSources + AssetIndex;
            hha_asset *Dest = Assets->Assets + AssetIndex;

            Dest->DataOffset = ftell(Out);

            if(Source->Type == AssetType_Sound)
            {
                loaded_sound WAV = LoadWAV(Source->Sound.FileName,
                                           Source->Sound.FirstSampleIndex,
                                           Dest->Sound.SampleCount);

                Dest->Sound.SampleCount = WAV.SampleCount;
                Dest->Sound.ChannelCount = WAV.ChannelCount;

                for(u32 ChannelIndex = 0;
                    ChannelIndex < WAV.ChannelCount;
                    ++ChannelIndex)
                {
                    fwrite(WAV.Samples[ChannelIndex], Dest->Sound.SampleCount*sizeof(s16), 1, Out);
                }

                free(WAV.Free);
            }
            else if(Source->Type == AssetType_Font)
            {
                loaded_font *Font = Source->Font.Font;

                FinalizeFontKerning(Font);

                u32 GlyphsSize = Font->GlyphCount*sizeof(hha_font_glyph);
                fwrite(Font->Glyphs, GlyphsSize, 1, Out);

                u8 *HorizontalAdvance = (u8 *)Font->HorizontalAdvance;
                for(u32 GlyphIndex = 0;
                    GlyphIndex < Font->GlyphCount;
                    ++GlyphIndex)
                {
                    u32 HorizontalAdvanceSliceSize = sizeof(r32)*Font->GlyphCount;
                    fwrite(HorizontalAdvance, HorizontalAdvanceSliceSize, 1, Out);
                    HorizontalAdvance += sizeof(r32)*Font->MaxGlyphCount;
                }
            }
            else
            {
                loaded_bitmap Bitmap;
                if(Source->Type == AssetType_FontGlyph)
                {
                    Bitmap = LoadGlyphBitmap(Source->Glyph.Font, Source->Glyph.Codepoint, Dest);
                }
                else
                {
                    Assert(Source->Type == AssetType_Bitmap);
                    Bitmap = LoadBMP(Source->Bitmap.FileName);
                }

                Dest->Bitmap.Dim[0] = Bitmap.Width;
                Dest->Bitmap.Dim[1] = Bitmap.Height;

                Assert((Bitmap.Width*4) == Bitmap.Pitch);
                fwrite(Bitmap.Memory, Bitmap.Width*Bitmap.Height*4, 1, Out);

                free(Bitmap.Free);
            }
        }
        fseek(Out, (u32)Header.Assets, SEEK_SET);
        fwrite(Assets->Assets, AssetArraySize, 1, Out);

        fclose(Out);
    }
    else
    {
        printf("ERROR: Couldn't open file :(\n");
    }
}

internal void
Initialize(game_assets *Assets)
{
    Assets->TagCount = 1;
    Assets->AssetCount = 1;
    Assets->DEBUGAssetType = 0;
    Assets->AssetIndex = 0;

    Assets->AssetTypeCount = Asset_Count;
    memset(Assets->AssetTypes, 0, sizeof(Assets->AssetTypes));
}

internal void
WriteFonts(void)
{
    game_assets Assets_;
    game_assets *Assets = &Assets_;
    Initialize(Assets);

    loaded_font *Fonts[] =
    {
#if USE_FONTS_FROM_WINDOWS
        LoadFont("c:/Windows/Fonts/arial.ttf", "Arial", 128),
        LoadFont("c:/Windows/Fonts/LiberationMono-Regular.ttf", "Liberation Mono", 20),
#else
        // NOTE(jeff): You can also pick a .ttf or .ttc font on OS X from either:
        //               /System/Library/Fonts
        //             or
        //               ~/Library/Fonts
        // Note that .dfont suitcases do not work with stb_truetype.
        LoadFont(FONT_DIR"/LiberationSans-Regular.ttf", "Liberation Sans", 128),
        LoadFont(FONT_DIR"/LiberationMono-Regular.ttf", "Liberation Mono", 20),
#endif
    };

    BeginAssetType(Assets, Asset_FontGlyph);
    for(u32 FontIndex = 0;
        FontIndex < ArrayCount(Fonts);
        ++FontIndex)
    {
        loaded_font *Font = Fonts[FontIndex];

        AddCharacterAsset(Assets, Font, ' ');
        for(u32 Character = '!';
            Character <= '~';
            ++Character)
        {
            AddCharacterAsset(Assets, Font, Character);
        }

        // NOTE(casey): Kanji OWL!!!!!!!
        AddCharacterAsset(Assets, Font, 0x5c0f);
        AddCharacterAsset(Assets, Font, 0x8033);
        AddCharacterAsset(Assets, Font, 0x6728);
        AddCharacterAsset(Assets, Font, 0x514e);
    }
    EndAssetType(Assets);

    // TODO(casey): This is kinda janky, because it means you have to get this
    // order right always!
    BeginAssetType(Assets, Asset_Font);
    AddFontAsset(Assets, Fonts[0]);
    AddTag(Assets, Tag_FontType, FontType_Default);
    AddFontAsset(Assets, Fonts[1]);
    AddTag(Assets, Tag_FontType, FontType_Debug);
    EndAssetType(Assets);

    WriteHHA(Assets, DATA_DIR"/testfonts.hha");
}

internal void
WriteHero(void)
{
    game_assets Assets_;
    game_assets *Assets = &Assets_;
    Initialize(Assets);

    real32 AngleRight = 0.0f*Tau32;
    real32 AngleBack = 0.25f*Tau32;
    real32 AngleLeft = 0.5f*Tau32;
    real32 AngleFront = 0.75f*Tau32;

    r32 HeroAlign[] = {0.5f, 0.156682029f};

    BeginAssetType(Assets, Asset_Head);
    AddBitmapAsset(Assets, TEST_DIR"/test_hero_right_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, TEST_DIR"/test_hero_back_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, TEST_DIR"/test_hero_left_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, TEST_DIR"/test_hero_front_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Cape);
    AddBitmapAsset(Assets, TEST_DIR"/test_hero_right_cape.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, TEST_DIR"/test_hero_back_cape.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, TEST_DIR"/test_hero_left_cape.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, TEST_DIR"/test_hero_front_cape.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Torso);
    AddBitmapAsset(Assets, TEST_DIR"/test_hero_right_torso.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, TEST_DIR"/test_hero_back_torso.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, TEST_DIR"/test_hero_left_torso.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, TEST_DIR"/test_hero_front_torso.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    WriteHHA(Assets, DATA_DIR"/test1.hha");
}

internal void
WriteNonHero(void)
{
    game_assets Assets_;
    game_assets *Assets = &Assets_;
    Initialize(Assets);

    BeginAssetType(Assets, Asset_Shadow);
    AddBitmapAsset(Assets, TEST_DIR"/test_hero_shadow.bmp", 0.5f, 0.156682029f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Tree);
    AddBitmapAsset(Assets, TEST2_DIR"/tree00.bmp", 0.493827164f, 0.295652181f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Sword);
    AddBitmapAsset(Assets, TEST2_DIR"/rock03.bmp", 0.5f, 0.65625f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Grass);
    AddBitmapAsset(Assets, TEST2_DIR"/grass00.bmp");
    AddBitmapAsset(Assets, TEST2_DIR"/grass01.bmp");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Tuft);
    AddBitmapAsset(Assets, TEST2_DIR"/tuft00.bmp");
    AddBitmapAsset(Assets, TEST2_DIR"/tuft01.bmp");
    AddBitmapAsset(Assets, TEST2_DIR"/tuft02.bmp");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Stone);
    AddBitmapAsset(Assets, TEST2_DIR"/ground00.bmp");
    AddBitmapAsset(Assets, TEST2_DIR"/ground01.bmp");
    AddBitmapAsset(Assets, TEST2_DIR"/ground02.bmp");
    AddBitmapAsset(Assets, TEST2_DIR"/ground03.bmp");
    EndAssetType(Assets);

    WriteHHA(Assets, DATA_DIR"/test2.hha");
}

internal void
WriteSounds(void)
{
    game_assets Assets_;
    game_assets *Assets = &Assets_;
    Initialize(Assets);

    BeginAssetType(Assets, Asset_Bloop);
    AddSoundAsset(Assets, TEST3_DIR"/bloop_00.wav");
    AddSoundAsset(Assets, TEST3_DIR"/bloop_01.wav");
    AddSoundAsset(Assets, TEST3_DIR"/bloop_02.wav");
    AddSoundAsset(Assets, TEST3_DIR"/bloop_03.wav");
    AddSoundAsset(Assets, TEST3_DIR"/bloop_04.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Crack);
    AddSoundAsset(Assets, TEST3_DIR"/crack_00.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Drop);
    AddSoundAsset(Assets, TEST3_DIR"/drop_00.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Glide);
    AddSoundAsset(Assets, TEST3_DIR"/glide_00.wav");
    EndAssetType(Assets);

    u32 OneMusicChunk = 10*48000;
    u32 TotalMusicSampleCount = 7468095;
    BeginAssetType(Assets, Asset_Music);
    for(u32 FirstSampleIndex = 0;
        FirstSampleIndex < TotalMusicSampleCount;
        FirstSampleIndex += OneMusicChunk)
    {
        u32 SampleCount = TotalMusicSampleCount - FirstSampleIndex;
        if(SampleCount > OneMusicChunk)
        {
            SampleCount = OneMusicChunk;
        }
        sound_id ThisMusic = AddSoundAsset(Assets, TEST3_DIR"/music_test.wav", FirstSampleIndex, SampleCount);
        if((FirstSampleIndex + OneMusicChunk) < TotalMusicSampleCount)
        {
            Assets->Assets[ThisMusic.Value].Sound.Chain = HHASoundChain_Advance;
        }
    }
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Puhp);
    AddSoundAsset(Assets, TEST3_DIR"/puhp_00.wav");
    AddSoundAsset(Assets, TEST3_DIR"/puhp_01.wav");
    EndAssetType(Assets);

    WriteHHA(Assets, DATA_DIR"/test3.hha");
}

int
main(int ArgCount, char **Args)
{
#if USE_FONTS_FROM_WINDOWS
    InitializeFontDC();
#endif

    WriteFonts();
    WriteNonHero();
    WriteHero();
    WriteSounds();
}


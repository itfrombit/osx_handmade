#if !defined(TEST_ASSET_BUILDER_H)
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2015 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "handmade_platform.h"
#include "handmade_file_formats.h"
#include "handmade_intrinsics.h"
#include "handmade_math.h"

#define ONE_PAST_MAX_FONT_CODEPOINT (0x10FFFF + 1)

//#define USE_FONTS_FROM_WINDOWS 1

#if USE_FONTS_FROM_WINDOWS
#include <windows.h>

#define MAX_FONT_WIDTH 1024
#define MAX_FONT_HEIGHT 1024

global_variable VOID *GlobalFontBits;
global_variable HDC GlobalFontDeviceContext;

#else
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#endif
 
struct loaded_bitmap
{
    int32 Width;
    int32 Height;
    int32 Pitch;
    void *Memory;

    void *Free;
};

#if not USE_FONTS_FROM_WINDOWS
struct font_metric
{
	int32 Ascent;
	int32 Descent;
	int32 Leading;
};
#endif

struct loaded_font
{
#if USE_FONTS_FROM_WINDOWS
    HFONT Win32Handle;
    TEXTMETRIC TextMetric;
#else
	stbtt_fontinfo FontInfo;
	font_metric FontMetric;
	r32 Scale;
#endif

    r32 LineAdvance;

    hha_font_glyph *Glyphs;
    r32 *HorizontalAdvance;

    u32 MinCodePoint;
    u32 MaxCodePoint;
    
    u32 MaxGlyphCount;
    u32 GlyphCount;

    u32 *GlyphIndexFromCodePoint;
    u32 OnePastHighestCodepoint;
};

enum asset_type
{
    AssetType_Sound,
    AssetType_Bitmap,
    AssetType_Font,
    AssetType_FontGlyph,
};

struct loaded_font;
struct asset_source_font
{
    loaded_font *Font;
};

struct asset_source_font_glyph
{
    loaded_font *Font;
    u32 Codepoint;
};

struct asset_source_bitmap
{
    char *FileName;
};

struct asset_source_sound
{
    char *FileName;
    u32 FirstSampleIndex;
};

struct asset_source
{
    asset_type Type;
    union
    {
        asset_source_bitmap Bitmap;
        asset_source_sound Sound;
        asset_source_font Font;
        asset_source_font_glyph Glyph;
    };
};

// TODO(casey): Are there larger numbers than 4096?  Do we have evidence
// in the natural world of things that can exist in quantities _larger_ than 4096?
#define VERY_LARGE_NUMBER 4096    // NOTE(casey): 4096 should be enough for anybody

struct game_assets
{
    u32 TagCount;
    hha_tag Tags[VERY_LARGE_NUMBER];

    u32 AssetTypeCount;
    hha_asset_type AssetTypes[Asset_Count];

    u32 AssetCount;
    asset_source AssetSources[VERY_LARGE_NUMBER];
    hha_asset Assets[VERY_LARGE_NUMBER];

    hha_asset_type *DEBUGAssetType;
    u32 AssetIndex;
};

#define TEST_ASSET_BUILDER_H
#endif

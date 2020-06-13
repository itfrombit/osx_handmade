// osx_hhfont.cpp

#include "handmade_platform.h"
#include "handmade_intrinsics.h"
#include "handmade_math.h"
#include "handmade_shared.h"
#include "handmade_memory.h"
#include "handmade_stream.h"
#include "handmade_png.h"
#include "handmade_file_formats.h"
#include "handmade_stream.cpp"
#include "handmade_png.cpp"
#include "handmade_file_formats.cpp"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>

#include <ctype.h>

#include <CoreText/CoreText.h>


struct glyph_result
{
    v2 AlignPercentage;

    u32 Width;
    u32 Height;
    u32 *Pixels;
};

struct code_point_group
{
    UniChar OnePastMaxCodePoint;
    u32 GlyphCount;
    UniChar* CodePoints;
};


////////////////////////////////////////////////////////////////////////
// CTFont

#pragma pack(1)

// From Microsoft's font documentation spec.
typedef struct
{
	u16 Version;
	u16 NTables;
} kern_table_header;

typedef struct
{
	u16 Version;
	u16 Length; // in bytes, including this header
	u16 Coverage;
} kern_subtable_header;

typedef struct
{
	u16 NPairs;
	u16 SearchRange;
	u16 EntrySelector;
	u16 RangeShift;
} kern_subtable_format_0;

typedef struct
{
	u16 LeftGlyphIndex;
	u16 RightGlyphIndex;
	s16 FUnitValue; // in FUnits. > 0: move apart, < 0: move together
} kern_pair;

#pragma options align=reset


typedef struct u32_point
{
	u32 Width;
	u32 Height;
} u32_point;


typedef struct
{
	const char* FontName;
	CTFontRef FontRef;
	u32 PixelHeight;

	u32 GlyphCount;
	f32 Ascent;
	f32 Descent;
	f32 Leading;
	f32 UnitsPerEm;
	CGRect BoundingBox;

	u32_point MaxGlyphDim;
	u32_point MinGlyphOffset;

} osx_font;


osx_font* OSXCreateFont(const char* FontName, f32 PixelHeight)
{
	CFStringRef FontNameCF = CFStringCreateWithCString(kCFAllocatorDefault, FontName, kCFStringEncodingUTF8);
	CTFontRef FontRef = CTFontCreateWithName(FontNameCF, PixelHeight, NULL);

	if (FontRef == NULL)
	{
		printf("Could not create font '%s'\n", FontName);
		return NULL;
	}

	u32 GlyphCount = CTFontGetGlyphCount(FontRef);
	float Ascent = CTFontGetAscent(FontRef);
	float Descent = CTFontGetDescent(FontRef);
	float Leading = CTFontGetLeading(FontRef);
	float UnitsPerEm = CTFontGetUnitsPerEm(FontRef);
	CGRect BoundingBox = CTFontGetBoundingBox(FontRef);

	u32_point MaxGlyphDim =
	{
		(u32)ceil(BoundingBox.size.width) + 1,
		(u32)ceil(BoundingBox.size.height) + 1,
	};

	u32_point MinGlyphOffset =
	{
		(u32)floor(BoundingBox.origin.x),
		(u32)floor(BoundingBox.origin.y),
	};

	osx_font* Font = (osx_font*)malloc(sizeof(osx_font));
	Font->FontName = FontName;
	Font->FontRef = FontRef;
	Font->PixelHeight = PixelHeight;

	Font->GlyphCount = GlyphCount;
	Font->Ascent = Ascent;
	Font->Descent = Descent;
	Font->Leading = Leading;
	Font->UnitsPerEm = UnitsPerEm;
	Font->BoundingBox = BoundingBox;
	Font->MaxGlyphDim = MaxGlyphDim;
	Font->MinGlyphOffset = MinGlyphOffset;

	///////////////////////////////////////////////////////////////////


	printf("Font Metrics:\n");
	printf("      glyph count: %u\n", GlyphCount);
	printf("           ascent: %f\n", Ascent);
	printf("          descent: %f\n", Descent);
	printf("          leading: %f\n", Leading);
	printf("     units per em: %f\n", UnitsPerEm);
	printf("     bounding box: min:[%f %f] max:[%f %f]\n",
			BoundingBox.origin.x,
			BoundingBox.origin.y,
			BoundingBox.size.width,
			BoundingBox.size.height);
	printf("    max glyph dim: [%d %d]\n",
			MaxGlyphDim.Width,
			MaxGlyphDim.Height);
	printf(" min glyph offset: [%d %d]\n",
			MinGlyphOffset.Width,
			MinGlyphOffset.Height);

	CFRelease(FontNameCF);

	return Font;
}

void OSXDeleteFont(osx_font* Font)
{
	if (Font)
	{
		CFRelease(Font->FontRef);
		free(Font);
	}
}


////////////////////////////////////////////////////////////////////////
//

typedef struct
{
	u32 Width;
	u32 Height;
	u32 ImageSize;

	CGContextRef Context;
	CGColorSpaceRef ColorSpace;
	CGColorRef FillColor;
	char* RenderData;

} osx_bitmap_context;


osx_bitmap_context* OSXCreateBitmapContext(u32 Width, u32 Height, CGFloat FillColorComponents[4])
{
	u32 BytesPerPixel = 4;
	u32 BitsPerComponent = 8;
	u32 BytesPerRow = Width * BytesPerPixel;
	u32 ImageSize = BytesPerRow * Height;

	const CGColorSpaceRef ColorSpace = CGColorSpaceCreateDeviceRGB();
	if (ColorSpace == NULL)
	{
		printf("Error creating ColorSpace\n");
		return NULL;
	}

	// TODO(jeff): Should we hardcode this Premultiplied Alpha flag?
	u32 BitmapInfo = kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst;

	char* RenderData = (char*)malloc(ImageSize);

	const CGContextRef Context = CGBitmapContextCreate(RenderData, Width, Height,
													   BitsPerComponent, BytesPerRow,
													   ColorSpace, BitmapInfo);
	if (Context == NULL)
	{
		printf("Error creating graphics context\n");
		return NULL;
	}

	CGColorRef FillColor = CGColorCreate(ColorSpace, FillColorComponents);
	CGContextSetTextDrawingMode(Context, kCGTextFill);
	CGContextSetFillColorWithColor(Context, FillColor);

	osx_bitmap_context* BitmapContext = (osx_bitmap_context*)malloc(sizeof(osx_bitmap_context));

	BitmapContext->Width = Width;
	BitmapContext->Height = Height;
	BitmapContext->ImageSize = ImageSize;
	BitmapContext->Context = Context;
	BitmapContext->ColorSpace = ColorSpace;
	BitmapContext->FillColor = FillColor;
	BitmapContext->RenderData = RenderData;

	return BitmapContext;
}


void OSXDeleteBitmapContext(osx_bitmap_context* BitmapContext)
{
	if (BitmapContext)
	{
		if (BitmapContext->RenderData)
		{
			free(BitmapContext->RenderData);
		}

		if (BitmapContext->ColorSpace)
		{
			CFRelease(BitmapContext->ColorSpace);
		}

		if (BitmapContext->FillColor)
		{
			CFRelease(BitmapContext->FillColor);
		}

		if (BitmapContext->Context)
		{
			CFRelease(BitmapContext->Context);
		}
	}
}


void OSXEraseBitmapContext(osx_bitmap_context* BitmapContext, u32 Value)
{
	memset(BitmapContext->RenderData, Value, BitmapContext->ImageSize);
}


void OSXBitmapContextPrintAsASCII(osx_bitmap_context* BitmapContext)
{
	for (u32 i = 0; i < BitmapContext->Height; ++i)
	{
		for (u32 j = 0; j < BitmapContext->Width; ++j)
		{
			u32 Offset = 4*(i * BitmapContext->Width + j);

			if (BitmapContext->RenderData[Offset] == 0x00000000)
			{
				printf(" ");
			}
			else
			{
				if (BitmapContext->RenderData[Offset] == 0xFFFFFFFF)
				{
					printf("+");
				}
				else
				{
					printf(".");
				}
			}
		}

#if 0
		for (u32 j = 0; j < BitmapContext->Width; ++j)
		{
			u32 Offset = 4*(i * BitmapContext->Width + j);
			printf(" %08x", BitmapContext->RenderData[Offset]);
		}
#endif

		printf("\n");
	}
	printf("\n");

}

////////////////////////////////////////////////////////////////////////
//

internal void
DataStreamToFILE(stream *Source, FILE *Dest)
{
    for(stream_chunk *Chunk = Source->First;
        Chunk;
        Chunk = Chunk->Next)
    {
        fwrite(Chunk->Contents.Data, Chunk->Contents.Count, 1, Dest);
    }
}


internal glyph_result
LoadGlyphBitmap(osx_bitmap_context* BitmapContext, osx_font* Font, CGGlyph GlyphIndex,
                void *OutMemory, u32 Scale)
{
	OSXEraseBitmapContext(BitmapContext, 0x00);

	CGPoint Origin = {-Font->MinGlyphOffset.Width,
					  -Font->MinGlyphOffset.Height};


	CGRect OpticalRect = {};
	CTFontGetOpticalBoundsForGlyphs(Font->FontRef, &GlyphIndex, &OpticalRect, 1, 0);

	// This is a more compact offset to use, but since we're
	// doing our own boundary detection below, we'll use the
	// above Origin which gives us lots of safety space.
	//CGPoint Origin = { -floor(OpticalRect.origin.x),
	//                  -floor(OpticalRect.origin.y)};

	u32 BoundWidth = OpticalRect.size.width;
	u32 BoundHeight = OpticalRect.size.height;

	CTFontDrawGlyphs(Font->FontRef, &GlyphIndex, &Origin, 1, BitmapContext->Context);

#if 1
	printf("------------------------------------------------------------\n");
	OSXBitmapContextPrintAsASCII(BitmapContext);

	printf("Glyph: %u\n", GlyphIndex);
	printf("\n");

	printf("BitmapContext size: [%d %d]\n",
			BitmapContext->Width,
			BitmapContext->Height);
	printf("MinGlyphOffset: [%d %d]\n",
			Font->MinGlyphOffset.Width,
			Font->MinGlyphOffset.Height);
	printf("optical bounds: min:[%f %f] max:[%f %f]\n",
			OpticalRect.origin.x,
			OpticalRect.origin.y,
			OpticalRect.size.width,
			OpticalRect.size.height);

#endif


#if 0
	// If we're doing our own oversampling.
    if (Scale > 1)
    {
        BoundWidth /= Scale;
        BoundHeight /= Scale;
        u32 *Row = (u32 *)FontBits + (MaxGlyphDim.height - 1) * MaxGlyphDim.width;
        u32 *SampleRow = (u32 *)FontBits + (MaxGlyphDim.height - 1) * MaxGlyphDim.width;
        for(u32 Y = 0;
            Y < BoundHeight;
            ++Y)
        {
            u32 *Pixel = Row;
            u32 *Sample = SampleRow;
            for(u32 X = 0;
                X < BoundWidth;
                ++X)
            {
                u32 Accum = 0;
                u32 *SampleInner = Sample;
                for(u32 YOffset = 0;
                    YOffset < Scale;
                    ++YOffset)
                {
                    for(u32 XOffset = 0;
                        XOffset < Scale;
                        ++XOffset)
                    {
                        Accum += (SampleInner[XOffset] & 0xFF);
                    }

                    SampleInner -= MaxGlyphDim.width;
                }
                Accum /= (Scale*Scale); // TODO(casey): Round this value?

                *Pixel++ = Accum;
                Sample += Scale;
            }
            Row -= MaxGlyphDim.width;
            SampleRow -= Scale*MaxGlyphDim.width;
        }
    }
#endif

    u32 MinX = S32Max;
    u32 MinY = S32Max;
    u32 MaxX = 0;
    u32 MaxY = 0;

	char* RenderData = BitmapContext->RenderData;

	u32* Row = (u32*)RenderData;

    for (u32 Y = 0;
         //Y < BoundHeight;
         Y < Font->MaxGlyphDim.Height;
         ++Y)
    {
        u32 *Pixel = Row;
        for (u32 X = 0;
             //X < BoundWidth;
             X < Font->MaxGlyphDim.Width;
             ++X)
        {
            if (*Pixel != 0)
            {
                if (X < MinX)
                {
                    MinX = X;
                }

                if (Y < MinY)
                {
                    MinY = Y;
                }

                if (X > MaxX)
                {
                    MaxX = X;
                }

                if (Y > MaxY)
                {
                    MaxY = Y;
                }
            }

            ++Pixel;
        }

        Row += Font->MaxGlyphDim.Width;
    }

    glyph_result Result = {};

    f32 KerningChange = 0;
    v2 AlignPercentage = {0.5f, 0.5f};

    if (MinX <= MaxX)
    {
        u32 Width = (MaxX - MinX) + 1;
        u32 Height = (MaxY - MinY) + 1;

        u32 BytesPerPixel = 4;

        u32 OutWidth = Width + 2;
        u32 OutHeight = Height + 2;
        u32 OutPitch = OutWidth*BytesPerPixel;

        memset(OutMemory, 0, OutHeight*OutPitch);

        Result.Width = OutWidth;
        Result.Height = OutHeight;
        Result.Pixels = (u32 *)OutMemory;

		printf("MinX = %u  Max X = %u  MinY = %u  MaxY = %u\n", MinX, MaxX, MinY, MaxY);

        u32 *SourceRow = (u32 *)RenderData + MinY * Font->MaxGlyphDim.Width;

		// The + OutPitch is to leave an empty apron row of pixels on top
        u8 *DestRow = (u8 *)OutMemory + OutPitch;

        for (u32 Y = MinY;
             Y <= MaxY;
             ++Y)
        {
            u32 *Source = (u32 *)SourceRow + MinX;
            u32 *Dest = (u32 *)DestRow + 1;
            for (u32 X = MinX;
                 X <= MaxX;
                 ++X)
            {
                u32 Pixel = *Source;
                u32 Gray = (u32)(Pixel & 0xFF);
                *Dest++ = ((Gray << 24) | 0x00FFFFFF);
                ++Source;
            }

            DestRow += OutPitch;

            SourceRow += Font->MaxGlyphDim.Width;
        }

        AlignPercentage = V2((1.0f) / (f32)OutWidth,
                             (1.0f + (f32)(MaxY - ((f32)BitmapContext->Height - Origin.y))) / (f32)OutHeight);

		printf(
	"AlignPercentage: OutWidth = %d  OutHeight = %d  BoundHeight = %d  Descent = %.3f  Align.x = %.3f Align.y = %.3f\n",
		       OutWidth, OutHeight, BoundHeight, Font->Descent, AlignPercentage.x, AlignPercentage.y);
    }

    Result.AlignPercentage = AlignPercentage;

    return Result;

}

internal void
Sanitize(const char *Source, char *Dest)
{
    while(*Source)
    {
        int D = tolower(*Source);
        if(((D >= 'a') &&
            (D <= 'z')) ||
           ((D >= '0') &&
            (D <= '9')))
        {
            *Dest = (char)D;
        }
        else
        {
            *Dest = '_';
        }

        ++Dest;
        ++Source;
    }

    *Dest = 0;
}


internal void
ExtractFont(const char *FontName, u32 PixelHeight, b32x ManualAntialias, code_point_group *CodePointGroup,
            FILE *HHTOut, const char *PNGDestDir)
{
	////////////////////////////////////////////////////////////////////
	// Set up the file names for our output
	//
    char *NameStem = (char *)malloc(strlen(FontName) + 1);
    Sanitize(FontName, NameStem);

    char *PNGOutName = (char *)malloc(strlen(NameStem) + strlen(PNGDestDir) + 128);
    char *PNGFileNameOnly = PNGOutName + strlen(PNGDestDir) + 1;

	////////////////////////////////////////////////////////////////////
	//
    u32 GlyphCount = CodePointGroup->GlyphCount;
    UniChar* GlyphCodePoints = CodePointGroup->CodePoints;

	////////////////////////////////////////////////////////////////////
	// If we decide to do our own antialiasing...
    u32 Scale = (ManualAntialias) ? 4 : 1;
    f32 ScaleRatio = 1.0f / (f32)Scale;
    u32 SamplePixelHeight = Scale*PixelHeight;

	////////////////////////////////////////////////////////////////////
	// Use CoreText to create the Font and get the metrics
	//
	osx_font* Font = OSXCreateFont(FontName, SamplePixelHeight);

	if (Font)
	{
		////////////////////////////////////////////////////////////////////
		// Create a Bitmap context to render into
		//
		CGFloat FillColorComponents[4] = {1.0, 1.0, 1.0, 1.0};
		osx_bitmap_context* BitmapContext = OSXCreateBitmapContext(Font->MaxGlyphDim.Width,
		                                                           Font->MaxGlyphDim.Height,
		                                                           FillColorComponents);

		// Just experimenting
		//CGContextSetAllowsFontSubpixelQuantization(Context, false);
		//CGContextSetShouldSubpixelQuantizeFonts(Context, false);

		CGContextSetShouldAntialias(BitmapContext->Context, true);
		CGContextSetShouldSmoothFonts(BitmapContext->Context, true);

		OSXEraseBitmapContext(BitmapContext, 0x00);

		// Glyphs for the respective array of CodePoints
		CGGlyph* GlyphIndices = (CGGlyph*)malloc(GlyphCount * sizeof(CGGlyph));
		memset(GlyphIndices, 0, GlyphCount * sizeof(CGGlyph));

		// Default horizontal advances for the respective array of CodePoints
		CGSize* GlyphAdvances = (CGSize*)malloc(GlyphCount * sizeof(CGSize));
		memset(GlyphAdvances, 0, GlyphCount * sizeof(CGSize));

		// Note: these functions don't do any type of kerning adjustments
		CTFontGetGlyphsForCharacters(Font->FontRef, GlyphCodePoints, GlyphIndices, GlyphCount);
		CTFontGetAdvancesForGlyphs(Font->FontRef, kCTFontOrientationHorizontal, GlyphIndices, GlyphAdvances, GlyphCount);

		////////////////////////////////////////////////////////////////
		// A CodePoint is not synonymous with a Glyph Index.
		//
		// Also, there is not a 1-1 way to go from a Glyph Index to a
		// CodePoint. And the Glyph table that we care about is very
		// sparse. So keep an index that maps Glyph Index to the offset
		// index in our arrays.

		CGGlyph MaxGlyphIndex = 0;

		for (u32 i = 0; i < GlyphCount; ++i)
		{
			if (GlyphIndices[i] > MaxGlyphIndex)
			{
				MaxGlyphIndex = GlyphIndices[i];
			}
		}

		u32 OnePastMaxGlyphIndex = MaxGlyphIndex + 1;

		u32* ArrayIndexForGlyphIndex = (u32*)malloc(OnePastMaxGlyphIndex * sizeof(u32));
		memset(ArrayIndexForGlyphIndex, 0, OnePastMaxGlyphIndex * sizeof(u32));

		for (u32 i = 0; i < GlyphCount; ++i)
		{
			if (GlyphIndices[i] == 0)
			{
				printf("No GlyphIndex for CodePoint %u. Assuming this is a direct glyph.\n", GlyphCodePoints[i]);

				GlyphIndices[i] = GlyphCodePoints[i];
			}
			else
			{
#if 1
				printf("[%4u] CodePoint %u:  ASCII = %c  GlyphIndex = %u  Advance: [%f, %f]\n",
						i, GlyphCodePoints[i], GlyphCodePoints[i], GlyphIndices[i],
						GlyphAdvances[i].width, GlyphAdvances[i].height);
#endif
			}

			ArrayIndexForGlyphIndex[GlyphIndices[i]] = i;
		}

		u32 HorizontalAdvanceCount = GlyphCount * GlyphCount;
		size_t HorizontalAdvanceSize = sizeof(r32) * GlyphCount * GlyphCount;
		r32 *HorizontalAdvance = (r32*)malloc(HorizontalAdvanceSize);
		memset(HorizontalAdvance, 0, HorizontalAdvanceSize);

		for (u32 i = 1; i < GlyphCount; ++i)
		{
			for (u32 j = 1; j < GlyphCount; ++j)
			{
				CGGlyph GlyphPair[2];
				GlyphPair[0] = GlyphIndices[i];
				GlyphPair[1] = GlyphIndices[j];
				CGSize GlyphPairAdvance[2];

				// NOTE(jeff): CTFontGetAdvancesForGlyphs does NOT adjust for kerning
				// We adjust for kerning below, if applicable.
				f64 PairAdvance = GlyphAdvances[i].width;
				HorizontalAdvance[i * GlyphCount + j] = (r32)PairAdvance;

#if 0
				printf(
					"G1: cp[%u] asc[%c] gid[%u] adv=%.2lf  G2: cp[%u] asc[%c] gid[%u] adv=%.2lf  Advance: %.2lf  KernAdvance: %.2f\n",
						GlyphCodePoints[i], GlyphCodePoints[i], GlyphIndices[i], GlyphPairAdvance[0].width,
						GlyphCodePoints[j], GlyphCodePoints[j], GlyphIndices[j], GlyphPairAdvance[1].width,
						PairAdvance,
						HorizontalAdvance[i * GlyphCount + j]);
#endif
			}
		}

		////////////////////////////////////////////////////////////////
		// Parse the kerning tables, if present. They will generally
		// not be present for fixed width fonts.
		//
		// NOTE(jeff): It is clear that CoreText does not expect
		// anyone to manually parse these tables. The common CoreText
		// usage is to pass it a run of characters and it internally
		// lays out the text and applies kerning and other text metrics.
		// This is not that use case.
		CFDataRef KerningDataRef = CTFontCopyTable(Font->FontRef, kCTFontTableKern,
		                                           kCTFontTableOptionNoOptions);

		if (KerningDataRef == NULL)
		{
			printf("Could not retrieve Kern table. Maybe this is a monospace font?\n");
		}
		else
		{
			u8* KerningData = (u8*)CFDataGetBytePtr(KerningDataRef);
			CFIndex KerningDataLength = CFDataGetLength(KerningDataRef);

			printf("\n");
			printf("KerningDataRef length: %ld\n", KerningDataLength);

			u8* ReadPtr = KerningData;

			kern_table_header* KernTableHeaderPtr = (kern_table_header*)ReadPtr;
			kern_table_header KernTableHeader;
			KernTableHeader.Version = ntohs(KernTableHeaderPtr->Version);
			KernTableHeader.NTables = ntohs(KernTableHeaderPtr->NTables);
			ReadPtr += sizeof(kern_table_header);

			kern_subtable_header* KernSubtableHeaderPtr = (kern_subtable_header*)ReadPtr;
			kern_subtable_header KernSubtableHeader;
			KernSubtableHeader.Version = ntohs(KernSubtableHeaderPtr->Version);
			KernSubtableHeader.Length = ntohs(KernSubtableHeaderPtr->Length);
			KernSubtableHeader.Coverage = ntohs(KernSubtableHeaderPtr->Coverage);
			ReadPtr += sizeof(kern_subtable_header);

			kern_subtable_format_0* KernSubtableFormat0Ptr = (kern_subtable_format_0*)ReadPtr;
			kern_subtable_format_0 KernSubtableFormat0;
			KernSubtableFormat0.NPairs = ntohs(KernSubtableFormat0Ptr->NPairs);
			KernSubtableFormat0.SearchRange = ntohs(KernSubtableFormat0Ptr->SearchRange);
			KernSubtableFormat0.EntrySelector = ntohs(KernSubtableFormat0Ptr->EntrySelector);
			KernSubtableFormat0.RangeShift = ntohs(KernSubtableFormat0Ptr->RangeShift);
			ReadPtr += sizeof(kern_subtable_format_0);

			printf("  kern table header\n");
			printf("           version = %hu\n", KernTableHeader.Version);
			printf("          n_tables = %u\n", KernTableHeader.NTables);
			printf("\n");
			printf("  kern subtable header\n");
			printf("           version = %hu\n", KernSubtableHeader.Version);
			printf("            length = %hu\n", KernSubtableHeader.Length);
			printf("          coverage = %hu\n", KernSubtableHeader.Coverage);
			printf("\n");
			printf("  kern format 0 header\n");
			printf("            npairs = %hu\n", KernSubtableFormat0.NPairs);
			printf("      search_range = %hu\n", KernSubtableFormat0.SearchRange);
			printf("    entry_selector = %hu\n", KernSubtableFormat0.EntrySelector);
			printf("       range_shift = %hu\n", KernSubtableFormat0.RangeShift);
			printf("\n");

			// To convert from FUnits to pixel units
			r32 FUnitScaleFactor = (r32)PixelHeight / Font->UnitsPerEm;

			for (u32 i = 0; i < KernSubtableFormat0.NPairs; ++i)
			{
				kern_pair* KernPairPtr = (kern_pair*)ReadPtr;
				kern_pair KernPair;
				u16 LeftGlyphIndex = ntohs(KernPairPtr->LeftGlyphIndex);
				u16 RightGlyphIndex = ntohs(KernPairPtr->RightGlyphIndex);
				s16 FUnitValue = ntohs(KernPairPtr->FUnitValue); // in FUnits
				r32 ScaledValue = (r32)FUnitValue * FUnitScaleFactor;
				ReadPtr += sizeof(kern_pair);

				// Make sure this kerning pair falls within our range of glyphs
				u32 First = 0;
				if (LeftGlyphIndex < OnePastMaxGlyphIndex)
				{
					First = ArrayIndexForGlyphIndex[LeftGlyphIndex];
				}

				u32 Second = 0;
				if (RightGlyphIndex < OnePastMaxGlyphIndex)
				{
					Second = ArrayIndexForGlyphIndex[RightGlyphIndex];
				}

				if ((First != 0) && (Second != 0))
				{
					HorizontalAdvance[First * GlyphCount + Second] += ScaledValue;
				}

				//printf("left = %hu  right = %hu  FUnitValue = %hd  ScaledValue = %.2f\n",
				//		LeftGlyphIndex, RightGlyphIndex, FUnitValue, ScaledValue);
			}

			printf("\n");

			if (KerningDataRef) CFRelease(KerningDataRef);
		}


#if 0
		// debug dump HorizontalAdvance table
		for (u32 i = 1; i < GlyphCount; ++i)
		{
			for (u32 j = 1; j < GlyphCount; ++j)
			{
				printf(
					"G1: cp[%u] asc[%c] gid[%u] adv=%.2lf  G2: cp[%u] asc[%c] gid[%u] adv=%.2lf  Advance: %.2lf  KernAdvance: %.2lf\n",
						GlyphCodePoints[i], GlyphCodePoints[i], GlyphIndices[i], GlyphAdvances[i].width,
						GlyphCodePoints[j], GlyphCodePoints[j], GlyphIndices[j], GlyphAdvances[j].width,
						GlyphAdvances[i].width + GlyphAdvances[j].width,
						HorizontalAdvance[i * GlyphCount + j]);
			}
		}
#endif

		fprintf(HHTOut, "font \"%s\"\n", NameStem);
		fprintf(HHTOut, "{\n");
		fprintf(HHTOut, "    GlyphCount = %u;\n", GlyphCount);
		fprintf(HHTOut, "    Tags = FontType(10);\n");
		fprintf(HHTOut, "    AscenderHeight = %f;\n", ScaleRatio * Font->Ascent);
		fprintf(HHTOut, "    DescenderHeight = %f;\n", ScaleRatio * Font->Descent);
		fprintf(HHTOut, "    ExternalLeading = %f;\n", ScaleRatio * Font->Leading);

		void *OutMemory = malloc(Font->MaxGlyphDim.Width * Font->MaxGlyphDim.Height * sizeof(u32));

		for(u32 i = 1;
			i < GlyphCount;
			++i)
		{
			u32 CodePoint = GlyphCodePoints[i];
			CGGlyph GlyphIndex = GlyphIndices[i];

			glyph_result Glyph = LoadGlyphBitmap(BitmapContext, Font, GlyphIndex,
												 OutMemory, Scale);

			sprintf(PNGOutName, "%s/%s_%04u.png", PNGDestDir, NameStem, CodePoint);

			memory_arena TempArena = {};
			stream PNGStream = OnDemandMemoryStream(&TempArena);
			WritePNG(Glyph.Width, Glyph.Height, Glyph.Pixels, &PNGStream);
			FILE *PNGOutFile = fopen(PNGOutName, "wb");
			if(PNGOutFile)
			{
				DataStreamToFILE(&PNGStream, PNGOutFile);
				fclose(PNGOutFile);
			}
			else
			{
				fprintf(stderr, "ERROR: Unable to open \"%s\" for writing.\n", PNGOutName);
			}
			Clear(&TempArena);

			hha_align_point AlignPoint = {};
			SetAlignPoint(&AlignPoint, HHAAlign_Default, true, 1.0f,
						  V2(Glyph.AlignPercentage.x, Glyph.AlignPercentage.y));
			fprintf(HHTOut, "    Glyph[%u] = \"%s\", %u, {%u, %u};\n", i, PNGFileNameOnly,
					CodePoint, AlignPoint.PPercent[0], AlignPoint.PPercent[1]);
		}

		if (OutMemory)
		{
			free(OutMemory);
		}

		fprintf(HHTOut, "    HorizontalAdvance = \n        ");
		for(u32 Index = 0;
			Index < HorizontalAdvanceCount;
			++Index)
		{
			if(Index)
			{
				if((Index % 16) == 0)
				{
					fprintf(HHTOut, ",\n        ");
				}
				else
				{
					fprintf(HHTOut, ", ");
				}
			}

			fprintf(HHTOut, "%f", ScaleRatio * HorizontalAdvance[Index]);
		}
		fprintf(HHTOut, ";\n");
		fprintf(HHTOut, "};\n");

		// Cleanup
		if (KerningDataRef)
		{
			CFRelease(KerningDataRef);
		}

		if (HorizontalAdvance)
		{
			free(HorizontalAdvance);
		}

		if (ArrayIndexForGlyphIndex)
		{
			free(ArrayIndexForGlyphIndex);
		}

		if (GlyphIndices)
		{
			free(GlyphIndices);
		}

		if (GlyphAdvances)
		{
			free(GlyphAdvances);
		}

		OSXDeleteBitmapContext(BitmapContext);
		OSXDeleteFont(Font);
	}
	else
	{
		printf("Error creating font %s\n", FontName);
	}

    if (NameStem)
	{
		free(NameStem);
	}

    if (PNGOutName)
	{
		free(PNGOutName);
	}
}


s32 ExtractSingleCodePoint(const char* FontName, u32 PixelHeight, b32x ManualAntialias,
                           const char* CodePointInput)
{
	UniChar Character = 0;
	CGGlyph Glyph = 0;

	// If CodePointInput starts with a '#' character, then we'll
	// interpret is as a direct glyph index value. Otherwise,
	// we'll treat it as a code point and look up it's glyph index
	// below.
	if (strlen(CodePointInput) > 1 && CodePointInput[0] == '#')
	{
		// interpret as a direct glyph
		Glyph = atoi(&CodePointInput[1]);
	}
	else
	{
		Character = CodePointInput[0];
	}

	// TODO(jeff): Merge this code with LoadGlyphBitmap()!

	osx_font* Font = OSXCreateFont(FontName, PixelHeight);

	if (Font)
	{
		CGFloat FillColorComponents[4] = {1.0, 1.0, 1.0, 1.0};
		osx_bitmap_context* BitmapContext = OSXCreateBitmapContext(Font->MaxGlyphDim.Width,
																   Font->MaxGlyphDim.Height,
																   FillColorComponents);
		if (BitmapContext)
		{
			//CGContextSetAllowsFontSubpixelQuantization(BitmapContext->Context, true);
			//CGContextSetShouldSubpixelQuantizeFonts(BitmapContext->Context, true);

			CGContextSetShouldAntialias(BitmapContext->Context, true);
			CGContextSetShouldSmoothFonts(BitmapContext->Context, true);

			OSXEraseBitmapContext(BitmapContext, 0x00);

			// If the glyph wasn't directly passed in, convert the code point
			// to a glyph index.
			if (Glyph <= 0)
			{
				CTFontGetGlyphsForCharacters(Font->FontRef, &Character, &Glyph, 1);
			}

			CGRect OpticalRect = {};
			CTFontGetOpticalBoundsForGlyphs(Font->FontRef, &Glyph, &OpticalRect, 1, 0);

			CGPoint Origin = {-Font->MinGlyphOffset.Width,
			                  -Font->MinGlyphOffset.Height};

			CTFontDrawGlyphs(Font->FontRef, &Glyph, &Origin, 1, BitmapContext->Context);

			OSXBitmapContextPrintAsASCII(BitmapContext);

			printf("optical bounds: min:[%f %f] max:[%f %f]\n",
					OpticalRect.origin.x,
					OpticalRect.origin.y,
					OpticalRect.size.width,
					OpticalRect.size.height);

			printf("Glyph: %u\n", Glyph);
			printf("\n");


			OSXDeleteBitmapContext(BitmapContext);
		}
		else
		{
			printf("Could not create bitmap context\n");
			OSXDeleteFont(Font);
			return 1;
		}

		OSXDeleteFont(Font);
	}
	else
	{
		printf("Could not create font %s\n", FontName);
		return 1;
	}

	return 0;
}


internal void
Include(code_point_group *Group,
        u32 E0, u32 E1 = 0, u32 E2 = 0, u32 E3 = 0,
        u32 E4 = 0, u32 E5 = 0, u32 E6 = 0, u32 E7 = 0)
{
    u32 E[] = {E0, E1, E2, E3, E4, E5, E6, E7};
    for(u32 EIndex = 0;
        EIndex < ArrayCount(E);
        ++EIndex)
    {
        u32 CodePoint = E[EIndex];
        if(CodePoint)
        {
            if(Group->CodePoints)
            {
                Group->CodePoints[Group->GlyphCount] = CodePoint;
            }

            if(Group->OnePastMaxCodePoint <= CodePoint)
            {
                Group->OnePastMaxCodePoint = CodePoint + 1;
            }

            ++Group->GlyphCount;
        }
    }
}


internal void
IncludeRange(code_point_group *Group, UniChar MinCodePoint, UniChar MaxCodePoint)
{
    for(UniChar Character = MinCodePoint;
        Character <= MaxCodePoint;
        ++Character)
    {
        Include(Group, Character);
    }
}

#define CHAR_SET_CREATOR(name) void name(code_point_group *Group)
typedef CHAR_SET_CREATOR(char_set_creator_function);

CHAR_SET_CREATOR(DebugCharacters)
{
	Include(Group, 'a');
	Include(Group, 'g');
	Include(Group, '.');
	Include(Group, ',');
	Include(Group, 'A');
}

CHAR_SET_CREATOR(Test)
{
    Include(Group, ' ');
    IncludeRange(Group, '!', '~');

    // NOTE(casey): Ligatures
    IncludeRange(Group, 0xfb00, 0xfb05);

    // NOTE(casey): Kanji OWL!!!!!!!
 	Include(Group, 0x5c0f, 0x8033, 0x6728, 0x514e);
}

CHAR_SET_CREATOR(Essentials)
{
    Include(Group, ' ');
    IncludeRange(Group, '!', '~');
}

struct char_set_creator
{
    char *Name;
    char *Description;
    char_set_creator_function *Function;
};


global char_set_creator CharSets[] =
{
    {"Test", "Basic character set for testing font creation and display.", Test},
    {"Essentials", "Basic characters, no ligatures or kanji.", Essentials},
    {"Debug", "a,g,A characters for debugging.", DebugCharacters},
};


PLATFORM_ALLOCATE_MEMORY(CRTAllocateMemory)
{
    umm TotalSize = sizeof(platform_memory_block) + Size;
    platform_memory_block *Block = (platform_memory_block *)malloc(TotalSize);
    memset(Block, 0, TotalSize);

    Block->Size = Size;
    Block->Base = (u8 *)(Block + 1);
    Block->Used = 0;

    return(Block);
}

PLATFORM_DEALLOCATE_MEMORY(CRTDeallocateMemory)
{
    if(Block)
    {
        free(Block);
    }
}

platform_api Platform;


int
main(int argc, char *argv[])
{
    Platform.AllocateMemory = CRTAllocateMemory;
    Platform.DeallocateMemory = CRTDeallocateMemory;

    if (argc == 7)
    {
		// usage: <font_name> <size> <force_antialias> <char_set> <HHT_file> <PNG_dir>
        char *FontName = argv[1];
        u32 PixelHeight = atoi(argv[2]);
		b32x ManualAntialias = atoi(argv[3]);
        char *CharSetName = argv[4];
        char *HHTFileName = argv[5];
        char *PNGDirName = argv[6];

        char_set_creator *CharSetCreator = 0;

        for(u32 CharSetIndex = 0;
            CharSetIndex < ArrayCount(CharSets);
            ++CharSetIndex)
        {
            char_set_creator *Test = CharSets + CharSetIndex;
            if(strcmp(Test->Name, CharSetName) == 0)
            {
                CharSetCreator = Test;
                break;
            }
        }

        if (CharSetCreator)
        {
            FILE *HHTFile = fopen(HHTFileName, "wb");
            if(HHTFile)
            {
                fprintf(HHTFile,
                        "/* ========================================================================\n"
                        "   $File: %s $\n"
                        "   $Date: $\n"
                        "   $Revision: $\n"
                        "   $Creator: %s $\n"
                        "   $Notice: Extraction of font \"%s\" $\n"
                        "   ======================================================================== */\n",
                        HHTFileName, argv[0], FontName);

                code_point_group CounterGroup = {};
                CounterGroup.GlyphCount = 1;
                CharSetCreator->Function(&CounterGroup);

                code_point_group Group = {};
                Group.GlyphCount = 1;
                size_t GroupSize = CounterGroup.GlyphCount * sizeof(UniChar);
                Group.CodePoints = (UniChar*)malloc(GroupSize);
                memset(Group.CodePoints, 0, GroupSize);

                CharSetCreator->Function(&Group);

                fprintf(stdout, "Extracting font %s - %u glyphs, codepoint range %u... ",
                        FontName, Group.GlyphCount, Group.OnePastMaxCodePoint);

                ExtractFont(FontName, PixelHeight, ManualAntialias,
				            &Group, HHTFile, PNGDirName);

                fprintf(stdout, "done.\n");

                fclose(HHTFile);
            }
            else
            {
                fprintf(stderr, "ERROR: Unable to open HHT file \"%s\" for writing.\n", HHTFileName);
            }
        }
        else
        {
            fprintf(stderr, "ERROR: Unrecognized character set \"%s\".\n", CharSetName);
        }
    }
	else if (argc == 5)
	{
		///////////////////////////////////////////////////////////////
		// Simple test case to dump a single ASCII-fied codepoint/glyph
		//
		// usage: <font_name> <size> <force_antialias> <glyph>
		//
        char *FontName = argv[1];
        u32 PixelHeight = atoi(argv[2]);
		b32x ManualAntialias = atoi(argv[3]);
        char *CodePointInput = argv[4];

		ExtractSingleCodePoint(FontName, PixelHeight, ManualAntialias, CodePointInput);
	}
	else
	{
		// Not a valid set of command line options
		fprintf(stderr, "\n");
		fprintf(stderr, "%s usage:\n", argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "Most common usage:\n");
		fprintf(stderr, "------------------\n");
		fprintf(stderr, "Extract an entire set of code points for a font:\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "%s <font name> <pixel height> <manual antialiasing> <charset> <dest hht> <dest png dir>\n",
				argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "  example:\n");
		fprintf(stderr, "    %s HelveticaNeue 36 0 Test test_font.hht ./fonts\n", argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "The <font name> is NOT a filename. It is the name of the installed font in MacOS\n");
		fprintf(stderr, "as displayed in Font Book.\n");
		fprintf(stderr, "The <manual antialiasing> flag is 0 or 1.\n");
		fprintf(stderr, "  0 - use the default system antialiasing.\n");
		fprintf(stderr, "  1 - use our own manual supersampling.\n");
		fprintf(stderr, "Supported <charset> values:\n");
		for(u32 CharSetIndex = 0;
			CharSetIndex < ArrayCount(CharSets);
			++CharSetIndex)
		{
			fprintf(stderr, "  %s - %s\n", CharSets[CharSetIndex].Name, CharSets[CharSetIndex].Description);
		}
		fprintf(stderr, "\n");
		fprintf(stderr, "--- Alternate Usage ---\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "Single codepoint test usage:\n");
		fprintf(stderr, "----------------------------\n");
		fprintf(stderr, "Extract single code point or glyph for a font\n");
		fprintf(stderr, "and print ASCII version to terminal:\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "%s <font name> <pixel height> <manual antialiasing> <code point>\n",
				argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "If the code point is a character that is interpreted by the shell,\n");
		fprintf(stderr, "then enclose it in single or double quotes.\n");
		fprintf(stderr, "  example:\n");
		fprintf(stderr, "    %s HelveticaNeue 36 0 '&'\n", argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "You can also pass in a direct glyph index for the code point by\n");
		fprintf(stderr, "prefixing the glyph index with '#'. You will need to enclose the\n");
		fprintf(stderr, "glyph index expression in quotes.\n");
		fprintf(stderr, "  example:\n");
		fprintf(stderr, "    %s HelveticaNeue 36 0 '#13'\n", argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "The above example will also extract the ampersand character for\n");
		fprintf(stderr, "that particular font since the ampersand's glyph index is 13 in\n");
		fprintf(stderr, "HelveticaNeue.\n");

		return 1;
	}

	return 0;
}


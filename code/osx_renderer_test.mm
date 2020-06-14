///////////////////////////////////////////////////////////////////////
// osx_main.mm
//
// Jeff Buck
// Copyright 2014-2016. All Rights Reserved.
//

#include <Cocoa/Cocoa.h>

#include <mach/mach_time.h>
#include <dlfcn.h>

#include "handmade_types.h"
#include "handmade_intrinsics.h"
#include "handmade_math.h"
#include "handmade_shared.h"
#include "handmade_renderer.h"
#include "handmade_camera.h"

#include "osx_handmade_events.h"
#include "osx_handmade_cocoa.h"

void* OSXSimpleAllocateMemory(umm Size)
{
	void* P = (void*)mmap(0, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

	if (P == MAP_FAILED)
	{
		printf("OSXAllocateMemory: mmap error: %d  %s", errno, strerror(errno));
	}

	Assert(P);

	return P;
}

#include "osx_handmade_renderer.h"
#include "handmade_renderer.cpp"
#include "handmade_camera.cpp"

volatile global b32x GlobalRunning;

const r32 GlobalRenderWidth = 960;
const r32 GlobalRenderHeight = 540;

// Let the command line override
#ifndef HANDMADE_USE_VSYNC
#define HANDMADE_USE_VSYNC 1
#endif

#import "osx_handmade_cocoa.mm"

internal renderer_texture LoadBMP(renderer_texture_queue* TextureOpQueue,
                                  char* FileName, u16 TextureIndex);


///////////////////////////////////////////////////////////////////////
// Sample Renderer Scene

enum test_scene_element_type
{
	Element_Grass,
	Element_Tree,
	Element_Wall,
};

struct test_scene_element
{
	test_scene_element_type Type;
	cube_uv_layout CubeUVLayout;
};

#define TEST_SCENE_DIM_X 40
#define TEST_SCENE_DIM_Y 50

struct test_scene
{
	v3 MinP;
	test_scene_element Elements[TEST_SCENE_DIM_Y][TEST_SCENE_DIM_X];

	renderer_texture GrassTexture;
	renderer_texture WallTexture;
	renderer_texture TreeTexture;
	renderer_texture HeadTexture;
	renderer_texture CoverTexture;
};


internal b32x IsEmpty(test_scene* Scene, u32 X, u32 Y)
{
	b32x Result = (Scene->Elements[Y][X].Type == Element_Grass);
	return Result;
}


internal u32x CountOccupantsIn3x3(test_scene* Scene, u32 CenterX, u32 CenterY)
{
	u32 OccupantCount = 0;

	for (u32 Y = (CenterY - 1); Y <= (CenterY + 1); ++Y)
	{
		for (u32 X = (CenterX - 1); X <= (CenterX + 1); ++X)
		{
			if (!IsEmpty(Scene, X, Y))
			{
				++OccupantCount;
			}
		}
	}

	return OccupantCount;
}


internal void PlaceRandomInUnoccupied(test_scene* Scene, test_scene_element_type Element, u32 Count)
{
	u32 Placed = 0;

	while (Placed < Count)
	{
		u32 X = 1 + (rand() % (TEST_SCENE_DIM_X - 1));
		u32 Y = 1 + (rand() % (TEST_SCENE_DIM_Y - 1));

		if (CountOccupantsIn3x3(Scene, X, Y) == 0)
		{
			Scene->Elements[Y][X].Type = Element;
			++Placed;
		}
	}
}


internal b32x PlaceRectangularWall(test_scene* Scene, u32 MinX, u32 MinY, u32 MaxX, u32 MaxY)
{
	b32x Placed = true;

	cube_uv_layout WallUV = EncodeCubeUVLayout(0, 0,
	                                           2, 2, 2, 2,
											   0, 0);

	for (u32 Pass = 0; Placed && (Pass <= 1); ++Pass)
	{
		for (u32 X = MinX; X <= MaxX; ++X)
		{
			if (Pass == 0)
			{
				if (!(IsEmpty(Scene, X, MinY) && IsEmpty(Scene, X, MaxY)))
				{
					Placed = false;
					break;
				}
			}
			else
			{
				Scene->Elements[MinY][X].Type = Element_Wall;
				Scene->Elements[MaxY][X].Type = Element_Wall;

				if (X == MinX)
				{
					Scene->Elements[MinY][X].CubeUVLayout = EncodeCubeUVLayout(0, 0,
					                                                           1, 1, 3, 1,
																			   0, 0);
					Scene->Elements[MaxY][X].CubeUVLayout = EncodeCubeUVLayout(0, 0,
					                                                           1, 3, 1, 1,
																			   0, 0);
				}
				else if (X == MaxX)
				{
					Scene->Elements[MinY][X].CubeUVLayout = EncodeCubeUVLayout(0, 0,
					                                                           1, 3, 3, 3,
																			   0, 0);
					Scene->Elements[MaxY][X].CubeUVLayout = EncodeCubeUVLayout(0, 0,
					                                                           3, 1, 3, 3,
																			   0, 0);
				}
				else if (X == (MinX + 1))
				{
					Scene->Elements[MinY][X].CubeUVLayout = EncodeCubeUVLayout(0, 0,
					                                                           2, 3, 2, 2,
																			   0, 0);
					Scene->Elements[MaxY][X].CubeUVLayout = EncodeCubeUVLayout(0, 0,
					                                                           2, 2, 2, 1,
																			   0, 0);
				}
				else if (X == (MaxX - 1))
				{
					Scene->Elements[MinY][X].CubeUVLayout = EncodeCubeUVLayout(0, 0,
					                                                           2, 1, 2, 2,
																			   0, 0);
					Scene->Elements[MaxY][X].CubeUVLayout = EncodeCubeUVLayout(0, 0,
					                                                           2, 2, 2, 3,
																			   0, 0);
				}
				else
				{
					Scene->Elements[MinY][X].CubeUVLayout = WallUV;
					Scene->Elements[MaxY][X].CubeUVLayout = WallUV;
				}
			}
		}

		for (u32 Y = (MinY + 1); Y < MaxY; ++Y)
		{
			if (Pass == 0)
			{
				if (!(IsEmpty(Scene, MinX, Y) && IsEmpty(Scene, MaxX, Y)))
				{
					Placed = false;
					break;
				}
			}
			else
			{
				Scene->Elements[Y][MinX].Type = Element_Wall;
				Scene->Elements[Y][MaxX].Type = Element_Wall;

				if (Y == (MinY + 1))
				{
					Scene->Elements[Y][MinX].CubeUVLayout = EncodeCubeUVLayout(0, 0,
					                                                           1, 2, 2, 2,
																			   0, 0);
					Scene->Elements[Y][MaxX].CubeUVLayout = EncodeCubeUVLayout(0, 0,
					                                                           2, 2, 3, 2,
																			   0, 0);
				}
				else if (Y == (MaxY - 1))
				{
					Scene->Elements[Y][MinX].CubeUVLayout = EncodeCubeUVLayout(0, 0,
					                                                           3, 2, 2, 2,
																			   0, 0);
					Scene->Elements[Y][MaxX].CubeUVLayout = EncodeCubeUVLayout(0, 0,
					                                                           2, 2, 1, 2,
																			   0, 0);
				}
				else
				{
					Scene->Elements[Y][MinX].CubeUVLayout = WallUV;
					Scene->Elements[Y][MaxX].CubeUVLayout = WallUV;
				}
			}
		}
	}

	return Placed;
}


internal void InitTestScene(renderer_texture_queue* TextureOpQueue, test_scene* Scene)
{
	Scene->GrassTexture = LoadBMP(TextureOpQueue, "test_cube_grass.bmp", 1);
	Scene->WallTexture = LoadBMP(TextureOpQueue, "test_cube_wall.bmp", 2);
	Scene->TreeTexture = LoadBMP(TextureOpQueue, "test_sprite_tree.bmp", 3);
	Scene->HeadTexture = LoadBMP(TextureOpQueue, "test_sprite_head.bmp", 4);
	Scene->CoverTexture = LoadBMP(TextureOpQueue, "test_cover_grass.bmp", 5);

	Scene->MinP = V3(-0.5f*(f32)TEST_SCENE_DIM_X,
	                 -0.5f*(f32)TEST_SCENE_DIM_Y,
					 0.0f);

	for (u32 WallIndex = 0; WallIndex < 8; ++WallIndex)
	{
		u32 X = 1 + (rand() % (TEST_SCENE_DIM_X - 10));
		u32 Y = 1 + (rand() % (TEST_SCENE_DIM_Y - 10));

		u32 DimX = 2 + (rand() % 6);
		u32 DimY = 2 + (rand() % 6);

		PlaceRectangularWall(Scene, X, Y, X + DimX, Y + DimY);
	}

	u32 TotalSquareCount = TEST_SCENE_DIM_X*TEST_SCENE_DIM_Y;
	PlaceRandomInUnoccupied(Scene, Element_Tree, TotalSquareCount/15);
}


internal void PushSimpleScene(render_group* Group, test_scene* Scene)
{
	f32 WallRadius = 1.0f;

	srand(1234);

	for(s32 Y = 0; Y < TEST_SCENE_DIM_Y; ++Y)
	{
		for(s32 X = 0; X < TEST_SCENE_DIM_X; ++X)
		{
			test_scene_element Elem = Scene->Elements[Y][X];

			f32 Z = 0.4f*((f32)rand() / (f32)RAND_MAX);
			f32 R = 0.5f + 0.5f*((f32)rand() / (f32)RAND_MAX);

			if (Elem.Type == Element_Wall)
			{
				Z = 0.4f;
				R = 1.0f;
			}

			f32 ZRadius = 2.0f;
			v4 Color = V4(R, 1, 1, 1);
			v3 P = Scene->MinP + V3((f32)X, (f32)Y, Z);

			PushCube(Group, Scene->GrassTexture, P, V3(0.5f, 0.5f, ZRadius), Color);

			v3 GroundP = P + V3(0, 0, ZRadius);

			if (Elem.Type == Element_Tree)
			{
				PushUpright(Group, Scene->TreeTexture, GroundP, V2(2.0f, 2.5f));
			}
			else if (Elem.Type == Element_Wall)
			{
				PushCube(Group, Scene->WallTexture, GroundP + V3(0, 0, WallRadius),
				         V3(0.5f, 0.5f, WallRadius), Color, Elem.CubeUVLayout);
			}
			else
			{
				for (u32 CoverIndex = 0; CoverIndex < 30; ++CoverIndex)
				{
					v2 Disp = 0.8f*V2((f32)rand() / (f32)RAND_MAX,
									  (f32)rand() / (f32)RAND_MAX) - V2(0.4f, 0.4f);
					PushUpright(Group, Scene->CoverTexture, GroundP + V3(Disp, 0.0f),
					            V2(0.4f, 0.4f));
				}
			}
		}
	}

	PushUpright(Group, Scene->HeadTexture, V3(0, 2.0f, 3.0f), V2(4.0f, 4.0f));
}


///////////////////////////////////////////////////////////////////////
// Platform Layer Support

#pragma pack(push, 1)
struct bitmap_header
{
	u16 FileType;
	u32 FileSize;
	u16 Reserved1;
	u16 Reserved2;
	u32 BitmapOffset;
	u32 Size;
	s32 Width;
	s32 Height;
	u16 Planes;
	u16 BitsPerPixel;
	u32 Compression;
	u32 SizeOfBitmap;
	s32 HorzResolution;
	s32 VertResolution;
	u32 ColorsUsed;
	u32 ColorsImportant;

	u32 RedMask;
	u32 GreenMask;
	u32 BlueMask;
};
#pragma pack(pop)


struct loaded_bitmap
{
	void *Memory;
	s32 Width;
	s32 Height;
	s32 Pitch;
};


struct entire_file
{
	u32 ContentsSize;
	void *Contents;
};

entire_file ReadEntireFile(char* FileName);

internal renderer_texture LoadBMP(renderer_texture_queue* TextureOpQueue,
                                  char* FileName, u16 TextureIndex)
{
	loaded_bitmap Result = {};
	renderer_texture Texture = {};

	entire_file ReadResult = ReadEntireFile(FileName);

	if(ReadResult.ContentsSize != 0)
	{
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

		int32 RedShiftDown   = (int32)RedScan.Index;
		int32 GreenShiftDown = (int32)GreenScan.Index;
		int32 BlueShiftDown  = (int32)BlueScan.Index;
		int32 AlphaShiftDown = (int32)AlphaScan.Index;

		texture_op* Op = BeginTextureOp(TextureOpQueue, Result.Width, Result.Height);

		if (Op)
		{
			Texture = ReferToTexture(TextureIndex, Result.Width, Result.Height);
			Op->Texture = Texture;

			u32* Source = Pixels;
			u32* Dest = (u32*)Op->Data;

			for(int32 Y = 0; Y < Header->Height; ++Y)
			{
				for(int32 X = 0; X < Header->Width; ++X)
				{
					u32 C = *Source++;

					v4 Texel = {(r32)((C & RedMask)   >> RedShiftDown),
								(r32)((C & GreenMask) >> GreenShiftDown),
								(r32)((C & BlueMask)  >> BlueShiftDown),
								(r32)((C & AlphaMask) >> AlphaShiftDown)};

					Texel = SRGB255ToLinear1(Texel);
					Texel.rgb *= Texel.a;
					Texel = Linear1ToSRGB255(Texel);

					*Dest++ = (((u32)(Texel.a + 0.5f) << 24) |
							   ((u32)(Texel.r + 0.5f) << 16) |
							   ((u32)(Texel.g + 0.5f) << 8) |
							   ((u32)(Texel.b + 0.5f) << 0));
				}
			}

			CompleteTextureOp(TextureOpQueue, Op);
		}
		else
		{
			printf("ERROR: Out of texture transfer operations.\n");
		}
	}

	return Texture;
}


///////////////////////////////////////////////////////////////////////
// OS X Platform Support
//
struct frame_stats
{
	mach_timebase_info_data_t Timebase;
	f32 TimeScale;

	u64 LastCounter;

	f32 MinSPF;
	f32 MaxSPF;
	u32 DisplayCounter;
};

internal frame_stats InitFrameStats()
{
	frame_stats Stats = {};

	mach_timebase_info(&Stats.Timebase);
	assert(Stats.Timebase.denom != 0);

	Stats.TimeScale = (f32)Stats.Timebase.numer / (f32)Stats.Timebase.denom;

	Stats.MinSPF = F32Max;

	return Stats;
}


internal f32 UpdateFrameStats(frame_stats* Stats)
{
	f32 SecondsElapsed = 0.0f;

	u64 EndCounter = mach_absolute_time();

	if (Stats->LastCounter != 0)
	{
		u64 Elapsed = EndCounter - Stats->LastCounter;
		u64 Nanos = Elapsed * Stats->TimeScale;
		SecondsElapsed = (float)Nanos * 1.0E-9;

		if (Stats->MinSPF > SecondsElapsed)
		{
			Stats->MinSPF = SecondsElapsed;
		}

		if (Stats->MaxSPF < SecondsElapsed)
		{
			Stats->MaxSPF = SecondsElapsed;
		}

		if (Stats->DisplayCounter++ == 120)
		{
			printf("Min: %.02fms  Max: %.02fms\n",
			       1000.0f * Stats->MinSPF, 1000.0f * Stats->MaxSPF);

			Stats->MinSPF = F32Max;
			Stats->MaxSPF = 0.0f;
			Stats->DisplayCounter = 0;
		}
	}

	Stats->LastCounter = EndCounter;

	return SecondsElapsed;
}


entire_file ReadEntireFile(char* FileName)
{
	entire_file Result = {};

	FILE* In = fopen(FileName, "rb");

	if (In)
	{
		fseek(In, 0, SEEK_END);
		Result.ContentsSize = ftell(In);
		fseek(In, 0, SEEK_SET);

		Result.Contents = OSXSimpleAllocateMemory(Result.ContentsSize);
		fread(Result.Contents, Result.ContentsSize, 1, In);
		fclose(In);
	}
	else
	{
		printf("ERROR: Cannot open file %s.\n", FileName);
	}

	return Result;
}



int main(int argc, const char* argv[])
{
	#pragma unused(argc)
	#pragma unused(argv)

	@autoreleasepool
	{

	///////////////////////////////////////////////////////////////////
	// OS X platform code to set up application with default handlers
	//
    NSString* AppName = @"Handmade Renderer Test";

	OSXCocoaContext OSXAppContext = OSXInitCocoaContext(AppName, GlobalRenderWidth, GlobalRenderHeight);

	frame_stats Stats = InitFrameStats();

	OSXCreateSimpleMainMenu(AppName);

	//OSXInitOpenGLView(&OSXAppContext, GlobalRenderWidth, GlobalRenderHeight);

	///////////////////////////////////////////////////////////////////
	// Sample Renderer Scene Setup
	//
	platform_renderer_limits Limits = {};
	Limits.MaxQuadCountPerFrame = (1 << 18);
	Limits.MaxTextureCount = 256;
	Limits.TextureTransferBufferSize = (16 * 1024 * 1024);
	platform_renderer* Renderer = OSXInitDefaultRenderer(OSXAppContext.Window, &Limits);

	test_scene Scene = {};
	InitTestScene(&Renderer->TextureQueue, &Scene);

	camera Camera = GetStandardCamera();

	f32 tCameraShift = 0.0f;
	b32 CameraIsPanning = true;


	///////////////////////////////////////////////////////////////////
	// Run loop
	//
	GlobalRunning = true;

	v2u RenderDim = {960, 540};

	while (GlobalRunning)
	{
		OSXProcessMinimalPendingMessages();

		//CGRect WindowFrame = [window frame];
		CGRect ContentViewFrame = [[OSXAppContext.Window contentView] frame];

		float DrawWidth = ContentViewFrame.size.width;
		float DrawHeight = ContentViewFrame.size.height;

		rectangle2i DrawRegion = AspectRatioFit(RenderDim.Width, RenderDim.Height, DrawWidth, DrawHeight);

		///////////////////////////////////////////////////////////////
		// Sample Renderer Scene
		//
		if (tCameraShift > Tau32)
		{
			tCameraShift -= Tau32;
			CameraIsPanning = !CameraIsPanning;
		}

		if (CameraIsPanning)
		{
			Camera.Offset = 10.0f * V3(cosf(tCameraShift), -0.2f + sinf(tCameraShift), 0.0f);
		}
		else
		{
			Camera.Orbit = tCameraShift;
		}

		game_render_commands* Frame = Renderer->BeginFrame(Renderer, V2U(DrawWidth, DrawHeight), RenderDim, DrawRegion);

		Frame->Settings.RequestVSync = false;
		Frame->Settings.LightingDisabled = true;

		v4 BackgroundColor = V4(0.15f, 0.15f, 0.15f, 0.0f);
		render_group Group = BeginRenderGroup(0, Frame, Render_Default, BackgroundColor);

		ViewFromCamera(&Group, &Camera);

		PushSimpleScene(&Group, &Scene);

		EndRenderGroup(&Group);

		Renderer->EndFrame(Renderer, Frame);
		//
		// End of Sample Renderer Scene
		///////////////////////////////////////////////////////////////

		f32 SecondsElapsed = UpdateFrameStats(&Stats);

		tCameraShift += 0.1f * SecondsElapsed;
	}

	printf("Handmade Renderer Test finished running\n");

	} // @autoreleasepool
}


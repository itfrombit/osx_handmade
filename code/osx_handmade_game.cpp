


#if HANDMADE_INTERNAL
global_variable debug_table GlobalDebugTable_;
debug_table* GlobalDebugTable = &GlobalDebugTable_;
#endif


CGLPixelFormatAttribute GLPixelFormatAttribs[] =
	{
		kCGLPFAAccelerated,
		kCGLPFADoubleBuffer,
		(CGLPixelFormatAttribute)0
	};


void OSXToggleGlobalPause()
{
	GlobalPause = !GlobalPause;
}

b32 OSXIsGameRunning()
{
	return GlobalRunning;
}

void OSXStopGame()
{
	GlobalRunning = 0;
}



void OSXDebugInternalLogOpenGLErrors(const char* label)
{
	GLenum err = glGetError();
	const char* errString = "No error";

	while (err != GL_NO_ERROR)
	{
		switch(err)
		{
			case GL_INVALID_ENUM:
				errString = "Invalid Enum";
				break;

			case GL_INVALID_VALUE:
				errString = "Invalid Value";
				break;

			case GL_INVALID_OPERATION:
				errString = "Invalid Operation";
				break;

			case GL_INVALID_FRAMEBUFFER_OPERATION:
				errString = "Invalid Framebuffer Operation";
				break;

			case GL_OUT_OF_MEMORY:
				errString = "Out of Memory";
				break;

			case GL_STACK_UNDERFLOW:
				errString = "Stack Underflow";
				break;

			case GL_STACK_OVERFLOW:
				errString = "Stack Overflow";
				break;

			default:
				errString = "Unknown Error";
				break;
		}
		printf("glError on %s: %s\n", label, errString);

		err = glGetError();
	}
}


void OSXSetupSound(osx_game_data* GameData)
{
	//_SoundOutput.Frequency = 800.0;
	GameData->SoundOutput.SoundBuffer.SamplesPerSecond = 48000;
	GameData->SoundOutput.SoundBufferSize = GameData->SoundOutput.SoundBuffer.SamplesPerSecond * sizeof(int16) * 2;

	u32 MaxPossibleOverrun = 8 * 2 * sizeof(int16);

	GameData->SoundOutput.SoundBuffer.Samples = (int16*)mmap(0, GameData->SoundOutput.SoundBufferSize + MaxPossibleOverrun,
											PROT_READ|PROT_WRITE,
											MAP_PRIVATE | MAP_ANON,
											-1,
											0);
	if (GameData->SoundOutput.SoundBuffer.Samples == MAP_FAILED)
	{
		printf("Sound Buffer Samples mmap error: %d  %s", errno, strerror(errno));
	}
	memset(GameData->SoundOutput.SoundBuffer.Samples, 0, GameData->SoundOutput.SoundBufferSize);

	GameData->SoundOutput.CoreAudioBuffer = (int16*)mmap(0, GameData->SoundOutput.SoundBufferSize + MaxPossibleOverrun,
										PROT_READ|PROT_WRITE,
										MAP_PRIVATE | MAP_ANON,
										-1,
										0);
	if (GameData->SoundOutput.CoreAudioBuffer == MAP_FAILED)
	{
		printf("Core Audio Buffer mmap error: %d  %s", errno, strerror(errno));
	}
	memset(GameData->SoundOutput.CoreAudioBuffer, 0, GameData->SoundOutput.SoundBufferSize);

	GameData->SoundOutput.ReadCursor = GameData->SoundOutput.CoreAudioBuffer;
	GameData->SoundOutput.WriteCursor = GameData->SoundOutput.CoreAudioBuffer;

	OSXInitCoreAudio(&GameData->SoundOutput);
}


void OSXSetPixelFormat()
{
}


void OSXSetupOpenGL(osx_game_data* GameData)
{
	glGenTextures(1, &GameData->TextureId);

	OpenGLDefaultInternalTextureFormat = GL_RGBA8;
	OpenGLDefaultInternalTextureFormat = GL_SRGB8_ALPHA8;
	glEnable(GL_FRAMEBUFFER_SRGB);
}

///////////////////////////////////////////////////////////////////////
// Game Code
//
osx_thread_startup OSXGetThreadStartupForGL(CGLContextObj ShareContext)
{
	osx_thread_startup Result = {};

	CGLPixelFormatObj PixelFormat = NULL;
	GLint PixelFormatCount = 0;
	CGLChoosePixelFormat(GLPixelFormatAttribs, &PixelFormat, &PixelFormatCount);

	CGLContextObj NewContext;
	CGLCreateContext(PixelFormat, ShareContext, &NewContext);
	Result.OpenGLContext = NewContext;

	return Result;
}


void OSXSetupGameData(osx_game_data* GameData, CGLContextObj CGLContext)
{
	if (GameData->SetupComplete)
	{
		return;
	}


	///////////////////////////////////////////////////////////////////
	// Worker Threads
	//
	
	osx_thread_startup HighPriStartups[6] = {};
	//OSXMakeQueue(&GameData->HighPriorityQueue, 6);
	OSXMakeQueue(&GameData->HighPriorityQueue, ArrayCount(HighPriStartups), HighPriStartups);


	osx_thread_startup LowPriStartups[2] = {};
	LowPriStartups[0] = OSXGetThreadStartupForGL(CGLContext);
	LowPriStartups[1] = OSXGetThreadStartupForGL(CGLContext);
	OSXMakeQueue(&GameData->LowPriorityQueue, ArrayCount(LowPriStartups), LowPriStartups);


	///////////////////////////////////////////////////////////////////
	// Rendering Frame Rate
	//
	GameData->RenderAtHalfSpeed = 0;

	GameData->TargetFramesPerSecond = 60.0;
	GameData->TargetSecondsPerFrame = 1.0 / 60.0;

	if (GameData->RenderAtHalfSpeed)
	{
		GameData->TargetSecondsPerFrame += GameData->TargetSecondsPerFrame;
		GameData->TargetFramesPerSecond /= 2;
	}

	printf("TargetSecondsPerFrame: %f\n", GameData->TargetSecondsPerFrame);
	printf("Target frames per second = %d\n", GameData->TargetFramesPerSecond);

	// Get the conversion factor for doing profile timing with mach_absolute_time()
	mach_timebase_info_data_t timebase;
	mach_timebase_info(&timebase);
	GameData->MachTimebaseConversionFactor = (double)timebase.numer / (double)timebase.denom;


	///////////////////////////////////////////////////////////////////
	// Get the game shared library paths
	//
	OSXGetAppFilename(&GameData->OSXState);

	OSXBuildAppPathFilename(&GameData->OSXState, (char*)"libhandmade.dylib",
	                        sizeof(GameData->SourceGameCodeDLFullPath),
	                        GameData->SourceGameCodeDLFullPath);

	// NOTE(jeff): We don't have to create a temp file
	GameData->Game = OSXLoadGameCode(GameData->SourceGameCodeDLFullPath);


	///////////////////////////////////////////////////////////////////
	// Set up memory
	//

	GameData->FrameTempArenaSize = Megabytes(64);
	InitializeArena(&GameData->FrameTempArena,
					GameData->FrameTempArenaSize,
					OSXAllocateMemory(GameData->FrameTempArenaSize));

	GameData->PushBufferSize = Megabytes(64);
	GameData->PushBuffer = OSXAllocateMemory(GameData->PushBufferSize);


#if HANDMADE_INTERNAL
	char* RequestedAddress = (char*)Gigabytes(8);
	uint32 AllocationFlags = MAP_PRIVATE|MAP_ANON|MAP_FIXED;
#else
	char* RequestedAddress = (char*)0;
	uint32 AllocationFlags = MAP_PRIVATE|MAP_ANON;
#endif

	GameData->GameMemory.PermanentStorageSize = Megabytes(256); //Megabytes(256);
	GameData->GameMemory.TransientStorageSize = Gigabytes(1); //Gigabytes(1);
	GameData->GameMemory.DebugStorageSize = Megabytes(256); //Megabytes(64);
#if HANDMADE_INTERNAL
	GameData->GameMemory.DebugTable = GlobalDebugTable;
#endif

	GameData->OSXState.TotalSize = GameData->GameMemory.PermanentStorageSize +
	                      GameData->GameMemory.TransientStorageSize +
	                      GameData->GameMemory.DebugStorageSize;


#ifndef HANDMADE_USE_VM_ALLOCATE
	// NOTE(jeff): I switched to mmap as the default, so unless the above
	// HANDMADE_USE_VM_ALLOCATE is defined in the build/make process,
	// we'll use the mmap version.

	GameData->OSXState.GameMemoryBlock = mmap(RequestedAddress, GameData->OSXState.TotalSize,
	                                 PROT_READ|PROT_WRITE,
	                                 AllocationFlags,
	                                 -1, 0);
	if (GameData->OSXState.GameMemoryBlock == MAP_FAILED)
	{
		printf("mmap error: %d  %s", errno, strerror(errno));
	}

#else
	kern_return_t result = vm_allocate((vm_map_t)mach_task_self(),
									   (vm_address_t*)&GameData->OSXState.GameMemoryBlock,
									   GameData->OSXState.TotalSize,
									   VM_FLAGS_ANYWHERE);
	if (result != KERN_SUCCESS)
	{
		// TODO(jeff): Diagnostic
		NSLog(@"Error allocating memory");
	}
#endif

	GameData->GameMemory.PermanentStorage = GameData->OSXState.GameMemoryBlock;
	GameData->GameMemory.TransientStorage = ((uint8*)GameData->GameMemory.PermanentStorage
								   + GameData->GameMemory.PermanentStorageSize);
	GameData->GameMemory.DebugStorage = (u8*)GameData->GameMemory.TransientStorage +
								GameData->GameMemory.TransientStorageSize;

	GameData->GameMemory.HighPriorityQueue = &GameData->HighPriorityQueue;
	GameData->GameMemory.LowPriorityQueue = &GameData->LowPriorityQueue;

	GameData->GameMemory.PlatformAPI.AddEntry = OSXAddEntry;
	GameData->GameMemory.PlatformAPI.CompleteAllWork = OSXCompleteAllWork;

	GameData->GameMemory.PlatformAPI.GetAllFilesOfTypeBegin = OSXGetAllFilesOfTypeBegin;
	GameData->GameMemory.PlatformAPI.GetAllFilesOfTypeEnd = OSXGetAllFilesOfTypeEnd;
	GameData->GameMemory.PlatformAPI.OpenNextFile = OSXOpenNextFile;
	GameData->GameMemory.PlatformAPI.ReadDataFromFile = OSXReadDataFromFile;
	GameData->GameMemory.PlatformAPI.FileError = OSXFileError;

	GameData->GameMemory.PlatformAPI.AllocateTexture = AllocateTexture;
	GameData->GameMemory.PlatformAPI.DeallocateTexture = DeallocateTexture;

	GameData->GameMemory.PlatformAPI.AllocateMemory = OSXAllocateMemory;
	GameData->GameMemory.PlatformAPI.DeallocateMemory = OSXDeallocateMemory;

#if HANDMADE_INTERNAL
	GameData->GameMemory.PlatformAPI.DEBUGFreeFileMemory = DEBUGPlatformFreeFileMemory;
	GameData->GameMemory.PlatformAPI.DEBUGReadEntireFile = DEBUGPlatformReadEntireFile;
	GameData->GameMemory.PlatformAPI.DEBUGWriteEntireFile = DEBUGPlatformWriteEntireFile;

	GameData->GameMemory.PlatformAPI.DEBUGExecuteSystemCommand = DEBUGExecuteSystemCommand;
	GameData->GameMemory.PlatformAPI.DEBUGGetProcessState = DEBUGGetProcessState;
#endif

	Platform = GameData->GameMemory.PlatformAPI;

	///////////////////////////////////////////////////////////////////
	// Set up replay buffers

	// TODO(jeff): The loop replay is broken. Disable for now...
#if 0

	for (int ReplayIndex = 0;
	     ReplayIndex < ArrayCount(GameData->OSXState.ReplayBuffers);
	     ++ReplayIndex)
	{
		osx_replay_buffer* ReplayBuffer = &GameData->OSXState.ReplayBuffers[ReplayIndex];

		OSXGetInputFileLocation(&GameData->OSXState, false, ReplayIndex,
		                        sizeof(ReplayBuffer->Filename), ReplayBuffer->Filename);

		ReplayBuffer->FileHandle = open(ReplayBuffer->Filename, O_RDWR | O_CREAT | O_TRUNC, 0644);

		if (ReplayBuffer->FileHandle != -1)
		{
			int Result = ftruncate(ReplayBuffer->FileHandle, GameData->OSXState.TotalSize);

			if (Result != 0)
			{
				printf("ftruncate error on ReplayBuffer[%d]: %d: %s\n",
				       ReplayIndex, errno, strerror(errno));
			}

			ReplayBuffer->MemoryBlock = mmap(0, GameData->OSXState.TotalSize,
			                                 PROT_READ|PROT_WRITE,
			                                 MAP_PRIVATE,
			                                 ReplayBuffer->FileHandle,
			                                 0);

			if (ReplayBuffer->MemoryBlock != MAP_FAILED)
			{
#if 0
				fstore_t fstore = {};
				fstore.fst_flags = F_ALLOCATECONTIG;
				fstore.fst_posmode = F_PEOFPOSMODE;
				fstore.fst_offset = 0;
				fstore.fst_length = GameData->OSXState.TotalSize;

				int Result = fcntl(ReplayBuffer->FileHandle, F_PREALLOCATE, &fstore);

				if (Result != -1)
				{
					Result = ftruncate(ReplayBuffer->FileHandle, GameData->OSXState.TotalSize);

					if (Result != 0)
					{
						printf("ftruncate error on ReplayBuffer[%d]: %d: %s\n",
						       ReplayIndex, errno, strerror(errno));
					}
				}
				else
				{
					printf("fcntl error on ReplayBuffer[%d]: %d: %s\n",
					       ReplayIndex, errno, strerror(errno));
				}

				//memset(ReplayBuffer->MemoryBlock, 0, GameData->OSXState.TotalSize);
				//memset(ReplayBuffer->MemoryBlock, 0, GameData->OSXState.TotalSize);
				//memcpy(ReplayBuffer->MemoryBlock, 0, GameData->OSXState.TotalSize);
#else
				// NOTE(jeff): Tried out Filip's lseek suggestion to see if
				// it is any faster than ftruncate. Seems about the same.
				off_t SeekOffset = lseek(ReplayBuffer->FileHandle, GameData->OSXState.TotalSize - 1, SEEK_SET);

				if (SeekOffset)
				{
					int BytesWritten = write(ReplayBuffer->FileHandle, "", 1);

					if (BytesWritten != 1)
					{
						printf("Error writing to lseek offset of ReplayBuffer[%d]: %d: %s\n",
							   ReplayIndex, errno, strerror(errno));
					}
				}
#endif
			}
			else
			{
				printf("mmap error on ReplayBuffer[%d]: %d  %s",
				       ReplayIndex, errno, strerror(errno));
			}
		}
		else
		{
			printf("Error creating ReplayBuffer[%d] file %s: %d : %s\n",
					ReplayIndex, ReplayBuffer->Filename, errno, strerror(errno));
		}
	}

#endif

	///////////////////////////////////////////////////////////////////
	// Set up input
	//
	GameData->NewInput = &GameData->Input[0];
	GameData->OldInput = &GameData->Input[1];

	OSXSetupGamepad(GameData);


	///////////////////////////////////////////////////////////////////
	// Set up sound buffers and CoreAudio
	//
	OSXSetupSound(GameData);


	printf("------------------------------ Game setup complete\n");

	GameData->SetupComplete = 1;
}


void OSXSetupGameRenderBuffer(osx_game_data* GameData, float Width, float Height, int BytesPerPixel)
{
	GameData->RenderBuffer.Width = Width;
	GameData->RenderBuffer.Height = Height;

	GameData->RenderBuffer.Pitch = Align16(GameData->RenderBuffer.Width * BytesPerPixel);
	int BitmapMemorySize = (GameData->RenderBuffer.Pitch * GameData->RenderBuffer.Height);
	GameData->RenderBuffer.Memory = mmap(0,
								BitmapMemorySize,
	                            PROT_READ | PROT_WRITE,
	                            MAP_PRIVATE | MAP_ANON,
	                            -1,
	                            0);

	if (GameData->RenderBuffer.Memory == MAP_FAILED)
	{
		printf("Render Buffer Memory mmap error: %d  %s", errno, strerror(errno));
	}
}


void OSXKeyProcessing(bool32 IsDown, u32 Key,
					  int CommandKeyFlag, int ControlKeyFlag, int AlternateKeyFlag,
					  game_input* Input, osx_game_data* GameData)
{
	game_controller_input* Controller = GetController(Input, 0);

	switch (Key)
	{
		case 'w':
			OSXProcessKeyboardMessage(&Controller->MoveUp, IsDown);
			break;

		case 'a':
			OSXProcessKeyboardMessage(&Controller->MoveLeft, IsDown);
			break;

		case 's':
			OSXProcessKeyboardMessage(&Controller->MoveDown, IsDown);
			break;

		case 'd':
			OSXProcessKeyboardMessage(&Controller->MoveRight, IsDown);
			break;

		case 'q':
			if (IsDown && CommandKeyFlag)
			{
				OSXStopGame();
			}
			else
			{
				OSXProcessKeyboardMessage(&Controller->LeftShoulder, IsDown);
			}
			break;

		case 'e':
			OSXProcessKeyboardMessage(&Controller->RightShoulder, IsDown);
			break;

		case ' ':
			OSXProcessKeyboardMessage(&Controller->Start, IsDown);
			break;

		case 27:
			OSXProcessKeyboardMessage(&Controller->Back, IsDown);
			break;

		case 0xF700: //NSUpArrowFunctionKey
			OSXProcessKeyboardMessage(&Controller->ActionUp, IsDown);
			break;

		case 0xF702: //NSLeftArrowFunctionKey
			OSXProcessKeyboardMessage(&Controller->ActionLeft, IsDown);
			break;

		case 0xF701: //NSDownArrowFunctionKey
			OSXProcessKeyboardMessage(&Controller->ActionDown, IsDown);
			break;

		case 0xF703: //NSRightArrowFunctionKey
			OSXProcessKeyboardMessage(&Controller->ActionRight, IsDown);
			break;

#if HANDMADE_INTERNAL
		case 'p':
			if (IsDown)
			{
				OSXToggleGlobalPause();
			}
			break;

		case 'l':
#if 0
			if (IsDown)
			{
				if (CommandKeyFlag)
				{
					OSXBeginInputPlayback(&GameData->OSXState, 1);
				}
				else
				{
					if (GameData->OSXState.InputPlayingIndex == 0)
					{
						if (GameData->OSXState.InputRecordingIndex == 0)
						{
							OSXBeginRecordingInput(&GameData->OSXState, 1);
						}
						else
						{
							OSXEndRecordingInput(&GameData->OSXState);
							OSXBeginInputPlayback(&GameData->OSXState, 1);
						}
					}
					else
					{
						OSXEndInputPlayback(&GameData->OSXState);
					}
				}
			}
#endif
			break;
#endif
		default:
			return;
			break;
	}
}


void OSXDisplayBufferInWindow(platform_work_queue* RenderQueue,
							  game_offscreen_buffer* RenderBuffer,
							  game_render_commands* Commands,
						      s32 WindowWidth,
						      s32 WindowHeight,
						      memory_arena* TempArena,
						      GLuint TextureId)
{
	temporary_memory TempMem = BeginTemporaryMemory(TempArena);

	game_render_prep Prep = PrepForRender(Commands, TempArena);

	if (GlobalRenderingType == OSXRenderType_RenderOpenGL_DisplayOpenGL)
	{
		BEGIN_BLOCK("OpenGLRenderCommands");
		OpenGLRenderCommands(Commands, &Prep, WindowWidth, WindowHeight);
		END_BLOCK();
	}
	else
	{
		Assert(GlobalRenderingType == OSXRenderType_RenderSoftware_DisplayOpenGL);

		loaded_bitmap OutputTarget;
		OutputTarget.Memory = RenderBuffer->Memory;
		OutputTarget.Width = RenderBuffer->Width;
		OutputTarget.Height = RenderBuffer->Height;
		OutputTarget.Pitch = RenderBuffer->Pitch;

		//BEGIN_BLOCK("SoftwareRenderCommands");
		SoftwareRenderCommands(RenderQueue, Commands, &Prep, &OutputTarget);
		//END_BLOCK();

		// We always display via hardware

		OpenGLDisplayBitmap(RenderBuffer->Width, RenderBuffer->Height,
							RenderBuffer->Memory, RenderBuffer->Pitch,
							WindowWidth, WindowHeight,
							TextureId);
		//SwapBuffers();
	}

	EndTemporaryMemory(TempMem);
}



void OSXProcessFrameAndRunGameLogic(osx_game_data* GameData, CGRect WindowFrame,
									b32 MouseInWindowFlag, CGPoint MouseLocation,
									int MouseButtonMask)
{
	{DEBUG_DATA_BLOCK("Platform/Controls");
		DEBUG_B32(GlobalPause);
		DEBUG_B32(GlobalRenderingType);
		DEBUG_B32(GlobalShowSortGroups);
	}

	GameData->NewInput->dtForFrame = GameData->TargetSecondsPerFrame;

	//
	//
	//

	BEGIN_BLOCK("Input Processing");

	game_controller_input* OldKeyboardController = GetController(GameData->OldInput, 0);
	game_controller_input* NewKeyboardController = GetController(GameData->NewInput, 0);


	// TODO(jeff): Fix this for multiple controllers
	game_controller_input* NewController = &GameData->NewInput->Controllers[0];

	NewController->IsConnected = true;
	NewController->StickAverageX = GameData->HIDX;
	NewController->StickAverageY = GameData->HIDY;

	NewController->ActionDown.EndedDown = GameData->NewInput->Controllers[0].ActionDown.EndedDown;
	NewController->ActionUp.EndedDown = GameData->NewInput->Controllers[0].ActionUp.EndedDown;
	NewController->ActionLeft.EndedDown = GameData->NewInput->Controllers[0].ActionLeft.EndedDown;
	NewController->ActionRight.EndedDown = GameData->NewInput->Controllers[0].ActionRight.EndedDown;

	NewController->MoveUp.EndedDown = GameData->NewInput->Controllers[0].MoveUp.EndedDown;
	NewController->MoveDown.EndedDown = GameData->NewInput->Controllers[0].MoveDown.EndedDown;
	NewController->MoveLeft.EndedDown = GameData->NewInput->Controllers[0].MoveLeft.EndedDown;
	NewController->MoveRight.EndedDown = GameData->NewInput->Controllers[0].MoveRight.EndedDown;


	GameData->NewInput->dtForFrame = GameData->TargetSecondsPerFrame;

	if (MouseInWindowFlag)
	{
		//GameData->NewInput->MouseX = (-0.5f * (r32)GameData->RenderBuffer.Width + 0.5f) + (r32)PointInView.x;
		//GameData->NewInput->MouseY = (-0.5f * (r32)GameData->RenderBuffer.Height + 0.5f) + (r32)PointInView.y;
		//GameData->NewInput->MouseY = (r32)((GameData->RenderBuffer.Height - 1) - PointInView.y);

		GameData->NewInput->MouseX = (r32)MouseLocation.x;
		GameData->NewInput->MouseY = (r32)MouseLocation.y;

		GameData->NewInput->MouseZ = 0; // TODO(casey): Support mousewheel?

		for (u32 ButtonIndex = 0;
				ButtonIndex < PlatformMouseButton_Count;
				++ButtonIndex)
		{
			u32 IsDown = 0;

			if (ButtonIndex > 0)
			{
				IsDown = (MouseButtonMask >> ButtonIndex) & 0x0001;
			}
			else
			{
				IsDown = MouseButtonMask & 0x0001;
			}

			// NOTE(jeff): On OS X, Mouse Button 1 is Right, 2 is Middle
			u32 MouseButton = ButtonIndex;
			if (ButtonIndex == 1) MouseButton = PlatformMouseButton_Right;
			else if (ButtonIndex == 2) MouseButton = PlatformMouseButton_Middle;

			GameData->NewInput->MouseButtons[MouseButton] = GameData->OldInput->MouseButtons[MouseButton];
			GameData->NewInput->MouseButtons[MouseButton].HalfTransitionCount = 0;

			OSXProcessKeyboardMessage(&GameData->NewInput->MouseButtons[MouseButton], IsDown);
		}
	}
	else
	{
		GameData->NewInput->MouseX = GameData->OldInput->MouseX;
		GameData->NewInput->MouseY = GameData->OldInput->MouseY;
		GameData->NewInput->MouseZ = GameData->OldInput->MouseZ;
	}

	//int ModifierFlags = [[NSApp currentEvent] modifierFlags];
	//GameData->NewInput->ShiftDown = (ModifierFlags & NSShiftKeyMask);
	//GameData->NewInput->AltDown = (ModifierFlags & NSAlternateKeyMask);
	//GameData->NewInput->ControlDown = (ModifierFlags & NSControlKeyMask);

	CGEventFlags ModifierFlags = CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState);
	GameData->NewInput->ShiftDown = (ModifierFlags & kCGEventFlagMaskShift);
	GameData->NewInput->AltDown = (ModifierFlags & kCGEventFlagMaskAlternate);
	GameData->NewInput->ControlDown = (ModifierFlags & kCGEventFlagMaskControl);

#if 0
	// NOTE(jeff): Support for multiple controllers here...

	#define HID_MAX_COUNT 5

	uint32 MaxControllerCount = HID_MAX_COUNT;
	if (MaxControllerCount > (ArrayCount(GameData->NewInput->Controllers) - 1))
	{
		MaxControllerCount = (ArrayCount(GameData->NewInput->Controllers) - 1);
	}

	for (uint32 ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex)
	{
		// NOTE(jeff): index 0 is the keyboard
		uint32 OurControllerIndex = ControllerIndex + 1;
		game_controller_input* OldController = GetController(GameData->OldInput, OurControllerIndex);
		game_controller_input* NewController = GetController(GameData->NewInput, OurControllerIndex);
	}
#endif

	END_BLOCK();

	//
	//
	//

	BEGIN_BLOCK("Game Update");

	game_render_commands RenderCommands = RenderCommandStruct(
											GameData->PushBufferSize,
											GameData->PushBuffer,
											GameData->RenderBuffer.Width,
											GameData->RenderBuffer.Height);

	if (!GlobalPause)
	{
		if (GameData->OSXState.InputRecordingIndex)
		{
			OSXRecordInput(&GameData->OSXState, GameData->NewInput);
		}

		if (GameData->OSXState.InputPlayingIndex)
		{
			game_input Temp = *GameData->NewInput;

			OSXPlaybackInput(&GameData->OSXState, GameData->NewInput);

			for (u32 MouseButtonIndex = 0;
					MouseButtonIndex < PlatformMouseButton_Count;
					++MouseButtonIndex)
			{
				GameData->NewInput->MouseButtons[MouseButtonIndex] = Temp.MouseButtons[MouseButtonIndex];
			}
			GameData->NewInput->MouseX = Temp.MouseX;
			GameData->NewInput->MouseY = Temp.MouseY;
			GameData->NewInput->MouseZ = Temp.MouseZ;
		}

		if (GameData->Game.UpdateAndRender)
		{
			GameData->Game.UpdateAndRender(&GameData->GameMemory, GameData->NewInput, &RenderCommands);

			if (GameData->NewInput->QuitRequested)
			{
				GlobalRunning = false;
			}
			//HandleDebugCycleCounters(&GameData->GameMemory);
		}
	}

	END_BLOCK();

	//
	//
	//

	BEGIN_BLOCK("Audio Update");

	if (!GlobalPause)
	{
		// TODO(jeff): Move this into the sound render code
		//GameData->SoundOutput.Frequency = 440.0 + (15 * GameData->hidY);

		if (GameData->Game.GetSoundSamples)
		{
			// Sample Count is SamplesPerSecond / GameRefreshRate
			//GameData->SoundOutput.SoundBuffer.SampleCount = 1600; // (48000samples/sec) / (30fps)
			// ^^^ calculate this. We might be running at 30 or 60 fps
			GameData->SoundOutput.SoundBuffer.SampleCount = GameData->SoundOutput.SoundBuffer.SamplesPerSecond / GameData->TargetFramesPerSecond;

			GameData->Game.GetSoundSamples(&GameData->GameMemory, &GameData->SoundOutput.SoundBuffer);

			int16* CurrentSample = GameData->SoundOutput.SoundBuffer.Samples;
			for (int i = 0; i < GameData->SoundOutput.SoundBuffer.SampleCount; ++i)
			{
				*GameData->SoundOutput.WriteCursor++ = *CurrentSample++;
				*GameData->SoundOutput.WriteCursor++ = *CurrentSample++;

				if ((char*)GameData->SoundOutput.WriteCursor >= ((char*)GameData->SoundOutput.CoreAudioBuffer + GameData->SoundOutput.SoundBufferSize))
				{
					//printf("Write cursor wrapped!\n");
					GameData->SoundOutput.WriteCursor  = GameData->SoundOutput.CoreAudioBuffer;
				}
			}

			// Prime the pump to get the write cursor out in front of the read cursor...
			static bool firstTime = true;

			if (firstTime)
			{
				firstTime = false;

				GameData->Game.GetSoundSamples(&GameData->GameMemory, &GameData->SoundOutput.SoundBuffer);

				int16* CurrentSample = GameData->SoundOutput.SoundBuffer.Samples;
				for (int i = 0; i < GameData->SoundOutput.SoundBuffer.SampleCount; ++i)
				{
					*GameData->SoundOutput.WriteCursor++ = *CurrentSample++;
					*GameData->SoundOutput.WriteCursor++ = *CurrentSample++;

					if ((char*)GameData->SoundOutput.WriteCursor >= ((char*)GameData->SoundOutput.CoreAudioBuffer + GameData->SoundOutput.SoundBufferSize))
					{
						GameData->SoundOutput.WriteCursor  = GameData->SoundOutput.CoreAudioBuffer;
					}
				}
			}
		}
	}

	END_BLOCK();

	//
	//
	//

#if HANDMADE_INTERNAL
	BEGIN_BLOCK("Debug Collation");

	b32 ExecutableNeedsToBeReloaded = false;

	time_t NewDLWriteTime = OSXGetLastWriteTime(GameData->SourceGameCodeDLFullPath);
	if (NewDLWriteTime != GameData->Game.DLLastWriteTime)
	{
		ExecutableNeedsToBeReloaded = true;
	}

	GameData->GameMemory.ExecutableReloaded = false;

	if (ExecutableNeedsToBeReloaded)
	{
		OSXCompleteAllWork(&GameData->HighPriorityQueue);
		OSXCompleteAllWork(&GameData->LowPriorityQueue);
		DEBUGSetEventRecording(false);
	}

	if (GameData->Game.DEBUGFrameEnd)
	{
		GameData->Game.DEBUGFrameEnd(&GameData->GameMemory, GameData->NewInput, &RenderCommands);
	}

	if (ExecutableNeedsToBeReloaded)
	{
		OSXUnloadGameCode(&GameData->Game);

		for (u32 LoadTryIndex = 0;
			 !GameData->Game.IsValid && (LoadTryIndex < 100);
			 ++LoadTryIndex)
		{
			GameData->Game = OSXLoadGameCode(GameData->SourceGameCodeDLFullPath);
			usleep(100);
		}

		GameData->GameMemory.ExecutableReloaded = true;
		DEBUGSetEventRecording(GameData->Game.IsValid);
	}


	END_BLOCK();
#endif


	game_input* Temp = GameData->NewInput;
	GameData->NewInput = GameData->OldInput;
	GameData->OldInput = Temp;

	NewKeyboardController = GetController(GameData->NewInput, 0);
	OldKeyboardController = GetController(GameData->OldInput, 0);
	memset(NewKeyboardController, 0, sizeof(game_controller_input));
	NewKeyboardController->IsConnected = true;

	for (int ButtonIndex = 0;
			ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
			++ButtonIndex)
	{
		NewKeyboardController->Buttons[ButtonIndex].EndedDown = 
			OldKeyboardController->Buttons[ButtonIndex].EndedDown;
	}




	///////////////////////////////////////////////////////////////////
	// Draw the latest frame to the screen

	BEGIN_BLOCK("Frame Display");

	OSXDisplayBufferInWindow(&GameData->HighPriorityQueue,
							 &GameData->RenderBuffer,
							 &RenderCommands,
							 GameData->RenderBuffer.Width,
							 GameData->RenderBuffer.Height,
							 &GameData->FrameTempArena,
							 GameData->TextureId);

	END_BLOCK();
}



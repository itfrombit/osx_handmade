

#if HANDMADE_INTERNAL
global debug_table GlobalDebugTable_;
debug_table* GlobalDebugTable = &GlobalDebugTable_;
#endif


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


void OSXSetupSound(osx_game_data* GameData)
{
	//_SoundOutput.Frequency = 800.0;
	GameData->SoundOutput.SoundBuffer.SamplesPerSecond = 48000;
	GameData->SoundOutput.SoundBufferSize = GameData->SoundOutput.SoundBuffer.SamplesPerSecond * sizeof(int16) * 2;

	u32 MaxPossibleOverrun = 8 * 2 * sizeof(int16);

	GameData->SoundOutput.SoundBuffer.Samples = (int16*)mmap(0,
	                                        GameData->SoundOutput.SoundBufferSize + MaxPossibleOverrun,
											PROT_READ|PROT_WRITE,
											MAP_PRIVATE | MAP_ANON,
											-1,
											0);
	if (GameData->SoundOutput.SoundBuffer.Samples == MAP_FAILED)
	{
		printf("Sound Buffer Samples mmap error: %d  %s", errno, strerror(errno));
	}
	memset(GameData->SoundOutput.SoundBuffer.Samples, 0, GameData->SoundOutput.SoundBufferSize);

	GameData->SoundOutput.CoreAudioBuffer = (int16*)mmap(0,
	                                    GameData->SoundOutput.SoundBufferSize + MaxPossibleOverrun,
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


PLATFORM_ERROR_MESSAGE(OSXErrorMessage)
{
	// Type: PlatformError_Fatal | PlatformError_Warning
	// Message
	char* Caption = "Handmade Hero Warning";
	CFOptionFlags Flags = kCFUserNotificationCautionAlertLevel;

	if (Type == PlatformError_Fatal)
	{
		Caption = "Handmade Hero Fatal Error";
		Flags = kCFUserNotificationStopAlertLevel;
	}

	CFStringRef CaptionRef = CFStringCreateWithCString(NULL, Caption, strlen(Caption));
	CFStringRef MessageRef = CFStringCreateWithCString(NULL, Message, strlen(Message));

	CFOptionFlags ResponseFlags;

	CFUserNotificationDisplayAlert(0,      // timeout: 0 = never
	                               Flags,
								   NULL,   // iconURL: NULL = derive from above flag
								   NULL,   // soundURL
								   NULL,   // localizationURL
								   CaptionRef,
								   MessageRef,
								   NULL,   // defaultButtonTitle
								   NULL,   // alterateButtonTitle
								   NULL,   // otherButtonTitle
								   &ResponseFlags);

	CFRelease(CaptionRef);
	CFRelease(MessageRef);

	if (Type == PlatformError_Fatal)
	{
		exit(1);
	}
}


///////////////////////////////////////////////////////////////////////
// Game Code
//

memory_arena FrameTempArena;
game_memory GameMemory;


void OSXSetupGameData(NSWindow* Window, osx_game_data* GameData)
{
	if (GameData->SetupComplete)
	{
		return;
	}

	osx_state* OSXState = &GlobalOSXState;
	OSXState->MemorySentinel.Prev = &OSXState->MemorySentinel;
	OSXState->MemorySentinel.Next = &OSXState->MemorySentinel;

	GameData->Limits.MaxQuadCountPerFrame = (1 << 20);
	GameData->Limits.MaxTextureCount = HANDMADE_NORMAL_TEXTURE_COUNT;
	GameData->Limits.MaxSpecialTextureCount = HANDMADE_SPECIAL_TEXTURE_COUNT;
	GameData->Limits.TextureTransferBufferSize = HANDMADE_TEXTURE_TRANSFER_BUFFER_SIZE;

	OSXGetAppFilename(OSXState);

	OSXBuildAppPathFilename(OSXState, (char*)"lock.tmp",
							sizeof(GameData->CodeLockFullPath),
							GameData->CodeLockFullPath);

#if OSX_HANDMADE_USE_METAL
	OSXBuildAppPathFilename(OSXState, (char*)"libhandmade_metal.dylib",
							sizeof(GameData->RendererCodeDLFullPath),
							GameData->RendererCodeDLFullPath);
#else
	OSXBuildAppPathFilename(OSXState, (char*)"libhandmade_opengl.dylib",
							sizeof(GameData->RendererCodeDLFullPath),
							GameData->RendererCodeDLFullPath);
#endif

	GameData->RendererFunctions = {};
	GameData->RendererCode = {};
	GameData->RendererCode.TransientDLName = "libhandmade_renderer_temp.dylib";
	GameData->RendererCode.DLFullPath = GameData->RendererCodeDLFullPath;
	GameData->RendererCode.LockFullPath = GameData->CodeLockFullPath;
	GameData->RendererCode.FunctionCount = ArrayCount(OSXRendererFunctionTableNames);
	GameData->RendererCode.FunctionNames = OSXRendererFunctionTableNames;
	GameData->RendererCode.Functions = (void**)&GameData->RendererFunctions;

	OSXLoadCode(OSXState, &GameData->RendererCode);

	if (!GameData->RendererCode.IsValid)
	{
		OSXErrorMessage(PlatformError_Fatal, "Unable to load renderer.");
	}

	GameData->Renderer = GameData->RendererFunctions.LoadRenderer(Window, &GameData->Limits);

	///////////////////////////////////////////////////////////////////
	// Worker Threads
	//
	ZeroArray(ArrayCount(GameData->HighPriorityStartups), GameData->HighPriorityStartups);
	OSXMakeQueue(&GameData->HighPriorityQueue,
	             ArrayCount(GameData->HighPriorityStartups),
	             GameData->HighPriorityStartups);

	ZeroArray(ArrayCount(GameData->LowPriorityStartups), GameData->LowPriorityStartups);
	OSXMakeQueue(&GameData->LowPriorityQueue,
	             ArrayCount(GameData->LowPriorityStartups),
	             GameData->LowPriorityStartups);

	///////////////////////////////////////////////////////////////////
	// Rendering Frame Rate
	//
	GameData->RenderAtHalfSpeed = 0;
	GameData->MonitorRefreshHz = 60;
	GameData->GameUpdateHz = (f32)GameData->MonitorRefreshHz;
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
	OSXBuildAppPathFilename(OSXState, (char*)"libhandmade.dylib",
							sizeof(GameData->SourceGameCodeDLFullPath),
							GameData->SourceGameCodeDLFullPath);

	GameData->GameFunctions = {};
	GameData->GameCode = {};
	GameData->GameCode.TransientDLName = "libhandmade_game_temp.dylib";
	GameData->GameCode.DLFullPath = GameData->SourceGameCodeDLFullPath;
	GameData->GameCode.LockFullPath = GameData->CodeLockFullPath;
	GameData->GameCode.FunctionCount = ArrayCount(OSXGameFunctionTableNames);
	GameData->GameCode.FunctionNames = OSXGameFunctionTableNames;
	GameData->GameCode.Functions = (void**)&GameData->GameFunctions;
	OSXLoadCode(OSXState, &GameData->GameCode);

	DEBUGSetEventRecording(GameData->GameCode.IsValid);

	///////////////////////////////////////////////////////////////////
	// Set up memory
	//

#if HANDMADE_INTERNAL
	char* RequestedAddress = (char*)Gigabytes(8);
	uint32 AllocationFlags = MAP_PRIVATE|MAP_ANON|MAP_FIXED;
#else
	char* RequestedAddress = (char*)0;
	uint32 AllocationFlags = MAP_PRIVATE|MAP_ANON;
#endif


	GameMemory = {};

#if HANDMADE_INTERNAL
	GameMemory.DebugTable = GlobalDebugTable;
#endif

	GameMemory.HighPriorityQueue = &GameData->HighPriorityQueue;
	GameMemory.LowPriorityQueue = &GameData->LowPriorityQueue;

	GameMemory.PlatformAPI.AddEntry = OSXAddEntry;
	GameMemory.PlatformAPI.CompleteAllWork = OSXCompleteAllWork;

	GameMemory.PlatformAPI.GetAllFilesOfTypeBegin = OSXGetAllFilesOfTypeBegin;
	GameMemory.PlatformAPI.GetAllFilesOfTypeEnd = OSXGetAllFilesOfTypeEnd;
	GameMemory.PlatformAPI.OpenFile = OSXOpenFile;
	GameMemory.PlatformAPI.SetFileSize = OSXSetFileSize;
	GameMemory.PlatformAPI.GetFileByPath = OSXGetFileByPath;
	GameMemory.PlatformAPI.ReadDataFromFile = OSXReadDataFromFile;
	GameMemory.PlatformAPI.WriteDataToFile = OSXWriteDataToFile;
	GameMemory.PlatformAPI.AtomicReplaceFileContents = OSXAtomicReplaceFileContents;
	GameMemory.PlatformAPI.FileError = OSXFileError;
	GameMemory.PlatformAPI.CloseFile = OSXCloseFile;

	GameMemory.PlatformAPI.AllocateMemory = OSXAllocateMemory;
	GameMemory.PlatformAPI.DeallocateMemory = OSXDeallocateMemory;
	GameMemory.PlatformAPI.ErrorMessage = OSXErrorMessage;

#if HANDMADE_INTERNAL
	GameMemory.PlatformAPI.DEBUGExecuteSystemCommand = DEBUGExecuteSystemCommand;
	GameMemory.PlatformAPI.DEBUGGetProcessState = DEBUGGetProcessState;
	GameMemory.PlatformAPI.DEBUGGetMemoryStats = OSXGetMemoryStats;
#endif

	GameMemory.TextureQueue = &GameData->Renderer->TextureQueue;

	Platform = GameMemory.PlatformAPI;

	///////////////////////////////////////////////////////////////////
	// Set up replay buffers


	///////////////////////////////////////////////////////////////////
	// Set up input
	//
	GameData->NewInput = &GameData->Input[0];
	GameData->OldInput = &GameData->Input[1];

	OSXSetupGamepad(GameData);

	ZeroStruct(GameData->KeyboardState);
	ZeroStruct(GameData->OldKeyboardState);

	///////////////////////////////////////////////////////////////////
	// Set up sound buffers and CoreAudio
	//
	OSXSetupSound(GameData);

	GameData->RendererWasReloaded = false;
	GameData->ExpectedFramesPerUpdate = 1;
	GameData->TargetSecondsPerFrame = (f32)GameData->ExpectedFramesPerUpdate / (f32)GameData->GameUpdateHz;

	printf("------------------------------ Game setup complete\n");

	GameData->SetupComplete = 1;
}


void OSXKeyProcessing(bool32 IsKeyDown, u32 KeyCode, u32 Key,
					  int ShiftKeyFlag, int CommandKeyFlag, int ControlKeyFlag, int AlternateKeyFlag,
					  game_input* Input, osx_game_data* GameData)
{
	game_controller_input* Controller = GetController(Input, 0);

	b32 WasDown = GameData->OldKeyboardState[KeyCode];
	b32 IsDown = GameData->KeyboardState[KeyCode];

	//printf("KeyDown: %d  KeyCode: %d  Key: %c  WasDown: %d  IsDown: %d\n",
	//		IsKeyDown, KeyCode, Key, WasDown, IsDown);

	if (WasDown != IsDown)
	{
		if (KeyCode == kVK_ANSI_W)
		{
			OSXProcessKeyboardMessage(&Controller->MoveUp, IsDown);
		}
		else if (KeyCode == kVK_ANSI_A)
		{
			OSXProcessKeyboardMessage(&Controller->MoveLeft, IsDown);
		}
		else if (KeyCode == kVK_ANSI_S)
		{
			OSXProcessKeyboardMessage(&Controller->MoveDown, IsDown);
		}
		else if (KeyCode == kVK_ANSI_D)
		{
			OSXProcessKeyboardMessage(&Controller->MoveRight, IsDown);
		}
		else if (KeyCode == kVK_ANSI_Q)
		{
			if (IsDown && CommandKeyFlag)
			{
				OSXStopGame();
			}
			else
			{
				OSXProcessKeyboardMessage(&Controller->LeftShoulder, IsDown);
			}
		}
		else if (KeyCode == kVK_ANSI_E)
		{
			OSXProcessKeyboardMessage(&Controller->RightShoulder, IsDown);
		}
		else if (KeyCode == kVK_UpArrow)
		{
			OSXProcessKeyboardMessage(&Controller->ActionUp, IsDown);
		}
		else if (KeyCode == kVK_LeftArrow)
		{
			OSXProcessKeyboardMessage(&Controller->ActionLeft, IsDown);
		}
		else if (KeyCode == kVK_DownArrow)
		{
			OSXProcessKeyboardMessage(&Controller->ActionDown, IsDown);
		}
		else if (KeyCode == kVK_RightArrow)
		{
			OSXProcessKeyboardMessage(&Controller->ActionRight, IsDown);
		}
		else if (KeyCode == kVK_Escape)
		{
			OSXProcessKeyboardMessage(&Controller->Back, IsDown);
		}
		else if (KeyCode == kVK_Return)
		{
			OSXProcessKeyboardMessage(&Controller->Start, IsDown);
		}
		else if (KeyCode == kVK_Space || KeyCode == kVK_Shift)
		{
			b32 EitherDown = GameData->KeyboardState[kVK_Space] || ShiftKeyFlag;
			Controller->ClutchMax = (EitherDown ? 1.0f : 0.0f);
			printf("ClutchMax = %f\n", Controller->ClutchMax);
		}
#if HANDMADE_INTERNAL
		else if (KeyCode == kVK_ANSI_P)
		{
			if (IsDown)
			{
				OSXToggleGlobalPause();
			}
		}
		else if (KeyCode == kVK_ANSI_L)
		{
			if (IsDown)
			{
				osx_state* OSXState = &GlobalOSXState;

				if (CommandKeyFlag)
				{
					OSXBeginInputPlayback(OSXState, 1);
				}
				else
				{
					if (OSXState->InputPlayingIndex == 0)
					{
						if (OSXState->InputRecordingIndex == 0)
						{
							OSXBeginRecordingInput(OSXState, 1);
						}
						else
						{
							OSXEndRecordingInput(OSXState);
							OSXBeginInputPlayback(OSXState, 1);
						}
					}
					else
					{
						OSXEndInputPlayback(OSXState);
					}
				}
			}
		}
#endif

		if (IsDown)
		{
			if (KeyCode == kVK_F1)
			{
				Input->FKeyPressed[1] = true;
			}
			else if (KeyCode == kVK_F2)
			{
				Input->FKeyPressed[2] = true;
			}
			else if (KeyCode == kVK_F3)
			{
				Input->FKeyPressed[3] = true;
			}
			else if (KeyCode == kVK_F4)
			{
				Input->FKeyPressed[4] = true;
			}
			else if (KeyCode == kVK_F5)
			{
				Input->FKeyPressed[5] = true;
			}
			else if (KeyCode == kVK_F6)
			{
				Input->FKeyPressed[6] = true;
			}
			else if (KeyCode == kVK_F7)
			{
				Input->FKeyPressed[7] = true;
			}
			else if (KeyCode == kVK_F8)
			{
				Input->FKeyPressed[8] = true;
			}
			else if (KeyCode == kVK_F9)
			{
				Input->FKeyPressed[9] = true;
			}
			else if (KeyCode == kVK_F10)
			{
				Input->FKeyPressed[10] = true;
			}
			else if (KeyCode == kVK_F11)
			{
				Input->FKeyPressed[11] = true;
			}
			else if (KeyCode == kVK_F12)
			{
				Input->FKeyPressed[12] = true;
			}
		}
	}
}


void OSXInitializeGameInputForNewFrame(osx_game_data* GameData)
{
	game_controller_input* OldKeyboardController = GetController(GameData->OldInput, 0);
	game_controller_input* NewKeyboardController = GetController(GameData->NewInput, 0);
	*NewKeyboardController = {};
	NewKeyboardController->IsConnected = true;

	for (int ButtonIndex = 0;
	     ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
		 ++ButtonIndex)
	{
		NewKeyboardController->Buttons[ButtonIndex].EndedDown =
			OldKeyboardController->Buttons[ButtonIndex].EndedDown;
	}

	NewKeyboardController->ClutchMax = OldKeyboardController->ClutchMax;
}


void OSXUpdateMouseInput(b32 MouseInWindowFlag, CGPoint MouseLocation, int MouseButtonMask,
                         osx_game_data* GameData, rectangle2i DrawRegion)
{
	game_input* NewInput = GameData->NewInput;
	game_input* OldInput = GameData->OldInput;

	if (MouseInWindowFlag)
	{
		r32 MouseX = (r32)MouseLocation.x;
		r32 MouseY = (r32)MouseLocation.y;

		NewInput->ClipSpaceMouseP.x =
			ClampBinormalMapToRange((f32)DrawRegion.MinX, MouseX, (f32)DrawRegion.MaxX);

		NewInput->ClipSpaceMouseP.y =
			ClampBinormalMapToRange((f32)DrawRegion.MinY, MouseY, (f32)DrawRegion.MaxY);
		NewInput->ClipSpaceMouseP.z = 0; // TODO(jeff): Support mousewheel?

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

			NewInput->MouseButtons[MouseButton] = OldInput->MouseButtons[MouseButton];
			NewInput->MouseButtons[MouseButton].HalfTransitionCount = 0;

			OSXProcessKeyboardMessage(&NewInput->MouseButtons[MouseButton], IsDown);
		}
	}
	else
	{
		NewInput->ClipSpaceMouseP = OldInput->ClipSpaceMouseP;
	}

	CGEventFlags ModifierFlags = CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState);
	NewInput->ShiftDown = (ModifierFlags & kCGEventFlagMaskShift);
	NewInput->AltDown = (ModifierFlags & kCGEventFlagMaskAlternate);
	NewInput->ControlDown = (ModifierFlags & kCGEventFlagMaskControl);
}


void OSXUpdateGameControllerInput(osx_game_data* GameData)
{
#if 0
	// This is done in the HID callback in osx_handmade_hid.cpp
	game_controller_input* OldController = GetController(GameData->OldInput, 0);
	game_controller_input* NewController = GetController(GameData->NewInput, 0);

	NewController->StickAverageX = GameData->HIDX;
	NewController->StickAverageY = GameData->HIDY;

	NewController->ActionDown.EndedDown = NewController->ActionDown.EndedDown;
	NewController->ActionUp.EndedDown = NewController->ActionUp.EndedDown;
	NewController->ActionLeft.EndedDown = NewController->ActionLeft.EndedDown;
	NewController->ActionRight.EndedDown = NewController->ActionRight.EndedDown;

	NewController->MoveUp.EndedDown = NewController->MoveUp.EndedDown;
	NewController->MoveDown.EndedDown = NewController->MoveDown.EndedDown;
	NewController->MoveLeft.EndedDown = NewController->MoveLeft.EndedDown;
	NewController->MoveRight.EndedDown = NewController->MoveRight.EndedDown;

	NewController->ClutchMax = NewController
#endif

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

}


void OSXUpdateAudio(osx_game_data* GameData)
{
	// TODO(jeff): Move this into the sound render code
	//GameData->SoundOutput.Frequency = 440.0 + (15 * GameData->hidY);

	if (GameData->GameFunctions.GetSoundSamples)
	{
		// Sample Count is SamplesPerSecond / GameRefreshRate
		//GameData->SoundOutput.SoundBuffer.SampleCount = 1600; // (48000samples/sec) / (30fps)
		// ^^^ calculate this. We might be running at 30 or 60 fps
		GameData->SoundOutput.SoundBuffer.SampleCount = GameData->SoundOutput.SoundBuffer.SamplesPerSecond / GameData->TargetFramesPerSecond;

		GameData->GameFunctions.GetSoundSamples(&GameMemory, &GameData->SoundOutput.SoundBuffer);

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

			GameData->GameFunctions.GetSoundSamples(&GameMemory, &GameData->SoundOutput.SoundBuffer);

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


void OSXProcessFrameAndRunGameLogic(osx_game_data* GameData, CGRect WindowFrame, osx_mouse_data* MouseData)
{
	{DEBUG_DATA_BLOCK("Platform");
		DEBUG_VALUE(GameData->ExpectedFramesPerUpdate);
	}
	{DEBUG_DATA_BLOCK("Platform/Controls");
		DEBUG_B32(GlobalPause);
		DEBUG_B32(GlobalSoftwareRendering);
	}

	osx_state* OSXState = &GlobalOSXState;

	GameData->NewInput->dtForFrame = GameData->TargetSecondsPerFrame;


	BEGIN_BLOCK("Input Processing");

	GameData->RenderDim =
	{
		//192, 108
		//480, 270
		//960, 540
		//1280, 720
		//1279, 719
		1920, 1080
	};

	v2u Dimension = { (u32)WindowFrame.size.width, (u32)WindowFrame.size.height };

	rectangle2i DrawRegion = AspectRatioFit(GameData->RenderDim.Width, GameData->RenderDim.Height,
	                                        Dimension.Width, Dimension.Height);

	game_render_commands* Frame = 0;

	if (GameData->RendererCode.IsValid)
	{
		Frame = GameData->RendererFunctions.BeginFrame(GameData->Renderer, Dimension,
		                                               GameData->RenderDim, DrawRegion);
		Frame->Settings.RenderDim = GameData->RenderDim;
	}

	OSXUpdateMouseInput(MouseData->MouseInWindowFlag, MouseData->MouseLocation, MouseData->MouseButtonMask,
	                    GameData, DrawRegion);

	OSXUpdateGameControllerInput(GameData);

	END_BLOCK();

	//
	//
	//

	BEGIN_BLOCK("Game Update");

	if (Frame && !GlobalPause)
	{
		if (OSXState->InputRecordingIndex)
		{
			printf("...Recording input...\n");
			OSXRecordInput(OSXState, GameData->NewInput);
		}

		if (OSXState->InputPlayingIndex)
		{
			printf("...Playing back input...\n");
			game_input Temp = *GameData->NewInput;

			OSXPlaybackInput(OSXState, GameData->NewInput);

			for (u32 MouseButtonIndex = 0;
					MouseButtonIndex < PlatformMouseButton_Count;
					++MouseButtonIndex)
			{
				GameData->NewInput->MouseButtons[MouseButtonIndex] = Temp.MouseButtons[MouseButtonIndex];
			}

			GameData->NewInput->ClipSpaceMouseP = Temp.ClipSpaceMouseP;
		}

		if (GameData->GameFunctions.UpdateAndRender)
		{
			GameData->GameFunctions.UpdateAndRender(&GameMemory, GameData->NewInput, Frame);

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
		OSXUpdateAudio(GameData);
	}

	END_BLOCK();

	//
	//
	//

	{DEBUG_DATA_BLOCK("Renderer");
		DEBUG_VALUE(GameData->Renderer->TotalFramebufferMemory);
		DEBUG_VALUE(GameData->Renderer->TotalTextureMemory);
		DEBUG_VALUE(GameData->Renderer->UsedMultisampleCount);
	}

#if HANDMADE_INTERNAL
	BEGIN_BLOCK("Debug Collation");

	b32 ExecutableNeedsToBeReloaded = OSXCheckForCodeChange(&GameData->GameCode);

	GameMemory.ExecutableReloaded = false;

	if (ExecutableNeedsToBeReloaded)
	{
		OSXCompleteAllWork(&GameData->HighPriorityQueue);
		OSXCompleteAllWork(&GameData->LowPriorityQueue);
		DEBUGSetEventRecording(false);
	}

	if (GameData->GameFunctions.DEBUGFrameEnd)
	{
		GameData->GameFunctions.DEBUGFrameEnd(&GameMemory, GameData->NewInput, Frame);
	}

	if (ExecutableNeedsToBeReloaded)
	{
		OSXReloadCode(OSXState, &GameData->GameCode);

		GameMemory.ExecutableReloaded = true;
		DEBUGSetEventRecording(GameData->GameCode.IsValid);
	}


	END_BLOCK();
#endif


	game_input* Temp = GameData->NewInput;
	GameData->NewInput = GameData->OldInput;
	GameData->OldInput = Temp;


	///////////////////////////////////////////////////////////////////
	// Draw the latest frame to the screen

	BEGIN_BLOCK("Frame Display");

	if (GameData->RendererCode.IsValid)
	{
		if (GameData->RendererWasReloaded)
		{
			++Frame->Settings.Version;
			GameData->RendererWasReloaded = false;
		}

		GameData->RendererFunctions.EndFrame(GameData->Renderer, Frame);
	}

	if (OSXCheckForCodeChange(&GameData->RendererCode))
	{
		OSXReloadCode(OSXState, &GameData->RendererCode);
		GameData->RendererWasReloaded = true;
	}


	END_BLOCK();
}



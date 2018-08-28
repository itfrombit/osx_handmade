

osx_state GlobalOSXState;

#if HANDMADE_INTERNAL
global_variable debug_table GlobalDebugTable_;
debug_table* GlobalDebugTable = &GlobalDebugTable_;
#endif

#if 0
CGLPixelFormatAttribute GLPixelFormatAttribs[] =
	{
		kCGLPFAAccelerated,
		kCGLPFADoubleBuffer,
		(CGLPixelFormatAttribute)0
	};
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

/*
			case GL_INVALID_FRAMEBUFFER_OPERATION:
				errString = "Invalid Framebuffer Operation";
				break;
*/
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
	void* Image = dlopen("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", RTLD_LAZY);
	if (Image)
	{
#define OSXGetOpenGLFunction(Module, Name) Name = (type_##Name *)dlsym(Module, #Name)
        OSXGetOpenGLFunction(Image, glDebugMessageCallbackARB);
        OSXGetOpenGLFunction(Image, glBindVertexArray);
        OSXGetOpenGLFunction(Image, glGenVertexArrays);
        OSXGetOpenGLFunction(Image, glGetStringi);

		opengl_info Info = OpenGLGetInfo(true);

		glBindFramebuffer = (gl_bind_framebuffer*)dlsym(Image, "glBindFramebuffer");
		glGenFramebuffers = (gl_gen_framebuffers*)dlsym(Image, "glGenFramebuffers");
		glFramebufferTexture2D = (gl_framebuffer_texture_2D*)dlsym(Image, "glFramebufferTexture2D");
		glCheckFramebufferStatus = (gl_check_framebuffer_status*)dlsym(Image, "glCheckFramebufferStatus");
        glTexImage2DMultisample = (gl_tex_image_2d_multisample *)dlsym(Image, "glTexImage2DMultisample");
        glBlitFramebuffer = (gl_blit_framebuffer *)dlsym(Image, "glBlitFramebuffer");

		OSXGetOpenGLFunction(Image, glDeleteFramebuffers);
		//OSXGetOpenGLFunction(Image, glDrawBuffers);
		OSXGetOpenGLFunction(Image, glVertexAttribIPointer);
#if 0
        glAttachShader = (gl_attach_shader *)dlsym("glAttachShader");
        glCompileShader = (gl_compile_shader *)dlsym("glCompileShader");
        glCreateProgram = (gl_create_program *)dlsym("glCreateProgram");
        glCreateShader = (gl_create_shader *)dlsym("glCreateShader");
        glLinkProgram = (gl_link_program *)dlsym("glLinkProgram");
        glShaderSource = (gl_shader_source *)dlsym("glShaderSource");
        glUseProgram = (gl_use_program *)dlsym("glUseProgram");
        glGetProgramInfoLog = (gl_get_program_info_log *)dlsym("glGetProgramInfoLog");
        glGetShaderInfoLog = (gl_get_shader_info_log *)dlsym("glGetShaderInfoLog");
        glValidateProgram = (gl_validate_program *)dlsym("glValidateProgram");
        glGetProgramiv = (gl_get_program_iv *)dlsym("glGetProgramiv");
#endif

		if (glBindFramebuffer)
		{
			printf("OpenGL extension functions loaded\n");
		}
		else
		{
			printf("Could not dynamically load glBindFramebuffer\n");
		}

		Assert(glBindFramebuffer);
		Assert(glGenFramebuffers);
		Assert(glFramebufferTexture2D);
		Assert(glCheckFramebufferStatus);
		Assert(glTexImage2DMultisample);
		Assert(glBlitFramebuffer);
        Assert(glAttachShader);
        Assert(glCompileShader);
        Assert(glCreateProgram);
        Assert(glCreateShader);
        Assert(glLinkProgram);
        Assert(glShaderSource);
        Assert(glUseProgram);
        Assert(glGetProgramInfoLog);
        Assert(glGetShaderInfoLog);
        Assert(glValidateProgram);
        Assert(glGetProgramiv);

		//OpenGL.SupportsSRGBFramebuffer = true;
		OpenGLInit(Info, OpenGL.SupportsSRGBFramebuffer);

#if 0
		OpenGLDefaultInternalTextureFormat = GL_RGBA8;
		//OpenGLDefaultInternalTextureFormat = GL_SRGB8_ALPHA8;
		glEnable(GL_FRAMEBUFFER_SRGB);
#endif
	}
	else
	{
		printf("Could not dynamically load OpenGL\n");
	}

}

///////////////////////////////////////////////////////////////////////
// Game Code
//

memory_arena FrameTempArena;
game_memory GameMemory;


void OSXSetupGameData(osx_game_data* GameData, CGLContextObj CGLContext)
{
	if (GameData->SetupComplete)
	{
		return;
	}


	osx_state* OSXState = &GlobalOSXState;
	OSXState->MemorySentinel.Prev = &OSXState->MemorySentinel;
	OSXState->MemorySentinel.Next = &OSXState->MemorySentinel;


	///////////////////////////////////////////////////////////////////
	// Worker Threads
	//

	osx_thread_startup HighPriStartups[6] = {};
	//OSXMakeQueue(&GameData->HighPriorityQueue, 6);
	OSXMakeQueue(&GameData->HighPriorityQueue, ArrayCount(HighPriStartups), HighPriStartups);


	osx_thread_startup LowPriStartups[2] = {};
	OSXMakeQueue(&GameData->LowPriorityQueue, ArrayCount(LowPriStartups), LowPriStartups);


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
	OSXGetAppFilename(OSXState);

	OSXBuildAppPathFilename(OSXState, (char*)"libhandmade.dylib",
							sizeof(GameData->SourceGameCodeDLFullPath),
							GameData->SourceGameCodeDLFullPath);

	// NOTE(jeff): We don't have to create a temp file
	GameData->Game = OSXLoadGameCode(GameData->SourceGameCodeDLFullPath);


	///////////////////////////////////////////////////////////////////
	// Set up memory
	//
	//ZeroStruct(GameData->FrameTempArena);
	//ZeroStruct(GameData->GameMemory);

	GameData->PushBufferSize = Megabytes(64);
	GameData->PushBuffer = OSXAllocateMemory(GameData->PushBufferSize,
										     PlatformMemory_NotRestored)->Base;

	GameData->MaxVertexCount = 65536;
	GameData->VertexArray = (textured_vertex*)OSXAllocateMemory(GameData->MaxVertexCount * sizeof(textured_vertex),
															          PlatformMemory_NotRestored)->Base;

	GameData->BitmapArray = (loaded_bitmap**)OSXAllocateMemory(GameData->MaxVertexCount * sizeof(loaded_bitmap*),
															  PlatformMemory_NotRestored)->Base;

	GameData->LightBoxes = (lighting_box*)OSXAllocateMemory(LIGHT_DATA_WIDTH * sizeof(lighting_box),
	                                                          PlatformMemory_NotRestored)->Base;

#if HANDMADE_INTERNAL
	char* RequestedAddress = (char*)Gigabytes(8);
	uint32 AllocationFlags = MAP_PRIVATE|MAP_ANON|MAP_FIXED;
#else
	char* RequestedAddress = (char*)0;
	uint32 AllocationFlags = MAP_PRIVATE|MAP_ANON;
#endif


#if HANDMADE_INTERNAL
	GameMemory.DebugTable = GlobalDebugTable;
#endif

	//OSXState->TotalSize = 0;
	//OSXState->GameMemoryBlock = 0;

#if 0
#ifndef HANDMADE_USE_VM_ALLOCATE
	// NOTE(jeff): I switched to mmap as the default, so unless the above
	// HANDMADE_USE_VM_ALLOCATE is defined in the build/make process,
	// we'll use the mmap version.

	GameData->OSXState.GameMemoryBlock = mmap(RequestedAddress, GameData->OSXState.TotalSize,
												PROT_READ|PROT_WRITE,
												AllocationFlags,
												-1,
												0);
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
	//GameData->GameMemory.DebugStorage = (u8*)GameData->GameMemory.TransientStorage
	//										+ GameData->GameMemory.TransientStorageSize;
#endif

	GameMemory.HighPriorityQueue = &GameData->HighPriorityQueue;
	GameMemory.LowPriorityQueue = &GameData->LowPriorityQueue;

	GameMemory.PlatformAPI.AddEntry = OSXAddEntry;
	GameMemory.PlatformAPI.CompleteAllWork = OSXCompleteAllWork;

	GameMemory.PlatformAPI.GetAllFilesOfTypeBegin = OSXGetAllFilesOfTypeBegin;
	GameMemory.PlatformAPI.GetAllFilesOfTypeEnd = OSXGetAllFilesOfTypeEnd;
	GameMemory.PlatformAPI.OpenFile = OSXOpenFile;
	GameMemory.PlatformAPI.ReadDataFromFile = OSXReadDataFromFile;
	GameMemory.PlatformAPI.WriteDataToFile = OSXWriteDataToFile;
	GameMemory.PlatformAPI.FileError = OSXFileError;

	GameMemory.PlatformAPI.AllocateMemory = OSXAllocateMemory;
	GameMemory.PlatformAPI.DeallocateMemory = OSXDeallocateMemory;

#if HANDMADE_INTERNAL
	GameMemory.PlatformAPI.DEBUGFreeFileMemory = DEBUGPlatformFreeFileMemory;
	GameMemory.PlatformAPI.DEBUGReadEntireFile = DEBUGPlatformReadEntireFile;
	GameMemory.PlatformAPI.DEBUGWriteEntireFile = DEBUGPlatformWriteEntireFile;

	GameMemory.PlatformAPI.DEBUGExecuteSystemCommand = DEBUGExecuteSystemCommand;
	GameMemory.PlatformAPI.DEBUGGetProcessState = DEBUGGetProcessState;
	GameMemory.PlatformAPI.DEBUGGetMemoryStats = OSXGetMemoryStats;
#endif

	u32 TextureOpCount = 1024;
	platform_texture_op_queue* TextureOpQueue = &GameMemory.TextureOpQueue;
	TextureOpQueue->FirstFree = (texture_op*)malloc(TextureOpCount * sizeof(texture_op));
		//(texture_op*)OSXAllocateMemory(TextureOpCount * sizeof(texture_op));
	for (u32 TextureOpIndex = 0;
			TextureOpIndex < (TextureOpCount - 1);
			++TextureOpIndex)
	{
		texture_op* Op = TextureOpQueue->FirstFree + TextureOpIndex;
		Op->Next = TextureOpQueue->FirstFree + TextureOpIndex + 1;
	}


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

	GameData->ExpectedFramesPerUpdate = 1;
	GameData->TargetSecondsPerFrame = (f32)GameData->ExpectedFramesPerUpdate / (f32)GameData->GameUpdateHz;

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
			if (KeyCode == '+')
			{
				if (IsDown)
				{
				}
			}
			else if (KeyCode == kVK_ANSI_Equal)
			{
				if (ShiftKeyFlag)
				{
					OpenGL.DebugLightBufferIndex += 1;
				}
				else
				{
					OpenGL.DebugLightBufferTexIndex += 1;
				}
			}
			else if (KeyCode == kVK_ANSI_Minus)
			{
				if (ShiftKeyFlag)
				{
					OpenGL.DebugLightBufferIndex -= 1;
				}
				else
				{
					OpenGL.DebugLightBufferTexIndex -= 1;
				}
			}
			else if (KeyCode == kVK_F1)
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


void OSXDisplayBufferInWindow(platform_work_queue* RenderQueue,
							  game_offscreen_buffer* RenderBuffer,
							  game_render_commands* Commands,
							  rectangle2i DrawRegion,
							  u32 WindowWidth,
							  u32 WindowHeight,
						      memory_arena* TempArena)
{
	temporary_memory TempMem = BeginTemporaryMemory(TempArena);

	if (!GlobalSoftwareRendering)
	{
		BEGIN_BLOCK("OpenGLRenderCommands");

		OpenGLRenderCommands(Commands, DrawRegion, WindowWidth, WindowHeight);
		END_BLOCK();
	}
	else
	{
		loaded_bitmap OutputTarget;
		OutputTarget.Memory = RenderBuffer->Memory;
		OutputTarget.Width = RenderBuffer->Width;
		OutputTarget.Height = RenderBuffer->Height;
		OutputTarget.Pitch = RenderBuffer->Pitch;

		//BEGIN_BLOCK("SoftwareRenderCommands");
		SoftwareRenderCommands(RenderQueue, Commands, &OutputTarget, TempArena);
		//END_BLOCK();

		// We always display via hardware

		v4 ClearColor = {};

		OpenGLDisplayBitmap(RenderBuffer->Width, RenderBuffer->Height,
							RenderBuffer->Memory, RenderBuffer->Pitch,
							DrawRegion,
							ClearColor,
							OpenGL.ReservedBlitTexture);
		//SwapBuffers();
	}

	EndTemporaryMemory(TempMem);
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

		NewInput->MouseX = GameData->RenderCommands.Settings.Width
		                      * Clamp01MapToRange(DrawRegion.MinX, MouseX, DrawRegion.MaxX);
		NewInput->MouseY = GameData->RenderCommands.Settings.Height
		                      * Clamp01MapToRange(DrawRegion.MinY, MouseY, DrawRegion.MaxY);

		NewInput->MouseZ = 0; // TODO(jeff): Support mousewheel?

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
		NewInput->MouseX = OldInput->MouseX;
		NewInput->MouseY = OldInput->MouseY;
		NewInput->MouseZ = OldInput->MouseZ;
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

	if (GameData->Game.GetSoundSamples)
	{
		// Sample Count is SamplesPerSecond / GameRefreshRate
		//GameData->SoundOutput.SoundBuffer.SampleCount = 1600; // (48000samples/sec) / (30fps)
		// ^^^ calculate this. We might be running at 30 or 60 fps
		GameData->SoundOutput.SoundBuffer.SampleCount = GameData->SoundOutput.SoundBuffer.SamplesPerSecond / GameData->TargetFramesPerSecond;

		GameData->Game.GetSoundSamples(&GameMemory, &GameData->SoundOutput.SoundBuffer);

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

			GameData->Game.GetSoundSamples(&GameMemory, &GameData->SoundOutput.SoundBuffer);

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


void OSXProcessFrameAndRunGameLogic(osx_game_data* GameData, CGRect WindowFrame,
									b32 MouseInWindowFlag, CGPoint MouseLocation,
									int MouseButtonMask)
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

	if (GameData->RenderCommandsInitialized == 0)
	{
		GameData->RenderCommands = DefaultRenderCommands(
												GameData->PushBufferSize,
												GameData->PushBuffer,
												(u32)GameData->RenderBuffer.Width,
												(u32)GameData->RenderBuffer.Height,
												GameData->MaxVertexCount,
												GameData->VertexArray,
												GameData->BitmapArray,
												&OpenGL.WhiteBitmap,
												GameData->LightBoxes);
		GameData->RenderCommandsInitialized = 1;
	}

	rectangle2i DrawRegion = AspectRatioFit(GameData->RenderCommands.Settings.Width,
	                                        GameData->RenderCommands.Settings.Height,
											WindowFrame.size.width,
											WindowFrame.size.height);

	OSXUpdateMouseInput(MouseInWindowFlag, MouseLocation, MouseButtonMask, GameData, DrawRegion);

	OSXUpdateGameControllerInput(GameData);

	END_BLOCK();

	//
	//
	//

	BEGIN_BLOCK("Game Update");

	GameData->RenderCommands.LightPointIndex = 1;

	if (!GlobalPause)
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
			GameData->NewInput->MouseX = Temp.MouseX;
			GameData->NewInput->MouseY = Temp.MouseY;
			GameData->NewInput->MouseZ = Temp.MouseZ;
		}

		if (GameData->Game.UpdateAndRender)
		{
			GameData->Game.UpdateAndRender(&GameMemory, GameData->NewInput, &GameData->RenderCommands);

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


#if HANDMADE_INTERNAL
	BEGIN_BLOCK("Debug Collation");

	b32 ExecutableNeedsToBeReloaded = false;

	time_t NewDLWriteTime = OSXGetLastWriteTime(GameData->SourceGameCodeDLFullPath);
	if (NewDLWriteTime != GameData->Game.DLLastWriteTime)
	{
		ExecutableNeedsToBeReloaded = true;
	}

	GameMemory.ExecutableReloaded = false;

	if (ExecutableNeedsToBeReloaded)
	{
		OSXCompleteAllWork(&GameData->HighPriorityQueue);
		OSXCompleteAllWork(&GameData->LowPriorityQueue);
		DEBUGSetEventRecording(false);
	}

	if (GameData->Game.DEBUGFrameEnd)
	{
		GameData->Game.DEBUGFrameEnd(&GameMemory, GameData->NewInput, &GameData->RenderCommands);
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

		GameMemory.ExecutableReloaded = true;
		DEBUGSetEventRecording(GameData->Game.IsValid);
	}


	END_BLOCK();
#endif


	game_input* Temp = GameData->NewInput;
	GameData->NewInput = GameData->OldInput;
	GameData->OldInput = Temp;


	///////////////////////////////////////////////////////////////////
	// Draw the latest frame to the screen

	BEGIN_BLOCK("Frame Display");

	platform_texture_op_queue* TextureOpQueue = &GameMemory.TextureOpQueue;

	BeginTicketMutex(&TextureOpQueue->Mutex);
	texture_op* FirstTextureOp = TextureOpQueue->First;
	texture_op* LastTextureOp = TextureOpQueue->Last;
	TextureOpQueue->First = 0;
	TextureOpQueue->Last = 0;
	EndTicketMutex(&TextureOpQueue->Mutex);

	if (FirstTextureOp)
	{
		Assert(LastTextureOp);
		OpenGLManageTextures(FirstTextureOp);
		BeginTicketMutex(&TextureOpQueue->Mutex);
		LastTextureOp->Next = TextureOpQueue->FirstFree;
		TextureOpQueue->FirstFree = FirstTextureOp;
		EndTicketMutex(&TextureOpQueue->Mutex);
	}

	OSXDisplayBufferInWindow(&GameData->HighPriorityQueue,
							 &GameData->RenderBuffer,
							 &GameData->RenderCommands,
							 DrawRegion,
							 WindowFrame.size.width,
							 WindowFrame.size.height,
							 &FrameTempArena);

	GameData->RenderCommands.PushBufferDataAt = GameData->RenderCommands.PushBufferBase;
	GameData->RenderCommands.VertexCount = 0;
	GameData->RenderCommands.LightBoxCount = 0;

	END_BLOCK();
}



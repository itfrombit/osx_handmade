#if 0
#define OpenGLGlobalFunction(Name) global type_##Name *Name;
OpenGLGlobalFunction(glDebugMessageCallbackARB);
OpenGLGlobalFunction(glGetStringi);

OpenGLGlobalFunction(glBindVertexArray);
OpenGLGlobalFunction(glGenVertexArrays);
OpenGLGlobalFunction(glDeleteFramebuffers);
OpenGLGlobalFunction(glVertexAttribIPointer);

OpenGLGlobalFunction(glDrawElementsBaseVertex);
#endif


// NOTE(jeff): This is a weak reference that is
// updated during a hot reload. The "real" one
// is kept in the OpenGL.Header.Platform
extern NSOpenGLContext* GlobalGLContext;


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



internal void PlatformOpenGLSetVSync(open_gl* Renderer, b32x VSyncEnabled)
{
	NSOpenGLContext* GLContext = (NSOpenGLContext*)Renderer->Header.Platform;
	Assert(GLContext);

    GLint SwapInterval = VSyncEnabled ? 1 : 0;


	[GLContext setValues:&SwapInterval forParameter:NSOpenGLCPSwapInterval];
}


extern "C" RENDERER_BEGIN_FRAME(OSXBeginFrame)
{
	NSOpenGLContext* GLContext = (NSOpenGLContext*)Renderer->Platform;

	[GLContext makeCurrentContext];
	game_render_commands* Result = OpenGLBeginFrame((open_gl*)Renderer, OSWindowDim, RenderDim, DrawRegion);

	return Result;
}


extern "C" RENDERER_END_FRAME(OSXEndFrame)
{

	OpenGLEndFrame((open_gl*)Renderer, Frame);

	// flushes and forces vsync
	//
	NSOpenGLContext* GLContext = (NSOpenGLContext*)Renderer->Platform;
	[GLContext flushBuffer];

#if 0
#if HANDMADE_USE_VSYNC
	[GlobalGLContext flushBuffer];
#else
	glFlush();
#endif
#endif
}

internal open_gl* OSXInitOpenGL(platform_renderer_limits* Limits)
{
	open_gl* OpenGL = (open_gl*)OSXRendererAlloc(sizeof(open_gl));

	InitTextureQueue(&OpenGL->Header.TextureQueue,
	                 Limits->TextureTransferBufferSize,
	                 OSXRendererAlloc(Limits->TextureTransferBufferSize));

	u32 MaxVertexCount = Limits->MaxQuadCountPerFrame * 4;
	u32 MaxIndexCount = Limits->MaxQuadCountPerFrame * 6;
	OpenGL->MaxTextureCount = Limits->MaxTextureCount;
	OpenGL->MaxVertexCount = MaxVertexCount;
	OpenGL->MaxIndexCount = MaxIndexCount;
	OpenGL->MaxSpecialTextureCount = Limits->MaxSpecialTextureCount;

	// NOTE(casey): This is wayyyyyy overkill because you would not render all your quads
	// as separate textures, so this wastes a ton of memory.  At some point we may want
	// to restrict the number of these you could draw with separate textures.
	OpenGL->MaxQuadTextureCount = Limits->MaxQuadCountPerFrame;

	OpenGL->VertexArray = (textured_vertex*)OSXRendererAlloc(MaxVertexCount * sizeof(textured_vertex));
	OpenGL->IndexArray = (u16*)OSXRendererAlloc(MaxIndexCount * sizeof(u16));
	OpenGL->BitmapArray = (renderer_texture*)OSXRendererAlloc(OpenGL->MaxQuadTextureCount * sizeof(renderer_texture));

	if (Limits->MaxSpecialTextureCount)
	{
		OpenGL->SpecialTextureHandles = (GLuint*)OSXRendererAlloc(Limits->MaxSpecialTextureCount * sizeof(GLuint));
	}
	else
	{
		OpenGL->SpecialTextureHandles = NULL;
	}

	void* Image = dlopen("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", RTLD_LAZY);
	if (Image)
	{
#define OSXGetOpenGLFunction(Module, Name) OpenGL->Name = (type_##Name *)dlsym(Module, #Name)

        OSXGetOpenGLFunction(Image, glTexImage2DMultisample);
		OSXGetOpenGLFunction(Image, glBindFramebuffer);
		OSXGetOpenGLFunction(Image, glGenFramebuffers);
		OSXGetOpenGLFunction(Image, glFramebufferTexture2D);
		OSXGetOpenGLFunction(Image, glCheckFramebufferStatus);
        OSXGetOpenGLFunction(Image, glBlitFramebuffer);
        OSXGetOpenGLFunction(Image, glAttachShader);
        OSXGetOpenGLFunction(Image, glCompileShader);
        OSXGetOpenGLFunction(Image, glCreateProgram);
        OSXGetOpenGLFunction(Image, glCreateShader);
        OSXGetOpenGLFunction(Image, glLinkProgram);
        OSXGetOpenGLFunction(Image, glShaderSource);
        OSXGetOpenGLFunction(Image, glUseProgram);
        OSXGetOpenGLFunction(Image, glGetProgramInfoLog);
        OSXGetOpenGLFunction(Image, glGetShaderInfoLog);
        OSXGetOpenGLFunction(Image, glValidateProgram);
        OSXGetOpenGLFunction(Image, glGetProgramiv);
        OSXGetOpenGLFunction(Image, glGetUniformLocation);
        OSXGetOpenGLFunction(Image, glUniform4fv);
        OSXGetOpenGLFunction(Image, glUniformMatrix4fv);
        OSXGetOpenGLFunction(Image, glUniform1i);
        OSXGetOpenGLFunction(Image, glUniform1f);
        OSXGetOpenGLFunction(Image, glUniform2fv);
        OSXGetOpenGLFunction(Image, glUniform3fv);
        OSXGetOpenGLFunction(Image, glEnableVertexAttribArray);
        OSXGetOpenGLFunction(Image, glDisableVertexAttribArray);
        OSXGetOpenGLFunction(Image, glGetAttribLocation);
        OSXGetOpenGLFunction(Image, glVertexAttribPointer);
        OSXGetOpenGLFunction(Image, glVertexAttribIPointer);
        OSXGetOpenGLFunction(Image, glDebugMessageCallbackARB);
        OSXGetOpenGLFunction(Image, glBindVertexArray);
        OSXGetOpenGLFunction(Image, glGenVertexArrays);
        OSXGetOpenGLFunction(Image, glBindBuffer);
        OSXGetOpenGLFunction(Image, glGenBuffers);
        OSXGetOpenGLFunction(Image, glBufferData);
        OSXGetOpenGLFunction(Image, glActiveTexture);
        OSXGetOpenGLFunction(Image, glGetStringi);
        OSXGetOpenGLFunction(Image, glDeleteProgram);
        OSXGetOpenGLFunction(Image, glDeleteShader);
        OSXGetOpenGLFunction(Image, glDeleteFramebuffers);
        OSXGetOpenGLFunction(Image, glDrawBuffers);
        OSXGetOpenGLFunction(Image, glTexImage3D);
        OSXGetOpenGLFunction(Image, glTexSubImage3D);
        OSXGetOpenGLFunction(Image, glDrawElementsBaseVertex);

		opengl_info Info = OpenGLGetInfo(OpenGL, true);

		//OpenGL.SupportsSRGBFramebuffer = true;
		OpenGLInit(OpenGL, Info, OpenGL->SupportsSRGBFramebuffer);

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

	return OpenGL;
}


extern "C"
{
OSX_LOAD_RENDERER_ENTRY()
{
	void* GLContext = OSXInitOpenGLView(Window);
	platform_renderer* Result = (platform_renderer*)OSXInitOpenGL(Limits);
	Result->Platform = GLContext;

	return Result;
}
}


#if 0
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
		software_texture OutputTarget;
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
#endif


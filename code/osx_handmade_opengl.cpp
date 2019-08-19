
#define GL_BGRA_EXT			0x80E1
#define GL_NUM_EXTENSIONS	0x821D

typedef char GLchar;

typedef void gl_tex_image_2d_multisample(GLenum target, GLsizei samples, GLenum internalformat,
									GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void gl_bind_framebuffer(GLenum target, GLuint framebuffer);
typedef void gl_gen_framebuffers(GLsizei n, GLuint *framebuffers);
typedef void gl_framebuffer_texture_2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum gl_check_framebuffer_status(GLenum target);
typedef void gl_blit_framebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void gl_attach_shader(GLuint program, GLuint shader);
typedef void gl_compile_shader(GLuint shader);
typedef GLuint gl_create_program(void);
typedef GLuint gl_create_shader(GLenum type);
typedef void gl_link_program(GLuint program);
typedef void gl_shader_source(GLuint shader, GLsizei count, GLchar **string, GLint *length);
typedef void gl_use_program(GLuint program);
typedef void gl_get_program_info_log(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void gl_get_shader_info_log(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void gl_validate_program(GLuint program);
typedef void gl_get_program_iv(GLuint program, GLenum pname, GLint *params);

typedef void type_glBindVertexArray(GLuint array);
typedef void type_glGenVertexArrays(GLsizei n, GLuint *arrays);
typedef GLubyte* type_glGetStringi(GLenum name, GLuint index);

typedef void type_glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers);

typedef void type_glDrawBuffers(GLsizei n, const GLenum* bufs);

typedef void type_glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer);

typedef void type_glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
typedef void type_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex);

global gl_tex_image_2d_multisample *glTexImage2DMultisample;
global gl_bind_framebuffer* glBindFramebuffer;
global gl_gen_framebuffers* glGenFramebuffers;
global gl_framebuffer_texture_2D* glFramebufferTexture2D;
global gl_check_framebuffer_status* glCheckFramebufferStatus;
global gl_blit_framebuffer *glBlitFramebuffer;

#define GL_DEBUG_CALLBACK(Name) void Name(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam)
typedef GL_DEBUG_CALLBACK(GLDEBUGPROC);
typedef void type_glDebugMessageCallbackARB(GLDEBUGPROC *callback, const void *userParam);

#define OpenGLGlobalFunction(Name) global type_##Name *Name;
OpenGLGlobalFunction(glDebugMessageCallbackARB);
OpenGLGlobalFunction(glGetStringi);

#if 0
global gl_attach_shader *glAttachShader;
global gl_compile_shader *glCompileShader;
global gl_create_program *glCreateProgram;
global gl_create_shader *glCreateShader;
global gl_link_program *glLinkProgram;
global gl_shader_source *glShaderSource;
global gl_use_program *glUseProgram;
global gl_get_program_info_log *glGetProgramInfoLog;
global gl_get_shader_info_log *glGetShaderInfoLog;
global gl_validate_program *glValidateProgram;
global gl_get_program_iv *glGetProgramiv;
#endif

OpenGLGlobalFunction(glBindVertexArray);
OpenGLGlobalFunction(glGenVertexArrays);
OpenGLGlobalFunction(glDeleteFramebuffers);
OpenGLGlobalFunction(glVertexAttribIPointer);

OpenGLGlobalFunction(glTexStorage3D);
OpenGLGlobalFunction(glDrawElementsBaseVertex);

//OpenGLGlobalFunction(glDrawBuffers);



static NSOpenGLContext* GlobalGLContext;
static b32x GlobalVSyncEnabled;


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
	Assert(GlobalGLContext);

    GLint SwapInterval = VSyncEnabled ? 1 : 0;
	GlobalVSyncEnabled = VSyncEnabled;

	[GlobalGLContext setValues:&SwapInterval forParameter:NSOpenGLCPSwapInterval];
}


RENDERER_BEGIN_FRAME(OSXOpenGLBeginFrame)
{
	[GlobalGLContext makeCurrentContext];
	game_render_commands* Result = OpenGLBeginFrame((open_gl*)Renderer, WindowWidth, WindowHeight, DrawRegion);

	return Result;
}


RENDERER_END_FRAME(OSXOpenGLEndFrame)
{
	OpenGLEndFrame((open_gl*)Renderer, Frame);

	// flushes and forces vsync
	//
	[GlobalGLContext flushBuffer];

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

	OpenGL->Header.BeginFrame = OSXOpenGLBeginFrame;
	OpenGL->Header.EndFrame = OSXOpenGLEndFrame;

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
	OpenGL->SpecialTextureHandles = (GLuint*)OSXRendererAlloc(Limits->MaxSpecialTextureCount * sizeof(GLuint));

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
		OSXGetOpenGLFunction(Image, glVertexAttribIPointer);

		OSXGetOpenGLFunction(Image, glTexStorage3D);
		OSXGetOpenGLFunction(Image, glDrawElementsBaseVertex);

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
	OSXInitOpenGLView(Window);
	platform_renderer* Result = (platform_renderer*)OSXInitOpenGL(Limits);

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


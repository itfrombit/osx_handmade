#define OSX_LOAD_RENDERER(name) platform_renderer* name(NSWindow* Window, u32 MaxQuadCountPerFrame, u32 MaxTextureCount, u32 MaxSpecialTextureCount)
typedef OSX_LOAD_RENDERER(osx_load_renderer);
#define OSX_LOAD_RENDERER_ENTRY() OSX_LOAD_RENDERER(OSXLoadRenderer)

internal osx_load_renderer* OSXLoadRendererDylib(const char* Filename)
{
	void* lib = dlopen(Filename, RTLD_LAZY | RTLD_GLOBAL);
	osx_load_renderer* Result = (osx_load_renderer*)dlsym(lib, "OSXLoadRenderer");

	return Result;
}


internal platform_renderer* OSXInitDefaultRenderer(NSWindow* Window,
												   u32 MaxQuadCountPerFrame,
												   u32 MaxTextureCount,
												   u32 MaxSpecialTextureCount)
{
	osx_load_renderer* OSXLoadOpenGLRenderer = OSXLoadRendererDylib("libhandmade_opengl.dylib");

	if (!OSXLoadOpenGLRenderer)
	{
		OSXMessageBox(@"Missing Game Renderer",
			@"Please make sure osxhandmade_opengl.dylib is present in the application bundle.");
		exit(1);
	}

	platform_renderer* Renderer = OSXLoadOpenGLRenderer(Window,
	                                                    MaxQuadCountPerFrame,
														MaxTextureCount,
														MaxSpecialTextureCount);

	return Renderer;
}


void* OSXRendererAlloc(umm Size)
{
	void* P = (void*)mmap(0, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

	if (P == MAP_FAILED)
	{
		printf("OSXRendererAlloc: mmap error: %d  %s", errno, strerror(errno));
	}

	Assert(P);

	return P;
}


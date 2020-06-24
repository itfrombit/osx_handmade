#define OSX_LOAD_RENDERER(name) platform_renderer* name(NSWindow* Window, platform_renderer_limits* Limits)
typedef OSX_LOAD_RENDERER(osx_load_renderer);
#define OSX_LOAD_RENDERER_ENTRY() OSX_LOAD_RENDERER(OSXLoadRenderer)


struct osx_renderer_function_table
{
	osx_load_renderer* LoadRenderer;
	renderer_begin_frame* BeginFrame;
	renderer_end_frame* EndFrame;
};


global char* OSXRendererFunctionTableNames[] =
{
	"OSXLoadRenderer",
	"OSXBeginFrame",
	"OSXEndFrame",
};


void* OSXRendererAlloc(umm Size)
{
	void* P = (void*)mmap(0, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

	if (P == MAP_FAILED)
	{
		printf("OSXRendererAlloc: mmap error: %d  %s.  Requested size = %lu\n", errno, strerror(errno), Size);
	}

	Assert(P);

	return P;
}



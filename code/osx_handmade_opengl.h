
#define GL_BGRA_EXT			0x80E1
#define GL_NUM_EXTENSIONS	0x821D

typedef char GLchar;

typedef void type_glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat,
									      GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void type_glBindFramebuffer(GLenum target, GLuint framebuffer);
typedef void type_glGenFramebuffers(GLsizei n, GLuint *framebuffers);
typedef void type_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum type_glCheckFramebufferStatus(GLenum target);
typedef void type_glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void type_glAttachShader(GLuint program, GLuint shader);
typedef void type_glCompileShader(GLuint shader);
typedef GLuint type_glCreateProgram(void);
typedef GLuint type_glCreateShader(GLenum type);
typedef void type_glLinkProgram(GLuint program);
typedef void type_glShaderSource(GLuint shader, GLsizei count, GLchar **string, GLint *length);
typedef void type_glUseProgram(GLuint program);
typedef void type_glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void type_glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void type_glValidateProgram(GLuint program);
typedef void type_glGetProgramiv(GLuint program, GLenum pname, GLint *params);


/////////
// New
typedef GLint type_glGetUniformLocation(GLuint program, const GLchar *name);
typedef void type_glUniform4fv(GLint location, GLsizei count, const GLfloat *value);
typedef void type_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void type_glUniform1i(GLint location, GLint v0);
typedef void type_glUniform1f(GLint location, GLfloat v0);
typedef void type_glUniform2fv(GLint location, GLsizei count, const GLfloat *value);
typedef void type_glUniform3fv(GLint location, GLsizei count, const GLfloat *value);

typedef void type_glEnableVertexAttribArray(GLuint index);
typedef void type_glDisableVertexAttribArray(GLuint index);
typedef GLint type_glGetAttribLocation(GLuint program, const GLchar *name);
typedef void type_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void type_glVertexAttribIPointer (GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer);

typedef void type_glBindBuffer (GLenum target, GLuint buffer);
typedef void type_glGenBuffers (GLsizei n, GLuint *buffers);
typedef void type_glBufferData (GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void type_glActiveTexture (GLenum texture);
typedef void type_glDeleteProgram (GLuint program);
typedef void type_glDeleteShader (GLuint shader);
typedef void type_glDeleteFramebuffers (GLsizei n, const GLuint *framebuffers);
typedef void type_glDrawBuffers (GLsizei n, const GLenum *bufs);
typedef void type_glTexImage3D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void type_glTexSubImage3D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
//
////////


typedef void type_glBindVertexArray(GLuint array);
typedef void type_glGenVertexArrays(GLsizei n, GLuint *arrays);
typedef GLubyte* type_glGetStringi(GLenum name, GLuint index);
typedef void type_glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers);
typedef void type_glDrawBuffers(GLsizei n, const GLenum* bufs);
typedef void type_glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer);
typedef void type_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex);

#if 0
global gl_tex_image_2d_multisample *glTexImage2DMultisample;
global gl_bind_framebuffer* glBindFramebuffer;
global gl_gen_framebuffers* glGenFramebuffers;
global gl_framebuffer_texture_2D* glFramebufferTexture2D;
global gl_check_framebuffer_status* glCheckFramebufferStatus;
global gl_blit_framebuffer *glBlitFramebuffer;
#endif

#define GL_DEBUG_CALLBACK(Name) void Name(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam)
typedef GL_DEBUG_CALLBACK(GLDEBUGPROC);
typedef void type_glDebugMessageCallbackARB(GLDEBUGPROC *callback, const void *userParam);


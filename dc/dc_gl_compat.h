/*
 *	dc_gl_compat.h -- the handful of GL entry points GLdc does not have.
 *
 *	Aleph One's hardware renderer calls 61 GL entry points. GLdc, the OpenGL
 *	implementation KallistiOS ships for the PowerVR, provides 43 of them. This
 *	supplies the rest, and it is a far smaller job than it looks: seventeen
 *	identifiers, all in OGL_Render.cpp, in four groups.
 *
 *	  1. Double-precision matrix work. glLoadMatrixd, glMultMatrixd and
 *	     glGetDoublev. The PowerVR is a single-precision part and GLdc has the
 *	     float versions of all three, so these convert and forward. glGetDoublev
 *	     is only ever asked for GL_PROJECTION_MATRIX or GL_MODELVIEW_MATRIX, so
 *	     sixteen elements is always the right answer.
 *
 *	  2. Short-typed immediate mode. glVertex2s, glColor3usv, glColor4usv. The
 *	     colour calls take unsigned shorts over the full 0..65535 range, which is
 *	     why they scale rather than cast.
 *
 *	  3. Effects the PowerVR cannot do. glLogicOp with GL_OR, and
 *	     glPolygonStipple, together drawing the static/interference effect. There
 *	     is no raster logic op on this hardware. They are dropped, which costs
 *	     that one effect and nothing else.
 *
 *	  4. Clip planes -- the interesting one, because this is what the port has
 *	     recorded as the blocker for a year of build notes, and it is not.
 *
 *	CLIP PLANES
 *
 *	All six glClipPlane calls in OGL_Render.cpp sit inside RenderModelSetup(),
 *	which is reached only when rectangle_definition::ModelPtr is not null
 *	(OGL_Render.cpp:1829). That is the external-3D-model path: Aleph One can
 *	replace a sprite with a real mesh when an MML <model> declaration says to.
 *	Stock Marathon 2 declares none, and this disc ships none, so the function is
 *	never entered and the clip planes are never set.
 *
 *	So they are stubbed, and the stub traces the first time it is called. If a
 *	model ever does appear the log will say so rather than the picture quietly
 *	going wrong.
 *
 *	Were they needed, they still would not require the "renderer rewrite" the old
 *	note claimed. Planes 0 to 3 each pass through the eye and one screen-axis-
 *	aligned edge of the render rectangle, so together they cut a rectangular
 *	sub-frustum -- exactly what glScissor does, and GLdc has glScissor. Only
 *	plane 4, the liquid surface, is a genuinely oblique plane needing geometry
 *	clipped in software.
 *
 *	Display lists are handled in OGL_Render.cpp itself rather than here; see the
 *	DC block in OGL_RenderText().
 */

#ifndef DC_GL_COMPAT_H
#define DC_GL_COMPAT_H

#ifdef DC

#include <GL/gl.h>

/* Real OpenGL enum values, so that if GLdc ever grows these it agrees with us,
   and so an unknown enum reaching glEnable is one it will simply ignore. */
#ifndef GL_CLIP_PLANE0
#define GL_CLIP_PLANE0		0x3000
#define GL_CLIP_PLANE1		0x3001
#define GL_CLIP_PLANE2		0x3002
#define GL_CLIP_PLANE3		0x3003
#define GL_CLIP_PLANE4		0x3004
#define GL_CLIP_PLANE5		0x3005
#endif

#ifndef GL_COLOR_LOGIC_OP
#define GL_COLOR_LOGIC_OP	0x0BF2
#endif

#ifndef GL_OR
#define GL_OR				0x1507
#endif

#ifndef GL_COMPILE
#define GL_COMPILE			0x1300
#endif

#ifdef __cplusplus
extern "C" {
#endif
void dc_trace(int slot, const char *fmt, ...);
#ifdef __cplusplus
}
#endif

/* ---- 1. double-precision matrices ------------------------------------- */

static inline void dc_gl_d2f(const GLdouble *in, GLfloat *out, int n)
{
	int i;

	for (i = 0; i < n; i++)
		out[i] = (GLfloat)in[i];
}

static inline void glLoadMatrixd(const GLdouble *m)
{
	GLfloat f[16];

	dc_gl_d2f(m, f, 16);
	glLoadMatrixf(f);
}

static inline void glMultMatrixd(const GLdouble *m)
{
	GLfloat f[16];

	dc_gl_d2f(m, f, 16);
	glMultMatrixf(f);
}

/*
 *	Only ever called for GL_PROJECTION_MATRIX and GL_MODELVIEW_MATRIX, both of
 *	which are sixteen elements. Anything else would be a caller this port does
 *	not have, so it says so rather than overrunning the caller's buffer.
 */
static inline void glGetDoublev(GLenum pname, GLdouble *params)
{
	GLfloat f[16];
	int i;

	if (pname != GL_PROJECTION_MATRIX && pname != GL_MODELVIEW_MATRIX) {
		dc_trace(31, "gl: glGetDoublev(%04x) unsupported", (unsigned)pname);
		return;
	}

	glGetFloatv(pname, f);

	for (i = 0; i < 16; i++)
		params[i] = (GLdouble)f[i];
}

/* ---- 2. short-typed immediate mode ------------------------------------ */

static inline void glVertex2s(GLshort x, GLshort y)
{
	glVertex2f((GLfloat)x, (GLfloat)y);
}

static inline void glColor3usv(const GLushort *v)
{
	glColor3f(v[0] / 65535.0f, v[1] / 65535.0f, v[2] / 65535.0f);
}

static inline void glColor4usv(const GLushort *v)
{
	glColor4f(v[0] / 65535.0f, v[1] / 65535.0f,
	          v[2] / 65535.0f, v[3] / 65535.0f);
}

/* ---- 3. effects the PowerVR cannot do --------------------------------- */

static inline void glLogicOp(GLenum opcode)
{
	(void)opcode;	/* no raster logic op on this hardware */
}

static inline void glPolygonStipple(const GLubyte *mask)
{
	(void)mask;		/* costs the static/interference effect, nothing else */
}

/* ---- 4. clip planes: model path only, see the note above --------------- */

static inline void glClipPlane(GLenum plane, const GLdouble *equation)
{
	static int warned = 0;

	(void)equation;

	if (!warned) {
		warned = 1;
		dc_trace(31, "gl: clip plane %d requested -- a model is being drawn",
		         (int)(plane - GL_CLIP_PLANE0));
	}
}

#endif	/* DC */

#endif	/* DC_GL_COMPAT_H */

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
 *	     recorded as the blocker for a year of build notes, and it is not. It is
 *	     also where this file was wrong about its own subject: see below.
 *
 *	CLIP PLANES
 *
 *	Six of the seven glClipPlane calls are in OGL_Render.cpp, inside
 *	RenderModelSetup(), reached only when rectangle_definition::ModelPtr is not
 *	null -- the external-3D-model path. Stock Marathon 2 declares no models and
 *	this disc ships none, so that function is never entered.
 *
 *	THE SEVENTH IS NOT, AND THIS FILE USED TO SAY THERE WERE ONLY SIX.
 *
 *	HUDRenderer_OGL.cpp:365, in HUD_OGL_Class::SetClipPlane, clips motion sensor
 *	blips to the circular sensor area -- a half-plane tangent to the sensor
 *	circle, set per blip. That runs on the HUD path, every frame a blip is near
 *	the edge, and it is what actually fires the trace below.
 *
 *	The old trace text said "a model is being drawn", which is wrong and cost a
 *	session: it sends you into RenderPlaceObjs and OGL_GetModelData looking for a
 *	non-null ModelPtr that was never there. Verified by closing the model call
 *	site entirely under #ifdef DC -- the trace still fired.
 *
 *	WHAT STUBBING THIS COSTS, CONCRETELY: motion sensor blips are not clipped to
 *	the sensor, so a blip near the rim draws outside it, over the rest of the
 *	HUD. GLdc has glScissor, which could bound the blips to the sensor's
 *	rectangle -- not the circle, but far better than nothing. Not attempted here
 *	because it cannot be checked without looking at the screen.
 *
 *	DisableClipPlane() calls glDisable(GL_CLIP_PLANE0), which GLdc rejects with
 *	GL_INVALID_VALUE. That is the error printed once at startup; harmless, since
 *	it only sets the GL error flag.
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

#ifndef GL_XOR
#define GL_XOR				0x1506
#endif

#ifndef GL_COMPILE
#define GL_COMPILE			0x1300
#endif

#ifdef __cplusplus
extern "C" {
#endif
void dc_trace(int slot, const char *fmt, ...);

/* dc/dc_glu.c -- GLdc ships a glu.h without this, and TextureManager::Shrink
   needs it to bring oversized textures down to what the hardware accepts. */
int gluScaleImage(GLenum format,
                  GLsizei widthin, GLsizei heightin, GLenum typein,
                  const void *datain,
                  GLsizei widthout, GLsizei heightout, GLenum typeout,
                  void *dataout);
int gluBuild2DMipmaps(GLenum target, GLint internalFormat,
                      GLsizei width, GLsizei height,
                      GLenum format, GLenum type, const void *data);
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

/* ---- 3. effects the PowerVR cannot do --------------------------------- */

static inline void glLogicOp(GLenum opcode)
{
	(void)opcode;	/* no raster logic op on this hardware */
}

static inline void glPolygonStipple(const GLubyte *mask)
{
	(void)mask;		/* costs the static/interference effect, nothing else */
}

/* ---- 3b. attribute stack ---------------------------------------------
 *
 *	GLdc keeps no attribute stack. The callers here (FontSpecifier::OGL_Render)
 *	set every piece of state they depend on immediately after pushing, so the
 *	push is redundant; the pop is not, and dropping it leaves texturing, blend
 *	and cull state as the text renderer left them. Every drawing path in
 *	OGL_Render.cpp sets those explicitly before it draws, so nothing downstream
 *	reads them stale -- but it is a real difference from desktop GL and worth
 *	suspecting if something is textured or blended when it should not be.
 */

static inline void glPushAttrib(GLbitfield mask)
{
	(void)mask;
}

static inline void glPopAttrib(void)
{
}

/* ---- 4. clip planes: model path only, see the note above --------------- */

static inline void glClipPlane(GLenum plane, const GLdouble *equation)
{
	static int warned = 0;

	(void)equation;

	if (!warned) {
		warned = 1;
		dc_trace(31, "gl: clip plane %d stubbed (motion sensor blip clipping)",
		         (int)(plane - GL_CLIP_PLANE0));
	}
}


/* ---- 5. the current colour, for vertex arrays -------------------------
 *
 *	Desktop GL uses the colour set by glColor*() for every vertex when the colour
 *	array is disabled. GLdc does not. From its attributes.c:
 *
 *	    static ReadAttributeFunc calcReadDiffuseFunc(void) {
 *	        if((ATTRIB_LIST.enabled & COLOR_ENABLED_FLAG) != COLOR_ENABLED_FLAG) {
 *	            // Just fill the whole thing white if the attribute is disabled
 *	            return _fillWhiteARGB;
 *
 *	It keeps a current_color, but only immediate mode reads it. Anything drawn
 *	with glDrawArrays and no colour array comes out white.
 *
 *	Aleph One does exactly that in two places that matter. Walls are shaded by
 *	calling glColor3f with the light level and then drawing an array, so every
 *	surface rendered at full brightness and the lighting appeared not to work.
 *	And the screen faders draw a single quad with glColor4fv, so being hit filled
 *	the screen with opaque white instead of a translucent tint.
 *
 *	So the colour is tracked here, and a draw with no colour array gets a
 *	throwaway one filled with it. The wrappers are defined before the macros that
 *	rename them, so these call the real entry points.
 */

#define DC_GL_MAX_ARRAY_VERTS	256

static GLfloat dc_gl_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static int dc_gl_color_array_on = 0;

static inline void dc_gl_set_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	dc_gl_color[0] = r; dc_gl_color[1] = g;
	dc_gl_color[2] = b; dc_gl_color[3] = a;
	glColor4f(r, g, b, a);
}

static inline void dc_glColor3f(GLfloat r, GLfloat g, GLfloat b)
	{ dc_gl_set_color(r, g, b, 1.0f); }

static inline void dc_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
	{ dc_gl_set_color(r, g, b, a); }

static inline void dc_glColor3fv(const GLfloat *v)
	{ dc_gl_set_color(v[0], v[1], v[2], 1.0f); }

static inline void dc_glColor4fv(const GLfloat *v)
	{ dc_gl_set_color(v[0], v[1], v[2], v[3]); }

static inline void dc_glColor3usv(const GLushort *v)
	{ dc_gl_set_color(v[0]/65535.0f, v[1]/65535.0f, v[2]/65535.0f, 1.0f); }

static inline void dc_glColor4usv(const GLushort *v)
	{ dc_gl_set_color(v[0]/65535.0f, v[1]/65535.0f,
	                  v[2]/65535.0f, v[3]/65535.0f); }

static inline void dc_glEnableClientState(GLenum cap)
{
	if (cap == GL_COLOR_ARRAY)
		dc_gl_color_array_on = 1;
	glEnableClientState(cap);
}

static inline void dc_glDisableClientState(GLenum cap)
{
	if (cap == GL_COLOR_ARRAY)
		dc_gl_color_array_on = 0;
	glDisableClientState(cap);
}

/*
 *	Draw, supplying a flat colour array when the caller has not given one. Over
 *	the vertex limit the array is skipped rather than truncated: a wrong colour
 *	on a huge polygon beats reading past the end of the buffer.
 */
static inline void dc_gl_draw_with_color(GLenum mode, GLint first, GLsizei count)
{
	static GLfloat Flat[DC_GL_MAX_ARRAY_VERTS][4];
	GLsizei i;

	if (dc_gl_color_array_on || count > DC_GL_MAX_ARRAY_VERTS) {
		glDrawArrays(mode, first, count);
		return;
	}

	for (i = 0; i < count; i++) {
		Flat[i][0] = dc_gl_color[0];
		Flat[i][1] = dc_gl_color[1];
		Flat[i][2] = dc_gl_color[2];
		Flat[i][3] = dc_gl_color[3];
	}

	glColorPointer(4, GL_FLOAT, 0, Flat[0]);
	glEnableClientState(GL_COLOR_ARRAY);
	glDrawArrays(mode, first, count);
	glDisableClientState(GL_COLOR_ARRAY);
}

static inline void dc_glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	dc_gl_draw_with_color(mode, first, count);
}

/* Applied to game code only: everything above is already compiled. */
#define glColor3f			dc_glColor3f
#define glColor4f			dc_glColor4f
#define glColor3fv			dc_glColor3fv
#define glColor4fv			dc_glColor4fv
#define glColor3usv			dc_glColor3usv
#define glColor4usv			dc_glColor4usv
#define glEnableClientState	dc_glEnableClientState
#define glDisableClientState	dc_glDisableClientState
#define glDrawArrays		dc_glDrawArrays

#endif	/* DC */

#endif	/* DC_GL_COMPAT_H */

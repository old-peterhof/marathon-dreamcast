/*
	
	OpenGL Texture Manager,
	by Loren Petrich,
	March 12, 2000

	This implements texture handling for OpenGL.
	
	May 2, 2000:
	
	Fixed silhouette-texture bug: color 0 is now transparent
	
	May 24, 2000:
	
	Added support for setting landscape aspect ratios from outside;
	also added more graceful degradation for mis-sized textures.
	Walls must be a power of 2 horizontally and vertical;
	landscapes must be a power of 2 horizontally
	in order for the tiling to work properly.
	
	June 11, 2000:
	
	Added support for opacity shift factor (OpacityShift alongside OpacityScale);
	should be good for making dark colors somewhat opaque.

Jul 10, 2000:

	Fixed crashing bug when OpenGL is inactive with ResetTextures()

Sep 9, 2000:

	Restored old fix for AppleGL texturing as an option; this fix consists of setting
	the minimum size of a texture to be 128.

Nov 12, 2000 (Loren Petrich):
	Cleaned up some of the code to avoid explicit endianness usage;
	also implemented texture substitution.

Nov 18, 2000 (Loren Petrich):
	Added support for landscape vertical repeats;
	also added support for glow mapping of wall textures

Dec 16, 2000 (Loren Petrich):
	Fixed substitution of landscape textures

June 14, 2001 (Loren Petrich):
	Changed Width*Height to TxtrWidth*TxtrHeight in some places to ensure that some operations
	are done over complete textures
*/

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#include "cseries.h"

#ifdef HAVE_OPENGL

#ifdef __MVCPP__
#include <windows.h>
#endif

#if defined (__APPLE__) && defined (__MACH__)
# include <OpenGL/gl.h>
# include <OpenGL/glu.h>
#else
# include <GL/gl.h>
# include <GL/glu.h>
#endif

#ifdef mac
#include <agl.h>
#endif

#include "interface.h"
#include "render.h"
#include "map.h"
#include "collection_definition.h"
#ifdef DC
// For the GL_*_TEXTURE_MEMORY_KOS queries used in PlaceTexture.
#include <unistd.h>
#include <GL/glkos.h>
#include "dc_vq_sprites.h"
extern "C" int pvr_wait_ready(void);
extern "C" int pvr_wait_render_done(void);
#endif

#include "OGL_Setup.h"
#include "OGL_Render.h"
#include "OGL_Textures.h"

#ifdef DC
// Largest static texture rebuilt per frame (64x64 or 128x32); see PlaceTexture.
#ifndef STATIC_TEXEL_BUDGET
#define STATIC_TEXEL_BUDGET 4096
#endif
#endif

#ifdef DC
extern "C" void dc_trace(int slot, const char *fmt, ...);
extern "C" unsigned dc_heap_used(void);
#endif


// Texture mapping
struct TxtrTypeInfoData
{
	GLenum NearFilter;			// OpenGL parameter for near filter (GL_NEAREST, etc.)
	GLenum FarFilter;			// OpenGL parameter for far filter (GL_NEAREST, etc.)
	int Resolution;				// 0 is full-sized, 1 is half-sized, 2 is fourth-sized
	GLenum ColorFormat;			// OpenGL parameter for stored color format (RGBA8, etc.)
};


static TxtrTypeInfoData TxtrTypeInfoList[OGL_NUMBER_OF_TEXTURE_TYPES];


// Infravision: use algorithm (red + green + blue)/3 to compose intensity,
// then shade with these colors, one color for each collection.

struct InfravisionData
{
	GLfloat Red, Green, Blue;	// Infravision tint components: 0 to 1
	bool IsTinted;				// whether to use infravision with this collection
};

struct InfravisionData IVDataList[NUMBER_OF_COLLECTIONS] =
{
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false},
	{1,1,1,false}
};

// Is infravision currently active?
static bool InfravisionActive = false;


// Allocate some textures and indicate whether an allocation had happened.
bool TextureState::Allocate()
{
	if (!IsUsed)
	{
		glGenTextures(NUMBER_OF_TEXTURES,IDs);
		IsUsed = true;
		return true;
	}
	return false;
}

// Use a texture and indicate whether to load it
bool TextureState::Use(int Which)
{
	glBindTexture(GL_TEXTURE_2D,IDs[Which]);
	if (!IDsInUse[Which])
	{
		IDsInUse[Which] = true;
		return true;
	}
	return false;
}


// Resets the object's texture state
void TextureState::Reset()
{
	if (IsUsed)
	{
		glDeleteTextures(NUMBER_OF_TEXTURES,IDs);
		IsUsed = IsGlowing = IDsInUse[Normal] = IDsInUse[Glowing] = false;
	}
}


// Will distinguish by texture type as well as by collection;
// this is because different rendering modes deserve different treatment.
static CollBitmapTextureState* TextureStateSets[OGL_NUMBER_OF_TEXTURE_TYPES][MAXIMUM_COLLECTIONS];

#ifdef DC
/*
 * Keep a deliberate reserve rather than waiting for GLdc allocation failure.
 * A full-resolution raw fallback with a glow map can require hundreds of KiB
 * in one frame, and a full sky is 1 MB that has to be contiguous.
 *
 * Freeing a texture is safe once nothing can refer to its address. GLdc holds
 * a frame's polygons in RAM, texture addresses included, until glKosSwapBuffers
 * submits them; the PVR may still be rendering the scene before that one (KOS
 * double-buffers the lists). At the start of frame N the RAM lists are empty,
 * scene N-1 is submitted and scene N-2 may be rendering, so a texture last
 * bound in frame N-3 or earlier is referenced by neither: it can be freed
 * without waiting. That is the whole eviction path, and it runs at whatever
 * rate uploads need it.
 *
 * Moving textures is different: compaction relocates what the PVR is sampling.
 * It only runs when an upload was actually deferred for want of a contiguous
 * block, after pvr_wait_ready() and pvr_wait_render_done() (the first returns
 * once the last scene has *started* rendering; the second means idle). If the
 * pool still has no block that size, the request backs off for half a second
 * rather than waiting on the PVR every frame: in some levels the largest free
 * block never exceeds ~900 KB whatever is evicted, and doing this per frame
 * was 4-8 fps.
 *
 * GLdc's own defrag, the one it runs when an allocation fails mid-frame, has
 * neither guarantee: it updates texture objects but not the addresses already
 * in the RAM lists. The reserve exists so it never needs to run.
 */
static unsigned DC_TextureFrame = 1;
static const GLint DC_VRAM_LOW_WATER = 1024 * 1024;
static const GLint DC_VRAM_HIGH_WATER = 1536 * 1024;
static const unsigned DC_COMPACTION_BACKOFF_FRAMES = 15;
static GLint DC_RequestedTextureBytes = 0;
static unsigned DC_NextCompactionFrame = 0;

// GLdc's allocation fallback compacts immediately, even with polygons queued.
// Refuse that path; retry after the frame-boundary eviction/compaction instead.
bool OGL_TextureAllocationFits(unsigned bytes)
{
	GLint contiguous = 0;
	glGetIntegerv(GL_FREE_CONTIGUOUS_TEXTURE_MEMORY_KOS, &contiguous);
	if (bytes <= (unsigned)MAX(contiguous, 0)) return true;
	DC_RequestedTextureBytes = MAX(DC_RequestedTextureBytes, (GLint)bytes);
	return false;
}

void OGL_TextureFrameStart()
{
	++DC_TextureFrame;
	if (!DC_TextureFrame) DC_TextureFrame = 1;

	GLint FreeVRAM = 0;
	glGetIntegerv(GL_FREE_TEXTURE_MEMORY_KOS, &FreeVRAM);
	GLint Contiguous = 0;
	glGetIntegerv(GL_FREE_CONTIGUOUS_TEXTURE_MEMORY_KOS, &Contiguous);
	if (DC_RequestedTextureBytes && Contiguous >= DC_RequestedTextureBytes)
		DC_RequestedTextureBytes = 0;

	const GLint Required = MAX(DC_VRAM_LOW_WATER, DC_RequestedTextureBytes);
	const GLint Target = MAX(DC_VRAM_HIGH_WATER, Required);
	const bool NeedCompaction = DC_RequestedTextureBytes &&
	                            DC_TextureFrame >= DC_NextCompactionFrame;
	if (FreeVRAM >= Required && !NeedCompaction)
		return;

	const GLint Before = FreeVRAM;
	unsigned Evicted = 0;

	while (FreeVRAM < Target)
	{
		TextureState *Oldest = NULL;
		unsigned OldestAge = 0;

		for (int it=0; it<OGL_NUMBER_OF_TEXTURE_TYPES; ++it)
		{
			/* The sky is 1 MB that must be contiguous. Once it is in, it stays
			   for the level: re-uploading it into a fragmented pool fails, and
			   a sky drawn from an empty texture is VRAM garbage on hardware. */
			if (it == OGL_Txtr_Landscape) continue;
			for (int ic=0; ic<MAXIMUM_COLLECTIONS; ++ic)
			{
				/* HUD art is small, used outside the world pass, and pinned. */
				if (ic == _collection_interface) continue;
				if (!is_collection_present(ic)) continue;
				CollBitmapTextureState *Set = TextureStateSets[it][ic];
				if (!Set) continue;
				const int Count = get_number_of_collection_bitmaps(ic);
				for (int ib=0; ib<Count; ++ib)
					for (int is=0; is<NUMBER_OF_OPENGL_BITMAP_SETS; ++is)
					{
						TextureState& State = Set[ib].CTStates[is];
						if (!State.IsUsed) continue;
						const unsigned Age = DC_TextureFrame - State.LastUsedFrame;
						/* Bound in the last two frames: possibly still being rendered. */
						if (Age <= 2 || Age <= OldestAge) continue;
						Oldest = &State;
						OldestAge = Age;
					}
			}
		}

		if (!Oldest)
			break;
		Oldest->Reset();
		++Evicted;
		glGetIntegerv(GL_FREE_TEXTURE_MEMORY_KOS, &FreeVRAM);
	}

	bool Compacted = false;
	glGetIntegerv(GL_FREE_CONTIGUOUS_TEXTURE_MEMORY_KOS, &Contiguous);
	if (NeedCompaction && Contiguous < DC_RequestedTextureBytes)
	{
		dc_trace(53, "vram lru: need %d KB contiguous, have %d KB; waiting for PVR",
		         (int)(DC_RequestedTextureBytes/1024), (int)(Contiguous/1024));
		if (pvr_wait_ready() < 0 || pvr_wait_render_done() < 0)
		{
			/* Never move storage that a render may still be sampling. */
			dc_trace(53, "vram lru: PVR wait timed out; compaction deferred");
		}
		else
		{
			glDefragmentTextureMemory_KOS();
			glGetIntegerv(GL_FREE_CONTIGUOUS_TEXTURE_MEMORY_KOS, &Contiguous);
			Compacted = true;
		}
		if (Contiguous < DC_RequestedTextureBytes)
			DC_NextCompactionFrame = DC_TextureFrame + DC_COMPACTION_BACKOFF_FRAMES;
	}
	if (Contiguous >= DC_RequestedTextureBytes) DC_RequestedTextureBytes = 0;
	if (Evicted || Compacted)
		dc_trace(53, "vram lru: evicted %u%s, %d -> %d KB free, largest=%d KB",
		         Evicted, Compacted ? ", compacted" : "",
		         (int)(Before/1024), (int)(FreeVRAM/1024), (int)(Contiguous/1024));
}
#endif


// Initialize the texture accounting
void OGL_StartTextures()
{
	// Initialize the texture accounting proper
	for (int it=0; it<OGL_NUMBER_OF_TEXTURE_TYPES; it++)
		for (int ic=0; ic<MAXIMUM_COLLECTIONS; ic++)
		{
#ifdef DC
			// Most collections are only rendered through one texture type.
			// Allocate their accounting when that type first uses them.
			TextureStateSets[it][ic] = NULL;
#else
			bool CollectionPresent = is_collection_present(ic);
			short NumberOfBitmaps =
				CollectionPresent ? get_number_of_collection_bitmaps(ic) : 0;
			TextureStateSets[it][ic] =
				(CollectionPresent && NumberOfBitmaps) ?
					(new CollBitmapTextureState[NumberOfBitmaps]) : 0;
#endif
		}
	
	// Initialize the texture-type info
	const int NUMBER_OF_NEAR_FILTERS = 2;
	const GLenum NearFilterList[NUMBER_OF_NEAR_FILTERS] =
	{
		GL_NEAREST,
		GL_LINEAR
	};
	const int NUMBER_OF_FAR_FILTERS = 6;
	const GLenum FarFilterList[NUMBER_OF_FAR_FILTERS] =
	{
		GL_NEAREST,
		GL_LINEAR,
		GL_NEAREST_MIPMAP_NEAREST,
		GL_LINEAR_MIPMAP_NEAREST,
		GL_NEAREST_MIPMAP_LINEAR,
		GL_LINEAR_MIPMAP_LINEAR
	};
	const int NUMBER_OF_COLOR_FORMATS = 3;
	const GLenum ColorFormatList[NUMBER_OF_COLOR_FORMATS] = 
	{
		GL_RGBA8,
		GL_RGBA4,
		GL_RGBA2
	};
	
	OGL_ConfigureData& ConfigureData = Get_OGL_ConfigureData();
	
	for (int k=0; k<OGL_NUMBER_OF_TEXTURE_TYPES; k++)
	{
		OGL_Texture_Configure& TxtrConfigure = ConfigureData.TxtrConfigList[k];
		TxtrTypeInfoData& TxtrTypeInfo = TxtrTypeInfoList[k];
		
		byte NearFilter = TxtrConfigure.NearFilter;
		if (NearFilter < NUMBER_OF_NEAR_FILTERS)
			TxtrTypeInfo.NearFilter = NearFilterList[NearFilter];
		else
			TxtrTypeInfo.NearFilter = GL_NEAREST;
		
		byte FarFilter = TxtrConfigure.FarFilter;
		if (FarFilter < NUMBER_OF_FAR_FILTERS)
			TxtrTypeInfo.FarFilter = FarFilterList[FarFilter];
		else
			TxtrTypeInfo.FarFilter = GL_NEAREST;
		
		TxtrTypeInfo.Resolution = TxtrConfigure.Resolution;
		
		byte ColorFormat = TxtrConfigure.ColorFormat;
		if (ColorFormat < NUMBER_OF_COLOR_FORMATS)
			TxtrTypeInfo.ColorFormat = ColorFormatList[ColorFormat];
		else
			TxtrTypeInfo.ColorFormat = GL_RGBA8;
#ifdef DC
		dc_trace(37 + (k > 2 ? 2 : k),
		         "txtr[%d]: res=%d fmt=%04x far=%04x", k,
		         (int)TxtrTypeInfo.Resolution,
		         (unsigned)TxtrTypeInfo.ColorFormat,
		         (unsigned)TxtrTypeInfo.FarFilter);
#endif
	}
}


// Done with the texture accounting
void OGL_StopTextures()
{
	// Clear the texture accounting
	for (int it=0; it<OGL_NUMBER_OF_TEXTURE_TYPES; it++)
		for (int ic=0; ic<MAXIMUM_COLLECTIONS; ic++)
		{
			delete []TextureStateSets[it][ic];
			TextureStateSets[it][ic] = NULL;
		}
}


// Find an OpenGL-friendly color table from a Marathon shading table
static void FindOGLColorTable(int NumSrcBytes, byte *OrigColorTable, uint32 *ColorTable)
{
	// Stretch the original color table to 4 bytes per value for OpenGL convenience;
	// all the intermediate calculations will be done in RGBA 8888 form,
	// because that is what OpenGL prefers as a texture input
	switch(NumSrcBytes) {
	case 2:
		for (int k=0; k<MAXIMUM_SHADING_TABLE_INDEXES; k++)
		{
			byte *OrigPtr = OrigColorTable + NumSrcBytes*k;
			uint32 &Color = ColorTable[k];
			
			// Convert from ARGB 5551 to RGBA 8888; make opaque
			uint16 Intmd;
			uint8 *IntmdPtr = (uint8 *)(&Intmd);
			IntmdPtr[0] = OrigPtr[0];
			IntmdPtr[1] = OrigPtr[1];
			Color = Convert_16to32(Intmd);
		}
		break;
		
	case 4:
		for (int k=0; k<MAXIMUM_SHADING_TABLE_INDEXES; k++)
		{
			byte *OrigPtr = OrigColorTable + NumSrcBytes*k;
			uint32 &Color = ColorTable[k];
			
			// Convert from ARGB 8888 to RGBA 8888; make opaque
			uint8 *ColorPtr = (uint8 *)(&Color);
			ColorPtr[0] = OrigPtr[1];
			ColorPtr[1] = OrigPtr[2];
			ColorPtr[2] = OrigPtr[3];
			ColorPtr[3] = 0xff;
		}
		break;
	}
}


inline bool IsLandscapeFlatColored()
{
	OGL_ConfigureData& ConfigureData = Get_OGL_ConfigureData();
	return (TEST_FLAG(ConfigureData.Flags,OGL_Flag_FlatLand) != 0);
}


static void MakeAverage(int Length, GLuint *Buffer)
{
	float Sum[4];
	
	for (int q=0; q<4; q++) Sum[q] = 0;
	
	// Extract the bytes; the lowest byte gets done first
	for (int k=0; k<Length; k++)
	{
		GLuint PixVal = Buffer[k];
		for (int q=0; q<4; q++)
		{
			Sum[q] += (PixVal & 0x000000ff);
			PixVal >>= 8;
		}
	}
	
	// This processes the bytes from highest to lowest
	GLuint AvgVal = 0;
	for (int q=0; q<4; q++)
	{
		AvgVal <<= 8;	// Must come before adding in a byte
		AvgVal |= PIN(int(Sum[3-q]/Length + 0.5),0,255);
	}

	for (int k=0; k<Length; k++)
		Buffer[k] = AvgVal;
}

// Modify color-table index if necessary;
// makes it the infravision or silhouette one if necessary
short ModifyCLUT(short TransferMode, short CLUT)
{
	short CTable;
	
	// Tinted mode is only used for invisibility, and infravision will make objects visible
	if (TransferMode == _static_transfer) CTable = SILHOUETTE_BITMAP_SET;
	else if (TransferMode == _tinted_transfer) CTable = SILHOUETTE_BITMAP_SET;
	else if (InfravisionActive) CTable = INFRAVISION_BITMAP_SET;
	else CTable = CLUT;
	
	return CTable;
}

/*
	Routine for using some texture; it will load the texture if necessary.
	It parses a shape descriptor and checks on whether the collection's texture type
	is one of those given.
	It will check for more than one intended texture type,
	a convenience for multiple texture types sharing the same handling.
	
	It uses the transfer mode and the transfer data to work out
	what transfer modes to use (invisibility is a special case of tinted)
*/
bool TextureManager::Setup()
{

	// Parse the shape descriptor and check on whether the texture type
	// is the texture's intended type
	short CollColor = GET_DESCRIPTOR_COLLECTION(ShapeDesc);
	Collection = GET_COLLECTION(CollColor);
	CTable = ModifyCLUT(TransferMode,GET_COLLECTION_CLUT(CollColor));
	Frame = GET_DESCRIPTOR_SHAPE(ShapeDesc);
	Bitmap = get_bitmap_index(Collection,Frame);
	if (Bitmap == NONE) return false;
	
	// Get the texture-state info: first, per-collection, then per-bitmap
	CollBitmapTextureState *CBTSList = TextureStateSets[TextureType][Collection];
#ifdef DC
	if (!CBTSList && is_collection_present(Collection)) {
		const int Count = get_number_of_collection_bitmaps(Collection);
		if (Count <= 0 || Bitmap < 0 || Bitmap >= Count) return false;
		CBTSList = new CollBitmapTextureState[Count];
		TextureStateSets[TextureType][Collection] = CBTSList;
	}
#endif
	if (CBTSList == NULL) return false;
	CollBitmapTextureState& CBTS = CBTSList[Bitmap];
	
	// Get the control info for this texture type:
	TxtrTypeInfoData& TxtrTypeInfo = TxtrTypeInfoList[TextureType];
	
	// Get the rendering options for this texture:
	TxtrOptsPtr = OGL_GetTextureOptions(Collection,CTable,Bitmap);
	
	// Get the texture-state info: per-color-table -- be sure to preserve this for later
	// Set the texture ID, and load the texture if necessary
	// If "Use()" is true, then load, otherwise, assume the texture is loaded and skip
	TxtrStatePtr = &CBTS.CTStates[CTable];
	TextureState &CTState = *TxtrStatePtr;
#ifdef DC
	CTState.LastUsedFrame = DC_TextureFrame;
	/*
	 *	Static -- a teleporting object, the compiler's shot landing -- is a
	 *	texture whose opaque texels are noise, rebuilt every frame so the noise
	 *	moves (see StoreSourceRow). The PowerVR has no logic ops or stipple, so
	 *	the desktop renderer's flicker has to live in the texels.
	 *
	 *	The texture object is kept and re-uploaded in place, not deleted and
	 *	recreated: glTexImage2D onto an existing GLdc texture of the same size
	 *	writes into the same VRAM block, so nothing is freed while a polygon
	 *	already queued this frame -- another object using this bitmap, say --
	 *	still holds the address. A frame that is still sampling the block gets
	 *	the next frame's noise, which in a noise sprite is not visible. No
	 *	deletes, so no waiting on the PVR at the frame boundary either.
	 */
	StaticNoise = (TransferMode == _static_transfer);
	if (StaticNoise && CTState.IsUsed)
	{
		CTState.IDsInUse[TextureState::Normal] = false;
		CTState.IDsInUse[TextureState::Glowing] = false;
	}
#endif
	if (!CTState.IsUsed
#ifdef DC
	    || !CTState.IDsInUse[TextureState::Normal]
	    || (CTState.IsGlowing && !CTState.IDsInUse[TextureState::Glowing])
#endif
	   )
	{
		// Initial sprite scale/offset
		U_Scale = V_Scale = 1;
		U_Offset = V_Offset = 0;
		
		// Try to load a substitute texture, and if that fails,
		// get the geometry from the shapes bitmap.
		const bool UsedSubstitute = LoadSubstituteTexture();
		if (!UsedSubstitute)
			if (!SetupTextureGeometry()) return false;

#ifdef DC
		/*
		 * The offline pack contains stock, crisp object/scenery pixels only.
		 * Dynamic infravision/silhouette tables, MML alpha effects and
		 * substitute art must continue through the ordinary RGBA path.
		 * A missing pack key is harmless: PlaceTexture falls back to Buffer.
		 */
		UseVQPack = !UsedSubstitute &&
			(TextureType == OGL_Txtr_Inhabitant ||
			 TextureType == OGL_Txtr_WeaponsInHand) &&
			Collection != _collection_interface &&
			CTable >= 0 && CTable < MAXIMUM_CLUTS_PER_COLLECTION &&
			TxtrOptsPtr->OpacityType == OGL_OpacType_Crisp;
#endif
				
		// Store sprite scale/offset
		CBTS.U_Scale = U_Scale;
		CBTS.V_Scale = V_Scale;
		CBTS.U_Offset = U_Offset;
		CBTS.V_Offset = V_Offset;
		
		// This finding of color tables sets the glow state
		FindColorTables();
		// Override if textures had been substituted;
		// if the normal texture had been substituted, it will be assumed to be
		// non-glowing unless the glow texture has also been substituted.
		if (GlowBuffer) IsGlowing = true;
		else if (NormalBuffer) IsGlowing = false;
		
		CTState.IsGlowing = IsGlowing;
		
		// Load the fake landscape if selected
		if (TextureType == OGL_Txtr_Landscape)
		{
			if (IsLandscapeFlatColored())
			{
				if (NormalBuffer) delete []NormalBuffer;
				NormalBuffer = GetFakeLandscape();
			}
		}
		
		// Display size: may be shrunk
#ifdef DC
		// Static is rebuilt every frame, so its cost is bounded by texel
		// count, not by a fixed shrink: small sprites keep their resolution,
		// large ones drop to a quarter. The PowerVR's smallest texture is 8x8.
		int TxtrRes = TxtrTypeInfo.Resolution;
		if (StaticNoise)
			for (TxtrRes = 0; TxtrRes < 2 && (TxtrWidth >> (TxtrRes+1)) >= 8 && (TxtrHeight >> (TxtrRes+1)) >= 8
			     && (TxtrWidth >> TxtrRes) * (TxtrHeight >> TxtrRes) > STATIC_TEXEL_BUDGET; TxtrRes++) {}
#else
		const int TxtrRes = TxtrTypeInfo.Resolution;
#endif
		LoadedWidth = MAX(TxtrWidth >> TxtrRes, 1);
		LoadedHeight = MAX(TxtrHeight >> TxtrRes, 1);

#ifdef DC
		/*
		 * Do not allocate a full RGBA staging image merely to discard it after
		 * uploading the pre-encoded VQ payload.  Require every texture this
		 * state needs up front; an absent normal or glow entry keeps both on the
		 * ordinary path.  PlaceTexture still builds the buffer lazily if a later
		 * disc read or compressed upload fails.
		 */
		if (UseVQPack)
			UseVQPack =
				dc_vq_sprite_available(Collection, CTable, Bitmap, false,
				                       LoadedWidth, LoadedHeight) &&
				(!IsGlowing ||
				 dc_vq_sprite_available(Collection, CTable, Bitmap, true,
				                        LoadedWidth, LoadedHeight));
#endif
		
		// If not, then load the expected textures.
		//
		// GetOGLTexture builds each buffer at LoadedWidth x LoadedHeight
		// directly -- expansion and reduction are one pass now, so there is no
		// full-size intermediate to shrink afterwards and no second buffer to
		// hold it. See the note above FlushAccumRow. This used to be the peak
		// allocation of the whole level load: a 1024x512 landscape needed 2MB
		// for the full-size image plus another 512KB for the reduced one, and
		// on a 16MB machine that was the difference between fitting and not.
		if (!NormalBuffer
#ifdef DC
		    && !UseVQPack
#endif
		   )
			NormalBuffer = GetOGLTexture(NormalColorTable);
		
		if (IsGlowing && !GlowBuffer
#ifdef DC
		    && !UseVQPack
#endif
		   )
			GlowBuffer = GetOGLTexture(GlowColorTable);
		
		// Kludge for making top and bottom look flat
		/*
		if (TextureType == OGL_Txtr_Landscape)
		{
			MakeAverage(LoadedWidth,NormalBuffer);
			MakeAverage(LoadedWidth,NormalBuffer+LoadedWidth*(LoadedHeight-1));
		}
		*/
	}
	else
	{
		// Get sprite scale/offset
		U_Scale = CBTS.U_Scale;
		V_Scale = CBTS.V_Scale;
		U_Offset = CBTS.U_Offset;
		V_Offset = CBTS.V_Offset;
		
		// Get glow state
		IsGlowing = CTState.IsGlowing;
	}
		
	// Done!!!
	return true;
}


// Next power of 2; since OpenGL prefers powers of 2, it is necessary to work out
// the next one up for each texture dimension.
inline int NextPowerOfTwo(int n)
{
	int p = 1;
	while(p < n) {p <<= 1;}
	return p;
}


inline bool WhetherTextureFix()
{
	OGL_ConfigureData& ConfigureData = Get_OGL_ConfigureData();
	return (TEST_FLAG(ConfigureData.Flags,OGL_Flag_TextureFix) != 0);
}


// Conversion of color data types

inline int MakeEightBit(GLfloat Chan)
{
	return int(PIN(int(255*Chan+0.5),0,255));
}

uint32 MakeIntColor(GLfloat *FloatColor)
{
	uint32 IntColor;
	uint8 *ColorPtr = (uint8 *)(&IntColor);
	for (int k=0; k<4; k++)
		ColorPtr[k] = MakeEightBit(FloatColor[k]);
	return IntColor;
}

void MakeFloatColor(uint32 IntColor, GLfloat *FloatColor)
{
	uint8 *ColorPtr = (uint8 *)(&IntColor);
	for (int k=0; k<4; k++)
		FloatColor[k] = float(ColorPtr[k])/float(255);
}



bool TextureManager::LoadSubstituteTexture()
{
	// Is there a texture to be substituted?
	ImageDescriptor& NormalImg = TxtrOptsPtr->NormalImg;
	if (!NormalImg.IsPresent()) return false;
	
	// Be sure to take care of the glowing version, where supported
	ImageDescriptor& GlowImg = TxtrOptsPtr->GlowImg;
	
	// Idiot-proofing
	if (NormalBuffer)
	{
		delete []NormalBuffer;
		NormalBuffer = NULL;
	}
	if (GlowBuffer)
	{
		delete []GlowBuffer;
		GlowBuffer = NULL;
	}
	
	int Width = NormalImg.GetWidth();
	int Height = NormalImg.GetHeight();
	
	int HeightOffset;
	int OrigHeightOffset, OGLHeightOffset, CopyingHeight;
	
	switch(TextureType)
	{
	case OGL_Txtr_Wall:
		// For tiling to be possible, the width and height must be powers of 2;
		// also, be sure to transpose the texture
		TxtrWidth = Height;
		TxtrHeight = Width;
		if (TxtrWidth != NextPowerOfTwo(TxtrWidth)) return false;
		if (TxtrHeight != NextPowerOfTwo(TxtrHeight)) return false;
		
		NormalBuffer = new uint32[TxtrWidth*TxtrHeight];
		for (int v=0; v<Height; v++)
			for (int h=0; h<Width; h++)
				NormalBuffer[h*Height+v] = NormalImg.GetPixel(h,v);
		
		// Walls can glow...
		if (GlowImg.IsPresent())
		{
			GlowBuffer = new uint32[TxtrWidth*TxtrHeight];
			for (int v=0; v<Height; v++)
				for (int h=0; h<Width; h++)
					GlowBuffer[h*Height+v] = GlowImg.GetPixel(h,v);
		}
		
		break;
	
	case OGL_Txtr_Landscape:
		// For tiling to be possible, the width must be a power of 2;
		// the height need not be such a power.
		// Also, flip the vertical dimension to get the orientation correct.
		// Some of this code is cribbed from some other code here in order to get
		// consistent landscape loading
		TxtrWidth = Width;
		TxtrHeight = (Landscape_AspRatExp >= 0) ?
			(TxtrWidth >> Landscape_AspRatExp) :
				(TxtrWidth << (-Landscape_AspRatExp));
		if (TxtrWidth != NextPowerOfTwo(TxtrWidth)) return false;
		
		NormalBuffer = new uint32[TxtrWidth*TxtrHeight];
		memset(NormalBuffer,0,TxtrWidth*TxtrHeight*sizeof(uint32));
		
		// only valid source and destination pixels
		// will get worked with (no off-edge ones, that is).
		HeightOffset = (TxtrHeight - Height) >> 1;
		if (HeightOffset >= 0)
		{
			CopyingHeight = Height;
			OrigHeightOffset = 0;
			OGLHeightOffset = HeightOffset;
		}
		else
		{
			CopyingHeight = TxtrHeight;
			OrigHeightOffset = - HeightOffset;
			OGLHeightOffset = 0;
		}
		
		for (int v=0; v<CopyingHeight; v++)
		{
			// This optimization is reasonable because both the read-in-image
			// and the OpenGL-texture arrays have the same width/height arrangement
			uint32 *Src = &NormalImg.GetPixel(0,OrigHeightOffset+v);
			uint32 *Dest = NormalBuffer + (OGLHeightOffset + ((CopyingHeight-1)-v))*Width;
			memcpy(Dest,Src,Width*sizeof(uint32));
		}
		
		// No glow map here
		break;
		
	case OGL_Txtr_Inhabitant:
	case OGL_Txtr_WeaponsInHand:
		// Much of the code here has been copied from elsewhere.
		// Set these for convenience; sprites are transposed, as walls are.
		BaseTxtrWidth = Height;
		BaseTxtrHeight = Width;
		
		// The 2 here is so that there will be an empty border around a sprite,
		// so that the texture can be conveniently mipmapped.
		TxtrWidth = NextPowerOfTwo(BaseTxtrWidth+2);
		TxtrHeight = NextPowerOfTwo(BaseTxtrHeight+2);
		
		// This kludge no longer necessary
		// Restored due to some people still having AppleGL 1.1.2
		if (WhetherTextureFix())
		{
			TxtrWidth = MAX(TxtrWidth,128);
			TxtrHeight = MAX(TxtrHeight,128);
		}
					
		// Offsets
		WidthOffset = (TxtrWidth - BaseTxtrWidth) >> 1;
		HeightOffset = (TxtrHeight - BaseTxtrHeight) >> 1;
		
		// We can calculate the scales and offsets here
		double TWidRecip = 1/double(TxtrWidth);
		double THtRecip = 1/double(TxtrHeight);
		U_Scale = TWidRecip*double(BaseTxtrWidth);
		U_Offset = TWidRecip*WidthOffset;
		V_Scale = THtRecip*double(BaseTxtrHeight);
		V_Offset = THtRecip*HeightOffset;
		
		NormalBuffer = new uint32[TxtrWidth*TxtrHeight];
		objlist_clear(NormalBuffer,TxtrWidth*TxtrHeight);
		for (int v=0; v<Height; v++)
			for (int h=0; h<Width; h++)
				NormalBuffer[(HeightOffset+h)*TxtrWidth+(WidthOffset+v)] = NormalImg.GetPixel(h,v);
		
		// Objects can glow...
		if (GlowImg.IsPresent())
		{
			GlowBuffer = new uint32[TxtrWidth*TxtrHeight];
			objlist_clear(GlowBuffer,TxtrWidth*TxtrHeight);
			for (int v=0; v<Height; v++)
				for (int h=0; h<Width; h++)
					GlowBuffer[(HeightOffset+h)*TxtrWidth+(WidthOffset+v)] = GlowImg.GetPixel(h,v);
		}
		break;
	}
	
	// Use the Tomb Raider opacity hack if selected
	SetPixelOpacities(*TxtrOptsPtr,TxtrWidth*TxtrHeight,NormalBuffer);
	
	// Modify if infravision is active
	if (CTable == INFRAVISION_BITMAP_SET)
	{
		if (NormalBuffer)
		{
			for (int k=0; k<TxtrWidth*TxtrHeight; k++)
			{
				uint32& IntPxl = NormalBuffer[k];
				GLfloat FloatPxl[4];
				MakeFloatColor(IntPxl,FloatPxl);
				FindInfravisionVersion(Collection,FloatPxl);
				IntPxl = MakeIntColor(FloatPxl);
			}
		}
		// Infravision textures don't glow
		if (GlowBuffer)
		{
			delete []GlowBuffer;
			GlowBuffer = NULL;
		}
	}
	else if (CTable == SILHOUETTE_BITMAP_SET)
	{
		if (NormalBuffer)
		{
			for (int k=0; k<TxtrWidth*TxtrHeight; k++)
			{
				// Make the color white, but keep the opacity
				uint8 *PxlPtr = (uint8 *)(NormalBuffer + k);
				PxlPtr[0] = PxlPtr[1] = PxlPtr[2] = 0xff;
			}
		}
		// Silhouette textures don't glow
		if (GlowBuffer)
		{
			delete []GlowBuffer;
			GlowBuffer = NULL;
		}
	}
	return true;
}

bool TextureManager::SetupTextureGeometry()
{	
	// How many rows (scanlines) and columns
	if (Texture->flags&_COLUMN_ORDER_BIT)
	{
		BaseTxtrWidth = Texture->height;
		BaseTxtrHeight = Texture->width;
	}
	else
	{
		BaseTxtrWidth = Texture->width;
		BaseTxtrHeight = Texture->height;
	}
	
	short RowBytes = Texture->bytes_per_row;
	if (RowBytes != NONE)
		if (BaseTxtrWidth != RowBytes) return false;
	
	// The default
	WidthOffset = HeightOffset = 0;
	
	switch(TextureType)
	{
	case OGL_Txtr_Wall:
		// For tiling to be possible, the width and height must be powers of 2
		TxtrWidth = BaseTxtrWidth;
		TxtrHeight = BaseTxtrHeight;
		if (TxtrWidth != NextPowerOfTwo(TxtrWidth)) return false;
		if (TxtrHeight != NextPowerOfTwo(TxtrHeight)) return false;
		break;
		
	case OGL_Txtr_Landscape:
		if (IsLandscapeFlatColored())
		{
			TxtrWidth = 128;
			TxtrHeight = 128;
		}
		else
		{
			// Width is horizontal direction here
			TxtrWidth = BaseTxtrWidth;
			if (TxtrWidth != NextPowerOfTwo(TxtrWidth)) return false;
			// Use the landscape height here
			TxtrHeight = (Landscape_AspRatExp >= 0) ?
				(TxtrWidth >> Landscape_AspRatExp) :
					(TxtrWidth << (-Landscape_AspRatExp));
			
			// Offsets
			WidthOffset = (TxtrWidth - BaseTxtrWidth) >> 1;
			HeightOffset = (TxtrHeight - BaseTxtrHeight) >> 1;
		}
		
		break;
		
	case OGL_Txtr_Inhabitant:
	case OGL_Txtr_WeaponsInHand:
		{			
			// The 2 here is so that there will be an empty border around a sprite,
			// so that the texture can be conveniently mipmapped.
			TxtrWidth = NextPowerOfTwo(BaseTxtrWidth+2);
			TxtrHeight = NextPowerOfTwo(BaseTxtrHeight+2);
			
			// This kludge no longer necessary
			// Restored due to some people still having AppleGL 1.1.2
			if (WhetherTextureFix())
			{
				TxtrWidth = MAX(TxtrWidth,128);
				TxtrHeight = MAX(TxtrHeight,128);
			}
						
			// Offsets
			WidthOffset = (TxtrWidth - BaseTxtrWidth) >> 1;
			HeightOffset = (TxtrHeight - BaseTxtrHeight) >> 1;
			
			// We can calculate the scales and offsets here
			double TWidRecip = 1/double(TxtrWidth);
			double THtRecip = 1/double(TxtrHeight);
			U_Scale = TWidRecip*double(BaseTxtrWidth);
			U_Offset = TWidRecip*WidthOffset;
			V_Scale = THtRecip*double(BaseTxtrHeight);
			V_Offset = THtRecip*HeightOffset;
		}
		break;
	}
	
	// Success!
	return true;
}


void TextureManager::FindColorTables()
{
	// Default
	IsGlowing = false;
	
	// The silhouette case is easy
	if (CTable == SILHOUETTE_BITMAP_SET)
	{
		NormalColorTable[0] = 0;
		for (int k=1; k<MAXIMUM_SHADING_TABLE_INDEXES; k++)
			NormalColorTable[k] = 0xffffffff;
		return;
	}

	// Interface collection? Then use the CLUT directly
	if (Collection == 0) {
		int num_colors;
		struct rgb_color_value *q = get_collection_colors(Collection, CTable, num_colors);
		uint8 *p = (uint8 *)NormalColorTable;
		for (int k=0; k<num_colors; k++) {
			int idx = q[k].value;
			p[idx * 4 + 0] = q[k].red >> 8;
			p[idx * 4 + 1] = q[k].green >> 8;
			p[idx * 4 + 2] = q[k].blue >> 8;
			p[idx * 4 + 3] = 0xff;
		}
		SetPixelOpacities(*TxtrOptsPtr, MAXIMUM_SHADING_TABLE_INDEXES, NormalColorTable);
		NormalColorTable[0] = 0;
		return;
	}
	
	// Number of source bytes, for reading off of the shading table
	short NumSrcBytes = bit_depth / 8;
	
	// Shadeless polygons use the first, instead of the last, shading table
	byte *OrigColorTable = (byte *)ShadingTables;
	byte *OrigGlowColorTable = OrigColorTable;
	if (!IsShadeless) OrigColorTable +=
		NumSrcBytes*(number_of_shading_tables - 1)*MAXIMUM_SHADING_TABLE_INDEXES;
	
	// Find the normal color table,
	// and set its opacities as if there was no glow table.
	FindOGLColorTable(NumSrcBytes,OrigColorTable,NormalColorTable);
	SetPixelOpacities(*TxtrOptsPtr,MAXIMUM_SHADING_TABLE_INDEXES,NormalColorTable);
	
	// Find the glow-map color table;
	// only inhabitants are glowmapped.
	// Also, it seems that only infravision textures are shadeless.
	if (!IsShadeless && (TextureType != OGL_Txtr_Landscape))
	{
		// Find the glow table from the lowest-illumination color table
		FindOGLColorTable(NumSrcBytes,OrigGlowColorTable,GlowColorTable);
		
		// Search for self-luminous colors; ignore the first one as the transparent one
		for (int k=1; k<MAXIMUM_SHADING_TABLE_INDEXES; k++)
		{
			// Check for illumination-independent colors
			uint8 *NormalEntry = (uint8 *)(NormalColorTable + k);
			uint8 *GlowEntry = (uint8 *)(GlowColorTable + k);
			
			bool EntryIsGlowing = false;
			for (int q=0; q<3; q++)
				if (GlowEntry[q] >= 0x0f) EntryIsGlowing = true;
			
			// Make the glow color the original color, to get continuity
			for (int q=0; q<3; q++)
				GlowEntry[q] = NormalEntry[q];
			
			if (EntryIsGlowing && NormalEntry[3])
			{
				IsGlowing = true;
				// Make half-opaque, to get more like the software rendering
				float Opacity = NormalEntry[3]/float(0xff);
				NormalEntry[3] = MakeEightBit(Opacity/(2-Opacity));
				GlowEntry[3] = MakeEightBit(Opacity/2);
			}
			else
			{
				// Make transparent, to get appropriate continuity
				GlowEntry[3] = 0;
			}
		}
	}
		
	// The first color is always the transparent color,
	// except if it is a landscape color
	if (TextureType != OGL_Txtr_Landscape)
		{NormalColorTable[0] = 0; GlowColorTable[0] = 0;}	
}


/*
 *	Building a texture straight into its loaded size.
 *
 *	This used to be two whole-image passes over two heap buffers: expand every
 *	index through the CLUT into a full-size 32-bit image, then box-filter that
 *	down into a second, smaller one. At half resolution -- which is what the
 *	Dreamcast runs -- the two buffers alive at once came to five bytes for every
 *	pixel of the full-size image, and a 1024x512 landscape is 2.6MB of that. On a
 *	16MB machine it is the difference between fitting and not.
 *
 *	Nothing required the full-size image to exist. The box filter reduces by an
 *	exact power of two, so each destination pixel covers a fixed block of source
 *	pixels, and the source is already in memory as the shape's own 8-bit rows.
 *	So the expansion and the filter fold into one pass that writes the reduced
 *	image directly, and the peak drops to one byte per full-size pixel.
 *
 *	The filter has to run in 8-bit colour, before any packing, or the averaging
 *	compounds whatever quantisation the pack introduces.
 *
 *	Source rows arrive in increasing order, so only one destination row is ever
 *	being accumulated: a few kilobytes, not an image. Colour is weighted by alpha
 *	for the reason set out at the top of dc/dc_glu.c -- index 0 is transparent
 *	*black*, and letting it vote on colour puts a dark fringe around every sprite.
 *	Alpha itself is divided by the full block size, not by the number of source
 *	pixels that happened to be written, because the pixels that were never
 *	written are transparent and have to count as such.
 */

static void FlushAccumRow(uint32 *Buffer, int LoadedW, int Row,
	const uint32 *Acc, unsigned BlockPixels)
{
	uint8 *Dest = (uint8 *)(Buffer + (size_t)Row*LoadedW);
	
	for (int x=0; x<LoadedW; x++, Dest+=4)
	{
		uint32 A = Acc[4*x+3];
		
		if (A)
		{
			Dest[0] = uint8(Acc[4*x+0]/A);
			Dest[1] = uint8(Acc[4*x+1]/A);
			Dest[2] = uint8(Acc[4*x+2]/A);
		}
		else
		{
			// Nothing opaque in the block; there is no colour to keep.
			Dest[0] = Dest[1] = Dest[2] = 0;
		}
		
		Dest[3] = uint8(A/BlockPixels);
	}
}


// One source row, either copied straight across or folded into the accumulator.
#ifdef DC
/*
 *	One texel of static. The generator's high half decides whether this texel
 *	shows noise or the dark silhouette -- the rule the stipple version used, so
 *	a teleport dissolves at the same rate -- and its low bits are the colour.
 *	Alpha is the top byte: RGBA bytes in memory, little-endian.
 */
static inline uint32 StaticTexel(int Density)
{
	static uint32 s = 0x9E3779B9u;
	s ^= s << 13; s ^= s >> 17; s ^= s << 5;
	return ((s >> 16) >= (uint32)Density) ? (0xFF000000u | (s & 0x00FFFFFFu)) : 0xFF000000u;
}
#endif

static void StoreSourceRow(uint32 *Buffer, uint32 *Acc, int& AccRow,
	int LoadedW, int LoadedH, int XShift, int YShift,
	unsigned BlockPixels, bool Reduce,
	int oy, int ox, int Count, const byte *Src, const uint32 *ColorTable,
	bool Packed, int StaticDensity)
{
	if (!Reduce)
	{
#ifdef DC
		if (StaticDensity >= 0) {
			uint32 *Dest = Buffer + (size_t)oy*LoadedW + ox;
			for (int w=0; w<Count; ++w)
				*(Dest++) = (ColorTable[*(Src++)] & 0xFF000000u) ? StaticTexel(StaticDensity) : 0;
			return;
		}
#endif
		if (Packed) {
			uint16 *Dest = (uint16 *)Buffer + (size_t)oy*LoadedW + ox;
			for (int w=0; w<Count; ++w) {
				const uint8 *p = (const uint8 *)&ColorTable[*(Src++)];
				// Exactly GLdc's RGBA8888 -> ARGB4444 conversion.
				*(Dest++) = ((p[3]&0xf0)<<8) | ((p[0]&0xf0)<<4) |
				             (p[1]&0xf0) | (p[2]>>4);
			}
			return;
		}
		uint32 *Dest = Buffer + (size_t)oy*LoadedW + ox;
		for (int w=0; w<Count; w++)
			*(Dest++) = ColorTable[*(Src++)];
		return;
	}
	
	int dy = oy >> YShift;
	if (dy >= LoadedH) dy = LoadedH - 1;
	
#ifdef DC
	if (StaticDensity >= 0) {
		// Noise is not averaged: one source texel per destination texel
		// decides whether it is part of the shape at all.
		if (oy & ((1 << YShift) - 1)) return;
		uint32 *Row = Buffer + (size_t)dy*LoadedW;
		for (int w=0; w<Count; w += (1 << XShift)) {
			int dx = (ox + w) >> XShift;
			if (dx >= LoadedW) dx = LoadedW - 1;
			Row[dx] = (ColorTable[Src[w]] & 0xFF000000u) ? StaticTexel(StaticDensity) : 0;
		}
		return;
	}
#endif
	if (dy != AccRow)
	{
		if (AccRow >= 0)
			FlushAccumRow(Buffer,LoadedW,AccRow,Acc,BlockPixels);
		memset(Acc,0,sizeof(uint32)*4*LoadedW);
		AccRow = dy;
	}
	
	for (int w=0; w<Count; w++)
	{
		uint32 Color = ColorTable[*(Src++)];
		const uint8 *p = (const uint8 *)&Color;
		unsigned Alpha = p[3];
		
		int dx = (ox + w) >> XShift;
		if (dx >= LoadedW) dx = LoadedW - 1;
		
		uint32 *a = Acc + 4*dx;
		a[0] += p[0]*Alpha;
		a[1] += p[1]*Alpha;
		a[2] += p[2]*Alpha;
		a[3] += Alpha;
	}
}


uint32 *TextureManager::GetOGLTexture(uint32 *ColorTable)
{
	// The image is built at its loaded size, not at full size and then reduced;
	// see the note above FlushAccumRow.
	const int LoadedW = MAX(int(LoadedWidth),1);
	const int LoadedH = MAX(int(LoadedHeight),1);
	
	int XShift = 0;
	while ((int(TxtrWidth)  >> XShift) > LoadedW) XShift++;
	int YShift = 0;
	while ((int(TxtrHeight) >> YShift) > LoadedH) YShift++;
	
	const bool Reduce = (XShift != 0 || YShift != 0);
	bool Packed = false;
#ifdef DC
	// A stock full-size sky needs no RGBA staging image. Preserve every texel
	// in the same 16-bit representation the PVR would receive from GLdc.
	Packed = PackedLandscape = !Reduce && TextureType == OGL_Txtr_Landscape;
	const int StaticDensity = StaticNoise ? (int)(uint16)TransferData : -1;
#else
	const int StaticDensity = -1;
#endif
	const unsigned BlockPixels = (1u << XShift) << YShift;
	
	// Allocate and set to black and transparent
	int NumPixels = LoadedW*LoadedH;
	const int Words = Packed ? (NumPixels+1)/2 : NumPixels;
	uint32 *Buffer = new uint32[Words];
	objlist_clear(Buffer,Words);
	
	// Accumulator for the one destination row currently being built.
	uint32 *Acc = Reduce ? new uint32[4*LoadedW] : NULL;
	int AccRow = -1;
	
	// The dimension, the offset in the original texture, and the offset in the OpenGL texture
	short Width, OrigWidthOffset, OGLWidthOffset;
	short Height, OrigHeightOffset, OGLHeightOffset;
	
	// Calculate original-texture and OpenGL-texture offsets
	// and how many scanlines to do.
	// The loop start points and counts are set so that
	// only valid source and destination pixels
	// will get worked with (no off-edge ones, that is).
	if (HeightOffset >= 0)
	{
		Height = BaseTxtrHeight;
		OrigHeightOffset = 0;
		OGLHeightOffset = HeightOffset;
	}
	else
	{
		Height = TxtrHeight;
		OrigHeightOffset = - HeightOffset;
		OGLHeightOffset = 0;
	}

	if (Texture->bytes_per_row == NONE)
	{
		short horig = OrigHeightOffset;
		for (short h=0; h<Height; h++)
		{
			byte *OrigStrip = Texture->row_addresses[horig];

			// Cribbed from textures.c:
			// This is the Marathon 2 sprite-interpretation scheme;
			// assumes big-endian data
				
			// First destination location
			uint16 First = uint16(*(OrigStrip++)) << 8;
			First |= uint16(*(OrigStrip++));
			// Last destination location (last pixel is just before it)
			uint16 Last = uint16(*(OrigStrip++)) << 8;
			Last |= uint16(*(OrigStrip++));
			
			// Calculate original-texture and OpenGL-texture offsets
			// and how many pixels to do
			OrigWidthOffset = 0;
			OGLWidthOffset = WidthOffset + First;
			
			if (OGLWidthOffset < 0)
			{
				OrigWidthOffset -= OGLWidthOffset;
				OGLWidthOffset = 0;
			}
			
			short OrigWidthFinish = Last - First;
			short OGLWidthFinish = WidthOffset + Last;
			
			short OGLWidthExcess = OGLWidthFinish - TxtrWidth;
			if (OGLWidthExcess > 0)
			{
				OrigWidthFinish -= OGLWidthExcess;
				OGLWidthFinish = TxtrWidth;
			}
			
			short Width = OrigWidthFinish - OrigWidthOffset;
			OrigStrip += OrigWidthOffset;
			
			StoreSourceRow(Buffer,Acc,AccRow,LoadedW,LoadedH,XShift,YShift,
				BlockPixels,Reduce,OGLHeightOffset+h,OGLWidthOffset,Width,
				OrigStrip,ColorTable,Packed,StaticDensity);
			horig++;
		}
	}
	else
	{
		// Calculate original-texture and OpenGL-texture offsets
		// and how many pixels to do
		if (WidthOffset >= 0)
		{
			Width = BaseTxtrWidth;
			OrigWidthOffset = 0;
			OGLWidthOffset = WidthOffset;
		}
		else
		{
			Width = TxtrWidth;
			OrigWidthOffset = - WidthOffset;
			OGLWidthOffset = 0;
		}
		short horig = OrigHeightOffset;
		for (short h=0; h<Height; h++)
		{
			byte *OrigStrip = Texture->row_addresses[horig] + OrigWidthOffset;
			StoreSourceRow(Buffer,Acc,AccRow,LoadedW,LoadedH,XShift,YShift,
				BlockPixels,Reduce,OGLHeightOffset+h,OGLWidthOffset,Width,
				OrigStrip,ColorTable,Packed,StaticDensity);
			horig++;
		}
	}
	
	if (Reduce)
	{
		if (AccRow >= 0)
			FlushAccumRow(Buffer,LoadedW,AccRow,Acc,BlockPixels);
		delete []Acc;
	}
	
	return Buffer;
}


uint32 *TextureManager::GetFakeLandscape()
{
	// Allocate and set to black and transparent
	int NumPixels = int(TxtrWidth)*int(TxtrHeight);
	uint32 *Buffer = new uint32[NumPixels];
	objlist_clear(Buffer,NumPixels);
	
	// Set up land and sky colors;
	// be sure to idiot-proof out-of-range ones
	OGL_ConfigureData& ConfigureData = Get_OGL_ConfigureData();
	int LscpIndx = static_world->song_index;
	if (!LandscapesLoaded || (LscpIndx < 0 && LscpIndx >= 4))
	{
		memset(Buffer,0,NumPixels*sizeof(uint32));
		return Buffer;
	}
	
	RGBColor OrigLandColor = ConfigureData.LscpColors[LscpIndx][0];
	RGBColor OrigSkyColor = ConfigureData.LscpColors[LscpIndx][1];
	
	// Set up floating-point ones, complete with alpha channel
	GLfloat LandColor[4], SkyColor[4];
	MakeFloatColor(OrigLandColor,LandColor);
	LandColor[3] = 1;
	MakeFloatColor(OrigSkyColor,SkyColor);
	SkyColor[3] = 1;
	
	// Modify if infravision is active
	if (CTable == INFRAVISION_BITMAP_SET)
	{
		FindInfravisionVersion(Collection,LandColor);
		FindInfravisionVersion(Collection,SkyColor);
	}
	
	uint32 TxtrLandColor = MakeIntColor(LandColor);
	uint32 TxtrSkyColor = MakeIntColor(SkyColor);
	
	// Textures' vertical dimension is upward;
	// put in the land after the sky
	uint32 *BufPtr = Buffer;
	for (int h=0; h<TxtrHeight/2; h++)
		for (int w=0; w<TxtrWidth; w++)
			*(BufPtr++) = TxtrLandColor;
	for (int h=0; h<TxtrHeight/2; h++)
		for (int w=0; w<TxtrWidth; w++)
			*(BufPtr++) = TxtrSkyColor;
		
	return Buffer;
}


uint32 *TextureManager::Shrink(uint32 *Buffer)
{
	int NumPixels = int(LoadedWidth)*int(LoadedHeight);
	GLuint *NewBuffer = new GLuint[NumPixels];
	gluScaleImage(GL_RGBA, TxtrWidth, TxtrHeight, GL_UNSIGNED_BYTE, Buffer,
		LoadedWidth, LoadedHeight, GL_UNSIGNED_BYTE, NewBuffer);
	
	return (uint32 *)NewBuffer;
}


// This places a texture into the OpenGL software and gives it the right
// mapping attributes
bool TextureManager::PlaceTexture(uint32 *Buffer, bool Glowing)
{

	TxtrTypeInfoData& TxtrTypeInfo = TxtrTypeInfoList[TextureType];

#ifdef DC
	// Emulator-only fault injection: fail one upload, then observe the same
	// texture state succeeding on a later draw. Never staged in hardware images.
	static int UploadTest = -1;
	static TextureState *FailedTestState = NULL;
	if (UploadTest < 0) UploadTest = access("/cd/AlephOne/UPLOADTEST", F_OK) == 0;
	if (UploadTest == 1)
	{
		UploadTest = 2;
		FailedTestState = TxtrStatePtr;
		dc_trace(39, "upload-test: injected failure");
		return false;
	}
	/*
	 * These blobs are already ARGB4444, VQ-compressed and twiddled for the
	 * PowerVR. GLdc copies them directly into its texture pool. Dimensions are
	 * checked by the loader, so a stale pack or a half-resolution configuration
	 * automatically takes the existing upload path below.
	 */
	if (UseVQPack && dc_vq_sprite_upload(Collection, CTable, Bitmap, Glowing,
	                                    LoadedWidth, LoadedHeight))
		goto texture_uploaded;

	// PVR true-colour textures are 16-bit. Mip generation can need the old
	// base allocation and the new 4/3-size chain simultaneously; reserve both.
	if (!OGL_TextureAllocationFits((unsigned)LoadedWidth * LoadedHeight *
	    ((TxtrTypeInfo.FarFilter == GL_NEAREST ||
	      TxtrTypeInfo.FarFilter == GL_LINEAR) ? 2u : 5u)))
	{
		dc_trace(39, "txtr: deferred %dx%d upload until safe compaction",
		         LoadedWidth, LoadedHeight);
		return false;
	}

	/* A failed pack read/upload remains recoverable through the old path. */
	if (!Buffer)
	{
		Buffer = GetOGLTexture(Glowing ? GlowColorTable : NormalColorTable);
		if (Glowing) GlowBuffer = Buffer;
		else NormalBuffer = Buffer;
	}
#endif

	// Attribute errors to this upload, including the mipmapped path.
#ifdef DC
	while (glGetError() != GL_NO_ERROR) {}
	if (PackedLandscape) {
		// GLdc's packed-to-twiddled path copies individual bytes into VRAM.
		// PVR memory requires >=16-bit writes. Keep this already-packed sky
		// linear so GLdc uses FASTCPY, retaining every texel and colour bit.
		glTexImage2D(GL_TEXTURE_2D, 0, GL_ARGB4444_KOS,
		             LoadedWidth, LoadedHeight, 0, GL_BGRA,
		             GL_UNSIGNED_SHORT_4_4_4_4_REV, Buffer);
		goto texture_uploaded;
	}
#endif
	// Load the texture
	switch(TxtrTypeInfo.FarFilter)
	{
	case GL_NEAREST:
	case GL_LINEAR:
	glTexImage2D(GL_TEXTURE_2D, 0, TxtrTypeInfo.ColorFormat, LoadedWidth, LoadedHeight,
			0, GL_RGBA, GL_UNSIGNED_BYTE, Buffer);
		break;
	case GL_NEAREST_MIPMAP_NEAREST:
	case GL_LINEAR_MIPMAP_NEAREST:
	case GL_NEAREST_MIPMAP_LINEAR:
	case GL_LINEAR_MIPMAP_LINEAR:
		gluBuild2DMipmaps(GL_TEXTURE_2D, TxtrTypeInfo.ColorFormat, LoadedWidth, LoadedHeight,
			GL_RGBA, GL_UNSIGNED_BYTE, Buffer);
		break;
	
	default:
		// Shouldn't happen
		assert(false);
	}

#ifdef DC
texture_uploaded:
	{
		static unsigned uploads = 0;
		GLenum error = glGetError();
		GLint available = 0;
		glGetIntegerv(GL_FREE_TEXTURE_MEMORY_KOS, &available);
		++uploads;
		if (error || uploads == 1 || uploads % 25 == 0)
			dc_trace(39, "txtr: %u uploads; %dx%d type=%d error=%04x free=%d KB",
			         uploads, LoadedWidth, LoadedHeight, TextureType, error, available/1024);
		if (error != GL_NO_ERROR) return false;
		if (FailedTestState == TxtrStatePtr)
		{
			dc_trace(39, "upload-test: failed texture recovered");
			FailedTestState = NULL;
		}
	}
#endif
	
	// Set texture-mapping features
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TxtrTypeInfo.NearFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TxtrTypeInfo.FarFilter);
	
	switch(TextureType)
	{
	case OGL_Txtr_Wall:
		// Walls are tiled in both direction
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		break;
		
	case OGL_Txtr_Landscape:
		// Landscapes repeat horizontally, have vertical limits or repeats vertically
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		if (LandscapeVertRepeat)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		else
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		break;
		
	case OGL_Txtr_Inhabitant:
	case OGL_Txtr_WeaponsInHand:
		// Sprites have both horizontal and vertical limits
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		break;
	}
	return true;
}


// What to render:

#ifdef DC
/*
 * A texture whose upload was deferred (no VRAM block large enough this frame)
 * has a GL texture object with no storage behind it. Drawing with it bound
 * hands the PowerVR texture address 0: the frame buffer, tiled across the
 * polygon. Flycast paints something harmless there, the console does not --
 * it showed as a sky made of tight repeating rectangles. So a polygon whose
 * texture is not resident is drawn untextured for that frame, and texturing
 * is re-enabled on the next resident one.
 */
static bool DC_TexturingSuspended = false;
#endif

// Always call this one and call it first; safe to allocate texture ID's in it
bool TextureManager::RenderNormal()
{
	TxtrStatePtr->Allocate();
	
	if (TxtrStatePtr->UseNormal())
	{
#ifdef DC
		assert(NormalBuffer || UseVQPack);
#else
		assert(NormalBuffer);
#endif
		TxtrStatePtr->IDsInUse[TextureState::Normal] = PlaceTexture(NormalBuffer, false);
	}
#ifdef DC
	if (!TxtrStatePtr->IDsInUse[TextureState::Normal])
	{
		glDisable(GL_TEXTURE_2D);
		DC_TexturingSuspended = true;
		return false;
	}
	if (DC_TexturingSuspended)
	{
		glEnable(GL_TEXTURE_2D);
		DC_TexturingSuspended = false;
	}
#endif
	return true;
}

// Call this one after RenderNormal()
bool TextureManager::RenderGlowing()
{
	if (TxtrStatePtr->UseGlowing())
	{
#ifdef DC
		assert(GlowBuffer || UseVQPack);
#else
		assert(GlowBuffer);
#endif
		TxtrStatePtr->IDsInUse[TextureState::Glowing] = PlaceTexture(GlowBuffer, true);
	}
#ifdef DC
	return TxtrStatePtr->IDsInUse[TextureState::Glowing];
#else
	return true;
#endif
}


// Init
TextureManager::TextureManager()
{
	NormalBuffer = 0;
	GlowBuffer = 0;
	
	ShadingTables = NULL;
	TransferMode = 0;
	TransferData = 0;
	IsShadeless = false;
	TextureType = 0;
	LandscapeVertRepeat = false;
	
	TxtrStatePtr = 0;
	TxtrOptsPtr = 0;
#ifdef DC
	UseVQPack = false;
	PackedLandscape = false;
	StaticNoise = false;
#endif
	
	// Marathon default
	Landscape_AspRatExp = 1;
}

// Cleanup
TextureManager::~TextureManager()
{
	if (NormalBuffer != 0) delete []NormalBuffer;
	if (GlowBuffer != 0) delete []GlowBuffer;
}

extern void ResetScreenFont();
extern void OGL_ResetMapFonts(bool IsStarting);
extern void OGL_ResetHUDFonts(bool IsStarting);

void OGL_ResetTextures()
{
	// Fix for crashing bug when OpenGL is inactive
	if (!OGL_IsActive()) return;
	
	// Reset the textures:
	for (int it=0; it<OGL_NUMBER_OF_TEXTURE_TYPES; it++)
		for (int ic=0; ic<MAXIMUM_COLLECTIONS; ic++)
		{
			bool CollectionPresent = is_collection_present(ic);
			short NumberOfBitmaps =
				CollectionPresent ? get_number_of_collection_bitmaps(ic) : 0;
			
			CollBitmapTextureState *CBTSSet = TextureStateSets[it][ic];
#ifdef DC
			// Sets are allocated on first use here; an unused one is NULL.
			if (!CBTSSet) continue;
#endif
			for (int ib=0; ib<NumberOfBitmaps; ib++)
			{
				TextureState *TSSet = CBTSSet[ib].CTStates;
				for (int ist=0; ist<NUMBER_OF_OPENGL_BITMAP_SETS; ist++)
					TSSet[ist].Reset();
			}
		}
	
	// Reset the surface textures for all the models:
	OGL_ResetModelSkins(OGL_IsActive());
	
	// Reset the font textures
	ResetScreenFont();
	OGL_ResetMapFonts(false);
	OGL_ResetHUDFonts(false);
}


void LoadModelSkin(ImageDescriptor& Image, short Collection, short CLUT)
{
	// A lot of this is copies of TextureManager member code
	
	// Texture-buffer management
	GLuint *Buffer;
	vector<GLuint> ImageBuffer;
	vector<GLuint> ShrunkBuffer;
	
	int TxtrWidth = Image.GetWidth();
	int TxtrHeight = Image.GetHeight();
	Buffer = (GLuint *)Image.GetPixelBasePtr();
	
	bool IsInfravision = (CLUT == INFRAVISION_BITMAP_SET);
	bool IsSilhouette = (CLUT == SILHOUETTE_BITMAP_SET);
	
	// Use special texture buffer because the originals may be needed for "normal" textures
	int ImageSize = Image.GetNumPixels();
	if (IsInfravision || IsSilhouette)
	{
		ImageBuffer.resize(ImageSize);
		objlist_copy(&ImageBuffer[0],Buffer,ImageSize);
		Buffer = &ImageBuffer[0];
	}
	
	if (IsInfravision)
	{
		for (int k=0; k<ImageSize; k++)
		{
			// uint32 is unsigned long here and GLuint is unsigned int, so a
			// uint32& will not bind to Buffer[k] even though they are the same
			// width. Copy through a value instead of aliasing.
			uint32 IntPxl = Buffer[k];
			GLfloat FloatPxl[4];
			MakeFloatColor(IntPxl,FloatPxl);
			FindInfravisionVersion(Collection,FloatPxl);
			Buffer[k] = MakeIntColor(FloatPxl);
		}
	}
	
	if (IsSilhouette)
	{
		for (int k=0; k<ImageSize; k++)
		{
			// Make the color white, but keep the opacity
			uint8 *PxlPtr = (uint8 *)(Buffer + k);
			PxlPtr[0] = PxlPtr[1] = PxlPtr[2] = 0xff;
		}
	}
	
	TxtrTypeInfoData& TxtrTypeInfo = TxtrTypeInfoList[OGL_Txtr_Inhabitant];

	// Display size: may be shrunk
	const int TxtrRes = TxtrTypeInfo.Resolution;
	int LoadedWidth = MAX(TxtrWidth >> TxtrRes, 1);
	int LoadedHeight = MAX(TxtrHeight >> TxtrRes, 1);
	
	if (LoadedWidth != TxtrWidth || LoadedHeight != TxtrHeight)
	{
		int NumPixels = int(LoadedWidth)*int(LoadedHeight);
		ShrunkBuffer.resize(NumPixels);
		gluScaleImage(GL_RGBA, TxtrWidth, TxtrHeight, GL_UNSIGNED_BYTE, Buffer,
			LoadedWidth, LoadedHeight, GL_UNSIGNED_BYTE, &ShrunkBuffer[0]);
		
		Buffer = &ShrunkBuffer[0];
	}

	// Load the texture
	switch(TxtrTypeInfo.FarFilter)
	{
	case GL_NEAREST:
	case GL_LINEAR:
		glTexImage2D(GL_TEXTURE_2D, 0, TxtrTypeInfo.ColorFormat, LoadedWidth, LoadedHeight,
			0, GL_RGBA, GL_UNSIGNED_BYTE, Buffer);
		break;
	case GL_NEAREST_MIPMAP_NEAREST:
	case GL_LINEAR_MIPMAP_NEAREST:
	case GL_NEAREST_MIPMAP_LINEAR:
	case GL_LINEAR_MIPMAP_LINEAR:
		gluBuild2DMipmaps(GL_TEXTURE_2D, TxtrTypeInfo.ColorFormat, LoadedWidth, LoadedHeight,
			GL_RGBA, GL_UNSIGNED_BYTE, Buffer);
		break;
	
	default:
		// Shouldn't happen
		assert(false);
	}
	
	// Set texture-mapping features
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TxtrTypeInfo.NearFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TxtrTypeInfo.FarFilter);

	
	// Like sprites, model textures have both horizontal and vertical limits
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

}


// Infravision (I'm blue, are you?)
bool& IsInfravisionActive() {return InfravisionActive;}


// Sets the infravision tinting color for a shapes collection, and whether to use such tinting;
// the color values are from 0 to 1.
bool SetInfravisionTint(short Collection, bool IsTinted, float Red, float Green, float Blue)
{	
	assert(Collection >= 0 && Collection < NUMBER_OF_COLLECTIONS);
	InfravisionData& IVData = IVDataList[Collection];
	
	IVData.Red = Red;
	IVData.Green = Green;
	IVData.Blue = Blue;
	IVData.IsTinted = IsTinted;
	
	return true;
}


// Finds the infravision version of a color for some collection set;
// it makes no change if infravision is inactive.
void FindInfravisionVersion(short Collection, GLfloat *Color)
{
	if (InfravisionActive)
	{
		InfravisionData& IVData = IVDataList[Collection];
		if (IVData.IsTinted)
		{
			GLfloat AvgColor = (Color[0] + Color[1] + Color[2])/3;
			Color[0] = IVData.Red*AvgColor;
			Color[1] = IVData.Green*AvgColor;
			Color[2] = IVData.Blue*AvgColor;
		}
	}
}

/*
// Stuff for doing 16->32 pixel-format conversion, 1555 ARGB to 8888 RGBA
GLuint *ConversionTable_16to32 = NULL;

void MakeConversion_16to32(int BitDepth)
{
	// This is for allocating a 16->32 conversion table only when necessary
	if (BitDepth == 16 && (!ConversionTable_16to32))
	{
		// Allocate it
		int TableSize = (1 << 15);
		ConversionTable_16to32 = new GLuint[TableSize];
	
		// Fill it
		for (word InVal = 0; InVal < TableSize; InVal++)
			ConversionTable_16to32[InVal] = Convert_16to32(InVal);
	}
	else if (ConversionTable_16to32)
	{
		// Get rid of it
		delete []ConversionTable_16to32;
		ConversionTable_16to32 = NULL;
	}
}
*/

#endif // def HAVE_OPENGL

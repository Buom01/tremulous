/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2013 Darklegion Development
Copyright (C) 2015-2019 GrangerHub

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, see <https://www.gnu.org/licenses/>

===========================================================================
*/
// tr_font.c
//
//
// The font system uses FreeType 2.x to render TrueType fonts for use within the game.
// As of this writing ( Nov, 2000 ) Team Arena uses these fonts for all of the ui and
// about 90% of the cgame presentation. A few areas of the CGAME were left uses the old
// fonts since the code is shared with standard Q3A.
//
// If you include this font rendering code in a commercial product you MUST include the
// following somewhere with your product, see www.freetype.org for specifics or changes.
// The Freetype code also uses some hinting techniques that MIGHT infringe on patents
// held by apple so be aware of that also.
//
// As of Q3A 1.25+ and Team Arena, we are shipping the game with the font rendering code
// disabled. This removes any potential patent issues and it keeps us from having to
// distribute an actual TrueTrype font which is 1. expensive to do and 2. seems to require
// an act of god to accomplish.
//
// What we did was pre-render the fonts using FreeType ( which is why we leave the FreeType
// credit in the credits ) and then saved off the glyph data and then hand touched up the
// font bitmaps so they scale a bit better in GL.
//
// There are limitations in the way fonts are saved and reloaded in that it is based on
// point size and not name. So if you pre-render Helvetica in 18 point and Impact in 18 point
// you will end up with a single 18 point data file and image set. Typically you will want to
// choose 3 sizes to best approximate the scaling you will be doing in the ui scripting system
//
// In the UI Scripting code, a scale of 1.0 is equal to a 48 point font. In Team Arena, we
// use three or four scales, most of them exactly equaling the specific rendered size. We
// rendered three sizes in Team Arena, 12, 16, and 20.
//
// To generate new font data you need to go through the following steps.
// 1. delete the fontImage_x_xx.tga files and fontImage_xx.dat files from the fonts path.
// 2. in a ui script, specificy a font, smallFont, and bigFont keyword with font name and
//    point size. the original TrueType fonts must exist in fonts at this point.
// 3. run the game, you should see things normally.
// 4. Exit the game and there will be three dat files and at least three tga files. The
//    tga's are in 256x256 pages so if it takes three images to render a 24 point font you
//    will end up with fontImage_0_24.tga through fontImage_2_24.tga
// 5. In future runs of the game, the system looks for these images and data files when a s
//    specific point sized font is rendered and loads them for use.
// 6. Because of the original beta nature of the FreeType code you will probably want to hand
//    touch the font bitmaps.
//
// Currently a define in the project turns on or off the FreeType code which is currently
// defined out. To pre-render new fonts you need enable the define ( BUILD_FREETYPE ) and
// uncheck the exclude from build check box in the FreeType2 area of the Renderer project.

#include "tr_common.h"

#include "qcommon/qcommon.h"

#ifdef BUILD_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ERRORS_H
#include FT_SYSTEM_H
#include FT_IMAGE_H
#include FT_OUTLINE_H

#define CANVAS_SIZE 1024
#define DPI 72*2

#define _FLOOR(x)  ((x) & -64)
#define _CEIL(x)   (((x)+63) & -64)
#define _TRUNC(x)  ((x) >> 6)

FT_Library ftLibrary = NULL;

#endif

#define MAX_FONTS 9
static int registeredFontCount = 0;
static newFontInfo_t registeredFont[MAX_FONTS];

#ifdef BUILD_FREETYPE
void R_GetGlyphInfo(FT_GlyphSlot glyph, int *left, int *right, int *width, int *top, int *bottom, int *height, int *pitch) {
	*left  = _FLOOR( glyph->metrics.horiBearingX );
	*right = _CEIL( glyph->metrics.horiBearingX + glyph->metrics.width );
	*width = _TRUNC(*right - *left);

	*top    = _CEIL( glyph->metrics.horiBearingY );
	*bottom = _FLOOR( glyph->metrics.horiBearingY - glyph->metrics.height );
	*height = _TRUNC( *top - *bottom );
	*pitch  = ( qtrue ? (*width+3) & -4 : (*width+7) >> 3 );
}


FT_Bitmap *R_RenderGlyph(FT_GlyphSlot glyph, glyphInfo_t* glyphOut) {
	FT_Bitmap  *bit2;
	int left, right, width, top, bottom, height, pitch, size;

	R_GetGlyphInfo(glyph, &left, &right, &width, &top, &bottom, &height, &pitch);

	if ( glyph->format == ft_glyph_format_outline ) {
		size   = pitch*height;

		bit2 = (FT_Bitmap*)ri.Malloc(sizeof(FT_Bitmap));

		bit2->width      = width;
		bit2->rows       = height;
		bit2->pitch      = pitch;
		bit2->pixel_mode = ft_pixel_mode_grays;
		//bit2->pixel_mode = ft_pixel_mode_mono;
		bit2->buffer     = (byte*)ri.Malloc(pitch*height);
		bit2->num_grays = 256;

		Com_Memset( bit2->buffer, 0, size );

		FT_Outline_Translate( &glyph->outline, -left, -bottom );

		FT_Outline_Get_Bitmap( ftLibrary, &glyph->outline, bit2 );

		glyphOut->height = height;
		glyphOut->pitch = pitch;
		glyphOut->top = (glyph->metrics.horiBearingY >> 6) + 1;
		glyphOut->bottom = bottom;

		return bit2;
	} else {
		ri.Printf(PRINT_ALL, "Non-outline fonts are not supported\n");
	}
	return NULL;
}

void WriteTGA (char *filename, byte *data, int width, int height) {
	byte			*buffer;
	int				i, c;
	int             row;
	unsigned char  *flip;
	unsigned char  *src, *dst;

	buffer = (byte*)ri.Malloc(width*height*4 + 18);
	Com_Memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width&255;
	buffer[13] = width>>8;
	buffer[14] = height&255;
	buffer[15] = height>>8;
	buffer[16] = 32;	// pixel size

	// swap rgb to bgr
	c = 18 + width * height * 4;
	for (i=18 ; i<c ; i+=4)
	{
		buffer[i] = data[i-18+2];		// blue
		buffer[i+1] = data[i-18+1];		// green
		buffer[i+2] = data[i-18+0];		// red
		buffer[i+3] = data[i-18+3];		// alpha
	}

	// flip upside down
	flip = (unsigned char *)ri.Malloc(width*4);
	for(row = 0; row < height/2; row++)
	{
		src = buffer + 18 + row * 4 * width;
		dst = buffer + 18 + (height - row - 1) * 4 * width;

		Com_Memcpy(flip, src, width*4);
		Com_Memcpy(src, dst, width*4);
		Com_Memcpy(dst, flip, width*4);
	}
	ri.Free(flip);

	ri.FS_WriteFile(filename, buffer, c);

	//f = fopen (filename, "wb");
	//fwrite (buffer, 1, c, f);
	//fclose (f);

	ri.Free (buffer);
}

static glyphInfo_t *RE_ConstructGlyphInfo(unsigned char *imageOut, int *xOut, int *yOut, int *maxHeight, FT_Face face, const unsigned char c, qboolean calcHeight, int margin) {
	int i;
	static glyphInfo_t glyph;
	unsigned char *src, *dst;
	int scaled_width, scaled_height;
	FT_Bitmap *bitmap = NULL;
	int margedMaxHeight;

	Com_Memset(&glyph, 0, sizeof(glyphInfo_t));
	// make sure everything is here
	if (face != NULL) {
		FT_Load_Glyph(face, FT_Get_Char_Index( face, c), FT_LOAD_DEFAULT );
		bitmap = R_RenderGlyph(face->glyph, &glyph);
		if (bitmap) {
			glyph.xSkip = (face->glyph->metrics.horiAdvance >> 6) + 1;
		} else {
			return &glyph;
		}

		if (glyph.height > *maxHeight) {
			*maxHeight = glyph.height;
		}

		if (calcHeight) {
			ri.Free(bitmap->buffer);
			ri.Free(bitmap);
			return &glyph;
		}

		margedMaxHeight = *maxHeight + margin*2;

/*
		// need to convert to power of 2 sizes so we do not get
		// any scaling from the gl upload
		for (scaled_width = 1 ; scaled_width < glyph.pitch ; scaled_width<<=1)
			;
		for (scaled_height = 1 ; scaled_height < glyph.height ; scaled_height<<=1)
			;
*/

		scaled_width = glyph.pitch + margin*2;
		scaled_height = glyph.height + margin*2;

		// we need to make sure we fit
		if (*xOut + scaled_width + 1 >= (CANVAS_SIZE - 1)) {
			*xOut = 0;
			*yOut += margedMaxHeight + 1;
		}

		if (*yOut + margedMaxHeight + 1 >= (CANVAS_SIZE - 1)) {
			*yOut = -1;
			*xOut = -1;
			ri.Free(bitmap->buffer);
			ri.Free(bitmap);
			return &glyph;
		}


		src = bitmap->buffer;
		dst = imageOut + ((*yOut + margin) * CANVAS_SIZE) + (*xOut + margin);

		if (bitmap->pixel_mode == ft_pixel_mode_mono) {
			for (i = 0; i < glyph.height; i++) {
				int j;
				unsigned char *_src = src;
				unsigned char *_dst = dst;
				unsigned char mask = 0x80;
				unsigned char val = *_src;
				for (j = 0; j < glyph.pitch; j++) {
					if (mask == 0x80) {
						val = *_src++;
					}
					if (val & mask) {
						*_dst = 0xff;
					}
					mask >>= 1;

					if ( mask == 0 ) {
						mask = 0x80;
					}
					_dst++;
				}

				src += glyph.pitch;
				dst += CANVAS_SIZE;
			}
		} else {
			for (i = 0; i < glyph.height; i++) {
				Com_Memcpy(dst, src, glyph.pitch);
				src += glyph.pitch;
				dst += CANVAS_SIZE;
			}
		}

		// we now have an 8 bit per pixel grey scale bitmap
		// that is width wide and pf->ftSize->metrics.y_ppem tall

		glyph.imageHeight = scaled_height;
		glyph.imageWidth = scaled_width;
		glyph.s = (float)*xOut / CANVAS_SIZE;
		glyph.t = (float)*yOut / CANVAS_SIZE;
		glyph.s2 = glyph.s + (float)scaled_width / CANVAS_SIZE;
		glyph.t2 = glyph.t + (float)scaled_height / CANVAS_SIZE;

		*xOut += scaled_width + 1;

		ri.Free(bitmap->buffer);
		ri.Free(bitmap);
	}

	return &glyph;
}
#endif

static int fdOffset;
static byte	*fdFile;

int readInt( void ) {
	int i = ((unsigned int)fdFile[fdOffset] | ((unsigned int)fdFile[fdOffset+1]<<8) | ((unsigned int)fdFile[fdOffset+2]<<16) | ((unsigned int)fdFile[fdOffset+3]<<24));
	fdOffset += 4;
	return i;
}

typedef union {
	byte	fred[4];
	float	ffred;
} poor;

float readFloat( void ) {
	poor	me;
#if defined Q3_BIG_ENDIAN
	me.fred[0] = fdFile[fdOffset+3];
	me.fred[1] = fdFile[fdOffset+2];
	me.fred[2] = fdFile[fdOffset+1];
	me.fred[3] = fdFile[fdOffset+0];
#elif defined Q3_LITTLE_ENDIAN
	me.fred[0] = fdFile[fdOffset+0];
	me.fred[1] = fdFile[fdOffset+1];
	me.fred[2] = fdFile[fdOffset+2];
	me.fred[3] = fdFile[fdOffset+3];
#endif
	fdOffset += 4;
	return me.ffred;
}

void RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font) {
#ifdef BUILD_FREETYPE
	FT_Face face;
	int j, k, xOut, yOut, lastStart, imageNumber;
	int scaledSize, newSize, maxHeight, left;
	unsigned char *out, *imageBuff;
	glyphInfo_t *glyph;
	image_t *image;
	qhandle_t h;
	float max;
	float dpi = (float)DPI;
	float glyphScale;
#endif
	void *faceData;
	int i, len;
	char name[1024];

	if (!fontName || !*fontName) {
		ri.Printf(PRINT_ALL, "RE_RegisterFont: called with empty name\n");
		return;
	}

	if (pointSize <= 0) {
		pointSize = 12;
	}

	R_IssuePendingRenderCommands();

	Com_sprintf(name, sizeof(name), "fonts/fontImage_%i.dat",pointSize);
	for (i = 0; i < registeredFontCount; i++) {
		if (Q_stricmp(name, registeredFont[i].name) == 0) {
			Com_Memcpy(font, &registeredFont[i], sizeof(fontInfo_t));
			return;
		}
	}

	if (registeredFontCount >= MAX_FONTS) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: Too many fonts registered already when registering %s.\n", fontName);
		return;
	}

	Com_Memset(font, 0, sizeof(fontInfo_t));

	len = ri.FS_ReadFile(name, NULL);
	if (len == sizeof(fontInfo_t)) {
		ri.FS_ReadFile(name, &faceData);
		fdOffset = 0;
		fdFile = (byte*)faceData;
		for(i=0; i<GLYPHS_PER_FONT; i++) {
			font->glyphs[i].height		= readInt();
			font->glyphs[i].top			= readInt();
			font->glyphs[i].bottom		= readInt();
			font->glyphs[i].pitch		= readInt();
			font->glyphs[i].xSkip		= readInt();
			font->glyphs[i].imageWidth	= readInt();
			font->glyphs[i].imageHeight = readInt();
			font->glyphs[i].s			= readFloat();
			font->glyphs[i].t			= readFloat();
			font->glyphs[i].s2			= readFloat();
			font->glyphs[i].t2			= readFloat();
			font->glyphs[i].glyph		= readInt();
			Q_strncpyz(font->glyphs[i].shaderName, (const char *)&fdFile[fdOffset], sizeof(font->glyphs[i].shaderName));
			fdOffset += sizeof(font->glyphs[i].shaderName);
		}
		font->glyphScale = readFloat();
		Com_Memcpy(font->name, &fdFile[fdOffset], MAX_QPATH);

//		Com_Memcpy(font, faceData, sizeof(fontInfo_t));
		Q_strncpyz(font->name, name, sizeof(font->name));
		for (i = GLYPH_START; i <= GLYPH_END; i++) {
			font->glyphs[i].glyph = RE_RegisterShaderNoMip(font->glyphs[i].shaderName);
		}
		Com_Memset(&registeredFont[registeredFontCount], 0, sizeof(newFontInfo_t));
		Com_Memcpy(&registeredFont[registeredFontCount++], font, sizeof(fontInfo_t));
		ri.FS_FreeFile(faceData);
		return;
	}

#ifndef BUILD_FREETYPE
	ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType code not available\n");
#else
	if (ftLibrary == NULL) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType not initialized.\n");
		return;
	}

	len = ri.FS_ReadFile(fontName, &faceData);
	if (len <= 0) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: Unable to read font file '%s'\n", fontName);
		return;
	}

	// allocate on the stack first in case we fail
	if (FT_New_Memory_Face( ftLibrary, (const FT_Byte*)faceData, len, 0, &face )) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType, unable to allocate new face.\n");
		return;
	}


	if (FT_Set_Char_Size( face, pointSize << 6, pointSize << 6, dpi, dpi)) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: FreeType, unable to set face char size.\n");
		return;
	}

	//*font = &registeredFonts[registeredFontCount++];

	// make a 256x256 image buffer, once it is full, register it, clean it and keep going
	// until all glyphs are rendered

	out = (unsigned char*)ri.Malloc(CANVAS_SIZE*CANVAS_SIZE);
	if (out == NULL) {
		ri.Printf(PRINT_WARNING, "RE_RegisterFont: ri.Malloc failure during output image creation.\n");
		return;
	}
	Com_Memset(out, 0, CANVAS_SIZE*CANVAS_SIZE);

	maxHeight = 0;

	for (i = GLYPH_START; i <= GLYPH_END; i++) {
		RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, qtrue, 0);
	}

	xOut = 0;
	yOut = 0;
	i = GLYPH_START;
	lastStart = i;
	imageNumber = 0;

	while ( i <= GLYPH_END + 1 ) {

		if ( i == GLYPH_END + 1 ) {
			// upload/save current image buffer
			xOut = yOut = -1;
		} else {
			glyph = RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, qfalse, 0);
		}

		if (xOut == -1 || yOut == -1)  {
			// ran out of room
			// we need to create an image from the bitmap, set all the handles in the glyphs to this point
			//

			scaledSize = CANVAS_SIZE*CANVAS_SIZE;
			newSize = scaledSize * 4;
			imageBuff = (unsigned char*)ri.Malloc(newSize);
			left = 0;
			max = 0;
			for ( k = 0; k < (scaledSize) ; k++ ) {
				if (max < out[k]) {
					max = out[k];
				}
			}

			if (max > 0) {
				max = 255/max;
			}

			for ( k = 0; k < (scaledSize) ; k++ ) {
				imageBuff[left++] = 255;
				imageBuff[left++] = 255;
				imageBuff[left++] = 255;

				imageBuff[left++] = ((float)out[k] * max);
			}

			Com_sprintf (name, sizeof(name), "fonts/fontImage_%i_%i.tga", imageNumber++, pointSize);
			if (r_saveFontData->integer) {
				WriteTGA(name, imageBuff, CANVAS_SIZE, CANVAS_SIZE);
			}

			//Com_sprintf (name, sizeof(name), "fonts/fontImage_%i_%i", imageNumber++, pointSize);
			image = R_CreateImage(name, imageBuff, CANVAS_SIZE, CANVAS_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_CLAMPTOEDGE, 0 );
			h = RE_RegisterShaderFromImage(name, LIGHTMAP_2D, image, qfalse);
			for (j = lastStart; j < i; j++) {
				font->glyphs[j].glyph = h;
				Q_strncpyz(font->glyphs[j].shaderName, name, sizeof(font->glyphs[j].shaderName));
			}
			lastStart = i;
			Com_Memset(out, 0, CANVAS_SIZE*CANVAS_SIZE);
			xOut = 0;
			yOut = 0;
			ri.Free(imageBuff);
			if ( i == GLYPH_END + 1 )
				i++;
		} else {
			Com_Memcpy(&font->glyphs[i], glyph, sizeof(glyphInfo_t));
			i++;
		}
	}

	// change the scale to be relative to 1 based on 72 dpi ( so dpi of 144 means a scale of .5 )
	glyphScale = 72.0f / dpi;

	// we also need to adjust the scale based on point size relative to 48 points as the ui scaling is based on a 48 point font
	glyphScale *= 48.0f / pointSize;

	registeredFont[registeredFontCount].glyphScale = glyphScale;
	font->glyphScale = glyphScale;
	Com_Memcpy(&registeredFont[registeredFontCount++], font, sizeof(fontInfo_t));

	if (r_saveFontData->integer) {
		ri.FS_WriteFile(va("fonts/fontImage_%i.dat", pointSize), font, sizeof(fontInfo_t));
	}

	ri.Free(out);

	ri.FS_FreeFile(faceData);
#endif
}

#ifdef BUILD_FREETYPE

static double *RE_GaussianKernel(int radius, double sigma)
{
	int kernelSize = radius * 2 + 1;
	double *kernel;
	int kernelOffset = radius;
	int x, y, i;
	double sum = 0.0;

	if ( !(kernel = (double*)ri.Malloc(kernelSize*kernelSize * sizeof(double))) )
	{
		ri.Printf(PRINT_ALL, "RE_GaussianKernel: Can't allocate kernel buffer\n");
		return NULL;
	}


	for (x = 0; x < kernelSize; x++)
	{
    for (y = 0; y < kernelSize; y++)
		{
      kernel[kernelSize*y + x] =
					exp( -0.5 * ( pow( (x-kernelOffset)/sigma, 2 ) + pow( (y-kernelOffset)/sigma, 2 ) ) )
		      		/ (2 * M_PI * pow(sigma, 2));
			sum += kernel[kernelSize*y + x];
    }
	}

	for (i = 0; i < kernelSize*kernelSize; i++)
    kernel[i] /= sum;

	return (kernel);
}

static void RE_ApplyBlur(int canvasSize, int radius, unsigned char *buff, double *kernel)
{
	int kernelSize = radius * 2 + 1;
	int x, y, x1, y1, x2, y2;
	unsigned char *buffCpy;
	double pixelValue;
	int kernelOffset = radius;
	int channel;

	if ( !(buffCpy = (unsigned char*)ri.Malloc((canvasSize*canvasSize * 4) * sizeof(unsigned char))) )
	{
		ri.Printf(PRINT_ALL, "RE_ApplyBlur: Can't copy original image\n");
		return;
	}

	Com_Memcpy(buffCpy, buff, (canvasSize*canvasSize * 4) * sizeof(unsigned char));


	// Absolutly don't care about performances since it does not run in production
	for (channel = 0; channel < 4; channel++) {
		for (x = 0; x < canvasSize; x++) {
			for (y = 0; y < canvasSize; y++) {
				pixelValue = 0;

				for (x1 = 0; x1 < kernelSize; x1++) {
					for (y1 = 0; y1 < kernelSize; y1++) {

						x2 = x + x1 - kernelOffset;
						y2 = y + y1 - kernelOffset;

						if (x2 < 0)
							x2 = 0;
						if (x2 > canvasSize)
							x2 = canvasSize;
						if (y2 < 0)
							y2 = 0;
						if (y2 > canvasSize)
							y2 = canvasSize;

						pixelValue += kernel[y1*kernelSize + x1] * buffCpy[(y2*canvasSize + x2) * 4 + channel];

					}
				}

				buff[(y*canvasSize + x) * 4 + channel] = (unsigned char)pixelValue;

			}
		}
	}

	ri.Free(buffCpy);
}

#endif


void RE_RegisterNewFont(const char *fontName, const char *simpleName, int pointSize, newFontInfo_t *font) {
#ifdef BUILD_FREETYPE
	FT_Face face;
	int k, l, xOut, yOut, lastStart, imageNumber;
	int scaledSize, newSize, maxHeight, left;
	unsigned char *out, *imageBuff;
	glyphInfo_t *glyph;
	image_t *image;
	qhandle_t h;
	float max;
	float dpi = (float)DPI;
	float glyphScale;
	int elevation, margin;
	double sigma;
	double *kernel;
#endif
	void *faceData;
	int i, j, len;
	char name[1024];

	if (!fontName || !*fontName) {
		ri.Printf(PRINT_ALL, "RE_RegisterNewFont: called with empty name\n");
		return;
	}

	if (pointSize <= 0) {
		pointSize = 12;
	}

	R_IssuePendingRenderCommands();

	Com_sprintf(name, sizeof(name), "fonts/%s_%i.dat", simpleName, pointSize);
	for (i = 0; i < registeredFontCount; i++) {
		if (Q_stricmp(name, registeredFont[i].name) == 0) {
			Com_Memcpy(font, &registeredFont[i], sizeof(newFontInfo_t));
			return;
		}
	}

	if (registeredFontCount >= MAX_FONTS) {
		ri.Printf(PRINT_WARNING, "RE_RegisterNewFont: Too many fonts registered already when registering %s.\n", fontName);
		return;
	}

	Com_Memset(font, 0, sizeof(newFontInfo_t));

	len = ri.FS_ReadFile(name, NULL);
	if (len == sizeof(newFontInfo_t)) {
		ri.FS_ReadFile(name, &faceData);
		fdOffset = 0;
		fdFile = (byte*)faceData;

		// Reading glyphs
		for(i=0; i < GLYPHS_PER_FONT; i++) {
			font->glyphs[i].height		= readInt();
			font->glyphs[i].top			= readInt();
			font->glyphs[i].bottom		= readInt();
			font->glyphs[i].pitch		= readInt();
			font->glyphs[i].xSkip		= readInt();
			font->glyphs[i].imageWidth	= readInt();
			font->glyphs[i].imageHeight = readInt();
			font->glyphs[i].s			= readFloat();
			font->glyphs[i].t			= readFloat();
			font->glyphs[i].s2			= readFloat();
			font->glyphs[i].t2			= readFloat();
			font->glyphs[i].glyph		= readInt();
			Q_strncpyz(font->glyphs[i].shaderName, (const char *)&fdFile[fdOffset], sizeof(font->glyphs[i].shaderName));
			fdOffset += sizeof(font->glyphs[i].shaderName);
		}

		// reading glyphScale
		font->glyphScale = readFloat();

		// Save the name (mainly used by caching)
		// Com_Memcpy(font->name, &fdFile[fdOffset], MAX_QPATH);
		Q_strncpyz(font->name, name, sizeof(font->name));  // Using this ensure that we recover it from registeredFont later
		fdOffset += sizeof(font->name);  // skip the name from the .dat file

		// Reading shadow data
		for (i = 0; i < FONT_MAXSHADOW; i++) {

			//Reading shadow glyph
			for(j=0; j < GLYPHS_PER_FONT; j++) {
				font->shadows[i].glyphs[j].height		= readInt();
				font->shadows[i].glyphs[j].top			= readInt();
				font->shadows[i].glyphs[j].bottom		= readInt();
				font->shadows[i].glyphs[j].pitch		= readInt();
				font->shadows[i].glyphs[j].xSkip		= readInt();
				font->shadows[i].glyphs[j].imageWidth	= readInt();
				font->shadows[i].glyphs[j].imageHeight = readInt();
				font->shadows[i].glyphs[j].s			= readFloat();
				font->shadows[i].glyphs[j].t			= readFloat();
				font->shadows[i].glyphs[j].s2			= readFloat();
				font->shadows[i].glyphs[j].t2			= readFloat();
				font->shadows[i].glyphs[j].glyph		= readInt();
				Q_strncpyz(font->shadows[i].glyphs[j].shaderName, (const char *)&fdFile[fdOffset], sizeof(font->shadows[i].glyphs[j].shaderName));
				fdOffset += sizeof(font->shadows[i].glyphs[j].shaderName);
			}


			// Save margin informations of the shadow glyphs
			font->shadows[i].margin = readInt();


			// Save if the shadow exists or is null.
			font->shadows[i].available = readInt() == 1;
		}

		// And finally, register shaders
		// Font one
		for (i = GLYPH_START; i <= GLYPH_END; i++) {
			font->glyphs[i].glyph = RE_RegisterShaderNoMip(font->glyphs[i].shaderName);
		}
		// Shadow one
		for (i = 0; i < FONT_MAXSHADOW; i++)
			if (font->shadows[i].available == 1)
				for (j = GLYPH_START; j <= GLYPH_END; j++)
					font->shadows[i].glyphs[j].glyph = RE_RegisterShaderNoMip(font->shadows[i].glyphs[j].shaderName);


		Com_Memcpy(&registeredFont[registeredFontCount++], font, sizeof(newFontInfo_t));
		ri.FS_FreeFile(faceData);
		return;
	}

#ifndef BUILD_FREETYPE
	ri.Printf(PRINT_WARNING, "RE_RegisterNewFont: FreeType code not available\n");
#else
	if (ftLibrary == NULL) {
		ri.Printf(PRINT_WARNING, "RE_RegisterNewFont: FreeType not initialized.\n");
		return;
	}

	len = ri.FS_ReadFile(fontName, &faceData);
	if (len <= 0) {
		ri.Printf(PRINT_WARNING, "RE_RegisterNewFont: Unable to read font file '%s'\n", fontName);
		return;
	}

	// allocate on the stack first in case we fail
	if (FT_New_Memory_Face( ftLibrary, (const FT_Byte*)faceData, len, 0, &face )) {
		ri.Printf(PRINT_WARNING, "RE_RegisterNewFont: FreeType, unable to allocate new face.\n");
		return;
	}


	if (FT_Set_Char_Size( face, pointSize << 6, pointSize << 6, dpi, dpi)) {
		ri.Printf(PRINT_WARNING, "RE_RegisterNewFont: FreeType, unable to set face char size.\n");
		return;
	}

	//*font = &registeredFonts[registeredFontCount++];

	// make a 256x256 image buffer, once it is full, register it, clean it and keep going
	// until all glyphs are rendered

	out = (unsigned char*)ri.Malloc(CANVAS_SIZE*CANVAS_SIZE);
	if (out == NULL) {
		ri.Printf(PRINT_WARNING, "RE_RegisterNewFont: ri.Malloc failure during output image creation.\n");
		return;
	}
	Com_Memset(out, 0, CANVAS_SIZE*CANVAS_SIZE);

	maxHeight = 0;

	for (i = GLYPH_START; i <= GLYPH_END; i++) {
		RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, qtrue, 0);
	}

	xOut = 0;
	yOut = 0;
	i = GLYPH_START;
	lastStart = i;
	imageNumber = 0;

	while ( i <= GLYPH_END + 1 ) {

		if ( i == GLYPH_END + 1 ) {
			// upload/save current image buffer
			xOut = yOut = -1;
		} else {
			glyph = RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, qfalse, 0);
		}

		if (xOut == -1 || yOut == -1)  {
			// ran out of room
			// we need to create an image from the bitmap, set all the handles in the glyphs to this point
			//

			scaledSize = CANVAS_SIZE*CANVAS_SIZE;
			newSize = scaledSize * 4;
			imageBuff = (unsigned char*)ri.Malloc(newSize);
			left = 0;
			max = 0;
			for ( k = 0; k < (scaledSize) ; k++ ) {
				if (max < out[k]) {
					max = out[k];
				}
			}

			if (max > 0) {
				max = 255/max;
			}

			for ( k = 0; k < (scaledSize) ; k++ ) {
				imageBuff[left++] = 255;
				imageBuff[left++] = 255;
				imageBuff[left++] = 255;

				imageBuff[left++] = ((float)out[k] * max);
			}

			Com_sprintf (name, sizeof(name), "fonts/%s_%i_%i.tga", simpleName, imageNumber++, pointSize);
			if (r_saveFontData->integer) {
				WriteTGA(name, imageBuff, CANVAS_SIZE, CANVAS_SIZE);
			}

			//Com_sprintf (name, sizeof(name), "fonts/%s_%i_%i", simpleName, imageNumber++, pointSize);
			image = R_CreateImage(name, imageBuff, CANVAS_SIZE, CANVAS_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_CLAMPTOEDGE, 0 );
			h = RE_RegisterShaderFromImage(name, LIGHTMAP_2D, image, qfalse);
			for (j = lastStart; j < i; j++) {
				font->glyphs[j].glyph = h;
				Q_strncpyz(font->glyphs[j].shaderName, name, sizeof(font->glyphs[j].shaderName));
			}
			lastStart = i;
			Com_Memset(out, 0, CANVAS_SIZE*CANVAS_SIZE);
			xOut = 0;
			yOut = 0;
			ri.Free(imageBuff);
			if ( i == GLYPH_END + 1 )
				i++;
		} else {
			Com_Memcpy(&font->glyphs[i], glyph, sizeof(glyphInfo_t));
			i++;
		}
	}

	for (l = 0; l < FONT_MAXSHADOW; l++) {
		xOut = 0;
		yOut = 0;
		i = GLYPH_START;
		lastStart = i;
		imageNumber = 0;
		elevation = l + 1;
		sigma = elevation*dpi/pointSize/4; // should also depend of pointsize
		margin = (int)ceil(sigma*2);
		kernel = RE_GaussianKernel(margin, sigma);

		while ( i <= GLYPH_END + 1 ) {

			if ( i == GLYPH_END + 1 ) {
				// upload/save current image buffer
				xOut = yOut = -1;
			} else {
				glyph = RE_ConstructGlyphInfo(out, &xOut, &yOut, &maxHeight, face, (unsigned char)i, qfalse, margin);
			}

			if (xOut == -1 || yOut == -1)  {
				// ran out of room
				// we need to create an image from the bitmap, set all the handles in the glyphs to this point
				//

				scaledSize = CANVAS_SIZE*CANVAS_SIZE;
				newSize = scaledSize * 4;
				imageBuff = (unsigned char*)ri.Malloc(newSize);
				left = 0;
				max = 0;
				for ( k = 0; k < (scaledSize) ; k++ ) {
					if (max < out[k]) {
						max = out[k];
					}
				}

				if (max > 0) {
					max = 255/max;
				}

				for ( k = 0; k < (scaledSize) ; k++ ) {
					imageBuff[left++] = 255;
					imageBuff[left++] = 255;
					imageBuff[left++] = 255;

					imageBuff[left++] = ((float)out[k] * max);
				}

				RE_ApplyBlur(CANVAS_SIZE, margin, imageBuff, kernel);

				Com_sprintf (name, sizeof(name), "fonts/%s_shad%i_%i_%i.tga", simpleName, l, imageNumber++, pointSize);
				if (r_saveFontData->integer) {
					WriteTGA(name, imageBuff, CANVAS_SIZE, CANVAS_SIZE);
				}

				//Com_sprintf (name, sizeof(name), "fonts/%s_%i_%i", simpleName, imageNumber++, pointSize);
				image = R_CreateImage(name, imageBuff, CANVAS_SIZE, CANVAS_SIZE, IMGTYPE_COLORALPHA, IMGFLAG_CLAMPTOEDGE, 0 );
				h = RE_RegisterShaderFromImage(name, LIGHTMAP_2D, image, qfalse);
				for (j = lastStart; j < i; j++) {
					font->shadows[l].glyphs[j].glyph = h;
					Q_strncpyz(font->shadows[l].glyphs[j].shaderName, name, sizeof(font->shadows[l].glyphs[j].shaderName));
				}
				lastStart = i;
				Com_Memset(out, 0, CANVAS_SIZE*CANVAS_SIZE);
				xOut = 0;
				yOut = 0;
				ri.Free(imageBuff);
				if ( i == GLYPH_END + 1 )
					i++;
			} else {
				Com_Memcpy(&font->shadows[l].glyphs[i], glyph, sizeof(font->shadows[l].glyphs[i]));
				i++;
			}
		}

		font->shadows[l].margin = margin;
		font->shadows[l].available = 1;
	}

	// change the scale to be relative to 1 based on 72 dpi ( so dpi of 144 means a scale of .5 )
	glyphScale = 72.0f / dpi;

	// we also need to adjust the scale based on point size relative to 48 points as the ui scaling is based on a 48 point font
	glyphScale *= 48.0f / pointSize;

	registeredFont[registeredFontCount].glyphScale = glyphScale;
	font->glyphScale = glyphScale;
	Com_Memcpy(&registeredFont[registeredFontCount++], font, sizeof(newFontInfo_t));

	if (r_saveFontData->integer) {
		ri.FS_WriteFile(va("fonts/%s_%i.dat", simpleName, pointSize), font, sizeof(newFontInfo_t));
	}

	ri.Free(out);

	ri.FS_FreeFile(faceData);
#endif
}


void R_InitFreeType(void) {
#ifdef BUILD_FREETYPE
	if (FT_Init_FreeType( &ftLibrary )) {
		ri.Printf(PRINT_WARNING, "R_InitFreeType: Unable to initialize FreeType.\n");
	}
#endif
	registeredFontCount = 0;
}


void R_DoneFreeType(void) {
#ifdef BUILD_FREETYPE
	if (ftLibrary) {
		FT_Done_FreeType( ftLibrary );
		ftLibrary = NULL;
	}
#endif
	registeredFontCount = 0;
}

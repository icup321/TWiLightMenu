/*-----------------------------------------------------------------
 Copyright (C) 2015
	Matthew Scholefield

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/

#include "graphics.h"
#include "../errorScreen.h"
#include "fontHandler.h"
#include "common/tonccpy.h"
#include "common/twlmenusettings.h"
#include "graphics/gif.hpp"
#include "graphics/lodepng.h"

#include <nds.h>

extern bool fadeType;
extern bool fadeSpeed;
extern bool controlTopBright;
extern bool controlBottomBright;
int fadeDelay = 0;

int screenBrightness = 31;
int imageType = 0;

u8* dsImageBuffer8;
u16* dsImageBuffer[2];

int bg3Sub;
int bg2Main;
int bg3Main;
int bg2Sub;

void ClearBrightness(void) {
	fadeType = true;
	screenBrightness = 0;
	swiWaitForVBlank();
	swiWaitForVBlank();
}

// Ported from PAlib (obsolete)
void SetBrightness(u8 screen, s8 bright) {
	u16 mode = 1 << 14;

	if (bright < 0) {
		mode = 2 << 14;
		bright = -bright;
	}
	if (bright > 31) bright = 31;
	*(u16*)(0x0400006C + (0x1000 * screen)) = bright + mode;
}

u16 convertVramColorToGrayscale(u16 val) {
	u8 b,g,r,max,min;
	b = ((val)>>10)&31;
	g = ((val)>>5)&31;
	r = (val)&31;
	// Value decomposition of hsv
	max = (b > g) ? b : g;
	max = (max > r) ? max : r;

	// Desaturate
	min = (b < g) ? b : g;
	min = (min < r) ? min : r;
	max = (max + min) / 2;

	return BIT(15)|(max<<10)|(max<<5)|(max);
}

void vBlankHandler() {
	if(fadeType == true) {
		if(!fadeDelay) {
			screenBrightness--;
			if (screenBrightness < 0) screenBrightness = 0;
		}
		if (!fadeSpeed) {
			fadeDelay++;
			if (fadeDelay == 3) fadeDelay = 0;
		} else {
			fadeDelay = 0;
		}
	} else {
		if(!fadeDelay) {
			screenBrightness++;
			if (screenBrightness > 31) screenBrightness = 31;
		}
		if (!fadeSpeed) {
			fadeDelay++;
			if (fadeDelay == 3) fadeDelay = 0;
		} else {
			fadeDelay = 0;
		}
	}
	if (controlTopBright) SetBrightness(0, screenBrightness);
	if (controlBottomBright && !ms().macroMode) SetBrightness(1, screenBrightness);

	if (imageType > 1) {
		static bool secondBuffer = false;
		dmaCopyHalfWordsAsynch(0, dsImageBuffer[secondBuffer], BG_GFX, (256*192)*2);
		secondBuffer = !secondBuffer;
	}

	//updateText(true);
	//updateText(false);
}

void imageLoad(const char* filename) {
	if (imageType == 2) { // PNG
		std::vector<unsigned char> image;
		unsigned width, height;
		lodepng::decode(image, width, height, filename);
		if (width > 256 || height > 192) return;

		int xPos = 0;
		if (width <= 254) {
			// Adjust X position
			for (int i = width; i < 256; i += 2) {
				xPos++;
			}
		}

		int yPos = 0;
		if (height <= 190) {
			// Adjust Y position
			for (int i = height; i < 192; i += 2) {
				yPos++;
			}
		}

		bool alternatePixel = false;
		int x = 0;
		int y = 0;
		for(unsigned i=0;i<image.size()/4;i++) {
			image[(i*4)+3] = 0;
			if (alternatePixel) {
				if (image[(i*4)] >= 0x4) {
					image[(i*4)] -= 0x4;
					image[(i*4)+3] |= BIT(0);
				}
				if (image[(i*4)+1] >= 0x4) {
					image[(i*4)+1] -= 0x4;
					image[(i*4)+3] |= BIT(1);
				}
				if (image[(i*4)+2] >= 0x4) {
					image[(i*4)+2] -= 0x4;
					image[(i*4)+3] |= BIT(2);
				}
			}
			dsImageBuffer[0][(xPos+x+(y*256))+(yPos*256)] = image[i*4]>>3 | (image[(i*4)+1]>>3)<<5 | (image[(i*4)+2]>>3)<<10 | BIT(15);
			if (alternatePixel) {
				if (image[(i*4)+3] & BIT(0)) {
					image[(i*4)] += 0x4;
				}
				if (image[(i*4)+3] & BIT(1)) {
					image[(i*4)+1] += 0x4;
				}
				if (image[(i*4)+3] & BIT(2)) {
					image[(i*4)+2] += 0x4;
				}
			} else {
				if (image[(i*4)] >= 0x4) {
					image[(i*4)] -= 0x4;
				}
				if (image[(i*4)+1] >= 0x4) {
					image[(i*4)+1] -= 0x4;
				}
				if (image[(i*4)+2] >= 0x4) {
					image[(i*4)+2] -= 0x4;
				}
			}
			dsImageBuffer[1][(xPos+x+(y*256))+(yPos*256)] = image[i*4]>>3 | (image[(i*4)+1]>>3)<<5 | (image[(i*4)+2]>>3)<<10 | BIT(15);
			x++;
			if ((unsigned)x == width) {
				alternatePixel = !alternatePixel;
				x=0;
				y++;
			}
			alternatePixel = !alternatePixel;
		}
		return;
	} else if (imageType == 1) { // BMP
		FILE* file = fopen(filename, "rb");
		if (!file)
			return;

		// Read width & height
		fseek(file, 0x12, SEEK_SET);
		u32 width, height;
		fread(&width, 1, sizeof(width), file);
		fread(&height, 1, sizeof(height), file);

		if (width > 256 || height > 192) {
			fclose(file);
			return;
		}

		fseek(file, 0xE, SEEK_SET);
		u8 headerSize = fgetc(file);
		bool rgb565 = false;
		if(headerSize == 0x38) {
			// Check the upper byte green mask for if it's got 5 or 6 bits
			fseek(file, 0x2C, SEEK_CUR);
			rgb565 = fgetc(file) == 0x07;
			fseek(file, headerSize - 0x2E, SEEK_CUR);
		} else {
			fseek(file, headerSize - 1, SEEK_CUR);
		}
		u16 *bmpImageBuffer = new u16[width * height];
		fread(bmpImageBuffer, 2, width * height, file);
		u16 *dst = BG_GFX + ((191 - ((192 - height) / 2)) * 256) + (256 - width) / 2;
		u16 *src = bmpImageBuffer;
		for (uint y = 0; y < height; y++, dst -= 256) {
			for (uint x = 0; x < width; x++) {
				u16 val = *(src++);
				*(dst + x) = ((val >> (rgb565 ? 11 : 10)) & 0x1F) | ((val >> (rgb565 ? 1 : 0)) & (0x1F << 5)) | (val & 0x1F) << 10 | BIT(15);
			}
		}

		delete[] bmpImageBuffer;
		fclose(file);
		return;
	}

	Gif gif(filename, false, false, true);
	std::vector<u8> pageImage = gif.frame(0).image.imageData;
	int width = gif.frame(0).descriptor.w;
	int height = gif.frame(0).descriptor.h;
	if (width > 256 || height > 192) return;

	int xPos = 0;
	if (width <= 254) {
		// Adjust X position
		for (int i = width; i < 256; i += 2) {
			xPos++;
		}
	}

	int yPos = 0;
	if (height <= 190) {
		// Adjust Y position
		for (int i = height; i < 192; i += 2) {
			yPos++;
		}
	}

	tonccpy(BG_PALETTE, gif.gct().data(), gif.gct().size() * 2);

	int x = 0;
	int y = 0;
	for (int i = 0; i < width*height; i++) {
		dsImageBuffer8[(xPos+x+(y*256))+(yPos*256)] = pageImage[i];
		x++;
		if (x == width) {
			x=0;
			y++;
		}
	}
	dmaCopyWordsAsynch(0, dsImageBuffer8, bgGetGfxPtr(bg3Main), 256*192);
}

void bgLoad(void) {
	if (ms().macroMode) {
		return;
	}

	Gif gif("nitro:/graphics/bg.gif", false, false, true);
	const auto &frame = gif.frame(0);
	u16 *dst = bgGetGfxPtr(bg3Sub);

	tonccpy(BG_PALETTE_SUB, gif.gct().data(), gif.gct().size() * 2);

	for(uint i = 0; i < frame.image.imageData.size() - 2; i += 2) {
		toncset16(dst++, (frame.image.imageData[i]) | (frame.image.imageData[i + 1]) << 8, 1);
	}
}

void graphicsInit() {
	*(u16*)(0x0400006C) |= BIT(14);
	*(u16*)(0x0400006C) &= BIT(15);
	SetBrightness(0, 31);
	SetBrightness(1, 31);

	////////////////////////////////////////////////////////////
	videoSetMode(MODE_5_2D);
	videoSetModeSub(MODE_5_2D);

	vramSetBankA(VRAM_A_MAIN_BG);
	//vramSetBankB(VRAM_B_MAIN_BG); // May be needed for larger images
	vramSetBankC(VRAM_C_SUB_BG);

	if (imageType > 0) {
		//bg3Main = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

		videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);

		REG_BG3CNT = BG_MAP_BASE(0) | BG_BMP16_256x256 | BG_PRIORITY(0);
		REG_BG3X = 0;
		REG_BG3Y = 0;
		REG_BG3PA = 1<<8;
		REG_BG3PB = 0;
		REG_BG3PC = 0;
		REG_BG3PD = 1<<8;

		if (imageType > 1) {
			dsImageBuffer[0] = new u16[256*192];
			dsImageBuffer[1] = new u16[256*192];
			toncset16(dsImageBuffer[0], 0, 256*192);
			toncset16(dsImageBuffer[1], 0, 256*192);
		}
	} else {
		bg3Main = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
		dsImageBuffer8 = new u8[256*192];
		toncset(dsImageBuffer8, 0, 256*192);
	}
	bgSetPriority(bg3Main, 3);

	//bg2Main = bgInit(2, BgType_Bmp8, BgSize_B8_256x256, 3, 0);
	//bgSetPriority(bg2Main, 0);

	bg3Sub = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
	bgSetPriority(bg3Sub, 3);

	bg2Sub = bgInitSub(2, BgType_Bmp8, BgSize_B8_256x256, 3, 0);
	bgSetPriority(bg2Sub, 0);

	if (ms().macroMode) {
		lcdMainOnBottom();
	}

	irqSet(IRQ_VBLANK, vBlankHandler);
	irqEnable(IRQ_VBLANK);
}

#include <ace/utils/bitmap.h>
#include <ace/utils/endian.h>
#include <ace/utils/planar.h>

/* Globals */

/* Functions */

tBitMap *bitmapCreate(UWORD uwWidth, UWORD uwHeight, UBYTE ubDepth, UBYTE ubFlags) {
	tBitMap *pBitMap;
	UBYTE i;
	
	logBlockBegin("bitmapCreate(uwWidth: %u, uwHeight: %u, ubDepth: %hu, ubFlags: %hu)", uwWidth, uwHeight, ubDepth, ubFlags);
	pBitMap = (tBitMap*) memAllocFastClear(sizeof(tBitMap));
	logWrite("addr: %p\n", pBitMap);

	InitBitMap(pBitMap, ubDepth, uwWidth, uwHeight);
	
	if(ubFlags & BMF_INTERLEAVED) {
		UWORD uwRealWidth;
		uwRealWidth = pBitMap->BytesPerRow;
		pBitMap->BytesPerRow *= ubDepth;
		
		pBitMap->Planes[0] = (PLANEPTR) memAllocChip(pBitMap->BytesPerRow*uwHeight);
		if(!pBitMap->Planes[0]) {
			logWrite("ERR: Can't alloc interleaved bitplanes\n");
			memFree(pBitMap, sizeof(tBitMap));
			logBlockEnd("bitmapCreate()");
			return 0;			
		}
		for(i = 1; i != ubDepth; ++i)
			pBitMap->Planes[i] = pBitMap->Planes[i-1] + uwRealWidth;
		
		if (ubFlags & BMF_CLEAR)
			BltClear(pBitMap->Planes[0], pBitMap->Rows * pBitMap->BytesPerRow, 1);
	}
	else
		for(i = ubDepth; i--;) {
			pBitMap->Planes[i] = (PLANEPTR) memAllocChip(pBitMap->BytesPerRow*uwHeight);
			if(!pBitMap->Planes[i]) {
				logWrite("ERR: Can't alloc bitplane %hu/%hu\n", ubDepth-i+1,ubDepth);
				while(i) {
					memFree(pBitMap->Planes[i], pBitMap->BytesPerRow*uwHeight);
					--i;
				}
				memFree(pBitMap, sizeof(tBitMap));
				logBlockEnd("bitmapCreate()");
				return 0;
			}
			if (ubFlags & BMF_CLEAR)
				BltClear(pBitMap->Planes[i], pBitMap->Rows * pBitMap->BytesPerRow, 1);
		}

	if (ubFlags & BMF_CLEAR)
		WaitBlit();

	logBlockEnd("bitmapCreate()");
	return pBitMap;
}

void bitmapLoadFromFile(tBitMap *pBitMap, char *szFilePath, UWORD uwStartX, UWORD uwStartY) {
	UWORD uwSrcWidth, uwDstWidth, uwSrcHeight;
	UBYTE ubSrcFlags, ubSrcBpp, ubSrcVersion;
	UWORD y;
	UWORD uwWidth;
	UBYTE ubPlane;
	FILE *pFile;
	
	logBlockBegin(
		"bitmapLoadFromFile(pBitMap: %p, szFilePath: %s, uwStartX: %u, uwStartY: %u)",
		pBitMap, szFilePath, uwStartX, uwStartY
	);
	
	// Open source bitmap
	pFile = fopen(szFilePath, "r");
	if(!pFile) {
		fclose(pFile);
		logWrite("File does not exist: %s\n", szFilePath);
		logBlockEnd("bitmapLoadFromFile()");
		return;
	}
	
	// Read header
	fread(&uwSrcWidth, 2, 1, pFile);
	fread(&uwSrcHeight, 2, 1, pFile);
	fread(&ubSrcBpp, 1, 1, pFile);
	fread(&ubSrcVersion, 1, 1, pFile);
	fread(&ubSrcFlags, 1, 1, pFile);
	fseek(pFile, 2, SEEK_CUR); // Skip unused 2 bytes
	if(ubSrcVersion != 0) {
		fclose(pFile);
		logWrite("Unknown file version: %hu\n", ubSrcVersion);
		logBlockEnd("bitmapLoadFromFile()");
		return;
	}
	
	// Interleaved check
	if(!!(ubSrcFlags & BITMAP_INTERLEAVED) != bitmapIsInterleaved(pBitMap)) {
		logWrite("ERR: Interleaved flag conflict\n");
		logBlockEnd("bitmapLoadFromFile()");
		return;
	}
	
	// Depth check
	if(ubSrcBpp > pBitMap->Depth) {
		logWrite(
			"ERR: Source has greater BPP than destination: %hu > %hu\n",
			ubSrcBpp, pBitMap->Depth
		);
		logBlockEnd("bitmapLoadFromFile()");
		return;
	}
	
	// Check bitmap dimensions
	uwDstWidth = bitmapGetWidth(pBitMap) << 3;
	if(uwStartX + uwSrcWidth > uwDstWidth || uwStartY + uwSrcHeight > (pBitMap->Rows)) {
		logWrite(
			"ERR: Source doesn't fit on dest: %ux%u @%u,%u > %ux%u\n",
			uwSrcWidth, uwSrcHeight,
			uwStartX, uwStartY,
			uwDstWidth, pBitMap->Rows
		);
		logBlockEnd("bitmapLoadFromFile()");
		return;
	}
	
	// Read data
	uwWidth = bitmapGetWidth(pBitMap);
	if(bitmapIsInterleaved(pBitMap)) {
		for(y = 0; y != uwSrcHeight; ++y)
			for(ubPlane = 0; ubPlane != pBitMap->Depth; ++ubPlane)
				fread(
					&pBitMap->Planes[0][
						uwWidth*((y*pBitMap->Depth)+ubPlane)+(uwStartX>>3)
					], ((uwSrcWidth+7)>>3), 1, pFile
				);
	}
	else {
		for(ubPlane = 0; ubPlane != pBitMap->Depth; ++ubPlane)
			for(y = 0; y != uwSrcHeight; ++y)
				fread(
					&pBitMap->Planes[ubPlane][
						uwWidth*(uwStartY+y) + (uwStartX>>3)
					], ((uwSrcWidth+7)>>3), 1, pFile
				);
	}
	fclose(pFile);
	logBlockEnd("bitmapLoadFromFile()");
}

tBitMap *bitmapCreateFromFile(char *szFilePath) {
	tBitMap *pBitMap;
	FILE *pFile;
	UWORD uwWidth, uwHeight;  // Image dimensions
	UBYTE ubVersion, ubFlags; // Format version & flags
	UBYTE ubPlaneCount;       // Bitplane count
	UBYTE i;
	
	logBlockBegin("bitmapCreateFromFile(szFilePath: %s)", szFilePath);
	pFile = fopen(szFilePath, "r");
	if(!pFile) {
		fclose(pFile);
		logWrite("File does not exist: %s\n", szFilePath);
		logBlockEnd("bitmapCreateFromFile()");
		return 0;
	}
	logWrite("Addr: %p\n",pBitMap);
	
	// Read header
	fread(&uwWidth, 2, 1, pFile);
	fread(&uwHeight, 2, 1, pFile);
	fread(&ubPlaneCount, 1, 1, pFile);
	fread(&ubVersion, 1, 1, pFile);
	fread(&ubFlags, 1, 1, pFile);
	fseek(pFile, 2, SEEK_CUR); // Skip unused 2 bytes
	if(ubVersion != 0) {
		fclose(pFile);
		logWrite("Unknown file version: %hu\n", ubVersion);
		logBlockEnd("bitmapCreateFromFile()");
		return 0;
	}
	
	// Init bitmap
	if(ubFlags & BITMAP_INTERLEAVED) {
		pBitMap = bitmapCreate(uwWidth, uwHeight, ubPlaneCount, BMF_INTERLEAVED);
		fread(pBitMap->Planes[0], 1, (uwWidth >> 3) * uwHeight * ubPlaneCount, pFile);
	}
	else {
		pBitMap = bitmapCreate(uwWidth, uwHeight, ubPlaneCount, 0);
		for (i = 0; i != ubPlaneCount; ++i)
			fread(pBitMap->Planes[i], 1, (uwWidth >> 3) * uwHeight, pFile);
	}
	fclose(pFile);
	
	logWrite("Dimensions: %ux%u@%uBPP, version: %hu, flags: %hu\n",uwWidth,uwHeight,ubPlaneCount,ubVersion,ubFlags);
	logBlockEnd("bitmapCreateFromFile()");
	return pBitMap;
}

void bitmapDestroy(tBitMap *pBitMap) {
	UBYTE i;
	logBlockBegin("bitmapDestroy(pBitMap: %p)", pBitMap);
	if (pBitMap) {
		WaitBlit();
		if(bitmapIsInterleaved(pBitMap))
			pBitMap->Depth = 1;
		for (i = pBitMap->Depth; i--;)
			memFree(pBitMap->Planes[i], pBitMap->BytesPerRow*pBitMap->Rows);
		memFree(pBitMap, sizeof(tBitMap));
	}
	logBlockEnd("bitmapDestroy()");
}

inline BYTE bitmapIsInterleaved(tBitMap *pBitMap) {
	return (pBitMap->Depth > 1 && ((ULONG)pBitMap->Planes[1] - (ULONG)pBitMap->Planes[0])*pBitMap->Depth == pBitMap->BytesPerRow);
}

void bitmapDump(tBitMap *pBitMap) {
	UBYTE i;
	
	logBlockBegin("bitmapDump(pBitMap: %p)", pBitMap);
	
	logWrite(
		"BytesPerRow: %u, Rows: %u, Flags: %hu, Depth: %hu, pad: %u\n",
		pBitMap->BytesPerRow, pBitMap->Rows, pBitMap->Flags,
		pBitMap->Depth, pBitMap->pad
	);
	for(i = 0; i != pBitMap->Depth; ++i)
		logWrite("Bitplane %hu addr: %p\n", i, pBitMap->Planes[i]);
	
	logBlockEnd("bitmapDump()");
}

void bitmapSaveBMP(tBitMap *pBitMap, UWORD *pPalette, char *szFilePath) {
	UWORD uwOut;
	UBYTE ubOut;
	ULONG ulOut;
	UWORD c;
	FILE *pOut;
	UWORD uwX, uwY;
	UBYTE pIndicesChunk[16];
	// TODO: EHB support
	
	pOut = fopen(szFilePath, "w");
	
	// BMP header
	fwrite("BM", 2, 1, pOut);

	ulOut = endianIntel32((pBitMap->BytesPerRow<<3) * pBitMap->Rows + 14+40+256*4);
	fwrite(&ulOut, sizeof(ULONG), 1, pOut); // BMP file size
	
	ulOut = 0;
	fwrite(&ulOut, sizeof(ULONG), 1, pOut); // Reserved
	
	ulOut = endianIntel32(14+40+256*4);
	fwrite(&ulOut, sizeof(ULONG), 1, pOut); // Bitmap data starting addr
	
	
	// Bitmap info header
	ulOut = endianIntel32(40);
	fwrite(&ulOut, sizeof(ULONG), 1, pOut); // Core header size
	
	ulOut = endianIntel32(pBitMap->BytesPerRow<<3);
	fwrite(&ulOut, sizeof(ULONG), 1, pOut); // Image width

	ulOut = endianIntel32(pBitMap->Rows);
	fwrite(&ulOut, sizeof(ULONG), 1, pOut); // Image height
	
	uwOut = endianIntel16(1);
	fwrite(&uwOut, sizeof(UWORD), 1, pOut); // Color plane count
	
	uwOut = endianIntel16(8);
	fwrite(&uwOut, sizeof(UWORD), 1, pOut); // Image BPP - 8bit indexed

	ulOut = endianIntel32(0);
	fwrite(&ulOut, sizeof(ULONG), 1, pOut); // Compression method - none	
	
	ulOut = endianIntel32((pBitMap->BytesPerRow<<3) * pBitMap->Rows);
	fwrite(&ulOut, sizeof(ULONG), 1, pOut); // Image size
	
	ulOut = endianIntel32(100);
	fwrite(&ulOut, sizeof(ULONG), 1, pOut); // Horizontal resolution - px/m
	
	ulOut = endianIntel32(100);
	fwrite(&ulOut, sizeof(ULONG), 1, pOut); // Vertical resolution - px/m
	
	ulOut = endianIntel32(0);
	fwrite(&ulOut, sizeof(ULONG), 1, pOut); // Palette length
	
	ulOut = endianIntel32(0);
	fwrite(&ulOut, sizeof(ULONG), 1, pOut); // Number of important colors - all	
	
	// Global palette
	for(c = 0; c != 1 << pBitMap->Depth; ++c) {
		ubOut = pPalette[c] & 0xF;
		ubOut |= ubOut << 4;
		fwrite(&ubOut, sizeof(UBYTE), 1, pOut); // B
		
		ubOut = (pPalette[c] >> 4) & 0xF;
		ubOut |= ubOut << 4;
		fwrite(&ubOut, sizeof(UBYTE), 1, pOut); // G
		
		ubOut = pPalette[c] >> 8;
		ubOut |= ubOut << 4;
		fwrite(&ubOut, sizeof(UBYTE), 1, pOut); // R
		
		ubOut = 0;
		fwrite(&ubOut, sizeof(UBYTE), 1, pOut); // 0
	}
	// Dummy fill up to 255 indices
	ulOut = 0;
	while(c < 256) {
		fwrite(&ulOut, sizeof(ULONG), 1, pOut);
		++c;
	}
		
	// Image data
	for(uwY = pBitMap->Rows; uwY--;) {
		for(uwX = 0; uwX < pBitMap->BytesPerRow<<3; uwX += 16) {
			planarRead16(pBitMap, uwX, uwY, pIndicesChunk);
			fwrite(pIndicesChunk, 16*sizeof(UBYTE), 1, pOut);
		}
		ubOut = 0;
		while(uwX & 0x3) {// 4-byte row padding
			fwrite(&ubOut, sizeof(UBYTE), 1, pOut);
			++uwX;
		}
	}	
	
	fclose(pOut);
}

UWORD bitmapGetWidth(tBitMap *pBitMap) {
	if(bitmapIsInterleaved(pBitMap)) {
		return ((ULONG)pBitMap->Planes[1] - (ULONG)pBitMap->Planes[0]);
	}
	return pBitMap->BytesPerRow;
}
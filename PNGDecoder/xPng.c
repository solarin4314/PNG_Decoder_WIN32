#include "xPng.h"
#include "xInflate.h"
#include "assert.h"

/* 
                           -= 작업 기록 =-

- 2010/05/06 코딩 시작
- 2010/05/10 24bit, 32bit 처리 및 필터 일부 구현
- 2010/05/11 FILTER_AVERAGE 구현. 1차 구현 완료
- 2010/05/13 최적화
	- 최적화전: 6780ms (XDib: 2300ms)
	- 멤버 변수를 로컬 변수화 : 5650ms
	- FILTER_SUB, 24비트 에서 테이블 참조 안하도록 수정 : 4860ms
	- FILTER_UP, 24비트 에서 테이블 참조 안하도록 수정 : 3150ms
	- FILTER_PAETH , 24비트 에서 테이블 참조 안하도록 수정 : 2800ms
	- 24BPP, 포인터 이동을 한번에 3바이트씩 : 2720ms
	- 최적화후: XDib(libpng) 대비 120~130% 정도 시간 걸림
- 2010/10/28
	- IDAT 가 여러개일때 처리못하던 버그 수정
	- ALPHA 채널 처리를 제대로 못하던 버그 수정
- 2010/11/01
	- 256 컬러(COLORTYPE_COLOR_PAL) 지원 추가
	- m_filter_b_dummy 추가
*/


#define DOERR(err)						{assert(0); enum m_err = err; return false;}
#define BE2HOST32(a)					((((a)&0xff)<<24)+(((a>>8)&0xff)<<16)+(((a>>16)&0xff)<<8)+(((a>>24)&0xff)))
#define BE2HOST16(a)					(((a)&0xff)<<8)+(((a>>8)&0xff))
#define XPNGFOURCC(ch0, ch1, ch2, ch3)  ((int)(unsigned char)(ch0) | ((int)(unsigned char)(ch1) << 8) | ((int)(unsigned char)(ch2) << 16) | ((int)(unsigned char)(ch3) << 24 ))
#define XPNG_ABS(x)						(x)< 0 ?  -(x) : (x)


#ifndef _XPNG_NO_CRC
	static const int _CRC32[256] = {0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,      0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,      0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,      0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,      0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,      0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,      0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,      0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,      0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,      0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,      0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,      0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,      0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,      0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,      0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,      0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,      0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,      0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,      0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,      0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,      0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,      0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,      0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,      0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,      0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,      0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,      0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,      0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,      0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,      0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,      0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,      0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,      0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,      0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,      0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,      0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,      0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,      0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d };
	#define DO1(buf) crc = _CRC32[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
	#define DO8(buf)  DO1(buf); DO1(buf); DO1(buf); DO1(buf); DO1(buf); DO1(buf); DO1(buf); DO1(buf); 
	static int CRC32(int crc, const unsigned char *buf, unsigned int len)
	{
		if (buf == NULL) return 0L;
		crc = crc ^ 0xffffffffL;
		while (len >= 8) {DO8(buf);len -= 8;}
		if(len) do {DO1(buf);} while (--len);
		return crc ^ 0xffffffffL;
	}
#endif
/*
// RGB BGR 변환 테이블
static const int	colorMapTable_RGBA_2_BGRA[] = {2, 0, -2, 0};
static const int	colorMapTable_RGB_2_BGR[] = {2, 0, -2};*/


// 청크 헤더 구조체
struct XPngChunkHeader
{
	int	length;							// chunk data 필드의 길이
	int	chunkType;
};
void Destory_png(struct XPng *xpng)
{
	free(xpng->m_pInflate);

	//free(xpng->m_imageBuffer);
	//free(xpng->m_imageCurBuffer);
	//free(xpng->m_scanline_before);
	//free(xpng->m_scanline_current);
	//free(xpng->m_filter_b);

	if(xpng->m_bPaletted)
		free(xpng->m_palette);

	//free(xpng->m_trans);

	free(xpng->m_filter_b_dummy);
}
void Init_png(struct XPng *xpng)
{
	xpng->m_err = XPNG_ERR_OK;
#ifndef _XPNG_NO_CRC
	xpng->m_crc32 = 0;
#endif
	xpng->m_bEnd = false;

	//xpng->m_imageBuffer = (unsigned char*)malloc(sizeof(unsigned char *) * 4);
	xpng->m_imageStride = 0;
	xpng->m_imageWidthByte = 0;
	xpng->m_imageBytePerPixel = 0;

	//xpng->m_imageCurBuffer = (unsigned char*)calloc(1, sizeof(unsigned char *) * 4);
	xpng->m_imageCurWidthByte = 0;
	xpng->m_imagePaddingByte = 0;

	xpng->m_bFirst = true;

	xpng->m_ColorMapTableCur;
	xpng->m_ColorMapTablePos = 0;
	xpng->m_ColorMapTableLen = 0;

	//xpng->m_scanline_before = (unsigned char*)calloc(1, sizeof(unsigned char *) * 4);
	//xpng->m_scanline_current = (unsigned char*)calloc(1, sizeof(unsigned char *) * 4);

	xpng->m_filter = 0;
	//xpng->m_filter_b = (unsigned char*)calloc(1, sizeof(unsigned char *) * 4);
	//xpng->m_palette = (unsigned char*)calloc(1, sizeof(unsigned char *) * 4);
	//xpng->m_trans = (unsigned char*)calloc(1, sizeof(unsigned char *) * 4);

}
////////////////////////////////////////////////////////////////////////////////////////////////////
///         4바이트 정수 읽기
/// @param  
/// @return 
/// @date   Thursday, May 06, 2010  11:42:55 AM
////////////////////////////////////////////////////////////////////////////////////////////////////
void CRCInit(struct XPng *xpng)
{
#ifndef _XPNG_NO_CRC
	xpng->m_crc32 = 0;
#endif
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         청크 맨 마지막의 crc 를 읽어서 체크한다.
/// @param  
/// @return 
/// @date   Thursday, May 06, 2010  12:15:54 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CheckCRC(struct XPng *xpng)
{
	int	crc32;

	//if(m_pStream->Read((BYTE*)&crc32, sizeof(UINT32))==FALSE)
	//{ASSERT(0); return FALSE;}

#ifndef _XPNG_NO_CRC

	// 엔디안 처리
	crc32 = BE2HOST32(crc32);

	// CRC 정보가 동일한가?
	if(crc32!=xpng->m_crc32)
	{ return false;}

#endif

	return true;
}




////////////////////////////////////////////////////////////////////////////////////////////////////
///         이미지 데이터를 읽는다
/// @param  
/// @return 
/// @date   Thursday, May 06, 2010  12:32:25 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
bool Decode(struct XPng *xpng)
{
	
	//struct XPngChunkHeader	chunkHeader;
	//bool			bSkipChunk;

	//if(xpng->m_bEnd) 
	//{assert(0); return false;}


	//for(;;)
	//{
	//	if(ReadChunkHeader(xpng, &chunkHeader)==false)
	//	{
	//		//DOERR(XPNG_ERR_CANT_READ);
	//	}

	//	bSkipChunk = false;

	//	switch(chunkHeader.chunkType)
	//	{
	//	case XPNGFOURCC('s', 'R', 'G', 'B' ) :
	//	case XPNGFOURCC('g', 'A', 'M', 'A' ) :
	//	case XPNGFOURCC('p', 'H', 'Y', 's' ) :
	//	case XPNGFOURCC('t', 'E', 'X', 't' ) :
	//	case XPNGFOURCC('t', 'I', 'M', 'E' ) :
	//	case XPNGFOURCC('i', 'C', 'C', 'P' ) :
	//	case XPNGFOURCC('s', 'B', 'I', 'T' ) :
	//	case XPNGFOURCC('b', 'K', 'G', 'D' ) :
	//	case XPNGFOURCC('o', 'F', 'F', 's' ) :
	//	case XPNGFOURCC('p', 'C', 'A', 'L' ) :
	//	case XPNGFOURCC('s', 'T', 'E', 'R' ) :
	//	case XPNGFOURCC('v', 'p', 'A', 'g' ) :
	//	case XPNGFOURCC('z', 'T', 'X', 't' ) :
	//	case XPNGFOURCC('c', 'H', 'R', 'M' ) :
	//		bSkipChunk = true;					// 무시한다.
	//		break;

	//	case XPNGFOURCC('I', 'D', 'A', 'T' ) :
	//		if(ProcessIDATChunk(xpng, chunkHeader.length)==false)
	//		{assert(0); return false;}
	//		break;
	//		
	//	case XPNGFOURCC('I', 'E', 'N', 'D' ) :
	//		xpng->m_bEnd = true;
	//		// 정상 종료
	//		return true;

	//	case XPNGFOURCC('P', 'L', 'T', 'E' ) :
	//		if(ProcessPalette(xpng, chunkHeader.length)==false)
	//		{assert(0); return false;}
	//		break;

	//	case XPNGFOURCC('t', 'R', 'N', 'S' ) :
	//		if(ProcessTrans(xpng, chunkHeader.length)==false)
	//		{assert(0); return false;}
	//		break;

	//	default :
	//		assert(0);
	//		bSkipChunk = true;					// 무시한다.
	//		break;
	//	}

	//	if(bSkipChunk)
	//	{
	//		// skip chunk;
	//		//if(	m_pStream->SkipData(chunkHeader.length)==false ||		// chunk data
	//		//	m_pStream->SkipData(sizeof(UINT32))==false)				// chunk crc;
	//		//{
	//		//	DOERR(XPNG_ERR_CANT_READ);
	//		//}
	//	}
	//}

	return false;
}


void SetDecodeData(struct XPng *xpng, unsigned char* imageBuffer, int stride, int paddingByte, int bytePerPixel)
{
	xpng->m_imageBuffer = imageBuffer;
	xpng->m_imageStride = stride;
	xpng->m_imageWidthByte = stride - paddingByte;
	xpng->m_imageBytePerPixel = bytePerPixel;
	xpng->m_imagePaddingByte = paddingByte;

	xpng->m_imageCurBuffer = imageBuffer;
	xpng->m_imageCurWidthByte = xpng->m_imageWidthByte;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// PNG 이미지 디코더 클래스
///
/// - 라이선스 : zlib license (http://www.kippler.com/etc/zlib_license/ 참고)
///
/// - 참고 자료
///   . RFC : http://www.faqs.org/rfcs/rfc2083.html
///   . TR : http://www.w3.org/TR/PNG/
/// 
/// @author   kippler@gmail.com
/// @date     Thursday, May 06, 2010  10:09:06 AM
/// 
/// Copyright(C) 2010 Bandisoft, All rights reserved.
/// 
////////////////////////////////////////////////////////////////////////////////////////////////////
/* 
	지원하는 스펙
		- RGB24, RGBA32 컬러 지원
		- RGB24 는 BGR 순서, RGBA32 는 BGRA 순서 사용(HBITMAP 과 동일)

	지원하지 않는 스펙
		- 컬러 심도는 8BIT만 지원 (1,2,4,16 지원 안함)
		- 인터레이스 이미지 지원 안함
		- 흑백 이미지 지원 안함

	PREDEFINE
		#define		_XPNG_NO_CRC		// <- 이걸 정의하면 crc 체크를 안한다. (속도 향상)
*/
#pragma once
typedef enum {false, true} bool;

enum XPNG_ERR
{
	XPNG_ERR_OK,
	XPNG_ERR_NOT_PNG_FILE,					// PNG 파일이 아님
	XPNG_ERR_CANT_READ_HEADER,				// 헤더 읽던중 에러 발생
	XPNG_ERR_UNSUPPORTED_FORMAT,			// 아직 지원하지 않는 포맷
	XPNG_ERR_CANT_READ,						// 데이타를 읽던중 에러 발생 (파일 뒷부분이 잘렸다던가..)
	XPNG_ERR_INVALID_CRC,					// crc 에러 발생.
	XPNG_ERR_INVALID_DATA,					// 잘못된 데이터 정보가 있음
};


struct XPngImageHeader						// 이미지 정보 (IHDR 정보)
{
	int	width;
	int	height;
	unsigned char	bitDepth;						// Bit depth is a single-byte integer giving the number of bits per sample or 
											// per palette index (not per pixel).
	unsigned char	colorType;						// COLORTYPE_COLOR_ALPHA 혹은 COLORTYPE_COLOR
	unsigned char	compressionMethod;
	unsigned char	filterMode;
	unsigned char	interlaceMethod;
};


struct XInflate;
struct XPngChunkHeader;

struct XPng;

typedef struct XPng{

	enum {	LEN_PNG_SIGNATURE = 8 } SIG;
	enum {	RGBA = 4 } RGBA;

			// 컬러	타입 - 각각의 비트 의미 [ 1:palette used , 2:color used , 4:alpha channel used ]
	enum {	COLORTYPE_BW = 0,				
			COLORTYPE_COLOR = 2,			// RGB, no alpha, no pal, 
			COLORTYPE_COLOR_PAL = 3,		// palette used + color used
			COLORTYPE_BW_ALPHA = 4,			// no color, alpha channel
			COLORTYPE_COLOR_ALPHA = 6} COLORTYPE;		// RGBA

	enum {	FILTER_NONE		= 0, 
			FILTER_SUB		= 1,
			FILTER_UP		= 2,
			FILTER_AVERAGE	= 3,
			FILTER_PAETH	= 4 } FILTER;

	enum {	LEN_TRANS		= 1 } TRANS;

	//struct XPngStream*			m_pStream;
	enum XPNG_ERR			m_err;
	struct XPngImageHeader		m_header;
#ifndef _XPNG_NO_CRC
	int				m_crc32;
#endif
	struct XInflate*			m_pInflate;
	bool				m_bEnd;

	unsigned char*				m_imageBuffer;
	int					m_imageStride;						// 한 라인과 다음 라인의 메모리 간격
	int					m_imageBytePerPixel;
	int					m_imageWidthByte;					// 한 라인이 차지하는 버퍼 바이트 수 == (m_imageStride - m_imagePaddingByte)
	int					m_imagePaddingByte;

	unsigned char*				m_imageCurBuffer;
	int					m_imageCurWidthByte;

	bool				m_bFirst;
	int					m_nBytePerPixel;					// 픽셀당 바이트수

	const int*			m_ColorMapTableCur;
	int					m_ColorMapTablePos;					// m_ColorMapTableCur 배열내 현재 위치 - 24비트일 경우 0~2 , 32비트일 경우 0~3
	int					m_ColorMapTableLen;

	unsigned char*				m_scanline_before;
	unsigned char*				m_scanline_current;

	// 필터
	unsigned char				m_filter;				// 현재 라인의 필터 종류
	unsigned char				m_filter_a[RGBA];		// 좀전 픽셀 정보
	unsigned char*				m_filter_b;				// 이전 스캔라인 포인터
	unsigned char				m_filter_c[RGBA];		// 좌상단 픽셀 정보
	unsigned char*				m_filter_b_dummy;		// 임시 버퍼 - 
												// 맨 처음 시작되는 필터가 FILTER_NONE 가 아니고, m_filter_b 를 참조하는 필터
												// (FILTER_PAETH와 같은)인 경우, 이전 필터 참조시 에러가 발생하는것을 막기 위해서
												// 임시로 0 으로 채워서 사용한다.

	bool						m_bPaletted;			// 팔레트 쓰기
	unsigned char*				m_palette;				// palette 테이블 (256*3)
	unsigned char*				m_trans;				// alpha 테이블 (256)

} XPng;
void Init_png(struct XPng *xpng);
void Destory_png(struct XPng *xpng);


void CRCInit(struct XPng* png);
bool CheckCRC(struct XPng *xpng);

bool Decode(struct XPng* xpng);
void SetDecodeData(struct XPng*, unsigned char* imageBuffer, int stride, int paddingByte, int bytePerPixel);
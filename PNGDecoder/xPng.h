////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// PNG �̹��� ���ڴ� Ŭ����
///
/// - ���̼��� : zlib license (http://www.kippler.com/etc/zlib_license/ ����)
///
/// - ���� �ڷ�
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
	�����ϴ� ����
		- RGB24, RGBA32 �÷� ����
		- RGB24 �� BGR ����, RGBA32 �� BGRA ���� ���(HBITMAP �� ����)

	�������� �ʴ� ����
		- �÷� �ɵ��� 8BIT�� ���� (1,2,4,16 ���� ����)
		- ���ͷ��̽� �̹��� ���� ����
		- ��� �̹��� ���� ����

	PREDEFINE
		#define		_XPNG_NO_CRC		// <- �̰� �����ϸ� crc üũ�� ���Ѵ�. (�ӵ� ���)
*/
#pragma once
typedef enum {false, true} bool;

enum XPNG_ERR
{
	XPNG_ERR_OK,
	XPNG_ERR_NOT_PNG_FILE,					// PNG ������ �ƴ�
	XPNG_ERR_CANT_READ_HEADER,				// ��� �д��� ���� �߻�
	XPNG_ERR_UNSUPPORTED_FORMAT,			// ���� �������� �ʴ� ����
	XPNG_ERR_CANT_READ,						// ����Ÿ�� �д��� ���� �߻� (���� �޺κ��� �߷ȴٴ���..)
	XPNG_ERR_INVALID_CRC,					// crc ���� �߻�.
	XPNG_ERR_INVALID_DATA,					// �߸��� ������ ������ ����
};


struct XPngImageHeader						// �̹��� ���� (IHDR ����)
{
	int	width;
	int	height;
	unsigned char	bitDepth;						// Bit depth is a single-byte integer giving the number of bits per sample or 
											// per palette index (not per pixel).
	unsigned char	colorType;						// COLORTYPE_COLOR_ALPHA Ȥ�� COLORTYPE_COLOR
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

			// �÷�	Ÿ�� - ������ ��Ʈ �ǹ� [ 1:palette used , 2:color used , 4:alpha channel used ]
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
	int					m_imageStride;						// �� ���ΰ� ���� ������ �޸� ����
	int					m_imageBytePerPixel;
	int					m_imageWidthByte;					// �� ������ �����ϴ� ���� ����Ʈ �� == (m_imageStride - m_imagePaddingByte)
	int					m_imagePaddingByte;

	unsigned char*				m_imageCurBuffer;
	int					m_imageCurWidthByte;

	bool				m_bFirst;
	int					m_nBytePerPixel;					// �ȼ��� ����Ʈ��

	const int*			m_ColorMapTableCur;
	int					m_ColorMapTablePos;					// m_ColorMapTableCur �迭�� ���� ��ġ - 24��Ʈ�� ��� 0~2 , 32��Ʈ�� ��� 0~3
	int					m_ColorMapTableLen;

	unsigned char*				m_scanline_before;
	unsigned char*				m_scanline_current;

	// ����
	unsigned char				m_filter;				// ���� ������ ���� ����
	unsigned char				m_filter_a[RGBA];		// ���� �ȼ� ����
	unsigned char*				m_filter_b;				// ���� ��ĵ���� ������
	unsigned char				m_filter_c[RGBA];		// �»�� �ȼ� ����
	unsigned char*				m_filter_b_dummy;		// �ӽ� ���� - 
												// �� ó�� ���۵Ǵ� ���Ͱ� FILTER_NONE �� �ƴϰ�, m_filter_b �� �����ϴ� ����
												// (FILTER_PAETH�� ����)�� ���, ���� ���� ������ ������ �߻��ϴ°��� ���� ���ؼ�
												// �ӽ÷� 0 ���� ä���� ����Ѵ�.

	bool						m_bPaletted;			// �ȷ�Ʈ ����
	unsigned char*				m_palette;				// palette ���̺� (256*3)
	unsigned char*				m_trans;				// alpha ���̺� (256)

} XPng;
void Init_png(struct XPng *xpng);
void Destory_png(struct XPng *xpng);


void CRCInit(struct XPng* png);
bool CheckCRC(struct XPng *xpng);

bool Decode(struct XPng* xpng);
void SetDecodeData(struct XPng*, unsigned char* imageBuffer, int stride, int paddingByte, int bytePerPixel);
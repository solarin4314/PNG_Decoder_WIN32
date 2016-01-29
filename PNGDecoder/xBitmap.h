#include "stdio.h"
#include "stdlib.h"

#pragma once

#define	PIXELFORMAT_24  24
#define	PIXELFORMAT_32  32

struct stBMFH // BitmapFileHeader & BitmapInfoHeader
{
	// BitmapFileHeader
	//char         bmtype[2];     // 2 bytes - 'B' 'M'
	unsigned int iFileSize;     // 4 bytes
	short int    reserved1;     // 2 bytes
	short int    reserved2;     // 2 bytes
	unsigned int iOffsetBits;   // 4 bytes
	// End of stBMFH structure - size of 14 bytes
	// BitmapInfoHeader
	unsigned int iSizeHeader;    // 4 bytes - 40
	unsigned int iWidth;         // 4 bytes
	unsigned int iHeight;        // 4 bytes
	short int    iPlanes;        // 2 bytes
	short int    iBitCount;      // 2 bytes
	unsigned int Compression;    // 4 bytes
	unsigned int iSizeImage;     // 4 bytes
	unsigned int iXPelsPerMeter; // 4 bytes
	unsigned int iYPelsPerMeter; // 4 bytes
	unsigned int iClrUsed;       // 4 bytes
	unsigned int iClrImportant;  // 4 bytes
	// End of stBMIF structure - size 40 bytes
	// Total size - 54 bytes
};
struct XBitmap
{
	int			m_stride;					// ���ٴ� ����Ʈ��
	int			m_paddingByte;				// ���ٴ� padding ����Ʈ ��
	int			m_width;					// �� pixel
	int			m_height;					// ���� pixel

	int	m_pixelFormat;				// �ȼ��� ��Ʈ ��
	unsigned char* m_pixel;					// �ȼ� �޸� ���� ��

	int			m_bitSize;				// ��Ʈ�� ���� ������

	FILE *fp;

};
void Init_Bmp(struct XBitmap* xbitmap);
void Destroy_Bmp(struct XBitmap* xbitmap);

void CreateDib(struct XBitmap* xbitmap, int width, int height, int pixelFormat);


void MakeBMPHeader(struct XBitmap* xbitmap, const char* bmpFileName);
void WriteBMFile(struct XBitmap* xbitmap);
void WriteOnePixel(struct XBitmap* xbitmap, unsigned char* buffer);

void CloseBMFile(struct XBitmap* xbitmap);

int GetRGBSize(struct XBitmap *xbitmap);
#include "xBitmap.h"

void Destroy_Bmp(struct XBitmap* xbitmap)
{
	free(xbitmap->m_pixel);
}
void Init_Bmp(struct XBitmap* xbitmap)
{
	xbitmap->m_stride = 0;					// ���ٴ� ����Ʈ��
	xbitmap->m_paddingByte = 0;				// ���ٴ� padding ����Ʈ ��
	xbitmap->m_width = 0;					// �� pixel
	xbitmap->m_height = 0;					// ���� pixel
	xbitmap->m_pixelFormat = 0;				// �ȼ��� ��Ʈ ��
	xbitmap->m_pixel = (unsigned char*)calloc(1, sizeof(unsigned char) * 320 * 240 * 4);					// �ȼ� �޸� ���� ��
	xbitmap->m_bitSize = 0;					// ��Ʈ�� ���� ������
}
void CreateDib(struct XBitmap* xbitmap, int width, int height, int pixelFormat)
{
	xbitmap->m_stride = ((width * (int)pixelFormat + 31) & ~31) >> 3;			// 8�� ���....width * RGB(3) or width * RGB(4)
	xbitmap->m_width = width;
	xbitmap->m_height = height;
	xbitmap->m_pixelFormat = pixelFormat;										// 24bit? 32bit?
	xbitmap->m_paddingByte = xbitmap->m_stride - (xbitmap->m_width * pixelFormat / 8);				// �е� ����Ʈ ���
}
void WriteOnePixel(struct XBitmap* xbitmap, unsigned char* buffer)
{
	fwrite(buffer, 1, 1, xbitmap->fp);
}
void WriteBMFile(struct XBitmap* xbitmap)
{
	// �ٷ� ����
	//fwrite(xbitmap->m_pixel, GetRGBSize(xbitmap), 1, xbitmap->fp);
	
	// ������ ����
	int x, y;
	int bmpDataWidth = xbitmap->m_width * xbitmap->m_pixelFormat / 8;

	for (y=xbitmap->m_height-1; y>=0; y--)
	{
		for (x=0; x<bmpDataWidth; x+= xbitmap->m_pixelFormat / 8)
		{
			fwrite(&xbitmap->m_pixel[bmpDataWidth * y + x], 1, 1, xbitmap->fp);
			fwrite(&xbitmap->m_pixel[bmpDataWidth * y + x+1], 1, 1, xbitmap->fp);
			fwrite(&xbitmap->m_pixel[bmpDataWidth * y + x+2], 1, 1, xbitmap->fp);
			
			// Alpha ���
			if(xbitmap->m_pixelFormat > 24)
				fwrite(&xbitmap->m_pixel[bmpDataWidth * y + x+3], 1, 1, xbitmap->fp);
		}
		if (((4 - (xbitmap->m_width * xbitmap->m_pixelFormat / 8) % 4) % 4)>0)
		{
			unsigned char pad = 0;
			fwrite(&pad, (4 - (xbitmap->m_width * xbitmap->m_pixelFormat / 8) % 4), 1, xbitmap->fp);
		}
	}
}
// Bitmap Pixel Data Size (bm������� ���)
int GetRGBSize(struct XBitmap *xbitmap)
{
	if(xbitmap->m_bitSize < 1)
		return (xbitmap->m_width*xbitmap->m_height*xbitmap->m_pixelFormat / 8) + ((4 - (xbitmap->m_width * xbitmap->m_pixelFormat / 8) % 4) % 4);
	else
		return xbitmap->m_bitSize;
}
void MakeBMPHeader(struct XBitmap* xbitmap, const char* bmpFileName)
{
	struct stBMFH bh;

	char bfType[2] = {'B', 'M'}; // BM

	int iNumPaddedBytes = 4 - (xbitmap->m_width * xbitmap->m_pixelFormat / 8) % 4; // 00 padding
	iNumPaddedBytes = iNumPaddedBytes % 4;


	memset(&bh, 0, sizeof(bh));


	xbitmap->fp = fopen(bmpFileName, "wb");

	fwrite(bfType, 2, 1, xbitmap->fp);	// ��Ʈ��id(BM : 42 4D) write 

	bh.iFileSize = GetRGBSize(xbitmap) + sizeof(bh) + 2; // ���ϻ�����


	bh.iOffsetBits = sizeof(bh) + 2; // header(52 + 2)
	bh.iSizeHeader = 40; // Image Header
	bh.iPlanes = 1; // plane 1
	bh.iWidth = xbitmap->m_width; 
	bh.iHeight = xbitmap->m_height;
	bh.iBitCount = xbitmap->m_pixelFormat; // 24��Ʈ �Ǵ� 32��Ʈ bitmap
	bh.iSizeImage = GetRGBSize(xbitmap); // pixelData Size
	fwrite(&bh, sizeof(bh), 1, xbitmap->fp); // bmp��� write

}
void CloseBMFile(struct XBitmap* xbitmap)
{
	fclose(xbitmap->fp);
}
#include "stdio.h"
#include "xPng.h"
#include "xInflate.h"
#include "xBitmap.h"

#define BYTE_TO_CHAR(x) ( (x)[0])
#define BYTE_TO_WORD(x) ( ((x)[0]<<8)|(x)[1])
#define BYTE_TO_INT(x)  ( ((x)[0]<<24) | ((x)[1]<<16) | ((x)[2] << 8) | ((x)[3]) << 0) 

//#define INPUT_PNG "c:\\DABImages\\DAB_PNG\\gorilla_1.png"
//#define OUTPUT_BMP "c:\\DABImages\\test\\gorilla_1.bmp"
//define INPUT_PNG "c:\\png\\piu.png"
//#define OUTPUT_BMP "c:\\toBmp\\piu.bmp"
#define INPUT_PNG "c:\\png\\test.png"
#define OUTPUT_BMP "c:\\toBmp\\test.bmp"

#define XPNG_ABS(x)						(x)< 0 ?  -(x) : (x)


#define IHDR (0x49 << 24) | (0x48 << 16) | (0x44 << 8) | (0x52 << 0)
#define PLTE (0x50 << 24) | (0x4C << 16) | (0x54 << 8) | (0x45 << 0) 
#define IDAT (0x49 << 24) | (0x44 << 16) | (0x41 << 8) | (0x54 << 0)
#define IEND (0x49 << 24) | (0x45 << 16) | (0x4E << 8) | (0x44 << 0)

#define tIME (0x74 << 24) | (0x49 << 16) | (0x4d << 8) | (0x45 << 0)
#define pHYs (0x70 << 24) | (0x48 << 16) | (0x59 << 8) | (0x73 << 0)
#define gAMA (0x67 << 24) | (0x41 << 16) | (0x4d << 8) | (0x41 << 0)
#define tEXt (0x74 << 24) | (0x45 << 16) | (0x58 << 8) | (0x74 << 0)
#define sRGB (0x73 << 24) | (0x52 << 16) | (0x47 << 8) | (0x42 << 0)

//#define sBIT
//#define bKGD
//#define oFFs
//#define pCAL
//#define sTER
//#define vpAg
//#define zTXt
//#define cHRM



////////////////////////////////////////////////
//
// �ߺ� �Ǵ� �κ� - �ӵ��� ���ؼ� ��ũ�η� ó��
//
#define MOVETONEXTBUFFER													\
	imageCurBuffer++;													\
	imageCurWidthByte--;												\
	ColorMapTablePos = (ColorMapTablePos+1) % ColorMapTableLen;			\
	\
	outBuf++;															\
	outBufLen--;														\
	\
	if(imageCurWidthByte==0)											\
		{																	\
		imageCurBuffer += imagePaddingByte;								\
		imageCurWidthByte = imageWidthByte;								\
		break;															\
		}

#define MOVETONEXTBUFFER_4_PAL												\
	imageCurBuffer+=4;													\
	imageCurWidthByte-=4;												\
	\
	outBuf++;															\
	outBufLen--;														\
	\
	if(imageCurWidthByte==0)											\
		{																	\
		imageCurBuffer += imagePaddingByte;								\
		imageCurWidthByte = imageWidthByte;								\
		break;															\
		}

#define MOVETONEXTBUFFER_3_PAL												\
	imageCurBuffer+=3;													\
	imageCurWidthByte-=4;												\
	\
	outBuf++;															\
	outBufLen--;														\
	\
	if(imageCurWidthByte==0)											\
		{																	\
		imageCurBuffer += imagePaddingByte;								\
		imageCurWidthByte = imageWidthByte;								\
		break;															\
		}


// ���̺� ó���� ���� �ʴ´�.
#define MOVETONEXTBUFFER_FAST(len)											\
	imageCurWidthByte-=len;												\
	outBufLen-=len;														\
	outBuf +=len ;														\
	imageCurBuffer += len;

int ConvertPngFile(const char* pngFileName, const char* bmpFileName);
void DecodePngFileData(const unsigned char* buf, const int sizeBuf, const char* bmpFileName);

void IHDRParse(struct XPng* xpng, struct XBitmap* bitmap, const unsigned char* stream, const char* bmpFileName);
void PLTEParse(struct XPng* png, const unsigned char* stream, int dataLength);
int IDATParse(struct XPng* png, struct XInflate* inflate, struct XBitmap* bitmap, const unsigned char* stream, int dataLength);
void IENDParse(struct XBitmap* bitmap);
unsigned int FileSize(FILE *fp);
int PngParseHeader(const unsigned char* buf, unsigned int size, const char* bmpFileName);

int ParsePng(const unsigned char* buffer, const unsigned char *stream, const char* bmpFileName);

// RGB BGR ��ȯ ���̺�
static const int	colorMapTable_RGBA_2_BGRA[] = {2, 0, -2, 0};
static const int	colorMapTable_RGB_2_BGR[] = {2, 0, -2};

int main (int argc, char* argv[]){


	if(argv[1] == NULL || argv[2] == NULL)
	{
		ConvertPngFile(INPUT_PNG, OUTPUT_BMP);
	}
	else
	{
		ConvertPngFile(argv[1], argv[2]);
	}
	return 0;
}

int ConvertPngFile(const char* pngFileName, const char* bmpFileName)
{
	FILE *fp;
	unsigned char *buf;			// ����
	unsigned int lengthOfFile;	// ����ũ��

	// ���Ͽ���
	fp = fopen(pngFileName, "rb");
	if (fp == NULL){
		printf("Cannot open png file: %s\n", pngFileName);
		return -1;
	}
	// ���ϻ����� ����
	lengthOfFile = FileSize(fp);

	// ���� �ʱ�ȭ
	buf = (unsigned char*)malloc(sizeof(unsigned char) * lengthOfFile + 4);

	if (buf == NULL){
		printf("Not enough memory for loading file\n");
		return -1;
	}
	// ���ۿ� png ������ ����
	fread(buf, lengthOfFile, 1, fp);
	// ���ϴݱ�
	fclose(fp);

	DecodePngFileData(buf, lengthOfFile, bmpFileName);

	// ���� ������
	free(buf);

	return 0;

}
// ���ϻ����� ���ϱ�
unsigned int FileSize(FILE *fp)
{
	long pos;
	fseek(fp, 0, SEEK_END);
	pos = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	return pos;
}

void DecodePngFileData(const unsigned char* buf, const int sizeBuf, const char* bmpFileName)
{
	if(PngParseHeader(buf, sizeBuf, bmpFileName)<0)
	{
		printf("ERROR > parsing png header\n");
		exit(1);
	}
}
int PngParseHeader(const unsigned char* buf, unsigned int size, const char* bmpFileName)
{
	const unsigned char* startStream; // ��Ʈ��

	// png identifier
	if ((buf[0] != 0x89) || (buf[1] != 0x50) || (buf[2] != 0x4E) || (buf[3] != 0x47) || (buf[4] != 0x0D) || (buf[5] != 0x0A) || (buf[6] != 0x1A) || (buf[7] != 0x0A))
	{
		printf("Not a PNG file..\n");
		return -1;
	}
	printf("Png File!! \n");

	// 89(1) + 50(1) + png identifier(6) = 8 ��Ʈ�� �̵�
	startStream = buf+8;
	// �Ľ� ����
	if(ParsePng(buf, startStream, bmpFileName) > 0)
	{
		printf("parse success!!\n");
		return 0;
	}
	else
	{
		printf("parse fail!!\n");
		return -1;
	}
}
int ParsePng(const unsigned char* buffer, const unsigned char *stream, const char* bmpFileName)
{
	int chunkLength;
	int marker;
	int end_marker = 0;

	// xpng, bitmap �ʱ�ȭ
	struct XPng *xpng;
	struct XBitmap *xbitmap;

	xpng = (struct XPng *)malloc(sizeof(struct XPng));
	Init_png(xpng);

	xbitmap = (struct XBitmap *)malloc(sizeof(struct XBitmap));
	Init_Bmp(xbitmap);

	// xpng �� ����� inflate �ʱ�ȭ
	xpng->m_pInflate = (struct XInflate *)calloc(1, sizeof(struct XInflate));
	Init_inf(xpng->m_pInflate);

	// end_marker�� ���������� ����
	while(!end_marker)
	{
		chunkLength = BYTE_TO_INT(stream); // ûũ�� ����
		marker = BYTE_TO_INT(stream+4);	   // ûũ��

		switch(marker)
		{
		case IHDR:
			stream += 4 + 4; // data length(4) + chunk length(4)

			printf("HEAD parsing..length : %d\n", chunkLength);
			IHDRParse(xpng, xbitmap, stream, bmpFileName);
			stream += chunkLength; // chunk size
			break;

		case sRGB:
			stream += 4 + 4; // data length(4) + chunk length(4)
			printf("sRGB parsing..length : %d\n", chunkLength);
			stream += chunkLength; // chunk size
			break;

		case pHYs:
			stream += 4 + 4; // data length(4) + chunk length(4)
			printf("pHYs parsing..length : %d\n", chunkLength);
			stream += chunkLength; // chunk size
			break;

		case gAMA:
			stream += 4 + 4; // data length(4) + chunk length(4)
			printf("gAMA parsing..length : %d\n", chunkLength);
			stream += chunkLength; // chunk size
			break;

		case PLTE:
			stream += 4 + 4; // data length(4) + chunk length(4)
			printf("PLTE parsing..length : %d\n", chunkLength);
			PLTEParse(xpng, stream, chunkLength);
			stream += chunkLength; // chunk size
			break;

		case IDAT:
			stream += 4 + 4; // data length(4) + chunk length(4)
			printf("IDAT parsing..length : %d\n", chunkLength);

			if(!xpng->m_bEnd)
			{
				stream += 2;
				chunkLength -= 2;

				xpng->m_filter_b_dummy = (unsigned char*)calloc(1, xpng->m_imageStride);

				// IDATParse �ȿ��� true�� �ٲ۴�
				//xpng->m_bEnd = true;
			}

			IDATParse(xpng, xpng->m_pInflate, xbitmap, stream, chunkLength);

			stream += chunkLength; // chunk size
			break;

		case tIME:
			stream += 4 + 4; // data length(4) + chunk length(4)
			printf("tIME parsing..length : %d\n", chunkLength);
			stream += chunkLength; // chunk size
			break;

		case tEXt:
			stream += 4 + 4; // data length(4) + chunk length(4)
			printf("tEXt parsing..length : %d\n", chunkLength);
			stream += chunkLength; // chunk size
			break;

		case IEND:
			end_marker = 1;

			IENDParse(xbitmap);

			printf("IEND.....\n");
			break;
		default:

			break;
		}

		stream += 4; // CRC(4)


	}

	// png, bitmap ����
	Destroy_inf(xpng->m_pInflate);
	Destory_png(xpng);
	free(xpng);

	Destroy_Bmp(xbitmap);
	free(xbitmap);



	return 1;
}

void IHDRParse(struct XPng* xpng, struct XBitmap* xbitmap, const unsigned char* stream,const char* bmpFileName)
{
	int pixelFormat;

	xpng->m_header.width= BYTE_TO_INT(stream);
	xpng->m_header.height = BYTE_TO_INT(stream+4);
	xpng->m_header.bitDepth = BYTE_TO_CHAR(stream+8);
	xpng->m_header.colorType = BYTE_TO_CHAR(stream+9);
	xpng->m_header.compressionMethod = BYTE_TO_CHAR(stream+10);
	xpng->m_header.filterMode = BYTE_TO_CHAR(stream+11);
	xpng->m_header.interlaceMethod = BYTE_TO_CHAR(stream+12);
	printf("HEAD parsing %d %d %02x %02x %02x %02x %02x\n",
		xpng->m_header.width,xpng->m_header.height,
		xpng->m_header.bitDepth,xpng->m_header.colorType, 
		xpng->m_header.compressionMethod,xpng->m_header.filterMode,
		xpng->m_header.interlaceMethod);


	xpng->m_ColorMapTablePos = 0;
	xpng->m_bPaletted = false;


	if(xpng->m_header.colorType==COLORTYPE_COLOR_ALPHA)
	{
		xpng->m_ColorMapTableCur = colorMapTable_RGBA_2_BGRA;
		xpng->m_ColorMapTableLen = sizeof(colorMapTable_RGBA_2_BGRA) / sizeof(colorMapTable_RGBA_2_BGRA[0]);	// �ᱹ�� 4
		pixelFormat = PIXELFORMAT_32;
	}
	else if(xpng->m_header.colorType==COLORTYPE_COLOR)
	{
		xpng->m_ColorMapTableCur = colorMapTable_RGB_2_BGR;
		xpng->m_ColorMapTableLen = sizeof(colorMapTable_RGB_2_BGR) / sizeof(colorMapTable_RGB_2_BGR[0]);		// �ᱹ�� 3
		pixelFormat = PIXELFORMAT_24;
	}
	else if(xpng->m_header.colorType==COLORTYPE_COLOR_PAL)
	{
		// ����Ʈ�� ����. ALPHA �� ����. �׷��� RGB 2 BGR ���
		xpng->m_ColorMapTableCur = colorMapTable_RGB_2_BGR;
		xpng->m_ColorMapTableLen = sizeof(colorMapTable_RGB_2_BGR) / sizeof(colorMapTable_RGB_2_BGR[0]);		// �ᱹ�� 3
		xpng->m_bPaletted = true;
		pixelFormat = PIXELFORMAT_32;
	}
	else
	{
	}

	CreateDib(xbitmap, xpng->m_header.width, xpng->m_header.height, pixelFormat);

	SetDecodeData(xpng, xbitmap->m_pixel, xbitmap->m_stride, xbitmap->m_paddingByte, xbitmap->m_pixelFormat / 8);


	// BMP Header ����
	MakeBMPHeader(xbitmap, bmpFileName);
}
void PLTEParse(struct XPng* png, const unsigned char* stream, int dataLength){

	// �ȷ�Ʈ ���
	png->m_palette = (unsigned char*)malloc(dataLength);

	memcpy(png->m_palette, stream, dataLength);

	printf("\n", png->m_palette);

	printf("PLTEParse\n");
}
int IDATParse(struct XPng* xpng, struct XInflate* inflate, struct XBitmap* xbitmap, const unsigned char* stream, int dataLength)
{
	const unsigned char* idatBuffer; // IDAT ����

	unsigned char*		imageCurBuffer;
	int					imageCurWidthByte;
	int					ColorMapTablePos;
	unsigned char		filter;
	unsigned char		filter_a[RGBA];
	unsigned char*		filter_b ;	
	unsigned char		filter_c[RGBA];
	const unsigned char* outBuf;
	unsigned int					outBufLen;
	int					ColorMapTableLen;
	int					imageWidthByte;
	int					imagePaddingByte;
	const int*			ColorMapTableCur;

	imageCurBuffer = xpng->m_imageCurBuffer;
	imageCurWidthByte = xpng->m_imageCurWidthByte;
	ColorMapTablePos = xpng->m_ColorMapTablePos;
	filter = xpng->m_filter;
	filter_b = xpng->m_filter_b;	
	*(int*)filter_a = *(int*)xpng->m_filter_a;
	*(int*)filter_c = *(int*)xpng->m_filter_c;


	ColorMapTableLen = xpng->m_ColorMapTableLen;
	imageWidthByte = xpng->m_imageWidthByte;
	imagePaddingByte = xpng->m_imagePaddingByte;
	ColorMapTableCur = xpng->m_ColorMapTableCur;

	if(xpng->m_pInflate->m_outBuffer == NULL)
	{
		xpng->m_pInflate->m_outBuffer = (unsigned char*)malloc(DEFAULT_ALLOC_SIZE); // jmlee �ʱ�ȭ
		xpng->m_pInflate->m_outBufferAlloc = DEFAULT_ALLOC_SIZE;
	}


	idatBuffer = (unsigned char *)malloc(dataLength); // ��Ʈ������ IDAT Data�� ������

	memcpy(idatBuffer, stream, dataLength);


	// �������� ����
	if(Inflate(inflate, idatBuffer, dataLength)!=XINFLATE_ERR_OK){
		printf("error!!!!");
	}

	//���ڵ� ��� ����
	outBuf = inflate->m_outBuffer;
	outBufLen = inflate->m_outBufferPos;

	// ��� ���ۿ� ����.
	while(outBufLen)
	{
		// �� ��ĵ���� ù��° ����Ʈ�� filter �����̴�.
		if(imageCurWidthByte == imageWidthByte)
		{
			filter = *outBuf;

			// ���Ϳ� ����� ���� �ʱ�ȭ
			*((unsigned int*)filter_a) = 0;
			*((unsigned int*)filter_c) = 0;
			filter_b = xpng->m_scanline_current;

			// ���� ����� ���̰� set
			if(xpng->m_bEnd == false)
			{
				filter_b = xpng->m_filter_b_dummy;
				xpng->m_bEnd = true;
			}

			// �⺻ �ʱ�ȭ
			ColorMapTablePos = 0;

			// ���� ��ĵ���� ��� ó��
			xpng->m_scanline_before = xpng->m_scanline_current;
			xpng->m_scanline_current = imageCurBuffer;

			// filter �о����� 1����
			outBuf++;
			outBufLen--;
		}

		// ���� ����Ÿ ����
		switch(filter)
		{
		case FILTER_NONE :

			if(xpng->m_bPaletted)
			{
				unsigned char palPixel;
				while(outBufLen)
				{
					palPixel = *outBuf;

					// ����Ʈ�� �����ؼ� RGBA �� �����ϱ�
					*(imageCurBuffer + 2) = xpng->m_palette[palPixel * 3 + 0];		// R
					*(imageCurBuffer + 1) = xpng->m_palette[palPixel * 3 + 1];		// G
					*(imageCurBuffer + 0) = xpng->m_palette[palPixel * 3 + 2];		// B

					// RGBA ������
					*(imageCurBuffer + 3) = 255;					// A

					//WriteOnePixel(xbitmap, &xpng->m_palette[palPixel * 3 + 2]);
					//WriteOnePixel(xbitmap, &xpng->m_palette[palPixel * 3 + 1]);
					//WriteOnePixel(xbitmap, &xpng->m_palette[palPixel * 3 + 0]);


					// a�� �������� 3���� �����ϰ� A�κ� �ּ�
					MOVETONEXTBUFFER_4_PAL

				}
			}
			else
			{
				if(ColorMapTablePos==0)
				{
					if(ColorMapTableCur == colorMapTable_RGB_2_BGR)
					{
						while(outBufLen>3 && imageCurWidthByte>3)
						{
							*(imageCurBuffer + 2) = *(outBuf+0);		// R
							*(imageCurBuffer + 1) = *(outBuf+1);		// G
							*(imageCurBuffer + 0) = *(outBuf+2);		// B

							MOVETONEXTBUFFER_FAST(3);
						}
					}
					else if(ColorMapTableCur == colorMapTable_RGBA_2_BGRA)
					{
						while(outBufLen>4 && imageCurWidthByte>4)
						{
							*(imageCurBuffer + 2) = *(outBuf+0);		// R
							*(imageCurBuffer + 1) = *(outBuf+1);		// G
							*(imageCurBuffer + 0) = *(outBuf+2);		// B
							*(imageCurBuffer + 3) = *(outBuf+3);		// A

							MOVETONEXTBUFFER_FAST(4);
						}
					}
				}

				while(outBufLen)
				{
					// RGB �� BGR �� Ȥ�� RGBA �� BGRA �� �ٲٱ� ���ؼ� ���̺� ����
					*(imageCurBuffer + ColorMapTableCur[ColorMapTablePos]) = *outBuf;

					MOVETONEXTBUFFER
				}
			}

			break;

		case FILTER_SUB :

			if(xpng->m_bPaletted){
			}
			else
			{
				// �ѹ��� 3,4 ����Ʈ�� �����Ѵ�.
				if(ColorMapTablePos==0)
				{
					if(ColorMapTableCur == colorMapTable_RGB_2_BGR)
					{
						while(outBufLen>3 && imageCurWidthByte>3)
						{
							filter_a[0] = *(imageCurBuffer + 2) = *(outBuf+0) + filter_a[0];	// R
							filter_a[1] = *(imageCurBuffer + 1) = *(outBuf+1) + filter_a[1];	// G
							filter_a[2] = *(imageCurBuffer + 0) = *(outBuf+2) + filter_a[2];	// B

							MOVETONEXTBUFFER_FAST(3);
						}
					}
					else if(ColorMapTableCur == colorMapTable_RGBA_2_BGRA)
					{
						while(outBufLen>4 && imageCurWidthByte>4)
						{
							filter_a[0] = *(imageCurBuffer + 2) = *(outBuf+0) + filter_a[0];	// R
							filter_a[1] = *(imageCurBuffer + 1) = *(outBuf+1) + filter_a[1];	// G
							filter_a[2] = *(imageCurBuffer + 0) = *(outBuf+2) + filter_a[2];	// B
							filter_a[3] = *(imageCurBuffer + 3) = *(outBuf+3) + filter_a[3];	// A

							MOVETONEXTBUFFER_FAST(4);
						}
					}
				}

				while(outBufLen)
				{
					// RGB �� BGR �� Ȥ�� RGBA �� BGRA �� �ٲٱ� ���ؼ� ���̺� ����
					// + ���� �ȼ� ����
					// + ���Ͱ� ����(���� �ȼ�)
					filter_a[ColorMapTablePos] = *(imageCurBuffer + ColorMapTableCur[ColorMapTablePos]) = *outBuf + filter_a[ColorMapTablePos];
					MOVETONEXTBUFFER
				}
			}
			break;

		case FILTER_UP :

			if(xpng->m_bPaletted){
			}
			else
			{
				if(filter_b==NULL)
				{
					//DOERR(XPNG_ERR_INVALID_DATA);		// ���� ��Ȳ�̴�.
				}

				// �ѹ��� 3, 4����Ʈ�� �����Ѵ�.
				if(ColorMapTablePos==0)
				{
					if(ColorMapTableCur == colorMapTable_RGB_2_BGR)
					{
						while(outBufLen>3 && imageCurWidthByte>3)
						{
							*(imageCurBuffer + 2) = *(outBuf+0) + *(filter_b + 2);	// R
							*(imageCurBuffer + 1) = *(outBuf+1) + *(filter_b + 1);	// G
							*(imageCurBuffer + 0) = *(outBuf+2) + *(filter_b + 0);	// B

							MOVETONEXTBUFFER_FAST(3);
							filter_b+= 3;
						}
					}
					else if(ColorMapTableCur == colorMapTable_RGBA_2_BGRA)
					{
						while(outBufLen>4 && imageCurWidthByte>4)
						{
							*(imageCurBuffer + 2) = *(outBuf+0) + *(filter_b + 2);	// R
							*(imageCurBuffer + 1) = *(outBuf+1) + *(filter_b + 1);	// G
							*(imageCurBuffer + 0) = *(outBuf+2) + *(filter_b + 0);	// B
							*(imageCurBuffer + 3) = *(outBuf+3) + *(filter_b + 3);	// A


							//WriteOnePixel(xbitmap, *outBuf+0 + *(filter_b + 2));
							//WriteOnePixel(xbitmap, *outBuf+1 + *(filter_b + 1));
							//WriteOnePixel(xbitmap, *outBuf+2 + *(filter_b + 0));
							//WriteOnePixel(xbitmap, *outBuf+3 + *(filter_b + 3));

							MOVETONEXTBUFFER_FAST(4);
							filter_b+= 4;
						}
					}
				}

				while(outBufLen)
				{
					// RGB �� BGR �� Ȥ�� RGBA �� BGRA �� �ٲٱ� ���ؼ� ���̺� ����
					// + ���� ���� ����
					*(imageCurBuffer + ColorMapTableCur[ColorMapTablePos]) = 
						*outBuf +
						*(filter_b + ColorMapTableCur[ColorMapTablePos]);					// ���� ��ĵ ����

					filter_b++;

					MOVETONEXTBUFFER
				}
			}
			break;


		case FILTER_AVERAGE :

			if(xpng->m_bPaletted){
			}
			else
			{
				if(filter_b==NULL)
				{
					//DOERR(XPNG_ERR_INVALID_DATA);		// ���� ��Ȳ�̴�.
				}



				// �ѹ��� 3,4 �ȼ� ó���ϱ�
				if(ColorMapTablePos==0)
				{
					// todo
					// �̰� �׽�Ʈ �غ��� �ϴµ�.. ������ ������ ����.. average �� ���� �Ⱦ��̴µ�?
				}

				while(outBufLen)
				{
					int a, b;
					unsigned char* dest;
					// ������ ����Ÿ
					a = filter_a[ColorMapTablePos];								// ����
					b = *(filter_b + ColorMapTableCur[ColorMapTablePos]);		// ��

					// ���� ��� ����
					dest = imageCurBuffer + ColorMapTableCur[ColorMapTablePos];

					*dest = *outBuf + (a+b)/2;

					// ���� ������Ʈ
					filter_a[ColorMapTablePos] = *dest;
					// ��� ������Ʈ
					filter_b++;

					MOVETONEXTBUFFER
				}
			}
			break;

		case FILTER_PAETH :
			if(xpng->m_bPaletted){
			}
			else
			{

				int a, b,c,pa,pb,pc;
				int p;
				unsigned char* dest;
				int cur;

				if(filter_b==NULL)
				{
					//DOERR(XPNG_ERR_INVALID_DATA);		// ���� ��Ȳ�̴�.
				}
				/////////////////////////////////////////////////////////////////////////////
				//
				// PAETH ó���� ��ũ��ȭ
				//
#define DO_PAETH(src, dst)													\
	a = filter_a[src]; b = *(filter_b +dst); c = filter_c[src];		\
	p = a + b - c;													\
	pa = XPNG_ABS(p-a);												\
	pb = XPNG_ABS(p-b);												\
	pc = XPNG_ABS(p-c);												\
	\
	/* ���� ��� ���� */											\
	dest = imageCurBuffer + dst;									\
	\
	/* ���ڵ��� �� */												\
	cur = *(outBuf+src);											\
	\
	/* ��ġ�� */													\
	if(pa<=pb && pa<=pc)	*dest = cur + a;						\
				else if(pb<=pc)			*dest = cur + b;						\
				else					*dest = cur + c;						\
				\
				/* ���� ������Ʈ */												\
				filter_a[src] = *dest;											\
				filter_c[src] = b;


				// �ѹ��� 3,4 �ȼ� ó���ϱ�
				if(ColorMapTablePos==0)
				{
					if(ColorMapTableCur == colorMapTable_RGB_2_BGR)
					{
						while(outBufLen>3 && imageCurWidthByte>3)
						{
							DO_PAETH(0, 2);		// R
							DO_PAETH(1, 1);		// G
							DO_PAETH(2, 0);		// B

							// ���� �ȼ���
							MOVETONEXTBUFFER_FAST(3);
							filter_b+= 3;
						}
					}
					else if(ColorMapTableCur == colorMapTable_RGBA_2_BGRA)
					{
						while(outBufLen>4 && imageCurWidthByte>4)
						{
							DO_PAETH(0, 2);		// R
							DO_PAETH(1, 1);		// G
							DO_PAETH(2, 0);		// B
							DO_PAETH(3, 3);		// A

							// ���� �ȼ���
							MOVETONEXTBUFFER_FAST(4);
							filter_b+= 4;
						}
					}
				}

				while(outBufLen)
				{
					// ������ ����Ÿ
					a = filter_a[ColorMapTablePos];								// ����
					b = *(filter_b + ColorMapTableCur[ColorMapTablePos]);		// ��
					c = filter_c[ColorMapTablePos];								// ���� ��

					p = a + b - c;

					pa = XPNG_ABS(p-a);
					pb = XPNG_ABS(p-b);
					pc = XPNG_ABS(p-c);


					// ���� ��� ����
					dest = imageCurBuffer + ColorMapTableCur[ColorMapTablePos];

					// ���ڵ��� ��
					cur = *outBuf;


					// ��ġ��
					if(pa<=pb && pa<=pc)
						*dest = cur + a;
					else if(pb<=pc)
						*dest = cur + b;
					else 
						*dest = cur + c;

					// ���� ������Ʈ
					filter_a[ColorMapTablePos] = *dest;
					// �»�� ������Ʈ
					filter_c[ColorMapTablePos] = b;
					// ��� ������Ʈ
					filter_b++;

					MOVETONEXTBUFFER
				}
			}
			break;
		default :
			return false;
		}
	}

	// ���� ������ ��� ������ �ǵ�����
	xpng->m_imageCurBuffer = imageCurBuffer;
	xpng->m_imageCurWidthByte = imageCurWidthByte;
	xpng->m_ColorMapTablePos = ColorMapTablePos ;

	xpng->m_filter = filter;
	xpng->m_filter_b = filter_b;
	*(unsigned int*)xpng->m_filter_a = *(unsigned int*)filter_a;
	*(unsigned int*)xpng->m_filter_c = *(unsigned int*)filter_c;


	free((unsigned char *)idatBuffer);
	// �ѹ��� �ٷ� ����
	//WriteBMFile(xbitmap);
	//WriteBMFile(xbitmap, xbitmap->m_pixel, 125182);


	return true;

	//++++++++++++++++++++++++++IDAT��+++++++++++++++++++++++++++++++++++
	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



}




void IENDParse(struct XBitmap* xbitmap)
{
	WriteBMFile(xbitmap);
	CloseBMFile(xbitmap);
}
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
// 중복 되는 부분 - 속도를 위해서 매크로로 처리
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


// 테이블 처리를 하지 않는다.
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

// RGB BGR 변환 테이블
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
	unsigned char *buf;			// 버퍼
	unsigned int lengthOfFile;	// 파일크기

	// 파일열기
	fp = fopen(pngFileName, "rb");
	if (fp == NULL){
		printf("Cannot open png file: %s\n", pngFileName);
		return -1;
	}
	// 파일사이즈 구함
	lengthOfFile = FileSize(fp);

	// 버퍼 초기화
	buf = (unsigned char*)malloc(sizeof(unsigned char) * lengthOfFile + 4);

	if (buf == NULL){
		printf("Not enough memory for loading file\n");
		return -1;
	}
	// 버퍼에 png 데이터 쓰기
	fread(buf, lengthOfFile, 1, fp);
	// 파일닫기
	fclose(fp);

	DecodePngFileData(buf, lengthOfFile, bmpFileName);

	// 버퍼 릴리즈
	free(buf);

	return 0;

}
// 파일사이즈 구하기
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
	const unsigned char* startStream; // 스트림

	// png identifier
	if ((buf[0] != 0x89) || (buf[1] != 0x50) || (buf[2] != 0x4E) || (buf[3] != 0x47) || (buf[4] != 0x0D) || (buf[5] != 0x0A) || (buf[6] != 0x1A) || (buf[7] != 0x0A))
	{
		printf("Not a PNG file..\n");
		return -1;
	}
	printf("Png File!! \n");

	// 89(1) + 50(1) + png identifier(6) = 8 스트림 이동
	startStream = buf+8;
	// 파싱 시작
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

	// xpng, bitmap 초기화
	struct XPng *xpng;
	struct XBitmap *xbitmap;

	xpng = (struct XPng *)malloc(sizeof(struct XPng));
	Init_png(xpng);

	xbitmap = (struct XBitmap *)malloc(sizeof(struct XBitmap));
	Init_Bmp(xbitmap);

	// xpng 의 멤버인 inflate 초기화
	xpng->m_pInflate = (struct XInflate *)calloc(1, sizeof(struct XInflate));
	Init_inf(xpng->m_pInflate);

	// end_marker를 만날때까지 돈다
	while(!end_marker)
	{
		chunkLength = BYTE_TO_INT(stream); // 청크의 길이
		marker = BYTE_TO_INT(stream+4);	   // 청크명

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

				// IDATParse 안에서 true로 바꾼다
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

	// png, bitmap 해제
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
		xpng->m_ColorMapTableLen = sizeof(colorMapTable_RGBA_2_BGRA) / sizeof(colorMapTable_RGBA_2_BGRA[0]);	// 결국은 4
		pixelFormat = PIXELFORMAT_32;
	}
	else if(xpng->m_header.colorType==COLORTYPE_COLOR)
	{
		xpng->m_ColorMapTableCur = colorMapTable_RGB_2_BGR;
		xpng->m_ColorMapTableLen = sizeof(colorMapTable_RGB_2_BGR) / sizeof(colorMapTable_RGB_2_BGR[0]);		// 결국은 3
		pixelFormat = PIXELFORMAT_24;
	}
	else if(xpng->m_header.colorType==COLORTYPE_COLOR_PAL)
	{
		// 빨레트를 쓴다. ALPHA 는 없다. 그래서 RGB 2 BGR 사용
		xpng->m_ColorMapTableCur = colorMapTable_RGB_2_BGR;
		xpng->m_ColorMapTableLen = sizeof(colorMapTable_RGB_2_BGR) / sizeof(colorMapTable_RGB_2_BGR[0]);		// 결국은 3
		xpng->m_bPaletted = true;
		pixelFormat = PIXELFORMAT_32;
	}
	else
	{
	}

	CreateDib(xbitmap, xpng->m_header.width, xpng->m_header.height, pixelFormat);

	SetDecodeData(xpng, xbitmap->m_pixel, xbitmap->m_stride, xbitmap->m_paddingByte, xbitmap->m_pixelFormat / 8);


	// BMP Header 생성
	MakeBMPHeader(xbitmap, bmpFileName);
}
void PLTEParse(struct XPng* png, const unsigned char* stream, int dataLength){

	// 팔레트 사용
	png->m_palette = (unsigned char*)malloc(dataLength);

	memcpy(png->m_palette, stream, dataLength);

	printf("\n", png->m_palette);

	printf("PLTEParse\n");
}
int IDATParse(struct XPng* xpng, struct XInflate* inflate, struct XBitmap* xbitmap, const unsigned char* stream, int dataLength)
{
	const unsigned char* idatBuffer; // IDAT 정보

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
		xpng->m_pInflate->m_outBuffer = (unsigned char*)malloc(DEFAULT_ALLOC_SIZE); // jmlee 초기화
		xpng->m_pInflate->m_outBufferAlloc = DEFAULT_ALLOC_SIZE;
	}


	idatBuffer = (unsigned char *)malloc(dataLength); // 스트림에서 IDAT Data만 가져옴

	memcpy(idatBuffer, stream, dataLength);


	// 압축해제 시작
	if(Inflate(inflate, idatBuffer, dataLength)!=XINFLATE_ERR_OK){
		printf("error!!!!");
	}

	//디코딩 결과 저장
	outBuf = inflate->m_outBuffer;
	outBufLen = inflate->m_outBufferPos;

	// 출력 버퍼에 쓰기.
	while(outBufLen)
	{
		// 매 스캔라인 첫번째 바이트는 filter 정보이다.
		if(imageCurWidthByte == imageWidthByte)
		{
			filter = *outBuf;

			// 필터에 사용할 변수 초기화
			*((unsigned int*)filter_a) = 0;
			*((unsigned int*)filter_c) = 0;
			filter_b = xpng->m_scanline_current;

			// 최초 실행시 더미값 set
			if(xpng->m_bEnd == false)
			{
				filter_b = xpng->m_filter_b_dummy;
				xpng->m_bEnd = true;
			}

			// 기본 초기화
			ColorMapTablePos = 0;

			// 이전 스캔라인 기억 처리
			xpng->m_scanline_before = xpng->m_scanline_current;
			xpng->m_scanline_current = imageCurBuffer;

			// filter 읽었으니 1증가
			outBuf++;
			outBufLen--;
		}

		// 영상 데이타 복사
		switch(filter)
		{
		case FILTER_NONE :

			if(xpng->m_bPaletted)
			{
				unsigned char palPixel;
				while(outBufLen)
				{
					palPixel = *outBuf;

					// 빨레트를 참고해서 RGBA 값 세팅하기
					*(imageCurBuffer + 2) = xpng->m_palette[palPixel * 3 + 0];		// R
					*(imageCurBuffer + 1) = xpng->m_palette[palPixel * 3 + 1];		// G
					*(imageCurBuffer + 0) = xpng->m_palette[palPixel * 3 + 2];		// B

					// RGBA 사용안함
					*(imageCurBuffer + 3) = 255;					// A

					//WriteOnePixel(xbitmap, &xpng->m_palette[palPixel * 3 + 2]);
					//WriteOnePixel(xbitmap, &xpng->m_palette[palPixel * 3 + 1]);
					//WriteOnePixel(xbitmap, &xpng->m_palette[palPixel * 3 + 0]);


					// a값 버리려면 3으로 변경하고 A부분 주석
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
					// RGB 를 BGR 로 혹은 RGBA 를 BGRA 로 바꾸기 위해서 테이블 참조
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
				// 한번에 3,4 바이트씩 복사한다.
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
					// RGB 를 BGR 로 혹은 RGBA 를 BGRA 로 바꾸기 위해서 테이블 참조
					// + 왼쪽 픽셀 참조
					// + 필터값 저장(좀전 픽셀)
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
					//DOERR(XPNG_ERR_INVALID_DATA);		// 에러 상황이다.
				}

				// 한번에 3, 4바이트씩 복사한다.
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
					// RGB 를 BGR 로 혹은 RGBA 를 BGRA 로 바꾸기 위해서 테이블 참조
					// + 이전 라인 참조
					*(imageCurBuffer + ColorMapTableCur[ColorMapTablePos]) = 
						*outBuf +
						*(filter_b + ColorMapTableCur[ColorMapTablePos]);					// 이전 스캔 라인

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
					//DOERR(XPNG_ERR_INVALID_DATA);		// 에러 상황이다.
				}



				// 한번에 3,4 픽셀 처리하기
				if(ColorMapTablePos==0)
				{
					// todo
					// 이거 테스트 해봐야 하는데.. 적당한 샘플이 없음.. average 는 별로 안쓰이는듯?
				}

				while(outBufLen)
				{
					int a, b;
					unsigned char* dest;
					// 참조할 데이타
					a = filter_a[ColorMapTablePos];								// 왼쪽
					b = *(filter_b + ColorMapTableCur[ColorMapTablePos]);		// 위

					// 현재 출력 지점
					dest = imageCurBuffer + ColorMapTableCur[ColorMapTablePos];

					*dest = *outBuf + (a+b)/2;

					// 좌측 업데이트
					filter_a[ColorMapTablePos] = *dest;
					// 상단 업데이트
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
					//DOERR(XPNG_ERR_INVALID_DATA);		// 에러 상황이다.
				}
				/////////////////////////////////////////////////////////////////////////////
				//
				// PAETH 처리를 매크로화
				//
#define DO_PAETH(src, dst)													\
	a = filter_a[src]; b = *(filter_b +dst); c = filter_c[src];		\
	p = a + b - c;													\
	pa = XPNG_ABS(p-a);												\
	pb = XPNG_ABS(p-b);												\
	pc = XPNG_ABS(p-c);												\
	\
	/* 현재 출력 지점 */											\
	dest = imageCurBuffer + dst;									\
	\
	/* 디코딩된 값 */												\
	cur = *(outBuf+src);											\
	\
	/* 합치기 */													\
	if(pa<=pb && pa<=pc)	*dest = cur + a;						\
				else if(pb<=pc)			*dest = cur + b;						\
				else					*dest = cur + c;						\
				\
				/* 필터 업데이트 */												\
				filter_a[src] = *dest;											\
				filter_c[src] = b;


				// 한번에 3,4 픽셀 처리하기
				if(ColorMapTablePos==0)
				{
					if(ColorMapTableCur == colorMapTable_RGB_2_BGR)
					{
						while(outBufLen>3 && imageCurWidthByte>3)
						{
							DO_PAETH(0, 2);		// R
							DO_PAETH(1, 1);		// G
							DO_PAETH(2, 0);		// B

							// 다음 픽셀로
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

							// 다음 픽셀로
							MOVETONEXTBUFFER_FAST(4);
							filter_b+= 4;
						}
					}
				}

				while(outBufLen)
				{
					// 참조할 데이타
					a = filter_a[ColorMapTablePos];								// 왼쪽
					b = *(filter_b + ColorMapTableCur[ColorMapTablePos]);		// 위
					c = filter_c[ColorMapTablePos];								// 왼쪽 위

					p = a + b - c;

					pa = XPNG_ABS(p-a);
					pb = XPNG_ABS(p-b);
					pc = XPNG_ABS(p-c);


					// 현재 출력 지점
					dest = imageCurBuffer + ColorMapTableCur[ColorMapTablePos];

					// 디코딩된 값
					cur = *outBuf;


					// 합치기
					if(pa<=pb && pa<=pc)
						*dest = cur + a;
					else if(pb<=pc)
						*dest = cur + b;
					else 
						*dest = cur + c;

					// 좌측 업데이트
					filter_a[ColorMapTablePos] = *dest;
					// 좌상단 업데이트
					filter_c[ColorMapTablePos] = b;
					// 상단 업데이트
					filter_b++;

					MOVETONEXTBUFFER
				}
			}
			break;
		default :
			return false;
		}
	}

	// 로컬 변수를 멤버 변수로 되돌리기
	xpng->m_imageCurBuffer = imageCurBuffer;
	xpng->m_imageCurWidthByte = imageCurWidthByte;
	xpng->m_ColorMapTablePos = ColorMapTablePos ;

	xpng->m_filter = filter;
	xpng->m_filter_b = filter_b;
	*(unsigned int*)xpng->m_filter_a = *(unsigned int*)filter_a;
	*(unsigned int*)xpng->m_filter_c = *(unsigned int*)filter_c;


	free((unsigned char *)idatBuffer);
	// 한번에 바로 찍음
	//WriteBMFile(xbitmap);
	//WriteBMFile(xbitmap, xbitmap->m_pixel, 125182);


	return true;

	//++++++++++++++++++++++++++IDAT끝+++++++++++++++++++++++++++++++++++
	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



}




void IENDParse(struct XBitmap* xbitmap)
{
	WriteBMFile(xbitmap);
	CloseBMFile(xbitmap);
}
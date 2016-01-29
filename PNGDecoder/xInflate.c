#include "xInflate.h"
#include "assert.h"




// DEFLATE 의 압축 타입
static enum 
{
	BTYPE_NOCOMP			= 0,
	BTYPE_FIXEDHUFFMAN		= 1,
	BTYPE_DYNAMICHUFFMAN	= 2,
	BTYPE_RESERVED			= 3		// ERROR
} BTYPE;

// len, dist 테이블 - halibut 의 deflate.c 에서 가져옴
struct coderecord
{
    short code, extrabits;
    int min, max;
} ;

static const struct coderecord lencodes[] = 
{
    {257, 0, 3, 3},		{258, 0, 4, 4},		{259, 0, 5, 5},		{260, 0, 6, 6},		{261, 0, 7, 7},		{262, 0, 8, 8},		{263, 0, 9, 9},		{264, 0, 10, 10},	{265, 1, 11, 12},	{266, 1, 13, 14},	{267, 1, 15, 16},	{268, 1, 17, 18},
    {269, 2, 19, 22},	{270, 2, 23, 26},	{271, 2, 27, 30},	{272, 2, 31, 34},	{273, 3, 35, 42},	{274, 3, 43, 50},	{275, 3, 51, 58},	{276, 3, 59, 66},	{277, 4, 67, 82},	{278, 4, 83, 98},	{279, 4, 99, 114},	{280, 4, 115, 130},
    {281, 5, 131, 162},	{282, 5, 163, 194},	{283, 5, 195, 226},	{284, 5, 227, 257},	{285, 0, 258, 258},	
};

static const struct coderecord distcodes[] = 
{
    {0, 0, 1, 1},			{1, 0, 2, 2},			{2, 0, 3, 3},			{3, 0, 4, 4},			{4, 1, 5, 6},			{5, 1, 7, 8},			{6, 2, 9, 12},			{7, 2, 13, 16},			{8, 3, 17, 24},			{9, 3, 25, 32},			{10, 4, 33, 48},		{11, 4, 49, 64},		{12, 5, 65, 96},		{13, 5, 97, 128},		{14, 6, 129, 192},		{15, 6, 193, 256},
    {16, 7, 257, 384},		{17, 7, 385, 512},		{18, 8, 513, 768},		{19, 8, 769, 1024},		{20, 9, 1025, 1536},	{21, 9, 1537, 2048},	{22, 10, 2049, 3072},	{23, 10, 3073, 4096},	{24, 11, 4097, 6144},	{25, 11, 6145, 8192},	{26, 12, 8193, 12288},	{27, 12, 12289, 16384},	{28, 13, 16385, 24576},	{29, 13, 24577, 32768},
};

// code length 
static const unsigned char lenlenmap[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};


// 유틸 매크로
#define HUFFTABLE_VALUE_NOT_EXIST	-1
#ifndef ASSERT
#define ASSERT(x) {}
#endif

////////////////////////////////////////////////
//
// 비트 스트림 매크로화
//
#define BS_EATBIT()								\
	bits & 0x01;								\
	bitLen --;									\
	bits >>= 1;									\
	if(bitLen<0) ASSERT(0);

#define BS_EATBITS(count)						\
	(bits & ((1<<(count))-1));					\
	bitLen -= count;							\
	bits >>= count;								\
	if(bitLen<0) ASSERT(0);

#define BS_REMOVEBITS(count)					\
	bits >>= (count);							\
	bitLen -= (count);							\
	if(bitLen<0) ASSERT(0); 

#define BS_MOVETONEXTBYTE						\
	BS_REMOVEBITS((bitLen % 8));

#define BS_ADDBYTE(byte)						\
	bits |= (byte) << bitLen;					\
	bitLen += 8;
//
// 비트 스트림 매크로 처리
//
////////////////////////////////////////////////


////////////////////////////////////////////////
//
// 디버깅용 매크로
//
#	define ADD_INPUT_COUNT		inflate->m_inputCount ++
#	define ADD_OUTPUT_COUNT		inflate->m_outputCount ++

#define	DOERR(x) { printf("ERROR...."); }

////////////////////////////////////////////////
//
// 반복 작업 매크로화
//
#define FILLBUFFER()							\
	while(bitLen<=24)							\
	{											\
		if(inBufferRemain==0)					\
			break;								\
		BS_ADDBYTE(*inBuffer);					\
		inBufferRemain--;						\
		inBuffer++;								\
		ADD_INPUT_COUNT;						\
	}											\
	if(bitLen<=0)								\
		goto END;


#define HUFFLOOKUP(result, pTable)								\
		/* 데이타 부족 */										\
		if(pTable->bitLenMin > bitLen)							\
		{														\
			result = -1;										\
		}														\
		else													\
		{														\
			pItem = &(pTable->pItem[pTable->mask & bits]);		\
			/* 데이타 부족 */									\
			if(pItem->bitLen > bitLen)							\
			{													\
				result = -1;									\
			}													\
			else												\
			{													\
				result = pItem->symbol;							\
				BS_REMOVEBITS(pItem->bitLen);					\
			}													\
		}


#define WRITE(byte)												\
	ADD_OUTPUT_COUNT;											\
	WIN_ADD(byte);												\
	CHECK_AND_INCREATE_OUT_BUFFER								\
	outBuffer[outBufferPos] = byte;								\
	outBufferPos++;												

#define CHECK_AND_INCREATE_OUT_BUFFER							\
	if(outBufferPos>=outBufferAlloc)							\
	{															\
		unsigned char* temp;									\
		if(inflate->m_isExternalOutBuffer)/* 버퍼를 늘릴수가 없는 상황*/	\
			DOERR(XINFLATE_ERR_INSUFFICIENT_OUT_BUFFER);		\
																\
		/* 버퍼 늘리기 */										\
		outBufferAlloc = outBufferAlloc * 2;					\
		temp = outBuffer;									\
		outBuffer = (unsigned char*)realloc(outBuffer, outBufferAlloc);	\
		/* alloc 실패 처리 */									\
		if(outBuffer==NULL)										\
		{														\
			assert(0);											\
			ret = XINFLATE_ERR_ALLOC_FAIL;						\
			outBuffer = temp;									\
			outBufferPos = 0; /* 그냥 처음 위치로 되돌린다.*/	\
			outBufferAlloc /= 2;								\
			goto END;											\
		}														\
	}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//                                              FastInflate
//


// 입력 버퍼가 충분한지 체크하지 않는다.
#define FILLBUFFER_FAST()								\
	while(bitLen<=24)									\
	{													\
		BS_ADDBYTE(*inBuffer);							\
		inBufferRemain--;								\
		inBuffer++;										\
		ADD_INPUT_COUNT;								\
	}													\

// 비트스트림 데이타(bits + bitLen)가 충분한지 체크하지 않는다.
#define HUFFLOOKUP_FAST(result, pTable)					\
		pItem = &(pTable->pItem[pTable->mask & bits]);	\
		result = pItem->symbol;							\
		BS_REMOVEBITS(pItem->bitLen);					


// 출력 버퍼가 충분한지 체크하지 않는다.
#define WRITE_FAST(byte)										\
	ADD_OUTPUT_COUNT;											\
	WIN_ADD(byte);												\
	outBuffer[outBufferPos] = byte;								\
	outBufferPos++;												


////////////////////////////////////////////////
//
// 윈도우 매크로화
//

#define WIN_ADD(b)												\
	windowBuf[windowPos] = b;									\
	windowPos = (windowPos+1) & (DEFLATE_WINDOW_SIZE-1);

#define WIN_GETBUF(dist)										\
	(windowBuf +  ((windowPos - dist) & (DEFLATE_WINDOW_SIZE-1)) )

//
// 윈도우 매크로화
//
////////////////////////////////////////////////



/////////////////////////////////////////////////////
//
// 허프만 트리를 빨리 탐색할 수 있게 하기 위해서
// 트리 전체를 하나의 배열로 만들어 버린다.
//   
//
void ReleaseXFastBuffTable(struct XFastHuffTable *fastHuff)
{
	free(fastHuff->pItem);
}
void Init_FastHufftem(struct XFastHuffItem* xfhItem)
{
	xfhItem->bitLen = 0;
	xfhItem->symbol = HUFFTABLE_VALUE_NOT_EXIST;
}

void Create(struct XFastHuffTable *fastHuff, int _bitLenMin, int _bitLenMax)
	{
		if(fastHuff->pItem) ASSERT(0);							// 발생 불가

		fastHuff->bitLenMin = _bitLenMin;
		fastHuff->bitLenMax = _bitLenMax;
		fastHuff->mask	  = (1<<fastHuff->bitLenMax) - 1;					// 마스크
		fastHuff->itemCount = 1<<fastHuff->bitLenMax;						// 조합 가능한 최대 테이블 크기
		fastHuff->pItem     = (struct XFastHuffItem *)calloc(fastHuff->itemCount, sizeof(struct XFastHuffItem));		// 2^maxBitLen 만큼 배열을 만든다. // jmlee 1000이면 에러안나는데 sizeof쓰면 에러남...
		Init_FastHufftem(fastHuff->pItem); // jmlee 생성자 호출
	}
	// 심볼 데이타를 가지고 전체 배열을 채운다.
	void	SetValue(struct XFastHuffTable *fastHuff, int symbol, int bitLen, int bitCode)
	{

	int		add2code = (1<<bitLen);		// bitLen 이 3 이라면 add2code 에는 1000(bin) 이 들어간다
		int revBitCode = 0;
		// 뒤집기
		int i;
		for(i=0;i<bitLen;i++)
		{
			revBitCode <<= 1;
			revBitCode |= (bitCode & 0x01);
			bitCode >>= 1;
		}

		

		// 배열 채우기
		for(;;)
		{
#ifdef _DEBUG
			if(fastHuff->pItem[revBitCode].symbol!=  HUFFTABLE_VALUE_NOT_EXIST) 
				ASSERT(0);
#endif

			fastHuff->pItem[revBitCode].bitLen = bitLen;
			fastHuff->pItem[revBitCode].symbol = symbol;

			// 조합 가능한 bit code 를 처리하기 위해서 값을 계속 더한다.
			revBitCode += add2code;

			// 조합 가능한 범위가 벗어난 경우 끝낸다
			if(revBitCode >= fastHuff->itemCount)
				break;
		}
	}




// 멤버 변수를 로컬로 복사하고 로컬 변수를 멤버 변수로 반환하기

#define	SAVE										\
	bits = inflate->m_bits;									\
	bitLen = inflate->m_bitLen;								\
	windowBuf = inflate->m_windowBuf;						\
	windowPos = inflate->m_windowPos;						\
	outBuffer = inflate->m_outBuffer;						\
	outBufferAlloc = inflate->m_outBufferAlloc;				\
	outBufferPos = inflate->m_outBufferPos;					\
	state = inflate->m_state;								\
	windowLen = inflate->m_windowLen;						\
	windowDistCode = inflate->m_windowDistCode;				\
	symbol = inflate->m_symbol;

#define RESTORE										\
	inflate->m_bits = bits;									\
	inflate->m_bitLen = bitLen;								\
	inflate->m_windowBuf = windowBuf;						\
	inflate->m_windowPos = windowPos;						\
	inflate->m_outBuffer = outBuffer;						\
	inflate->m_outBufferAlloc = outBufferAlloc;				\
	inflate->m_outBufferPos = outBufferPos;					\
	inflate->m_state = state;								\
	inflate->m_windowLen = windowLen;						\
	inflate->m_windowDistCode = windowDistCode;				\
	inflate->m_symbol =symbol;


const char* xinflate_copyright = 
"[XInflate - Copyright(C) 2010, by kippler]";


////////////////////////////////////////////////////////////////////////////////////////////////////
///         내부 변수 초기화
/// @param  
/// @return 
/// @date   Thursday, April 08, 2010  4:17:46 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
void Destroy_inf(struct XInflate *inflate)
{
	free(inflate->m_pLenLenTable);
	free(inflate->m_pDynamicInfTable);
	free(inflate->m_pDynamicDistTable);

	free(inflate->m_pStaticDistTable);
	free(inflate->m_pStaticInfTable);
	free(inflate->m_windowBuf);
	free(inflate->m_outBuffer);
}
void Init_inf(struct XInflate* inflate)
{
	inflate->m_pLenLenTable =  (struct XFastHuffTable*)calloc(1, sizeof(struct XFastHuffTable));
	inflate->m_pDynamicInfTable =  (struct XFastHuffTable*)calloc(1, sizeof(struct XFastHuffTable));
	inflate->m_pDynamicDistTable =  (struct XFastHuffTable*)calloc(1, sizeof(struct XFastHuffTable));

	inflate->m_pStaticDistTable = (struct XFastHuffTable*)calloc(1, sizeof(struct XFastHuffTable));
	inflate->m_pStaticInfTable = (struct XFastHuffTable*)calloc(1, sizeof(struct XFastHuffTable));

	inflate->m_pCurrentTable = 0;
	inflate->m_pCurrentDistTable = 0;

	inflate->m_state = STATE_START;
	inflate->m_uncompRemain = 0;
	inflate->m_bFinalBlock = FALSE;
	inflate->m_symbol = 0;
	inflate->m_windowLen = 0;
	inflate->m_windowDistCode = 0;
	inflate->m_bCompleted = FALSE;

	BS_Init(inflate);
	Win_Init(inflate);

	inflate->m_copyright = xinflate_copyright;

#ifdef _DEBUG
	inflate->m_inputCount = 0;
	inflate->m_outputCount = 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///         출력 버퍼를 내부에서 alloc 하지를 않고, 외부의 버퍼를 사용한다.
/// @param  
/// @return 
/// @date   Wednesday, May 19, 2010  12:44:50 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetOutBuffer(struct XInflate* inflate, unsigned char* outBuf, int outBufLen)
{
	// 이전에 사용하던 버퍼 있으면 버퍼 지우기.
	if(inflate->m_isExternalOutBuffer==fail)
	{
		//free(inflate->m_outBuffer);
	}

	if(outBuf)
	{
		// 외부 버퍼를 출력 버퍼로 지정하기
		inflate->m_outBuffer = outBuf;
		inflate->m_outBufferAlloc = outBufLen;
		inflate->m_outBufferPos = 0;
		inflate->m_isExternalOutBuffer = success;
	}
	else
	{
		// outBuf==NULL 이면 외부 버퍼를 더이상 사용 안하는 경우이다.
		inflate->m_outBuffer = outBuf;
		inflate->m_outBufferAlloc = 0;
		inflate->m_outBufferPos = 0;
		inflate->m_isExternalOutBuffer = fail;
	}

}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         압축 해제 - 호출전 Init() 을 반드시 호출해 줘야 한다.
/// @param  
/// @return 
/// @date   Thursday, April 08, 2010  4:18:55 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
enum XINFLATE_ERR Inflate(struct XInflate* inflate, const unsigned char* inBuffer, int inBufferRemain)
{
	enum XINFLATE_ERR	ret;
		// 속도 향상을 위해서 멤버 변수를 로컬로 복사
	unsigned int					bits;
	unsigned int					bitLen;
	unsigned char*		windowBuf;
	int					windowPos;
	unsigned char*		outBuffer;
	int					outBufferAlloc;
	int					outBufferPos;
	enum STATE			state;
	int					windowLen;
	int					windowDistCode;
	int					symbol;



	
	// 로컬 변수
	const struct coderecord*	rec;
	struct XFastHuffItem*		pItem;			// HUFFLOOKUP() 에서 사용
	unsigned char				byte;			// 임시 변수
	enum XINFLATE_ERR			err;
		
	// 출력 버퍼 위치 초기화
	inflate->m_outBufferPos = 0;

	SAVE;

	err = XINFLATE_ERR_OK;
	ret = XINFLATE_ERR_OK;

	// 이미 완료되었는지 검사
	if(inflate->m_bCompleted)
		DOERR(XINFLATE_ERR_BLOCK_COMPLETED);

	// 초기화 함수 호출 되었는지 검사
	if(inflate->m_copyright==NULL)
		DOERR(XINFLATE_ERR_NOT_INITIALIZED);


	// 루프 돌면서 압축 해제
	for(;;)
	{
		FILLBUFFER();

		switch(state)
		{
			// 헤더 분석 시작
		case STATE_START :
			if(bitLen<3) 
				goto END;

			// 마지막 블럭 여부
			inflate->m_bFinalBlock = BS_EATBIT();

			// 블럭타입 헤더는 2bit 
			{
				int bType = BS_EATBITS(2);

				if(bType==BTYPE_DYNAMICHUFFMAN)
					state = STATE_DYNAMIC_HUFFMAN;
				else if(bType==BTYPE_NOCOMP)
					state = STATE_NO_COMPRESSION;
				else if(bType==BTYPE_FIXEDHUFFMAN)
					state = STATE_FIXED_HUFFMAN;
				else
					DOERR(XINFLATE_ERR_HEADER);
			}
			break;

			// 압축 안됨
		case STATE_NO_COMPRESSION :
			BS_MOVETONEXTBYTE;
			state = STATE_NO_COMPRESSION_LEN;
			break;

		case STATE_NO_COMPRESSION_LEN :
			// LEN
			if(bitLen<16) goto END;
			inflate->m_uncompRemain = BS_EATBITS(16);
			state = STATE_NO_COMPRESSION_NLEN;

			break;

		case STATE_NO_COMPRESSION_NLEN :
			// NLEN
			if(bitLen<16) goto END;
			{
				UINT32 nlen = BS_EATBITS(16);
				// one's complement 
				if( (nlen^0xffff) != inflate->m_uncompRemain) 
					DOERR(XINFLATE_ERR_INVALID_NOCOMP_LEN);
			}
			state = STATE_NO_COMPRESSION_BYPASS;
			break;

		case STATE_NO_COMPRESSION_BYPASS :
			// 데이타 가져오기
			if(bitLen<8) goto END;

			//////////////////////////////////////////////
			//
			// 원래 코드
			//
			/*
			{
				byte = BS_EATBITS(8);
				WRITE(byte);
			}
			m_uncompRemain--;
			*/

			if(bitLen%8!=0) ASSERT(0);			// 발생불가, 그냥 확인용

			//////////////////////////////////////////////
			//
			// 아래는 개선된 코드. 그런데 별 차이가 없다 ㅠ.ㅠ
			//

			// 비트 스트림을 먼저 비우고

			while(bitLen && inflate->m_uncompRemain)
			{
				//unsigned char* temp;
				byte = BS_EATBITS(8);
				WRITE(byte);
				inflate->m_uncompRemain--;
			}

			// 나머지 데이타는 바이트 그대로 쓰기
			{
				int	toCopy, toCopy2;
				toCopy = toCopy2 = min((int)inflate->m_uncompRemain, inBufferRemain);

				/* 원래코드
				while(toCopy)
				{
					WRITE(*inBuffer);
					inBuffer++;
					toCopy--;
				}
				*/

				if(outBufferAlloc-outBufferPos > toCopy)
				{
					// 출력 버퍼가 충분한 경우 - 출력 버퍼가 충분한지 여부를 체크하지 않는다.
					while(toCopy)
					{
						WRITE_FAST(*inBuffer);
						inBuffer++;
						toCopy--;
					}
				}
				else
				{
					while(toCopy)
					{
						WRITE(*inBuffer);
						inBuffer++;
						toCopy--;
					}
				}


				inflate->m_uncompRemain-=toCopy2;
				inBufferRemain-=toCopy2;
			}
			//
			// 개선된 코드 끝
			//
			//////////////////////////////////////////////

			if(inflate->m_uncompRemain==0)
			{
				if(inflate->m_bFinalBlock)
					state = STATE_COMPLETED;
				else
					state = STATE_START;
			}
			break;

			// 고정 허프만
		case STATE_FIXED_HUFFMAN :
			//if(inflate->m_pStaticInfTable==NULL)
				CreateStaticTable(inflate);

			inflate->m_pCurrentTable = inflate->m_pStaticInfTable;
			inflate->m_pCurrentDistTable = inflate->m_pStaticDistTable;
			state = STATE_GET_SYMBOL;
			break;

			// 길이 가져오기
		case STATE_GET_LEN :
			// zlib 의 inflate_fast 호출 조건 흉내내기
			if(inBufferRemain>FASTINFLATE_MIN_BUFFER_NEED)
			{
				enum XINFLATE_ERR result;
				if(symbol<=256)	ASSERT(0);

				RESTORE;
				result = FastInflate(inflate, &inBuffer, &inBufferRemain);
				if(result!=XINFLATE_ERR_OK)
					return result;
				SAVE;
				break;
			}

			rec = &lencodes[symbol - 257];
			if (bitLen < rec->extrabits) goto END;

			// RFC 1951 3.2.5
			// 기본 길이에 extrabit 만큼의 비트의 내용을 더하면 진짜 길이가 나온다
			{
				int extraLen = BS_EATBITS(rec->extrabits);
				windowLen = rec->min + extraLen;	
			}
			state = STATE_GET_DISTCODE;
			//break;	필요 없다..

			// 거리 코드 가져오기
		case STATE_GET_DISTCODE :
			HUFFLOOKUP(windowDistCode, inflate->m_pCurrentDistTable);

			if(windowDistCode<0)
				goto END;

			if(windowDistCode==30 || windowDistCode==31)			// 30 과 31은 생길 수 없다. RFC1951 3.2.6
				DOERR(XINFLATE_ERR_INVALID_DIST);

			state = STATE_GET_DIST;

			FILLBUFFER();
			//break;		// 필요없다

			// 거리 가져오기
		case STATE_GET_DIST:
			rec = &distcodes[windowDistCode];
			// DIST 구하기
			if(bitLen<rec->extrabits)
				goto END;
			{
				int dist = rec->min + BS_EATBITS(rec->extrabits);

				// lz77 출력
				while(windowLen)
				{
					//unsigned char* temp;
					byte = *WIN_GETBUF(dist);
					WRITE(byte);
					windowLen--;
				}
			}
	
			state = STATE_GET_SYMBOL;

			FILLBUFFER();
			//break;		// 필요 없다

			// 심볼 가져오기.
		case STATE_GET_SYMBOL :
			HUFFLOOKUP(symbol, inflate->m_pCurrentTable);

			if(symbol<0) 
				goto END;
			else if(symbol<256)
			{

				byte = (unsigned char)symbol;
				WRITE(byte);	
				
				break;
			}
			else if(symbol==256)	// END OF BLOCK
			{
				if(inflate->m_bFinalBlock)
					state = STATE_COMPLETED;
				else
					state = STATE_START;
				break;
			}
			else if(symbol<286)
			{
				state = STATE_GET_LEN;
			}
			else
				DOERR(XINFLATE_ERR_INVALID_SYMBOL);		// 발생 불가

			break;

			// 다이나믹 허프만 시작
		case STATE_DYNAMIC_HUFFMAN :
			if(bitLen<5+5+4) {
				goto END;
			}

			inflate->m_literalsNum  = 257 + BS_EATBITS(5);
			inflate->m_distancesNum = 1   + BS_EATBITS(5);
			inflate->m_lenghtsNum   = 4   + BS_EATBITS(4);

			if(inflate->m_literalsNum > 286 || inflate->m_distancesNum > 30)
				DOERR(XINFLATE_ERR_INVALID_LEN);

			memset(inflate->m_lengths, 0, sizeof(inflate->m_lengths));
			inflate->m_lenghtsPtr = 0;

			state = STATE_DYNAMIC_HUFFMAN_LENLEN ;
			break;

			// 길이의 길이 가져오기.
		case STATE_DYNAMIC_HUFFMAN_LENLEN :

			if(bitLen<3) 
				goto END;

			// 3bit 씩 코드 길이의 코드 길이 정보 가져오기.
			while (inflate->m_lenghtsPtr < inflate->m_lenghtsNum && bitLen >= 3) 
			{
				if(inflate->m_lenghtsPtr>sizeof(lenlenmap))						// 입력값 체크..
					DOERR(XINFLATE_ERR_INVALID_LEN);

				
				inflate->m_lengths[lenlenmap[inflate->m_lenghtsPtr]] = BS_EATBITS(3);
				inflate->m_lenghtsPtr++;
			}

			// 다 가져왔으면..
			if (inflate->m_lenghtsPtr == inflate->m_lenghtsNum)
			{
				// 길이에 대한 허프만 테이블 만들기
				if(CreateTable(inflate, inflate->m_lengths, 19, &inflate->m_pLenLenTable, err)==FALSE)
					DOERR(err);
				state = STATE_DYNAMIC_HUFFMAN_LEN;
				inflate->m_lenghtsPtr = 0;
			}
			break;

			// 압축된 길이 정보를 m_pLenLenTable 를 거쳐서 가져오기.
		case STATE_DYNAMIC_HUFFMAN_LEN:

			// 다 가져왔으면
			if (inflate->m_lenghtsPtr >= inflate->m_literalsNum + inflate->m_distancesNum) 
			{
				// 최종 다이나믹 테이블 생성 (literal + distance)
				if(	CreateTable(inflate, inflate->m_lengths, inflate->m_literalsNum, &inflate->m_pDynamicInfTable, err)==FALSE ||
					CreateTable(inflate, inflate->m_lengths + inflate->m_literalsNum, inflate->m_distancesNum, &inflate->m_pDynamicDistTable, err)==FALSE)
					DOERR(err);

				// lenlen 테이블은 이제 더이상 필요없다.
				ReleaseXFastBuffTable(inflate->m_pLenLenTable);
				//free(inflate->m_pLenLenTable);

				// 테이블 세팅
				inflate->m_pCurrentTable = inflate->m_pDynamicInfTable;
				inflate->m_pCurrentDistTable = inflate->m_pDynamicDistTable;

				// 진짜 압축 해제 시작
				state = STATE_GET_SYMBOL;
				break;
			}

			{
				// 길이 정보 코드 가져오기
				int code=-1;
				HUFFLOOKUP(code, inflate->m_pLenLenTable);

				if (code == -1)
					goto END;

				if (code < 16) 
				{
					if(inflate->m_lenghtsPtr>sizeof(inflate->m_lengths))		// 값 체크
						DOERR(XINFLATE_ERR_INVALID_LEN);

					inflate->m_lengths[inflate->m_lenghtsPtr] = code;
					inflate->m_lenghtsPtr++;
				} 
				else 
				{
					inflate->m_lenExtraBits = (code == 16 ? 2 : code == 17 ? 3 : 7);
					inflate->m_lenAddOn = (code == 18 ? 11 : 3);
					inflate->m_lenRep = (code == 16 && inflate->m_lenghtsPtr > 0 ? inflate->m_lengths[inflate->m_lenghtsPtr - 1] : 0);
					state = STATE_DYNAMIC_HUFFMAN_LENREP;
				}
			}
			break;

		case STATE_DYNAMIC_HUFFMAN_LENREP:
			if (bitLen < inflate->m_lenExtraBits)
				goto END;

			{
				int repeat = inflate->m_lenAddOn + BS_EATBITS(inflate->m_lenExtraBits);

				while (repeat > 0 && inflate->m_lenghtsPtr < inflate->m_literalsNum + inflate->m_distancesNum) 
				{
					inflate->m_lengths[inflate->m_lenghtsPtr] = inflate->m_lenRep;
					inflate->m_lenghtsPtr++;
					repeat--;
				}
			}

			state = STATE_DYNAMIC_HUFFMAN_LEN;
			break;

		case STATE_COMPLETED :
			inflate->m_bCompleted = TRUE;
			goto END;
			break;

		default :
			ASSERT(0);
		}
	}

END :
	// 로컬 변수 멤버 변수로 다시 적용
	RESTORE;

	
	return ret;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         lengh 정보만 가지고 code 를 생성한다. 
//          RFC 1951 3.2.2
/// @param  
/// @return 
/// @date   Wednesday, April 07, 2010  1:53:36 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
bools CreateCodes(struct XInflate* inflate, unsigned char* lengths, int numSymbols, int* codes)
{
	int		bits;
	int		code = 0;
	int		bitLenCount[MAX_CODELEN+1];
	int		next_code[MAX_CODELEN+1];
	int		i;
	int		n;
	int		len;

	memset(bitLenCount, 0, sizeof(bitLenCount));
	for(i=0;i<numSymbols;i++)
	{
		bitLenCount[lengths[i]] ++;
	}

	bitLenCount[0] = 0;

	for(bits=1;bits<=MAX_CODELEN;bits++)
	{
		code = (code + bitLenCount[bits-1]) << 1;
		next_code[bits] = code;
	}

	for(n=0; n<numSymbols; n++)
	{
		len = lengths[n];
		if(len!=0)
		{
			codes[n] = next_code[len];
			next_code[len]++;
		}
	}

	return success;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///         codes + lengths 정보를 가지고 테이블을 만든다.
/// @param  
/// @return 
/// @date   Wednesday, April 07, 2010  3:43:21 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
bools CreateTable(struct XInflate* inflate, unsigned char* lengths, int numSymbols, struct XFastHuffTable** pTable, enum XINFLATE_ERR err)
{
	int			codes[MAX_SYMBOLS];
	// lengths 정보를 가지고 자동으로 codes 정보를 생성한다.
	if(CreateCodes(inflate,lengths, numSymbols, codes)==FALSE) 
	{	
		err = XINFLATE_ERR_CREATE_CODES;
		ASSERT(0); 
		return fail;
	}
	if(_CreateTable(inflate,codes, lengths, numSymbols, pTable)==FALSE)
	{
		err = XINFLATE_ERR_CREATE_TABLE;
		ASSERT(0); 
		return fail;
	}
	return success;
}
bools _CreateTable(struct XInflate* inflate, int* codes, unsigned char* lengths, int numSymbols, struct XFastHuffTable** pTable)
{
	int		bitLenCount[MAX_CODELEN+1];
	int		symbol;
	int		bitLen;

	int	bitLenMax;
	int	bitLenMin;
	//int i;
	// bit length 구하기
	memset(bitLenCount, 0, sizeof(bitLenCount));
	for(symbol=0;symbol<numSymbols;symbol++)
		bitLenCount[lengths[symbol]] ++;


	// 허프만 트리에서 유효한 최소 bitlen 과 최대 bitlen 구하기
	bitLenMax = 0;
	bitLenMin = MAX_CODELEN;

	for(bitLen=1;bitLen<=MAX_CODELEN;bitLen++)
	{
		if(bitLenCount[bitLen])
		{
			bitLenMax = max(bitLenMax, bitLen);
			bitLenMin = min(bitLenMin, bitLen);
		}
	}

	// 테이블 생성
	//*pTable = (struct XFastHuffTable*)malloc(sizeof(struct XFastHuffTable));

	
	if(pTable==0) 
		{ASSERT(0); return FALSE;}			// 발생 불가.

	Create(*pTable,bitLenMin, bitLenMax);


	// 테이블 채우기
	for(symbol=0;symbol<numSymbols;symbol++)
	{
		bitLen = lengths[symbol];
		if(bitLen)
			SetValue(*pTable,symbol, bitLen, codes[symbol]);
	}

#ifdef _DEBUG

	//for(i=0;i<pTable->itemCount;i++)
	//{
	//	if(pTable->pItem[i].symbol==-1)
	//		ASSERT(0);
	//}
#endif

	return success;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///         static 허프만에 사용할 테이블 만들기
/// @param  
/// @return 
/// @date   Thursday, April 08, 2010  4:17:06 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateStaticTable(struct XInflate* inflate)
{
	unsigned char		lengths[MAX_SYMBOLS];
	enum XINFLATE_ERR err;
	// static symbol 테이블 만들기
	// RFC1951 3.2.6
    memset(lengths, 8, 144);
    memset(lengths + 144, 9, 256 - 144);
    memset(lengths + 256, 7, 280 - 256);
    memset(lengths + 280, 8, 288 - 280);

	err = XINFLATE_ERR_OK;

	
	CreateTable(inflate, lengths, MAX_SYMBOLS, &inflate->m_pStaticInfTable, err);

	// static dist 테이블 만들기
	// RFC1951 3.2.6
	memset(lengths, 5, 32);
	CreateTable(inflate,lengths, 32, &inflate->m_pStaticDistTable, err);
}

// 비트스트림 초기화
void BS_Init(struct XInflate* inflate)
{
	inflate->m_bits = 0;
	inflate->m_bitLen = 0;
}
void Win_Init(struct XInflate *inflate)
{
	if(inflate->m_windowBuf==NULL)
		inflate->m_windowBuf = (unsigned char*)malloc(DEFLATE_WINDOW_SIZE);
	inflate->m_windowPos = 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
///         입력 버퍼가 충분할 경우 빠른 디코딩을 수행한다.
/// @param  
/// @return 
/// @date   Thursday, May 13, 2010  1:43:34 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
enum XINFLATE_ERR FastInflate(struct XInflate* inflate, const unsigned char** ppinBuffer, int* pinBufferRemain)//사용 18
{
	enum XINFLATE_ERR	ret = XINFLATE_ERR_OK;

	// 파라메터 저장
	const BYTE* inBuffer = *ppinBuffer;
	int  inBufferRemain = *pinBufferRemain;


	// 속도 향상을 위해서 멤버 변수를 로컬로 복사
	unsigned int					bits;
	int					bitLen;
	unsigned char*		windowBuf;
	int					windowPos;
	unsigned char*		outBuffer;
	int					outBufferAlloc;
	int					outBufferPos;
	enum STATE				state;
	int					windowLen;
	int					windowDistCode;
	int					symbol;
	// 로컬 변수
	const struct coderecord*	rec;
	struct XFastHuffItem*		pItem;			// HUFFLOOKUP() 에서 사용
	unsigned char		byte;			// 임시 변수
	int					extraLen;
	SAVE;

	// 루프 돌면서 압축 해제
	while(inBufferRemain>FASTINFLATE_MIN_BUFFER_NEED)
	{
		int dist = 0;

		FILLBUFFER_FAST();

		/////////////////////////////////////
		// 길이 가져오기
		rec = &lencodes[symbol - 257];

		// RFC 1951 3.2.5
		// 기본 길이에 extrabit 만큼의 비트의 내용을 더하면 진짜 길이가 나온다
		extraLen = BS_EATBITS(rec->extrabits);

		windowLen = rec->min + extraLen;	

		
		/////////////////////////////////////
		// 거리 코드 가져오기
		HUFFLOOKUP(windowDistCode, inflate->m_pCurrentDistTable);

		if(windowDistCode==30 || windowDistCode==31)			// 30 과 31은 생길 수 없다. RFC1951 3.2.6
			DOERR(XINFLATE_ERR_INVALID_DIST);

		FILLBUFFER_FAST();

		/////////////////////////////////////
		// 거리 가져오기
		rec = &distcodes[windowDistCode];
		// DIST 구하기
		dist = rec->min + BS_EATBITS(rec->extrabits);


		/////////////////////////////////////
		// lz77 출력
		if(outBufferAlloc-outBufferPos > windowLen)
		{
			// 출력 버퍼가 충분한 경우 - 출력 버퍼가 충분한지 여부를 체크하지 않는다.
			do
			{
				byte = *WIN_GETBUF(dist);
				WRITE_FAST(byte);
			}while(--windowLen);
		}
		else
		{
			// 출력 버퍼가 충분하지 않은 경우
			do
			{
				//unsigned char* temp;
				byte = *WIN_GETBUF(dist);
				WRITE(byte);
			}while(--windowLen);
		}


		/////////////////////////////////////
		// 심볼 가져오기.
		for(;;)
		{	
			// 입력 버퍼 체크
			if(!(inBufferRemain>FASTINFLATE_MIN_BUFFER_NEED)) 
			{
				state = STATE_GET_SYMBOL;
				goto END;
			}

			FILLBUFFER_FAST();

			HUFFLOOKUP_FAST(symbol, inflate->m_pCurrentTable);

			if(symbol<0) 
			{ASSERT(0); goto END;}					// 발생 불가.
			else if(symbol<256)
			{
				byte = (BYTE)symbol;
				WRITE(byte);
			}
			else if(symbol==256)	// END OF BLOCK
			{
				if(inflate->m_bFinalBlock)
					state = STATE_COMPLETED;
				else
					state = STATE_START;
				// 함수 종료
				goto END;
			}
			else if(symbol<286)
			{
				// 길이 가져오기로 진행한다.
				break;
			}
			else
				DOERR(XINFLATE_ERR_INVALID_SYMBOL);		// 발생 불가
		}
	}

END :
	RESTORE;

	// 파라메터 복원
	*ppinBuffer = inBuffer;
	*pinBufferRemain = inBufferRemain;

	return ret;
}
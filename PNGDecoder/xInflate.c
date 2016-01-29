#include "xInflate.h"
#include "assert.h"




// DEFLATE �� ���� Ÿ��
static enum 
{
	BTYPE_NOCOMP			= 0,
	BTYPE_FIXEDHUFFMAN		= 1,
	BTYPE_DYNAMICHUFFMAN	= 2,
	BTYPE_RESERVED			= 3		// ERROR
} BTYPE;

// len, dist ���̺� - halibut �� deflate.c ���� ������
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


// ��ƿ ��ũ��
#define HUFFTABLE_VALUE_NOT_EXIST	-1
#ifndef ASSERT
#define ASSERT(x) {}
#endif

////////////////////////////////////////////////
//
// ��Ʈ ��Ʈ�� ��ũ��ȭ
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
// ��Ʈ ��Ʈ�� ��ũ�� ó��
//
////////////////////////////////////////////////


////////////////////////////////////////////////
//
// ������ ��ũ��
//
#	define ADD_INPUT_COUNT		inflate->m_inputCount ++
#	define ADD_OUTPUT_COUNT		inflate->m_outputCount ++

#define	DOERR(x) { printf("ERROR...."); }

////////////////////////////////////////////////
//
// �ݺ� �۾� ��ũ��ȭ
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
		/* ����Ÿ ���� */										\
		if(pTable->bitLenMin > bitLen)							\
		{														\
			result = -1;										\
		}														\
		else													\
		{														\
			pItem = &(pTable->pItem[pTable->mask & bits]);		\
			/* ����Ÿ ���� */									\
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
		if(inflate->m_isExternalOutBuffer)/* ���۸� �ø����� ���� ��Ȳ*/	\
			DOERR(XINFLATE_ERR_INSUFFICIENT_OUT_BUFFER);		\
																\
		/* ���� �ø��� */										\
		outBufferAlloc = outBufferAlloc * 2;					\
		temp = outBuffer;									\
		outBuffer = (unsigned char*)realloc(outBuffer, outBufferAlloc);	\
		/* alloc ���� ó�� */									\
		if(outBuffer==NULL)										\
		{														\
			assert(0);											\
			ret = XINFLATE_ERR_ALLOC_FAIL;						\
			outBuffer = temp;									\
			outBufferPos = 0; /* �׳� ó�� ��ġ�� �ǵ�����.*/	\
			outBufferAlloc /= 2;								\
			goto END;											\
		}														\
	}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//                                              FastInflate
//


// �Է� ���۰� ������� üũ���� �ʴ´�.
#define FILLBUFFER_FAST()								\
	while(bitLen<=24)									\
	{													\
		BS_ADDBYTE(*inBuffer);							\
		inBufferRemain--;								\
		inBuffer++;										\
		ADD_INPUT_COUNT;								\
	}													\

// ��Ʈ��Ʈ�� ����Ÿ(bits + bitLen)�� ������� üũ���� �ʴ´�.
#define HUFFLOOKUP_FAST(result, pTable)					\
		pItem = &(pTable->pItem[pTable->mask & bits]);	\
		result = pItem->symbol;							\
		BS_REMOVEBITS(pItem->bitLen);					


// ��� ���۰� ������� üũ���� �ʴ´�.
#define WRITE_FAST(byte)										\
	ADD_OUTPUT_COUNT;											\
	WIN_ADD(byte);												\
	outBuffer[outBufferPos] = byte;								\
	outBufferPos++;												


////////////////////////////////////////////////
//
// ������ ��ũ��ȭ
//

#define WIN_ADD(b)												\
	windowBuf[windowPos] = b;									\
	windowPos = (windowPos+1) & (DEFLATE_WINDOW_SIZE-1);

#define WIN_GETBUF(dist)										\
	(windowBuf +  ((windowPos - dist) & (DEFLATE_WINDOW_SIZE-1)) )

//
// ������ ��ũ��ȭ
//
////////////////////////////////////////////////



/////////////////////////////////////////////////////
//
// ������ Ʈ���� ���� Ž���� �� �ְ� �ϱ� ���ؼ�
// Ʈ�� ��ü�� �ϳ��� �迭�� ����� ������.
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
		if(fastHuff->pItem) ASSERT(0);							// �߻� �Ұ�

		fastHuff->bitLenMin = _bitLenMin;
		fastHuff->bitLenMax = _bitLenMax;
		fastHuff->mask	  = (1<<fastHuff->bitLenMax) - 1;					// ����ũ
		fastHuff->itemCount = 1<<fastHuff->bitLenMax;						// ���� ������ �ִ� ���̺� ũ��
		fastHuff->pItem     = (struct XFastHuffItem *)calloc(fastHuff->itemCount, sizeof(struct XFastHuffItem));		// 2^maxBitLen ��ŭ �迭�� �����. // jmlee 1000�̸� �����ȳ��µ� sizeof���� ������...
		Init_FastHufftem(fastHuff->pItem); // jmlee ������ ȣ��
	}
	// �ɺ� ����Ÿ�� ������ ��ü �迭�� ä���.
	void	SetValue(struct XFastHuffTable *fastHuff, int symbol, int bitLen, int bitCode)
	{

	int		add2code = (1<<bitLen);		// bitLen �� 3 �̶�� add2code ���� 1000(bin) �� ����
		int revBitCode = 0;
		// ������
		int i;
		for(i=0;i<bitLen;i++)
		{
			revBitCode <<= 1;
			revBitCode |= (bitCode & 0x01);
			bitCode >>= 1;
		}

		

		// �迭 ä���
		for(;;)
		{
#ifdef _DEBUG
			if(fastHuff->pItem[revBitCode].symbol!=  HUFFTABLE_VALUE_NOT_EXIST) 
				ASSERT(0);
#endif

			fastHuff->pItem[revBitCode].bitLen = bitLen;
			fastHuff->pItem[revBitCode].symbol = symbol;

			// ���� ������ bit code �� ó���ϱ� ���ؼ� ���� ��� ���Ѵ�.
			revBitCode += add2code;

			// ���� ������ ������ ��� ��� ������
			if(revBitCode >= fastHuff->itemCount)
				break;
		}
	}




// ��� ������ ���÷� �����ϰ� ���� ������ ��� ������ ��ȯ�ϱ�

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
///         ���� ���� �ʱ�ȭ
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
///         ��� ���۸� ���ο��� alloc ������ �ʰ�, �ܺ��� ���۸� ����Ѵ�.
/// @param  
/// @return 
/// @date   Wednesday, May 19, 2010  12:44:50 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
void SetOutBuffer(struct XInflate* inflate, unsigned char* outBuf, int outBufLen)
{
	// ������ ����ϴ� ���� ������ ���� �����.
	if(inflate->m_isExternalOutBuffer==fail)
	{
		//free(inflate->m_outBuffer);
	}

	if(outBuf)
	{
		// �ܺ� ���۸� ��� ���۷� �����ϱ�
		inflate->m_outBuffer = outBuf;
		inflate->m_outBufferAlloc = outBufLen;
		inflate->m_outBufferPos = 0;
		inflate->m_isExternalOutBuffer = success;
	}
	else
	{
		// outBuf==NULL �̸� �ܺ� ���۸� ���̻� ��� ���ϴ� ����̴�.
		inflate->m_outBuffer = outBuf;
		inflate->m_outBufferAlloc = 0;
		inflate->m_outBufferPos = 0;
		inflate->m_isExternalOutBuffer = fail;
	}

}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         ���� ���� - ȣ���� Init() �� �ݵ�� ȣ���� ��� �Ѵ�.
/// @param  
/// @return 
/// @date   Thursday, April 08, 2010  4:18:55 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
enum XINFLATE_ERR Inflate(struct XInflate* inflate, const unsigned char* inBuffer, int inBufferRemain)
{
	enum XINFLATE_ERR	ret;
		// �ӵ� ����� ���ؼ� ��� ������ ���÷� ����
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



	
	// ���� ����
	const struct coderecord*	rec;
	struct XFastHuffItem*		pItem;			// HUFFLOOKUP() ���� ���
	unsigned char				byte;			// �ӽ� ����
	enum XINFLATE_ERR			err;
		
	// ��� ���� ��ġ �ʱ�ȭ
	inflate->m_outBufferPos = 0;

	SAVE;

	err = XINFLATE_ERR_OK;
	ret = XINFLATE_ERR_OK;

	// �̹� �Ϸ�Ǿ����� �˻�
	if(inflate->m_bCompleted)
		DOERR(XINFLATE_ERR_BLOCK_COMPLETED);

	// �ʱ�ȭ �Լ� ȣ�� �Ǿ����� �˻�
	if(inflate->m_copyright==NULL)
		DOERR(XINFLATE_ERR_NOT_INITIALIZED);


	// ���� ���鼭 ���� ����
	for(;;)
	{
		FILLBUFFER();

		switch(state)
		{
			// ��� �м� ����
		case STATE_START :
			if(bitLen<3) 
				goto END;

			// ������ �� ����
			inflate->m_bFinalBlock = BS_EATBIT();

			// ��Ÿ�� ����� 2bit 
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

			// ���� �ȵ�
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
			// ����Ÿ ��������
			if(bitLen<8) goto END;

			//////////////////////////////////////////////
			//
			// ���� �ڵ�
			//
			/*
			{
				byte = BS_EATBITS(8);
				WRITE(byte);
			}
			m_uncompRemain--;
			*/

			if(bitLen%8!=0) ASSERT(0);			// �߻��Ұ�, �׳� Ȯ�ο�

			//////////////////////////////////////////////
			//
			// �Ʒ��� ������ �ڵ�. �׷��� �� ���̰� ���� ��.��
			//

			// ��Ʈ ��Ʈ���� ���� ����

			while(bitLen && inflate->m_uncompRemain)
			{
				//unsigned char* temp;
				byte = BS_EATBITS(8);
				WRITE(byte);
				inflate->m_uncompRemain--;
			}

			// ������ ����Ÿ�� ����Ʈ �״�� ����
			{
				int	toCopy, toCopy2;
				toCopy = toCopy2 = min((int)inflate->m_uncompRemain, inBufferRemain);

				/* �����ڵ�
				while(toCopy)
				{
					WRITE(*inBuffer);
					inBuffer++;
					toCopy--;
				}
				*/

				if(outBufferAlloc-outBufferPos > toCopy)
				{
					// ��� ���۰� ����� ��� - ��� ���۰� ������� ���θ� üũ���� �ʴ´�.
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
			// ������ �ڵ� ��
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

			// ���� ������
		case STATE_FIXED_HUFFMAN :
			//if(inflate->m_pStaticInfTable==NULL)
				CreateStaticTable(inflate);

			inflate->m_pCurrentTable = inflate->m_pStaticInfTable;
			inflate->m_pCurrentDistTable = inflate->m_pStaticDistTable;
			state = STATE_GET_SYMBOL;
			break;

			// ���� ��������
		case STATE_GET_LEN :
			// zlib �� inflate_fast ȣ�� ���� �䳻����
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
			// �⺻ ���̿� extrabit ��ŭ�� ��Ʈ�� ������ ���ϸ� ��¥ ���̰� ���´�
			{
				int extraLen = BS_EATBITS(rec->extrabits);
				windowLen = rec->min + extraLen;	
			}
			state = STATE_GET_DISTCODE;
			//break;	�ʿ� ����..

			// �Ÿ� �ڵ� ��������
		case STATE_GET_DISTCODE :
			HUFFLOOKUP(windowDistCode, inflate->m_pCurrentDistTable);

			if(windowDistCode<0)
				goto END;

			if(windowDistCode==30 || windowDistCode==31)			// 30 �� 31�� ���� �� ����. RFC1951 3.2.6
				DOERR(XINFLATE_ERR_INVALID_DIST);

			state = STATE_GET_DIST;

			FILLBUFFER();
			//break;		// �ʿ����

			// �Ÿ� ��������
		case STATE_GET_DIST:
			rec = &distcodes[windowDistCode];
			// DIST ���ϱ�
			if(bitLen<rec->extrabits)
				goto END;
			{
				int dist = rec->min + BS_EATBITS(rec->extrabits);

				// lz77 ���
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
			//break;		// �ʿ� ����

			// �ɺ� ��������.
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
				DOERR(XINFLATE_ERR_INVALID_SYMBOL);		// �߻� �Ұ�

			break;

			// ���̳��� ������ ����
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

			// ������ ���� ��������.
		case STATE_DYNAMIC_HUFFMAN_LENLEN :

			if(bitLen<3) 
				goto END;

			// 3bit �� �ڵ� ������ �ڵ� ���� ���� ��������.
			while (inflate->m_lenghtsPtr < inflate->m_lenghtsNum && bitLen >= 3) 
			{
				if(inflate->m_lenghtsPtr>sizeof(lenlenmap))						// �Է°� üũ..
					DOERR(XINFLATE_ERR_INVALID_LEN);

				
				inflate->m_lengths[lenlenmap[inflate->m_lenghtsPtr]] = BS_EATBITS(3);
				inflate->m_lenghtsPtr++;
			}

			// �� ����������..
			if (inflate->m_lenghtsPtr == inflate->m_lenghtsNum)
			{
				// ���̿� ���� ������ ���̺� �����
				if(CreateTable(inflate, inflate->m_lengths, 19, &inflate->m_pLenLenTable, err)==FALSE)
					DOERR(err);
				state = STATE_DYNAMIC_HUFFMAN_LEN;
				inflate->m_lenghtsPtr = 0;
			}
			break;

			// ����� ���� ������ m_pLenLenTable �� ���ļ� ��������.
		case STATE_DYNAMIC_HUFFMAN_LEN:

			// �� ����������
			if (inflate->m_lenghtsPtr >= inflate->m_literalsNum + inflate->m_distancesNum) 
			{
				// ���� ���̳��� ���̺� ���� (literal + distance)
				if(	CreateTable(inflate, inflate->m_lengths, inflate->m_literalsNum, &inflate->m_pDynamicInfTable, err)==FALSE ||
					CreateTable(inflate, inflate->m_lengths + inflate->m_literalsNum, inflate->m_distancesNum, &inflate->m_pDynamicDistTable, err)==FALSE)
					DOERR(err);

				// lenlen ���̺��� ���� ���̻� �ʿ����.
				ReleaseXFastBuffTable(inflate->m_pLenLenTable);
				//free(inflate->m_pLenLenTable);

				// ���̺� ����
				inflate->m_pCurrentTable = inflate->m_pDynamicInfTable;
				inflate->m_pCurrentDistTable = inflate->m_pDynamicDistTable;

				// ��¥ ���� ���� ����
				state = STATE_GET_SYMBOL;
				break;
			}

			{
				// ���� ���� �ڵ� ��������
				int code=-1;
				HUFFLOOKUP(code, inflate->m_pLenLenTable);

				if (code == -1)
					goto END;

				if (code < 16) 
				{
					if(inflate->m_lenghtsPtr>sizeof(inflate->m_lengths))		// �� üũ
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
	// ���� ���� ��� ������ �ٽ� ����
	RESTORE;

	
	return ret;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
///         lengh ������ ������ code �� �����Ѵ�. 
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
///         codes + lengths ������ ������ ���̺��� �����.
/// @param  
/// @return 
/// @date   Wednesday, April 07, 2010  3:43:21 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
bools CreateTable(struct XInflate* inflate, unsigned char* lengths, int numSymbols, struct XFastHuffTable** pTable, enum XINFLATE_ERR err)
{
	int			codes[MAX_SYMBOLS];
	// lengths ������ ������ �ڵ����� codes ������ �����Ѵ�.
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
	// bit length ���ϱ�
	memset(bitLenCount, 0, sizeof(bitLenCount));
	for(symbol=0;symbol<numSymbols;symbol++)
		bitLenCount[lengths[symbol]] ++;


	// ������ Ʈ������ ��ȿ�� �ּ� bitlen �� �ִ� bitlen ���ϱ�
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

	// ���̺� ����
	//*pTable = (struct XFastHuffTable*)malloc(sizeof(struct XFastHuffTable));

	
	if(pTable==0) 
		{ASSERT(0); return FALSE;}			// �߻� �Ұ�.

	Create(*pTable,bitLenMin, bitLenMax);


	// ���̺� ä���
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
///         static �������� ����� ���̺� �����
/// @param  
/// @return 
/// @date   Thursday, April 08, 2010  4:17:06 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateStaticTable(struct XInflate* inflate)
{
	unsigned char		lengths[MAX_SYMBOLS];
	enum XINFLATE_ERR err;
	// static symbol ���̺� �����
	// RFC1951 3.2.6
    memset(lengths, 8, 144);
    memset(lengths + 144, 9, 256 - 144);
    memset(lengths + 256, 7, 280 - 256);
    memset(lengths + 280, 8, 288 - 280);

	err = XINFLATE_ERR_OK;

	
	CreateTable(inflate, lengths, MAX_SYMBOLS, &inflate->m_pStaticInfTable, err);

	// static dist ���̺� �����
	// RFC1951 3.2.6
	memset(lengths, 5, 32);
	CreateTable(inflate,lengths, 32, &inflate->m_pStaticDistTable, err);
}

// ��Ʈ��Ʈ�� �ʱ�ȭ
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
///         �Է� ���۰� ����� ��� ���� ���ڵ��� �����Ѵ�.
/// @param  
/// @return 
/// @date   Thursday, May 13, 2010  1:43:34 PM
////////////////////////////////////////////////////////////////////////////////////////////////////
enum XINFLATE_ERR FastInflate(struct XInflate* inflate, const unsigned char** ppinBuffer, int* pinBufferRemain)//��� 18
{
	enum XINFLATE_ERR	ret = XINFLATE_ERR_OK;

	// �Ķ���� ����
	const BYTE* inBuffer = *ppinBuffer;
	int  inBufferRemain = *pinBufferRemain;


	// �ӵ� ����� ���ؼ� ��� ������ ���÷� ����
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
	// ���� ����
	const struct coderecord*	rec;
	struct XFastHuffItem*		pItem;			// HUFFLOOKUP() ���� ���
	unsigned char		byte;			// �ӽ� ����
	int					extraLen;
	SAVE;

	// ���� ���鼭 ���� ����
	while(inBufferRemain>FASTINFLATE_MIN_BUFFER_NEED)
	{
		int dist = 0;

		FILLBUFFER_FAST();

		/////////////////////////////////////
		// ���� ��������
		rec = &lencodes[symbol - 257];

		// RFC 1951 3.2.5
		// �⺻ ���̿� extrabit ��ŭ�� ��Ʈ�� ������ ���ϸ� ��¥ ���̰� ���´�
		extraLen = BS_EATBITS(rec->extrabits);

		windowLen = rec->min + extraLen;	

		
		/////////////////////////////////////
		// �Ÿ� �ڵ� ��������
		HUFFLOOKUP(windowDistCode, inflate->m_pCurrentDistTable);

		if(windowDistCode==30 || windowDistCode==31)			// 30 �� 31�� ���� �� ����. RFC1951 3.2.6
			DOERR(XINFLATE_ERR_INVALID_DIST);

		FILLBUFFER_FAST();

		/////////////////////////////////////
		// �Ÿ� ��������
		rec = &distcodes[windowDistCode];
		// DIST ���ϱ�
		dist = rec->min + BS_EATBITS(rec->extrabits);


		/////////////////////////////////////
		// lz77 ���
		if(outBufferAlloc-outBufferPos > windowLen)
		{
			// ��� ���۰� ����� ��� - ��� ���۰� ������� ���θ� üũ���� �ʴ´�.
			do
			{
				byte = *WIN_GETBUF(dist);
				WRITE_FAST(byte);
			}while(--windowLen);
		}
		else
		{
			// ��� ���۰� ������� ���� ���
			do
			{
				//unsigned char* temp;
				byte = *WIN_GETBUF(dist);
				WRITE(byte);
			}while(--windowLen);
		}


		/////////////////////////////////////
		// �ɺ� ��������.
		for(;;)
		{	
			// �Է� ���� üũ
			if(!(inBufferRemain>FASTINFLATE_MIN_BUFFER_NEED)) 
			{
				state = STATE_GET_SYMBOL;
				goto END;
			}

			FILLBUFFER_FAST();

			HUFFLOOKUP_FAST(symbol, inflate->m_pCurrentTable);

			if(symbol<0) 
			{ASSERT(0); goto END;}					// �߻� �Ұ�.
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
				// �Լ� ����
				goto END;
			}
			else if(symbol<286)
			{
				// ���� ��������� �����Ѵ�.
				break;
			}
			else
				DOERR(XINFLATE_ERR_INVALID_SYMBOL);		// �߻� �Ұ�
		}
	}

END :
	RESTORE;

	// �Ķ���� ����
	*ppinBuffer = inBuffer;
	*pinBufferRemain = inBufferRemain;

	return ret;
}
#pragma once

#include <windows.h>
#define DEFLATE_WINDOW_SIZE 32768			// RFC 에 정의된 WINDOW 크기

#define DEFAULT_ALLOC_SIZE 1024*256			// 기본 출력 버퍼 크기
#define MAX_SYMBOLS 288 						// deflate 에 정의된 심볼 수
#define MAX_CODELEN 16 					// deflate 허프만 트리 최대 코드 길이
#define FASTINFLATE_MIN_BUFFER_NEED 6 		// FastInflate() 호출시 필요한 최소 입력 버퍼수

typedef enum {fail, success} bools;

// 에러 코드
enum 
{
	XINFLATE_ERR_OK,						// 성공
	XINFLATE_ERR_NOT_INITIALIZED,			// Init() 을 호출하지 않았음.
	XINFLATE_ERR_HEADER,					// 헤더에 문제 있음
	XINFLATE_ERR_INVALID_NOCOMP_LEN,		// no compressio 헤더에 문제 있음
	XINFLATE_ERR_ALLOC_FAIL,				// 메모리 ALLOC 실패
	XINFLATE_ERR_INVALID_DIST,				// DIST 정보에서 에러 발생
	XINFLATE_ERR_INVALID_SYMBOL,			// SYMBOL 정보에서 에러 발생
	XINFLATE_ERR_BLOCK_COMPLETED,			// 압축 해제가 이미 완료되었음.
	XINFLATE_ERR_CREATE_CODES,				// code 생성중 에러 발생
	XINFLATE_ERR_CREATE_TABLE,				// table 생성중 에러 발생
	XINFLATE_ERR_INVALID_LEN,				// LEN 정보에 문제 있음
	XINFLATE_ERR_INSUFFICIENT_OUT_BUFFER,	// 외부 출력 버퍼가 모자라다.
} XINFLATE_ERR;


typedef struct XFastHuffItem									// XFastHuffTable 의 아이템
{
	unsigned int		bitLen;										// 유효한 비트수
	int		symbol;										// 유효한 심볼

} XFastHuffItem;

struct XFastHuffTable
{
	struct XFastHuffItem*	pItem;							// (huff) code 2 symbol 아이템, code가 배열의 위치 정보가 된다.
	unsigned int			bitLenMin;						// 유효한 최소 비트수
	unsigned int			bitLenMax;						// 유효한 최대 비트수
	unsigned int			mask;							// bitLenMax 에 대한 bit mask
	unsigned int			itemCount;						// bitLenMax 로 생성 가능한 최대 아이템 숫자
}XFastHuffTable;
void	ReleaseXFastBuffTable(struct XFastHuffTable *fastHuff);
void	Create(struct XFastHuffTable *fastHuff, int _bitLenMin, int _bitLenMax);
void	SetValue(struct XFastHuffTable *fastHuff, int symbol, int bitLen, int bitCode);


struct XInflate
{
		unsigned char			m_lengths[286+32];		// literal + distance 의 length 가 같이 들어간다.
enum					// 내부 상태
	{
		STATE_START,
		
		STATE_NO_COMPRESSION,
		STATE_NO_COMPRESSION_LEN,
		STATE_NO_COMPRESSION_NLEN,
		STATE_NO_COMPRESSION_BYPASS,

		STATE_FIXED_HUFFMAN,

		STATE_GET_SYMBOL_ONLY,
		STATE_GET_LEN,
		STATE_GET_DISTCODE,
		STATE_GET_DIST,
		STATE_GET_SYMBOL,

		STATE_DYNAMIC_HUFFMAN,
		STATE_DYNAMIC_HUFFMAN_LENLEN,
		STATE_DYNAMIC_HUFFMAN_LEN,
		STATE_DYNAMIC_HUFFMAN_LENREP,

		STATE_COMPLETED,
	} STATE;

	enum STATE					m_state;				// 현재 압축 해제 상태

	struct XFastHuffTable*			m_pStaticInfTable;		// static huffman table
	struct XFastHuffTable*			m_pStaticDistTable;		// static huffman table (dist)
	struct XFastHuffTable*			m_pDynamicInfTable;		// 
	struct XFastHuffTable*			m_pDynamicDistTable;	// 
	struct XFastHuffTable*			m_pCurrentTable;		// 
	struct XFastHuffTable*			m_pCurrentDistTable;	// 
	unsigned int			m_symbol;				// current huffman symbol

	unsigned int			m_windowLen;			// LZ77 길이
	unsigned int			m_windowDistCode;		// dist 를 얻기 위한 코드


	unsigned char*			m_outBuffer;			// 출력 버퍼 (내부에서 alloc 한다)
	unsigned int			m_outBufferAlloc;		// 출력 버퍼 크기
	unsigned int			m_outBufferPos;			// 출력 버퍼 쓰는 위치
	bools					m_isExternalOutBuffer;	// 출력 버퍼를 외부 버퍼를 사용하남?

	unsigned int			m_uncompRemain;			// no compression 인 경우 블럭의 남은 데이타
	bools					m_bFinalBlock;			// 현재 마지막 블럭을 처리중인가?
	bools					m_bCompleted;			// 완료 되었는가?

	unsigned int			m_literalsNum;			// dynamic huffman - # of literal/length codes   (267~286)
	unsigned int			m_distancesNum;			// "               - # of distance codes         (1~32)
	unsigned int			m_lenghtsNum;			//                 - # of code length codes      (4~19)

	unsigned int			m_lenghtsPtr;			// m_lengths 에서 현재 위치
	unsigned int			m_lenExtraBits;
	unsigned int			m_lenAddOn;
	unsigned int			m_lenRep;
	struct XFastHuffTable*	m_pLenLenTable;
	const char*				m_copyright;
	
	
	// 비트 스트림 처리 부분
	unsigned int			m_bits;					// 32bit 비트 스트림
	unsigned int			m_bitLen;				// m_bits 에서 유효한 bit count

											// lz77 window 처리 부분
	unsigned char*			m_windowBuf;
	unsigned int			m_windowPos;			// 현재 쓰고자 하는 위치


//#ifdef _DEBUG
	unsigned int			m_inputCount;
	unsigned int			m_outputCount;
//#endif
};

void BS_Init(struct XInflate *inflate);
void Win_Init(struct XInflate *infalte); // jmlee WinInit 추가
void Init_inf(struct XInflate *inflate);
void Destroy_inf(struct XInflate *inflate);
enum XINFLATE_ERR Inflate(struct XInflate *inflate, const unsigned char* inBuffer, int inBufferLen);

void CreateStaticTable(struct XInflate *inflate);
bools CreateCodes(struct XInflate *inflate, unsigned char* lengths, int numSymbols, int* codes);
bools CreateTable(struct XInflate *inflate, unsigned char* lengths, int numSymbols, struct XFastHuffTable** pTable, enum XINFLATE_ERR err);
bools _CreateTable(struct XInflate* inflate, int* codes, unsigned char* lengths, int numSymbols, struct XFastHuffTable** pTable);

enum XINFLATE_ERR FastInflate(struct XInflate* inflate, const unsigned char** ppinBuffer, int* pinBufferRemain);


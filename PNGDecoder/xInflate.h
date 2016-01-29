#pragma once

#include <windows.h>
#define DEFLATE_WINDOW_SIZE 32768			// RFC �� ���ǵ� WINDOW ũ��

#define DEFAULT_ALLOC_SIZE 1024*256			// �⺻ ��� ���� ũ��
#define MAX_SYMBOLS 288 						// deflate �� ���ǵ� �ɺ� ��
#define MAX_CODELEN 16 					// deflate ������ Ʈ�� �ִ� �ڵ� ����
#define FASTINFLATE_MIN_BUFFER_NEED 6 		// FastInflate() ȣ��� �ʿ��� �ּ� �Է� ���ۼ�

typedef enum {fail, success} bools;

// ���� �ڵ�
enum 
{
	XINFLATE_ERR_OK,						// ����
	XINFLATE_ERR_NOT_INITIALIZED,			// Init() �� ȣ������ �ʾ���.
	XINFLATE_ERR_HEADER,					// ����� ���� ����
	XINFLATE_ERR_INVALID_NOCOMP_LEN,		// no compressio ����� ���� ����
	XINFLATE_ERR_ALLOC_FAIL,				// �޸� ALLOC ����
	XINFLATE_ERR_INVALID_DIST,				// DIST �������� ���� �߻�
	XINFLATE_ERR_INVALID_SYMBOL,			// SYMBOL �������� ���� �߻�
	XINFLATE_ERR_BLOCK_COMPLETED,			// ���� ������ �̹� �Ϸ�Ǿ���.
	XINFLATE_ERR_CREATE_CODES,				// code ������ ���� �߻�
	XINFLATE_ERR_CREATE_TABLE,				// table ������ ���� �߻�
	XINFLATE_ERR_INVALID_LEN,				// LEN ������ ���� ����
	XINFLATE_ERR_INSUFFICIENT_OUT_BUFFER,	// �ܺ� ��� ���۰� ���ڶ��.
} XINFLATE_ERR;


typedef struct XFastHuffItem									// XFastHuffTable �� ������
{
	unsigned int		bitLen;										// ��ȿ�� ��Ʈ��
	int		symbol;										// ��ȿ�� �ɺ�

} XFastHuffItem;

struct XFastHuffTable
{
	struct XFastHuffItem*	pItem;							// (huff) code 2 symbol ������, code�� �迭�� ��ġ ������ �ȴ�.
	unsigned int			bitLenMin;						// ��ȿ�� �ּ� ��Ʈ��
	unsigned int			bitLenMax;						// ��ȿ�� �ִ� ��Ʈ��
	unsigned int			mask;							// bitLenMax �� ���� bit mask
	unsigned int			itemCount;						// bitLenMax �� ���� ������ �ִ� ������ ����
}XFastHuffTable;
void	ReleaseXFastBuffTable(struct XFastHuffTable *fastHuff);
void	Create(struct XFastHuffTable *fastHuff, int _bitLenMin, int _bitLenMax);
void	SetValue(struct XFastHuffTable *fastHuff, int symbol, int bitLen, int bitCode);


struct XInflate
{
		unsigned char			m_lengths[286+32];		// literal + distance �� length �� ���� ����.
enum					// ���� ����
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

	enum STATE					m_state;				// ���� ���� ���� ����

	struct XFastHuffTable*			m_pStaticInfTable;		// static huffman table
	struct XFastHuffTable*			m_pStaticDistTable;		// static huffman table (dist)
	struct XFastHuffTable*			m_pDynamicInfTable;		// 
	struct XFastHuffTable*			m_pDynamicDistTable;	// 
	struct XFastHuffTable*			m_pCurrentTable;		// 
	struct XFastHuffTable*			m_pCurrentDistTable;	// 
	unsigned int			m_symbol;				// current huffman symbol

	unsigned int			m_windowLen;			// LZ77 ����
	unsigned int			m_windowDistCode;		// dist �� ��� ���� �ڵ�


	unsigned char*			m_outBuffer;			// ��� ���� (���ο��� alloc �Ѵ�)
	unsigned int			m_outBufferAlloc;		// ��� ���� ũ��
	unsigned int			m_outBufferPos;			// ��� ���� ���� ��ġ
	bools					m_isExternalOutBuffer;	// ��� ���۸� �ܺ� ���۸� ����ϳ�?

	unsigned int			m_uncompRemain;			// no compression �� ��� ���� ���� ����Ÿ
	bools					m_bFinalBlock;			// ���� ������ ���� ó�����ΰ�?
	bools					m_bCompleted;			// �Ϸ� �Ǿ��°�?

	unsigned int			m_literalsNum;			// dynamic huffman - # of literal/length codes   (267~286)
	unsigned int			m_distancesNum;			// "               - # of distance codes         (1~32)
	unsigned int			m_lenghtsNum;			//                 - # of code length codes      (4~19)

	unsigned int			m_lenghtsPtr;			// m_lengths ���� ���� ��ġ
	unsigned int			m_lenExtraBits;
	unsigned int			m_lenAddOn;
	unsigned int			m_lenRep;
	struct XFastHuffTable*	m_pLenLenTable;
	const char*				m_copyright;
	
	
	// ��Ʈ ��Ʈ�� ó�� �κ�
	unsigned int			m_bits;					// 32bit ��Ʈ ��Ʈ��
	unsigned int			m_bitLen;				// m_bits ���� ��ȿ�� bit count

											// lz77 window ó�� �κ�
	unsigned char*			m_windowBuf;
	unsigned int			m_windowPos;			// ���� ������ �ϴ� ��ġ


//#ifdef _DEBUG
	unsigned int			m_inputCount;
	unsigned int			m_outputCount;
//#endif
};

void BS_Init(struct XInflate *inflate);
void Win_Init(struct XInflate *infalte); // jmlee WinInit �߰�
void Init_inf(struct XInflate *inflate);
void Destroy_inf(struct XInflate *inflate);
enum XINFLATE_ERR Inflate(struct XInflate *inflate, const unsigned char* inBuffer, int inBufferLen);

void CreateStaticTable(struct XInflate *inflate);
bools CreateCodes(struct XInflate *inflate, unsigned char* lengths, int numSymbols, int* codes);
bools CreateTable(struct XInflate *inflate, unsigned char* lengths, int numSymbols, struct XFastHuffTable** pTable, enum XINFLATE_ERR err);
bools _CreateTable(struct XInflate* inflate, int* codes, unsigned char* lengths, int numSymbols, struct XFastHuffTable** pTable);

enum XINFLATE_ERR FastInflate(struct XInflate* inflate, const unsigned char** ppinBuffer, int* pinBufferRemain);


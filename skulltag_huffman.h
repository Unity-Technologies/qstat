//-------------------------------------------------------------------------------
//
// skulltag_huffman.h
//
//
//
// Version 5, released 5/13/2009. Compatible with Skulltag launchers and servers.
//
//-------------------------------------------------------------------------------

#ifndef __HUFFMAN_H__
#define __HUFFMAN_H__

//*****************************************************************************
//	STRUCTURES

typedef struct huffnode_s
{
	struct huffnode_s *zero;
	struct huffnode_s *one;
	unsigned char val;
	float freq;

} huffnode_t;

typedef struct
{
	unsigned int bits;
	int len;

} hufftab_t;

//*****************************************************************************
//	PROTOTYPES

void HUFFMAN_Construct( void );
void HUFFMAN_Destruct( void );

void HUFFMAN_Encode( unsigned char *in, unsigned char *out, int inlen, int *outlen );
void HUFFMAN_Decode( unsigned char *in, unsigned char *out, int inlen, int *outlen );

#endif // __HUFFMAN_H__

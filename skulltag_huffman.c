//-------------------------------------------------------------------------------
//
// huffman.c
//
//
//
// Version 5, released 5/13/2009. Compatible with Skulltag launchers and servers.
//
//-------------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <ctype.h>
#include <math.h>

#define USE_WINDOWS_DWORD
#include "skulltag_huffman.h"
#define M_Malloc malloc

//*****************************************************************************
//	PROTOTYPES

static void						huffman_BuildTree( float *freq );
static void						huffman_PutBit( unsigned char *puf, int pos, int bit );
static int						huffman_GetBit( unsigned char *puf, int pos );
#ifdef _DEBUG
static void						huffman_ZeroFreq( void );
#endif
static void						huffman_RecursiveFreeNode( huffnode_t *pNode );

//*****************************************************************************
//	VARIABLES

int LastCompMessageSize = 0;

static huffnode_t *HuffTree=0;
static hufftab_t HuffLookup[256];

#ifdef _DEBUG
static int HuffIn=0;
static int HuffOut=0;
#endif


static unsigned char Masks[8]=
{
	0x1,
	0x2,
	0x4,
	0x8,
	0x10,
	0x20,
	0x40,
	0x80
};

static float HuffFreq[256]=
{
 0.14473691,
 0.01147017,
 0.00167522,
 0.03831121,
 0.00356579,
 0.03811315,
 0.00178254,
 0.00199644,
 0.00183511,
 0.00225716,
 0.00211240,
 0.00308829,
 0.00172852,
 0.00186608,
 0.00215921,
 0.00168891,
 0.00168603,
 0.00218586,
 0.00284414,
 0.00161833,
 0.00196043,
 0.00151029,
 0.00173932,
 0.00218370,
 0.00934121,
 0.00220530,
 0.00381211,
 0.00185456,
 0.00194675,
 0.00161977,
 0.00186680,
 0.00182071,
 0.06421956,
 0.00537786,
 0.00514019,
 0.00487155,
 0.00493925,
 0.00503143,
 0.00514019,
 0.00453520,
 0.00454241,
 0.00485642,
 0.00422407,
 0.00593387,
 0.00458130,
 0.00343687,
 0.00342823,
 0.00531592,
 0.00324890,
 0.00333388,
 0.00308613,
 0.00293776,
 0.00258918,
 0.00259278,
 0.00377105,
 0.00267488,
 0.00227516,
 0.00415997,
 0.00248763,
 0.00301555,
 0.00220962,
 0.00206990,
 0.00270369,
 0.00231694,
 0.00273826,
 0.00450928,
 0.00384380,
 0.00504728,
 0.00221251,
 0.00376961,
 0.00232990,
 0.00312574,
 0.00291688,
 0.00280236,
 0.00252436,
 0.00229461,
 0.00294353,
 0.00241201,
 0.00366590,
 0.00199860,
 0.00257838,
 0.00225860,
 0.00260646,
 0.00187256,
 0.00266552,
 0.00242641,
 0.00219450,
 0.00192082,
 0.00182071,
 0.02185930,
 0.00157439,
 0.00164353,
 0.00161401,
 0.00187544,
 0.00186248,
 0.03338637,
 0.00186968,
 0.00172132,
 0.00148509,
 0.00177749,
 0.00144620,
 0.00192442,
 0.00169683,
 0.00209439,
 0.00209439,
 0.00259062,
 0.00194531,
 0.00182359,
 0.00159096,
 0.00145196,
 0.00128199,
 0.00158376,
 0.00171412,
 0.00243433,
 0.00345704,
 0.00156359,
 0.00145700,
 0.00157007,
 0.00232342,
 0.00154198,
 0.00140730,
 0.00288807,
 0.00152830,
 0.00151246,
 0.00250203,
 0.00224420,
 0.00161761,
 0.00714383,
 0.08188576,
 0.00802537,
 0.00119484,
 0.00123805,
 0.05632671,
 0.00305156,
 0.00105584,
 0.00105368,
 0.00099246,
 0.00090459,
 0.00109473,
 0.00115379,
 0.00261223,
 0.00105656,
 0.00124381,
 0.00100326,
 0.00127550,
 0.00089739,
 0.00162481,
 0.00100830,
 0.00097229,
 0.00078864,
 0.00107240,
 0.00084409,
 0.00265760,
 0.00116891,
 0.00073102,
 0.00075695,
 0.00093916,
 0.00106880,
 0.00086786,
 0.00185600,
 0.00608367,
 0.00133600,
 0.00075695,
 0.00122077,
 0.00566955,
 0.00108249,
 0.00259638,
 0.00077063,
 0.00166586,
 0.00090387,
 0.00087074,
 0.00084914,
 0.00130935,
 0.00162409,
 0.00085922,
 0.00093340,
 0.00093844,
 0.00087722,
 0.00108249,
 0.00098598,
 0.00095933,
 0.00427593,
 0.00496661,
 0.00102775,
 0.00159312,
 0.00118404,
 0.00114947,
 0.00104936,
 0.00154342,
 0.00140082,
 0.00115883,
 0.00110769,
 0.00161112,
 0.00169107,
 0.00107816,
 0.00142747,
 0.00279804,
 0.00085922,
 0.00116315,
 0.00119484,
 0.00128559,
 0.00146204,
 0.00130215,
 0.00101551,
 0.00091756,
 0.00161184,
 0.00236375,
 0.00131872,
 0.00214120,
 0.00088875,
 0.00138570,
 0.00211960,
 0.00094060,
 0.00088083,
 0.00094564,
 0.00090243,
 0.00106160,
 0.00088659,
 0.00114514,
	0.00095861,
	0.00108753,
	0.00124165,
	0.00427016,
	0.00159384,
	0.00170547,
	0.00104431,
	0.00091395,
	0.00095789,
	0.00134681,
	0.00095213,
	0.00105944,
	0.00094132,
	0.00141883,
	0.00102127,
	0.00101911,
	0.00082105,
	0.00158448,
	0.00102631,
	0.00087938,
	0.00139290,
	0.00114658,
	0.00095501,
	0.00161329,
	0.00126542,
	0.00113218,
	0.00123661,
	0.00101695,
	0.00112930,
	0.00317976,
	0.00085346,
	0.00101190,
	0.00189849,
	0.00105728,
	0.00186824,
	0.00092908,
	0.00160896,
};

//=============================================================================
//
//	HUFFMAN_Construct
//
//	Builds the Huffman tree.
//
//=============================================================================

void HUFFMAN_Construct( void )
{
#ifdef _DEBUG
	huffman_ZeroFreq();
#endif

	huffman_BuildTree(HuffFreq);

	// Free the table when the program closes.
	atexit( HUFFMAN_Destruct );
}

//=============================================================================
//
//	HUFFMAN_Destruct
//
//	Frees memory allocated by the Huffman tree.
//
//=============================================================================

void HUFFMAN_Destruct( void )
{
	huffman_RecursiveFreeNode( HuffTree );

	free( HuffTree );
	HuffTree = NULL;
}

//=============================================================================
//
//	HUFFMAN_Encode
//
//=============================================================================

void HUFFMAN_Encode( unsigned char *in, unsigned char *out, int inlen, int *outlen )
{
	int i,j,bitat;
	unsigned int t;
	bitat=0;
	for (i=0;i<inlen;i++)
	{
		t=HuffLookup[in[i]].bits;
		for (j=0;j<HuffLookup[in[i]].len;j++)
		{
			huffman_PutBit(out+1,bitat+HuffLookup[in[i]].len-j-1,t&1);
			t>>=1;
		}
		bitat+=HuffLookup[in[i]].len;
	}
	*outlen=1+(bitat+7)/8;
	*out=8*((*outlen)-1)-bitat;
	if(*outlen >= inlen+1)
	{
		*out=0xff;
		memcpy(out+1,in,inlen);
		*outlen=inlen+1;
	}
/*#if _DEBUG

	HuffIn+=inlen;
	HuffOut+=*outlen;

	{
		unsigned char *buf;
		int tlen;
		
		buf= (unsigned char *)Malloc(inlen);
		
		HuffDecode(out,buf,*outlen,&tlen);
		if(!tlen==inlen)
			I_FatalError("bogus compression");
		for (i=0;i<inlen;i++)
		{
			if(in[i]!=buf[i])
				I_FatalError("bogus compression");
		}
		free(buf);
	}
#endif*/
}

//=============================================================================
//
//	HUFFMAN_Decode
//
//=============================================================================

void HUFFMAN_Decode( unsigned char *in, unsigned char *out, int inlen, int *outlen )
{
	int bits,tbits;
	huffnode_t *tmp;	
	if (*in==0xff)
	{
		memcpy(out,in+1,inlen-1);
		*outlen=inlen-1;
		return;
	}
	tbits=(inlen-1)*8-*in;
	bits=0;
	*outlen=0;
	while (bits<tbits)
	{
		tmp=HuffTree;
		do
		{
			if (huffman_GetBit(in+1,bits))
				tmp=tmp->one;
			else
				tmp=tmp->zero;
			bits++;
		} while (tmp->zero);
		*out++=tmp->val;
		(*outlen)++;
	}
}

#ifdef _DEBUG
static int freqs[256];
void huffman_ZeroFreq(void)
{
	memset(freqs, 0, 256*sizeof(int));
}


void CalcFreq(unsigned char *packet, int packetlen)
{
	int ix;

	for (ix=0; ix<packetlen; ix++)
	{
		freqs[packet[ix]]++;
	}
}

void PrintFreqs(void)
{
	int ix;
	float total=0;
	char string[100];

	for (ix=0; ix<256; ix++)
	{
		total += freqs[ix];
	}
	if (total>.01)
	{
		for (ix=0; ix<256; ix++)
		{
			sprintf(string, "\t%.8f,\n",((float)freqs[ix])/total);
			//OutputDebugString(string);
//			Printf(PRINT_HIGH, string);
		}
	}
	huffman_ZeroFreq();
}
#endif


void FindTab(huffnode_t *tmp,int len,unsigned int bits)
{
//	if(!tmp)
//		I_FatalError("no huff node");
	if (tmp->zero)
	{
//		if(!tmp->one)
//			I_FatalError("no one in node");
//		if(len>=32)
//			I_FatalError("compression screwd");
		FindTab(tmp->zero,len+1,bits<<1);
		FindTab(tmp->one,len+1,(bits<<1)|1);
		return;
	}
	HuffLookup[tmp->val].len=len;
	HuffLookup[tmp->val].bits=bits;
	return;
}

//=============================================================================
//
//	huffman_PutBit
//
//=============================================================================

void huffman_PutBit(unsigned char *buf,int pos,int bit)
{
	if (bit)
		buf[pos/8]|=Masks[pos%8];
	else
		buf[pos/8]&=~Masks[pos%8];
}

//=============================================================================
//
//	huffman_GetBit
//
//=============================================================================

int huffman_GetBit(unsigned char *buf,int pos)
{
	if (buf[pos/8]&Masks[pos%8])
		return 1;
	else
		return 0;
}

//=============================================================================
//
//	huffman_BuildTree
//
//=============================================================================

void huffman_BuildTree( float *freq )
{
	float min1,min2;
	int i,j,minat1,minat2;
	huffnode_t *work[256];	
	huffnode_t *tmp;	
	for (i=0;i<256;i++)
	{
		work[i]=(huffnode_t *)M_Malloc(sizeof(huffnode_t));
		
		
		work[i]->val=(unsigned char)i;
		work[i]->freq=freq[i];
		work[i]->zero=0;
		work[i]->one=0;
		HuffLookup[i].len=0;
	}
	for (i=0;i<255;i++)
	{
		minat1=-1;
		minat2=-1;
		min1=1E30;
		min2=1E30;
		for (j=0;j<256;j++)
		{
			if (!work[j])
				continue;
			if (work[j]->freq<min1)
			{
				minat2=minat1;
				min2=min1;
				minat1=j;
				min1=work[j]->freq;
			}
			else if (work[j]->freq<min2)
			{
				minat2=j;
				min2=work[j]->freq;
			}
		}
/*
		if (minat1<0)
			I_FatalError("minatl: %f",minat1);
		if (minat2<0)
			I_FatalError("minat2: %f",minat2);
*/		
		tmp= (huffnode_t *)M_Malloc(sizeof(huffnode_t));
		
		
		tmp->zero=work[minat2];
		tmp->one=work[minat1];
		tmp->freq=work[minat2]->freq+work[minat1]->freq;
		tmp->val=0xff;
		work[minat1]=tmp;
		work[minat2]=0;
	}		
	HuffTree=tmp;
	FindTab(HuffTree,0,0);

#if _DEBUG
	for (i=0;i<256;i++)
	{
//		if(!HuffLookup[i].len&&HuffLookup[i].len<=32)
//			I_FatalError("bad frequency table");
		//Printf(PRINT_HIGH, "%d %d %2X\n", HuffLookup[i].len, HuffLookup[i].bits, i);
	}
#endif
}

//=============================================================================
//
//	huffman_RecursiveFreeNode
//
//=============================================================================

static void huffman_RecursiveFreeNode( huffnode_t *pNode )
{
	if ( pNode->one )
	{
		huffman_RecursiveFreeNode( pNode->one );

		free( pNode->one );
		pNode->one = NULL;
	}

	if ( pNode->zero )
	{
		huffman_RecursiveFreeNode( pNode->zero );

		free( pNode->zero );
		pNode->zero = NULL;
	}
}

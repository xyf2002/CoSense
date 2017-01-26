#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <stdint.h>
#include "flextypes.h"
#include "flexerror.h"
#include "flex.h"
#include "noisy-errors.h"
#include "version.h"
#include "noisy-timeStamps.h"
#include "noisy.h"
#include "newton-parser.h"
#include "noisy-parser.h"
#include "newton-lexer.h"


extern const char *	gNewtonTokenDescriptions[];


static inline void	checkTokenLength(NoisyState *  N, int  count);
static inline char	cur(NoisyState *  N);
static bool     eqf(NoisyState *  N);
static void		gobble(NoisyState *  N, int count);
static void		done(NoisyState *  N, NoisyToken *  newToken);
static void		checkComment(NoisyState *  N);
static void		checkSingle(NoisyState *  N, NoisyIrNodeType tokenType);
static void		checkDoubleQuote(NoisyState *  N);

static void		finishToken(NoisyState *  N);

static void		checkGt(NoisyState *  N);
static void		checkLt(NoisyState *  N);
static void     checkMul(NoisyState * N);
static bool checkProportionality(NoisyState * N);
static void checkDot(NoisyState *  N);

static void		makeNumericConst(NoisyState *  N);
static bool		isDecimal(NoisyState *  N, char *  string);
static char *		stringAtLeft(NoisyState *  N, char *  string, char  character);
static char *		stringAtRight(NoisyState *  N, char *  string, char  character);
static bool		isDecimalSeparatedWithChar(NoisyState *  N, char *  string, char  character);
static bool		isDecimalOrRealSeparatedWithChar(NoisyState *  N, char *  string, char  character);
static bool		isRadixConst(NoisyState *  N, char *  string);
static bool		isRealConst(NoisyState *  N, char *  string);
static bool		isEngineeringRealConst(NoisyState *  N, char *  string);
static uint64_t		stringToRadixConst(NoisyState *  N, char *  string);
static double		stringToRealConst(NoisyState *  N, char *  string);
static double		stringToEngineeringRealConst(NoisyState *  N, char *  string);

static bool		isOperatorOrSeparator(NoisyState *  N, char c);




NoisySourceInfo *
newtonLexAllocateSourceInfo(	NoisyState *  N, char **  genealogy, char *  fileName,
				uint64_t lineNumber, uint64_t columnNumber, uint64_t length)
{
	NoisySourceInfo *	newSourceInfo;

	newSourceInfo = (NoisySourceInfo *) calloc(1, sizeof(NoisySourceInfo));
	if (newSourceInfo == NULL)
	{
		noisyFatal(N, Emalloc);
	}

	newSourceInfo->genealogy	= genealogy;
	newSourceInfo->fileName		= (fileName == NULL ? NULL : strdup(fileName));
	newSourceInfo->lineNumber	= lineNumber;
	newSourceInfo->columnNumber	= columnNumber;
	newSourceInfo->length		= length;

	return newSourceInfo;
}


NoisyToken *
newtonLexAllocateToken(	
    NoisyState *  N, 
    NoisyIrNodeType type, 
    char *  identifier,
    uint64_t integerConst,
    double realConst, 
    char * stringConst,
	NoisySourceInfo *  sourceInfo
) {
	NoisyToken *	newToken;

	newToken = (NoisyToken *) calloc(1, sizeof(NoisyToken));
	if (newToken == NULL)
	{
		noisyFatal(N, Emalloc);
	}
	
	newToken->type		= type;
	newToken->identifier	= (identifier == NULL ? NULL : strdup(identifier));
	newToken->realConst	= realConst;
	newToken->integerConst= integerConst;
	newToken->stringConst	= (stringConst == NULL ? NULL : strdup(stringConst));
	newToken->sourceInfo	= sourceInfo;

	return newToken;
}



void
newtonLexPut(NoisyState *  N, NoisyToken *  newToken)
{
	if (newToken == NULL)
	{
		noisyFatal(N, Esanity);
	}

	/*
	 *	If the token list is empty, we allocate and put on an EOF node, which we never remove.
	 */	
	if (N->tokenList == NULL)
	{
		N->lastToken = N->tokenList = newToken;
	}
	else
	{
		newToken->prev = N->lastToken;
		N->lastToken->next = newToken;
		N->lastToken = newToken;
	}
}


NoisyToken *
newtonLexGet(NoisyState *  N)
{
	if (N->tokenList == NULL)
	{
		noisyFatal(N, Esanity);
	}

	NoisyToken *	t = N->tokenList;
	
	if (t->next != NULL)
	{
		N->tokenList = N->tokenList->next;
	}
	else if (t->type != kNewtonIrNodeType_Zeof)
	{
		noisyFatal(N, Esanity);
	}

	if (N->verbosityLevel & kNoisyVerbosityDebugLexer)
	{
		newtonLexDebugPrintToken(N, t);	
	}

	return t;
}


NoisyToken *
newtonLexPeek(NoisyState *  N, int lookAhead)
{
	if (N->tokenList == NULL)
	{
		noisyFatal(N, Esanity);
	}

	NoisyToken *	tmp = N->tokenList;
	int 		which = 1;
	while ((tmp != NULL) && (which++ < lookAhead))
	{
		tmp = tmp->next;
	}

	return tmp;
}

void
newtonLexPeekPrint(NoisyState *  N, int maxTokens, int formatCharacters)
{
	if (N->tokenList == NULL)
	{
		noisyFatal(N, Esanity);
	}

	int		tripCharacters = 0, done = 0;
	NoisyToken *	tmp = N->tokenList;

	flexprint(N->Fe, N->Fm, N->Fperr, "\t\tline %5d, token %3d\t", tmp->sourceInfo->lineNumber, tmp->sourceInfo->columnNumber);
	while (tmp != NULL)
	{
		if (maxTokens > 0 && (done++ > maxTokens))
		{
			flexprint(N->Fe, N->Fm, N->Fperr, "...");
			break;
		}

		{
			/*
			 *	NOTE: We currently don't keep newlines and whitespace on the token list...
			 */
			switch (tmp->type)
			{
                // TODO filter cases
				case kNewtonIrNodeType_Tidentifier:
				{
					flexprint(N->Fe, N->Fm, N->Fperr, "'%s' (identifier) ", tmp->identifier);

					/*
					 *	Account for the output string and the
					 *	two guarding "'" quotes.
					 */
					tripCharacters += strlen(tmp->identifier) + 14;

					break;
				}

				case kNewtonIrNodeType_TstringConst:
				{
					flexprint(N->Fe, N->Fm, N->Fperr, "'\"%s\"' ", tmp->stringConst);

					/*
					 *	Account for the output string and the
					 *	two guarding "'" quotes.
					 */
					tripCharacters += strlen(tmp->stringConst) + 2;

					break;
				}
				
				default:
				{
					if (gNewtonTokenDescriptions[tmp->type] != NULL)
					{
						flexprint(N->Fe, N->Fm, N->Fperr, "%s ", gNewtonTokenDescriptions[tmp->type]);

						/*
						 *	Account for the trailing space
						 */
						tripCharacters += strlen(gNewtonTokenDescriptions[tmp->type]) + 1;

					}
					else
					{
						flexprint(N->Fe, N->Fm, N->Fperr, ">>>BUG/TODO: un-handled type %s in newtonLexPeekPrint <<<", gNewtonTokenDescriptions[tmp->type]);
						noisyFatal(N, Esanity);
					}
				}
			}

			if ((tmp->next != NULL) && (tmp->sourceInfo->lineNumber != tmp->next->sourceInfo->lineNumber))
			{
				//flexprint(N->Fe, N->Fm, N->Fperr, "(newlines)");
				tripCharacters = 0;

				flexprint(N->Fe, N->Fm, N->Fperr, "\n\t\tline %5d\t\t", tmp->next->sourceInfo->lineNumber);
			}
			else if (tripCharacters >= formatCharacters)
			{
				tripCharacters = 0;
				flexprint(N->Fe, N->Fm, N->Fperr, "\n\t\t\t\t\t");
			}
		}

		tmp = tmp->next;
	}
	flexprint(N->Fe, N->Fm, N->Fperr, "\n");
}


void
newtonLexInit(NoisyState *  N, char *  fileName)
{
	FILE *			filePointer;
	size_t			lineBufferSize;


	N->fileName 		= fileName;
	N->columnNumber		= 1;
	N->lineNumber		= 1;
	N->lineLength		= 0;


	filePointer = fopen(fileName, "r");
	if (filePointer == NULL)
	{
		noisyFatal(N, Eopen);
	}


	/*
	 *	Notes:
	 *
	 *	(1)	The way we handle lexing in M and Noisy compilers does not use the
	 *		'stickies' as we do in our Yacc-based parsers.
	 *
	 *	(2)	We currently split up the input by '\n'-separated newline. This is
	 *		OK, since we also recognize '\r' as being a discardable whitespace.
	 *
	 *	(3)	In the CGI case, we dump the program to a temporary file (we do this
	 *		anyway to keep copies of inputs), and feed that file to the compiler.
	 */
	
	/*
	 *	The following two are needed in order for getline() to allocate the buffer itself
	 */
	N->lineBuffer = NULL;
	lineBufferSize = 0;

	while ((N->lineLength = getline(&(N->lineBuffer), &lineBufferSize, filePointer)) != -1)
	{
		N->columnNumber = 0;
		while (N->columnNumber < N->lineLength)
		{

	        flexprint(N->Fe, N->Fm, N->Fperr, "%c", cur(N));
			if (isOperatorOrSeparator(N, cur(N)))
			{
				switch (cur(N))
				{
					/*
					 *	These tokens may be paired with an equals sign or with another char (e.g., "::"),
					 *	but otherwise do not require additional special handling as in the case of ".".
					 *
					 *	We process the chars see so far as a finished token, then handle the following chars.
					 */
                  // I DONT HAVE ANY OF THESE


					/*
					 *	These tokens only occur alone.
					 *
					 *	We process the chars see so far as a finished token, then handle the following chars.
					 */
					case '~':
					{
						checkSingle(N, kNewtonIrNodeType_Tequivalent);
						continue;
					}
					case '=':
					{
						checkSingle(N, kNewtonIrNodeType_Tequals);
						continue;
					}
					case '(':
					{
						checkSingle(N, kNewtonIrNodeType_TleftParen);
						continue;
					}

					case ')':
					{
						checkSingle(N, kNewtonIrNodeType_TrightParen);
						continue;
					}
					
                    case '{':
					{
						checkSingle(N, kNewtonIrNodeType_TleftBrace);
						continue;
					}

					case '}':
					{
						checkSingle(N, kNewtonIrNodeType_TrightBrace);
						continue;
					}
					

                    case '+':
					{
						checkSingle(N, kNewtonIrNodeType_Tplus);
						continue;
					}

					case '-':
					{
						checkSingle(N, kNewtonIrNodeType_Tminus);
						continue;
					}
                    case '*':
					{
                        checkMul(N);
						continue;
					}
                    
                    case '/':
					{
						checkSingle(N, kNewtonIrNodeType_Tdiv);
						continue;
					}
					
                    case ',':
					{
						checkSingle(N, kNewtonIrNodeType_Tcomma);
						continue;
					}
                    
                    case ';':
					{
						checkSingle(N, kNewtonIrNodeType_Tsemicolon);
						continue;
					}
                    
                    case ':':
					{
						checkSingle(N, kNewtonIrNodeType_Tcolon);
						continue;
					}

                    case '.':
                    {
                        checkDot(N);
                        continue;
                    }
					

					/*
					 *	These tokens require special handling beyond being paired with an equals,
					 *	being part of a number, or doubled-up (e.g., ">>", etc.).
					 */
                    case '@':
                    {
                        checkProportionality(N);
                        continue;
                        // if (checkProportionality(N))
                        //     continue;
                        // break;
                    }
                    case '>':
					{
						checkGt(N);
						continue;
					}
					case '<':
					{
						checkLt(N);
						continue;
					}
					case '"':
					{
						checkDoubleQuote(N);
						continue;
					}

					case '#':
					{
						checkComment(N);
						
						/*
						 *	Set columnNumber to lineLength so we stop chomping on this line
						 */
						N->columnNumber = N->lineLength;
						continue;
					}

					/*
					 *	Whitespace.
					 */
					case ' ' :
					case '\n':
					case '\r':
					case '\t':
					{
						N->columnNumber++; finishToken(N);
						continue;
					}

					default:
					{
                        noisyConsolePrintBuffers(N);
						noisyFatal(N, Esanity);
					}
				}
			}
			
			checkTokenLength(N, 1);
			N->currentToken[N->currentTokenLength++] = N->lineBuffer[N->columnNumber++];
		}
		N->lineNumber++;
	
		/*
		 *	In order for getline() to allocate the buffer itself on the next iteration...
		 */
		N->lineBuffer = NULL;
		lineBufferSize = 0;
	}

	fclose(filePointer);

	NoisySourceInfo *	eofSourceInfo = newtonLexAllocateSourceInfo(N,	NULL /* genealogy */,
										N->fileName /* fileName */,
										N->lineNumber /* lineNumber */,
										N->columnNumber /* columnNumber */,
										0 /* length */);
	  								
	 NoisyToken *		eofToken = newtonLexAllocateToken(N,	kNewtonIrNodeType_Zeof /* type */,
	 								NULL /* identifier */,
	 								0 /* integerConst */,
	 								0.0 /* realConst */,
	 								NULL /* stringConst */,
	 								eofSourceInfo /* sourceInfo */);
	 newtonLexPut(N, eofToken);

	if (N->verbosityLevel & kNoisyVerbosityDebugLexer)
	{
		flexprint(N->Fe, N->Fm, N->Fperr, "Done lexing...\n");
		
		flexprint(N->Fe, N->Fm, N->Fperr, "\n\n");
		NoisyToken *	p = N->tokenList;
		while (p != NULL)
		{
			newtonLexDebugPrintToken(N, p);
			p = p->next;
		}
		flexprint(N->Fe, N->Fm, N->Fperr, "\n\n");
	}


	return;
}


void
newtonLexDebugPrintToken(NoisyState *  N, NoisyToken *  t)
{
	switch (t->type)
	{
        // TODO handle all the cases
		case kNewtonIrNodeType_Tidentifier:
		{
			flexprint(N->Fe, N->Fm, N->Fperr, "\"%20s\", ", t->identifier);
			break;
		}

		case kNewtonIrNodeType_TstringConst:
		{
			flexprint(N->Fe, N->Fm, N->Fperr, "\"%20s\", ", t->stringConst);
			break;
		}

		default:
		{
			if (gNewtonTokenDescriptions[t->type] != NULL)
			{
				flexprint(N->Fe, N->Fm, N->Fperr, "%22s, ", gNewtonTokenDescriptions[t->type]);
			}
			else
			{
				flexprint(N->Fe, N->Fm, N->Fperr, ">>>BUG: unhandled type [%d] in newtonLexDebugPrintToken <<<", t->type);
				noisyFatal(N, Esanity);
			}
		}
	}

	flexprint(N->Fe, N->Fm, N->Fperr, "source file: %16s, line %3d, pos %3d, length %3d\n",
		t->sourceInfo->fileName, t->sourceInfo->lineNumber, t->sourceInfo->columnNumber, t->sourceInfo->length);
}


void
newtonLexPrintToken(NoisyState *  N, NoisyToken *  t)
{
	switch (t->type)
	{
		case kNewtonIrNodeType_Tidentifier:
		{
			flexprint(N->Fe, N->Fm, N->Fperr, "\"%s\"", t->identifier);
			break;
		}

		case kNewtonIrNodeType_TstringConst:
		{
			flexprint(N->Fe, N->Fm, N->Fperr, "\"%s\"", t->stringConst);
			break;
		}

		default:
		{
			if (gNewtonTokenDescriptions[t->type] != NULL)
			{
				flexprint(N->Fe, N->Fm, N->Fperr, "%s", gNewtonTokenDescriptions[t->type]);
			}
			else
			{
				flexprint(N->Fe, N->Fm, N->Fperr, ">>>BUG: unhandled iii type [%d] in noisyLexPrintToken <<<", t->type);
				//noisyFatal(N, Esanity);
			}
		}
	}
}



/*
 *	Local non-exposed routines.
 */



static inline void
checkTokenLength(NoisyState *  N, int  count)
{
	if (N->currentTokenLength+count >= kNoisyMaxBufferLength)
	{
		noisyFatal(N, EtokenTooLong);
	}
}

static inline char
cur(NoisyState *  N)
{
	return N->lineBuffer[N->columnNumber];
}


static void
gobble(NoisyState *  N, int count)
{
	checkTokenLength(N, count);
	strncpy(N->currentToken, &N->lineBuffer[N->columnNumber], count);

	N->columnNumber += count;
}


static void
done(NoisyState *  N, NoisyToken *  newToken)
{
	newToken->sourceInfo = newtonLexAllocateSourceInfo(N,	NULL				/*   genealogy 	*/,
								N->fileName			/*   fileName 	*/,
								N->lineNumber			/*   lineNumber */,
								N->columnNumber - N->currentTokenLength /* columnNumber */,
								N->currentTokenLength		/*   length 	*/);

	bzero(N->currentToken, kNoisyMaxBufferLength);
	N->currentTokenLength = 0;
	newtonLexPut(N, newToken);
}


static void
checkComment(NoisyState *  N)
{
	/*
	 *	Gobble any extant chars
	 */
	finishToken(N);

	/*
	 *	NOTE: currentToken is allocated only once, but lineBuffer is allocated each 
	 *	time a line is read via getline().
	 */
	free(N->lineBuffer);
}


static void
checkSingle(NoisyState *  N, NoisyIrNodeType tokenType)
{
	/*
	 *	Gobble any extant chars.
	 */
	finishToken(N);

	gobble(N, 1);

	if (N->verbosityLevel & kNoisyVerbosityDebugLexer)
	{
		//flexprint(N->Fe, N->Fm, N->Fperr, "checkSingle(), tokenType = %d\n", tokenType);
//fprintf(stderr, "checkSingle(), tokenType = %d\n", tokenType);
	}

	NoisyToken *		newToken = newtonLexAllocateToken(N,	tokenType /* type	*/,
									NULL	/* identifier	*/,
									0	/* integerConst	*/,
									0.0	/* realConst	*/,
									NULL	/* stringConst	*/,
									NULL	/* sourceInfo	*/);

	/*
	 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
	 */
	done(N, newToken);
}


static void
checkDoubleQuote(NoisyState *  N)
{
	/*
	 *	TODO/BUG: we do not handle escaped dquotes in a strconst
	 */
	NoisyToken *	newToken;


	/*
	 *	Gobble any extant chars.
	 */
	finishToken(N);

	/*
	 *	String constants cannot contain an un-escaped newline;  the current
	 *	N->lineBuffer must contain the closing quote, else we flag this as a
	 *	bad string constant (kNewtonIrNodeType_ZbadStringConst)
	 */
	if (strchr(&N->lineBuffer[N->columnNumber+1], '"') == NULL)
	{
		newToken = newtonLexAllocateToken(N,	kNewtonIrNodeType_ZbadStringConst	/* type		*/,
							NULL					/* identifier	*/,
							0					/* integerConst	*/,
							0.0					/* realConst	*/,
							&N->lineBuffer[N->columnNumber]		/* stringConst	*/,
							NULL					/* sourceInfo	*/);
	}
	else
	{
		N->columnNumber++;

		/*
		 *	NOTE: N->currentToken is pre-allocated to be kNoisyMaxBufferLength characters long.
		 */
		while ((cur(N) != '"') && N->currentTokenLength < (kNoisyMaxBufferLength - 1) && (N->columnNumber < N->lineLength))
		{
			checkTokenLength(N, 1);
			N->currentToken[N->currentTokenLength++] = N->lineBuffer[N->columnNumber++];
		}

		checkTokenLength(N, 1);
		N->currentToken[N->currentTokenLength++] = '\0';

		if (cur(N) != '"')
		{
			/*
			 *	We ran out of buffer space or reached end of lineBuffer
			 */
			noisyFatal(N, EstringTooLongOrWithNewline);
		}
		else
		{
			/*
			 *	Move past the closing quote
			 */
			N->columnNumber++;
		}

		newToken = newtonLexAllocateToken(N,	kNewtonIrNodeType_TstringConst	/* type		*/,
							NULL				/* identifier	*/,
							0				/* integerConst	*/,
							0.0				/* realConst	*/,
							N->currentToken			/* stringConst	*/,
							NULL				/* sourceInfo	*/);
	}

	/*
	 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
	 */
	done(N, newToken);
}


// static void
// checkMinus(NoisyState *  N)
// {
// 	/*
// 	 *	Gobble any extant chars.
// 	 */
// 	finishToken(N);
// 
//     gobble(N, 1);
//     NoisyIrNodeType type = kNewtonIrNodeType_Tminus;
// 
// 	NoisyToken *		newToken = newtonLexAllocateToken(N,	type	/* type		*/,
// 									NULL	/* identifier	*/,
// 									0	/* integerConst	*/,
// 									0.0	/* realConst	*/,
// 									NULL	/* stringConst	*/,
// 									NULL	/* sourceInfo	*/);
// 
// 	/*
// 	 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
// 	 */
// 	done(N, newToken);
// }

static void
finishToken(NoisyState *  N)
{
	if (N->verbosityLevel & kNoisyVerbosityDebugLexer)
	{
		//flexprint(N->Fe, N->Fm, N->Fperr, "in finishToken(), N->currentToken = [%s]\n", N->currentToken);
fprintf(stderr, "in finishToken(), N->currentToken = [%s]\n", N->currentToken);
	}

	/*
	 *	Called when we've eaten (zero or more) chars which are
	 *	not operators or separators.  Whitespace isn't actually
	 *	added to N->currentToken, but rather we get called here.
	 */
	if (N->currentTokenLength == 0)
	{
		return;
	}

	for (int i = 0; i < kNoisyIrNodeTypeMax; i++)
	{
		if ((gNewtonTokenDescriptions[i] != NULL) && !strcmp(gNewtonTokenDescriptions[i], N->currentToken))
		{
			NoisyToken *	newToken = newtonLexAllocateToken(N,	i	/* type		*/,
										NULL	/* identifier	*/,
										0	/* integerConst	*/,
										0.0	/* realConst	*/,
										NULL	/* stringConst	*/,
										NULL	/* sourceInfo	*/);

			/*
			 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
			 */
			done(N, newToken);

			return;
		}
	}

	if (N->currentToken[0] >= '0' && N->currentToken[0] <= '9')
	{
		makeNumericConst(N);
		return;
	}

	/*
	 *	Otherwise, it's an identifier.  It will not have illegal chars in it
	 *	since we would have halted the building of the token on seing them
	 *	and gotten called here.
	 */
	NoisyToken *	newToken = newtonLexAllocateToken(N,	kNewtonIrNodeType_Tidentifier	/* type		*/,
								N->currentToken			/* identifier	*/,
								0	/* integerConst	*/,
								0.0	/* realConst	*/,
								NULL	/* stringConst	*/,
								NULL	/* sourceInfo	*/);

	/*
	 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
	 */
	done(N, newToken);
}

static void
makeNumericConst(NoisyState *  N)
{
	if (N->currentTokenLength == 0)
	{
		noisyFatal(N, EruntTokenInNumericConst);
	}

	/*
	 *	If it begins with a onenine or a zeronine (see regular grammar),
	 *	check if it's an intconst or realconst tokens which begin with 
	 *	numbers but do not make valid consts, make a "illegal identifier"
	 *	token kNoisyIrNodeType_ZbadIdentifier
	 */

	/*
	 *	There are only two valid cases with leading zero: "0" and "0.XXX".
	 *	You cannot have 0XXX or even 0rXXX
	 */
	if (N->currentToken[0] == '0')
	{
		if (N->currentTokenLength == 1)
		{
			NoisyToken *	newToken = newtonLexAllocateToken(N,	kNewtonIrNodeType_Tnumber	/* type		*/,
										NULL	/* identifier	*/,
										0	/* integerConst	*/,
										0.0	/* realConst	*/,
										NULL	/* stringConst	*/,
										NULL	/* sourceInfo	*/);

			/*
			 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
			 */
			done(N, newToken);

			return;
		}
		else if (N->currentToken[1] != '.')
		{
			NoisyToken *	newToken = newtonLexAllocateToken(N,	kNewtonIrNodeType_ZbadIdentifier	/* type		*/,
										N->currentToken	/* identifier	*/,
										0	/* integerConst	*/,
										0.0	/* realConst	*/,
										NULL	/* stringConst	*/,
										NULL	/* sourceInfo	*/);
			

			/*
			 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
			 */
			done(N, newToken);

			return;
		}
	}

	/*
	 *	First (before checking if it has a '.'), check if it has the form XXX 'e|E' YYY, XXX can be decimal or real, YYY must be decimal.
	 *
	 *	(TODO: this implies that 'e' and 'E' bind tighter than '.'. We should record this and discuss in the language manual.)
	 */
	if (isEngineeringRealConst(N, N->currentToken))
	{
		NoisyToken *	newToken = newtonLexAllocateToken(N,	kNewtonIrNodeType_Tnumber /* type		*/,
									NULL				/* identifier	*/,
									0				/* integerConst	*/,
									stringToEngineeringRealConst(N, N->currentToken) /* realConst	*/,
									NULL				/* stringConst	*/,
									NULL				/* sourceInfo	*/);

		/*
		 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
		 */
		done(N, newToken);

		return;
	}

	/*
	 *	Otherwise, check if it has the form XXX '.' YYY, XXX and YYY must be decimals.
	 */
	if (isRealConst(N, N->currentToken))
	{
		NoisyToken *	newToken = newtonLexAllocateToken(N,	kNewtonIrNodeType_Tnumber /* type		*/,
									NULL				/* identifier	*/,
									0				/* integerConst	*/,
									stringToRealConst(N, N->currentToken)	/* realConst	*/,
									NULL				/* stringConst	*/,
									NULL				/* sourceInfo	*/);

		/*
		 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
		 */
		done(N, newToken);

		return;
	}

	/*
	 *	Has the form XXX 'r' YYY, XXX must be decimal, YYY must be in base XXX.
	 */
	if (isRadixConst(N, N->currentToken))
	{
		NoisyToken *	newToken = newtonLexAllocateToken(N,	kNewtonIrNodeType_Tnumber/* type		*/,
									NULL				/* identifier	*/,
									stringToRadixConst(N, N->currentToken)	/* integerConst	*/,
									0				/* realConst	*/,
									NULL				/* stringConst	*/,
									NULL				/* sourceInfo	*/);

		/*
		 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
		 */
		done(N, newToken);

		return;
	}

	/*
	 *	At this point, if it is a non-decimal, it must be illegal.
	 */
	if (!isDecimal(N, N->currentToken))
	{
		NoisyToken *	newToken = newtonLexAllocateToken(N,	kNewtonIrNodeType_ZbadIdentifier	/* type		*/,
									N->currentToken			/* identifier	*/,
									0				/* integerConst	*/,
									0				/* realConst	*/,
									NULL				/* stringConst	*/,
									NULL				/* sourceInfo	*/);

		/*
		 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
		 */
		done(N, newToken);

		return;
	}

	/*
	 *	It's a decimal.
	 */
	char		tmp;
	char *		ep = &tmp;
	uint64_t 	decimalValue = strtoul(N->currentToken, &ep, 0);
	if (*ep == '\0')
	{
		NoisyToken *	newToken = newtonLexAllocateToken(N,	kNewtonIrNodeType_Tnumber/* type		*/,
									NULL				/* identifier	*/,
									decimalValue			/* integerConst	*/,
									0				/* realConst	*/,
									NULL				/* stringConst	*/,
									NULL				/* sourceInfo	*/);

		/*
		 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
		 */
		done(N, newToken);
	}
	else
	{
		noisyFatal(N, Esanity);
	}
}


static bool
isDecimal(NoisyState *  N, char *  string)
{
	if (string == NULL)
	{
		return false;
	}

	size_t	stringLength = strlen(string);
	for (int i = 0; i < stringLength; i++)
	{
		if (string[i] < '0' || string[i] > '9')
		{
			return false;
		}
	}

	return (string != NULL);
}



static char *
stringAtLeft(NoisyState *  N, char *  string, char character)
{
	if (string == NULL)
	{
		return string;
	}

	/*
	 *	NOTE: stringAtLeft (but not stringAtRight) makes
	 *	a copy, because it will modify 'string' by inserting
	 *	a '\0' at the position of 'character' and this has
	 *	to be freed by caller.
	 */
	char *	left = strdup(string);
	char *	right = strchr(string, character);

	if (right == NULL)
	{
		return left;
	}

	/*
	 *	right - string. Since left is a strdup, its address could be anywhere...
	 */
	left[right - string] = '\0';

	return left;
}

static char *
stringAtRight(NoisyState *  N, char *  string, char character)
{
	if (string == NULL)
	{
		return string;
	}

	char *	right = strchr(string, character);

	if (right == NULL)
	{
		return NULL;
	}

	return &right[1];
}


static bool
isDecimalSeparatedWithChar(NoisyState *  N, char *  string, char  character)
{
	if (string == NULL)
	{
		return false;
	}

	if (!strchr(string, character))
	{
		return false;
	}

	char *	left = stringAtLeft(N, string, character);
	char *	right = stringAtRight(N, string, character);
	bool	result = isDecimal(N, left) && isDecimal(N, right);

	/*
	 *	stringAtLeft() makes a copy, which needs to be freed.
	 *	(stringAtRight on the other hand does not need to make
	 *	a copy, and doesn't).
	 */
	free(left);


	return result;
}

static bool
isDecimalOrRealSeparatedWithChar(NoisyState *  N, char *  string, char  character)
{
	if (string == NULL)
	{
		return false;
	}

	if (!strchr(string, character))
	{
		return false;
	}

	char *	left = stringAtLeft(N, string, character);
	char *	right = stringAtRight(N, string, character);
	bool	result = (isDecimal(N, left)  || isRealConst(N, left))
			&&
			(isDecimal(N, right) || isRealConst(N, right));

	/*
	 *	stringAtLeft() makes a copy, which needs to be freed.
	 *	(stringAtRight on the other hand does not need to make
	 *	a copy, and doesn't).
	 */
	free(left);


	return result;
}


static bool
isRadixConst(NoisyState *  N, char *  string)
{
	if (string == NULL || !strchr(string, 'r'))
	{
		return false;
	}

	return isDecimalSeparatedWithChar(N, string, 'r');
}


static bool
isRealConst(NoisyState *  N, char *  string)
{
	if (string == NULL || !strchr(string, '.'))
	{
		return false;
	}

	return isDecimalSeparatedWithChar(N, string, '.');
}


static bool
isEngineeringRealConst(NoisyState *  N, char *  string)
{
	if (string == NULL || (!strchr(string, 'e') && !strchr(string, 'E')))
	{
		return false;
	}

//fprintf(stderr, "isEngineeringRealConst(N, %s)...\n", string);
	return (isDecimalOrRealSeparatedWithChar(N, string, 'e') || isDecimalOrRealSeparatedWithChar(N, string, 'E'));
}


static uint64_t
stringToRadixConst(NoisyState *  N, char *  string)
{
	char		tmp;
	char *		ep = &tmp;
	char *		left;
	char *		right;
	int		rightLength;
	uint64_t	base, value, p;


	left		= stringAtLeft(N, string, 'r');
	right		= stringAtRight(N, string, 'r');
	rightLength	= strlen(right);

	base = strtoul(left, &ep, 0);
	if (*ep != '\0')
	{
		/*
		 *	BUG/TODO: We should make sure that errorRecovery uses setjmp to eject
		 *	us out of here.
		 */
		noisyFatal(N, Esanity);

		/* Not reached */
	}

	value = 0;

	for (int i = 0; i < rightLength; i++)
	{
		char	digitChar;
		char	digitValue;

		if (i == 0)
		{
			p = 1;
		}
		else
		{
			p = base;
			for (int j = 0; j < i-1; j++)
			{
				p *= base;
			}
		}

		/*
		 *	Noisy supports up to base 36 (e.g., 36rZZZ), which is the most
		 *	human friendly range. We could in principle support, e.g., base64,
		 *	but that would lead to value strings that would unecessarily
		 *	complicate the lexer and prser (e.g., "37r{{{").
		 */
		digitChar = right[rightLength - 1 - i];
		if (digitChar >= '0' && digitChar <= '9')
		{
			digitValue = digitChar - '0';
		}
		else if (digitChar >= 'A' && digitChar <= 'Z')
		{
			digitValue = digitChar - 'A' + 10;
		}
		else if (digitChar >= 'a' && digitChar <= 'z')
		{
			digitValue = digitChar - 'a' + 10;
		}
		else
		{
			noisyFatal(N, Esanity);
		}

		value += p * digitValue;
	}

	/*
	 *	stringAtLeft() makes a copy, which needs to be freed.
	 *	(stringAtRight on the other hand does not need to make
	 *	a copy, and doesn't).
	 */
	free(left);


	return value;
}


static double
stringToRealConst(NoisyState *  N, char *  string)
{
	char		tmp;
	char *		ep = &tmp;
	char *		left;
	char *		right;
	int		rightLength;
	uint64_t	integerPart, fractionalPart;


	left		= stringAtLeft(N, string, '.');
	right		= stringAtRight(N, string, '.');
	if (right != NULL)
	{
		rightLength	= strlen(right);
	}

	integerPart = strtoul(left, &ep, 0);
	if (*ep != '\0')
	{
		noisyFatal(N, Esanity);

		/* Not reached */
	}

	
	if (right == NULL)
	{
		/*
		 *	stringAtLeft() makes a copy, which needs to be freed.
		 *	(stringAtRight on the other hand does not need to make
		 *	a copy, and doesn't).
		 */
		free(left);

		return (double)integerPart;
	}
	fractionalPart	= strtoul(right, &ep, 0);
	if (*ep != '\0')
	{
		noisyFatal(N, Esanity);

		/* Not reached */
	}

	/*
	 *	stringAtLeft() makes a copy, which needs to be freed.
	 *	(stringAtRight on the other hand does not need to make
	 *	a copy, and doesn't).
	 */
	free(left);


	return (double)integerPart + ((double)fractionalPart/pow(10.0, rightLength));
}


static double
stringToEngineeringRealConst(NoisyState *  N, char *  string)
{
	char		engineeringChar;
	char *		left;
	char *		right;
	double		mantissa, exponent;


	if (strchr(string, 'e'))
	{
		engineeringChar = 'e';
	}
	else
	{
		engineeringChar = 'E';
	}

	left		= stringAtLeft(N, string, engineeringChar);
	right		= stringAtRight(N, string, engineeringChar);

	mantissa 	= stringToRealConst(N, left);
	exponent 	= stringToRealConst(N, right);

	/*
	 *	stringAtLeft() makes a copy, which needs to be freed.
	 *	(stringAtRight on the other hand does not need to make
	 *	a copy, and doesn't).
	 */
	free(left);


	return (mantissa * pow(10.0, exponent));
}

static void
checkGt(NoisyState *  N)
{
	NoisyIrNodeType		type;

	/*
	 *	Gobble any extant chars.
	 */
	finishToken(N);

	if (eqf(N))
	{
		gobble(N, 2);
		type = kNewtonIrNodeType_Tge;
	}
	else
	{
		gobble(N, 1);
		type = kNewtonIrNodeType_Tgt;
	}

	NoisyToken *		newToken = newtonLexAllocateToken(N,	type	/* type		*/,
									NULL	/* identifier	*/,
									0	/* integerConst	*/,
									0.0	/* realConst	*/,
									NULL	/* stringConst	*/,
									NULL	/* sourceInfo	*/);

	/*
	 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
	 */
	done(N, newToken);
}


static void
checkLt(NoisyState *  N)
{
	NoisyIrNodeType		type;

	/*
	 *	Gobble any extant chars.
	 */
	finishToken(N);

	if (eqf(N))
	{
		gobble(N, 2);
		type = kNewtonIrNodeType_Tle;
	}
	else
	{
		gobble(N, 1);
		type = kNewtonIrNodeType_Tlt;
	}

	NoisyToken *		newToken = newtonLexAllocateToken(N,	type	/* type		*/,
									NULL	/* identifier	*/,
									0	/* integerConst	*/,
									0.0	/* realConst	*/,
									NULL	/* stringConst	*/,
									NULL	/* sourceInfo	*/);

	/*
	 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
	 */
	done(N, newToken);
}

static bool 
checkProportionality(NoisyState * N)
{
	NoisyIrNodeType		type;

	/*
	 *	Gobble any extant chars.
	 */
	finishToken(N);

	if (N->lineLength >= 2 && N->lineBuffer[N->columnNumber+1] == '<')
	{
		gobble(N, 2);
		type = kNewtonIrNodeType_Tproportionality;
	}
	else
	{
        return false; // starting with 'o' but not an operator
	}

	NoisyToken *		newToken = newtonLexAllocateToken(N,	type	/* type		*/,
									NULL	/* identifier	*/,
									0	/* integerConst	*/,
									0.0	/* realConst	*/,
									NULL	/* stringConst	*/,
									NULL	/* sourceInfo	*/);

	/*
	 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
	 */
	done(N, newToken);
    
    return true;
}

static void
checkMul(NoisyState *  N)
{
	NoisyIrNodeType		type;

	/*
	 *	Gobble any extant chars.
	 */
	finishToken(N);

	if (N->lineLength >= 2 && N->lineBuffer[N->columnNumber+1] == '*')
	{
		gobble(N, 2);
		type = kNewtonIrNodeType_Texponent;
	}
	else
	{
		gobble(N, 1);
		type = kNewtonIrNodeType_Tmul;
	}

	NoisyToken *		newToken = newtonLexAllocateToken(N,	type	/* type		*/,
									NULL	/* identifier	*/,
									0	/* integerConst	*/,
									0.0	/* realConst	*/,
									NULL	/* stringConst	*/,
									NULL	/* sourceInfo	*/);

	/*
	 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
	 */
	done(N, newToken);
}

static void
checkDot(NoisyState *  N)
{
	NoisyTimeStampTraceMacro(kNoisyTimeStampKeyLexerCheckDot);

	/*
	 *	If token thus far is	"0" | onenine {zeronine}	then
	 *	don't gobble; continue building token.  However, something like
	 *	"5zyyg".
	 */
	if (isDecimal(N, N->currentToken))
	{
		checkTokenLength(N, 1);
		N->currentToken[N->currentTokenLength++] = N->lineBuffer[N->columnNumber++];

		return;
	}

	/*
	 *	Gobble any extant chars.
	 */
	finishToken(N);

	gobble(N, 1);

	NoisyToken *		newToken = newtonLexAllocateToken(N,	kNewtonIrNodeType_Tdot /* type	*/,
									NULL	/* identifier	*/,
									0	/* integerConst	*/,
									0.0	/* realConst	*/,
									NULL	/* stringConst	*/,
									NULL	/* sourceInfo	*/);

	/*
	 *	done() sets the N->currentTokenLength to zero and bzero's the N->currentToken buffer.
	 */
	done(N, newToken);
}

static bool
isOperatorOrSeparator(NoisyState *  N, char c)
{
	/*
	 *	Unlike in our Yacc-driven compielers, we don't use a "stickies" array
	 */
	switch (c)
	{
		case '~':
		case '!':
		case '%':
        case '@': // only to check 'o<' kNewtonIrNodeType_Tproportionality
		case '^':
		case '&':
		case '*':
		case '(':
		case ')':
		case '-':
		case '+':
		case '=':
		case '/':
		case '>':
		case '<':
		case ';':
		case ':':
		case '\'':
		case '\"':
		case '{':
		case '}':
		case '[':
		case ']':
		case '|':
		case ',':
		case '.':
		case ' ':
		case '\n':
		case '\r':
		case '\t':
		case '#':
		{
			return true;
		}
	}

	return false;
}

static bool
eqf(NoisyState *  N)
{
	NoisyTimeStampTraceMacro(kNoisyTimeStampKeyLexerEqf);

	return (N->lineLength >= 2 && N->lineBuffer[N->columnNumber+1] == '=');
}

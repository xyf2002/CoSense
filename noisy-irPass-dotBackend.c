/*
	Authored 2015. Phillip Stanley-Marbell.

	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:

	*	Redistributions of source code must retain the above
		copyright notice, this list of conditions and the following
		disclaimer.

	*	Redistributions in binary form must reproduce the above
		copyright notice, this list of conditions and the following
		disclaimer in the documentation and/or other materials
		provided with the distribution.

	*	Neither the name of the author nor the names of its
		contributors may be used to endorse or promote products
		derived from this software without specific prior written
		permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <setjmp.h>
#include <sys/time.h>
#include <string.h>
#include "flextypes.h"
#include "flexerror.h"
#include "flex.h"
#include "noisy-errors.h"
#include "version.h"
#include "noisy-timeStamps.h"
#include "noisy.h"
#include "noisy-parser.h"
#include "noisy-lexer.h"
#include "noisy-irPass-helpers.h"
#include "noisy-types.h"

extern char *	gNoisyAstNodeStrings[];


static bool	isType(NoisyState *  N, NoisyIrNode *  node);
static char *	scope2id(NoisyState *  N, NoisyScope *  scope);
static char *	scope2id2(NoisyState *  N, NoisyScope *  scope);
static char *	symbol2id(NoisyState *  N, NoisySymbol *  symbol);


int
noisyIrPassDotAstDotFmt(NoisyState *  N, char *  buf, int buflen, NoisyIrNode *  p)
{
	NoisyTimeStampTraceMacro(kNoisyTimeStampKeyIrPassDotAstDotFmt);

	char *		nilFormatString;
	char *		tokenString = "";
	char *		typeString;
	char *		l;
	char *		nodeBorderString;
	char *		nodePropertiesString;
	char *		r;
	char *		src;
	int		n = 0;


	/*
	 *	TODO: if we run out of space in print buffer, we should
	 *	print a "..." rather than just ending like we do now.
	 */

	if (p->tokenString != NULL)
	{
		tokenString = p->tokenString;
	}

	/*
	 *	We use the pointer address of the NoisyIrNode *  p to give a unique
	 *	string for each node in the graph. NOTE: dot renders _much_ faster
	 *	if we don't supply a fontname (which it often cannot find anyway)...
	 */
	nilFormatString		= "style=filled,color=\"#003333\",fontcolor=white,fontsize=8,width=0.3,height=0.16,fixedsize=true,label=\"nil\", shape=record";
	nodePropertiesString	= "";
	nodeBorderString	= "M";
	typeString		= gNoisyAstNodeStrings[p->type];

	/*
	 *	For identifiers, different graph node properties
	 */
	if (p->type == kNoisyIrNodeType_Tidentifier)
	{
		nodePropertiesString = "style=filled,color=\"#ccff66\",";
		nodeBorderString = "";
	}

	/*
	 *	For X_SEQ, different graph node properties
	 */
	if (p->type == kNoisyIrNodeType_Xseq)
	{
		nodePropertiesString = "style=filled,color=\"#999999\",fixedsize=true,";
		nodeBorderString = "M";

		/*
		 *	X_SEQ is not part of gASTnodeSTRINGS, which is generated by our
		 *	ffi2code tools.
		 */
		typeString = "X_SEQ";
	}

	src = (char *) calloc(kNoisyMaxPrintBufferLength, sizeof(char));
	if (src == NULL)
	{
		noisyFatal(N, Emalloc);
	}

	if (p->type != kNoisyIrNodeType_Xseq)
	{
		snprintf(src, kNoisyMaxPrintBufferLength, "| source:%llu,%llu", p->sourceInfo->lineNumber, p->sourceInfo->columnNumber);
	}

	if (N->dotDetailLevel & kNoisyDotDetailLevelNoText)
	{
		n += snprintf(&buf[n], buflen,
			"\tP" FLEX_PTRFMTH " [%sfontsize=8,height=0.8,"
			"label=\"{ | {<left> | <right> }}\",shape=%srecord];\n",
			(FlexAddr)p, nodePropertiesString, nodeBorderString);
	}
	else
	{
		n += snprintf(&buf[n], buflen,
			"\tP" FLEX_PTRFMTH " [%sfontsize=8,height=0.8,"
			"label=\"{P" FLEX_PTRFMTH "\\ntype=%s\\n%s%s\\n%s %s%s%s| {<left> | <right> }}\",shape=%srecord];\n",
			(FlexAddr)p, nodePropertiesString, (FlexAddr)p, typeString, 
			((tokenString == NULL || strlen(tokenString) == 0) ? "" : " tokenString="), tokenString, 
			src, (isType(N, p) ? "#" : ""), (isType(N, p) ? noisyTypeMakeTypeSignature(N, p) : ""), (isType(N, p) ? "#" : ""), nodeBorderString);
	}

	buflen -= n;

	if (!(N->dotDetailLevel & kNoisyDotDetailLevelNoNilNodes) && (L(p) == NULL))
	{
		n += snprintf(&buf[n], buflen, "\tP" FLEX_PTRFMTH "_leftnil [%s];\n",
			(FlexAddr)p, nilFormatString);
		buflen -= n;
	}
	if (!(N->dotDetailLevel & kNoisyDotDetailLevelNoNilNodes) && (R(p) == NULL))
	{
		n += snprintf(&buf[n], buflen, "\tP" FLEX_PTRFMTH "_rightnil [%s];\n",
			(FlexAddr)p, nilFormatString);
		buflen -= n;
	}

	l = (char *)calloc(kNoisyMaxPrintBufferLength, sizeof(char));
	if (l == NULL)
	{
		noisyFatal(N, Emalloc);
	}

	r = (char *)calloc(kNoisyMaxPrintBufferLength, sizeof(char));
	if (r == NULL)
	{
		noisyFatal(N, Emalloc);
	}

	if (!(N->dotDetailLevel & kNoisyDotDetailLevelNoNilNodes) && (L(p) == NULL))
	{
		snprintf(l, kNoisyMaxPrintBufferLength, "P" FLEX_PTRFMTH "_leftnil", (FlexAddr)p);
	}
	else if (L(p) != NULL)
	{
		snprintf(l, kNoisyMaxPrintBufferLength, "P" FLEX_PTRFMTH "", (FlexAddr)L(p));
	}

	if (!(N->dotDetailLevel & kNoisyDotDetailLevelNoNilNodes) && (R(p) == NULL))
	{
		snprintf(r, kNoisyMaxPrintBufferLength, "P" FLEX_PTRFMTH "_rightnil", (FlexAddr)p);
	}
	else if (R(p) != NULL)
	{
		snprintf(r, kNoisyMaxPrintBufferLength, "P" FLEX_PTRFMTH, (FlexAddr)R(p));
	}

	if (strlen(l))
	{
		n += snprintf(&buf[n], buflen, "\tP" FLEX_PTRFMTH ":left -> %s;\n", (FlexAddr)p, l);
		buflen -= n;
	}

	if (strlen(r))
	{
		n += snprintf(&buf[n], buflen, "\tP" FLEX_PTRFMTH ":right -> %s;\n", (FlexAddr)p, r);
		buflen -= n;
	}

	USED(buflen);
	free(l);
	free(r);
	free(src);


	return n;
}


int
noisyIrPassDotSymbolTableDotFmt(NoisyState *  N, char *  buf, int buflen, NoisyScope *  scope)
{
	NoisyTimeStampTraceMacro(kNoisyTimeStampKeyIrPassDotSymbotTableDotFmt);

	char *		nilFormatString;
	char *		symbolFormatString;
	int		n = 0;


	/*
	 *	TODO: if we run out of space in print buffer, we should
	 *	print a "..." rather than just ending like we do now.
	 */

	nilFormatString		= "style=filled,color=\"#000000\",fontcolor=white,fontsize=8,width=0.3,height=0.16,fixedsize=true,fontname=\"LucidaSans-Typewriter\",label=\"nil\", shape=record";
	symbolFormatString	= "style=filled,color=\"#dddddd\",fontcolor=black,fontsize=8,height=0.16,fontname=\"LucidaSans-Typewriter\", shape=record";

	n += snprintf(	&buf[n], buflen, "\tscope%s [style=filled,color=\"#FFCC00\",fontname=\"LucidaSans-Typewriter\",fontsize=8,height=0.8,label=\"{%s | {<children> | <syms>}}\",shape=record];\n",
			scope2id(N, scope), scope2id2(N, scope));
	buflen -= n;

	if (scope->firstChild == NULL)
	{
		n += snprintf(&buf[n], buflen, "\tscope%s_kidsnil [%s];\n", scope2id(N, scope), nilFormatString);
		buflen -= n;
	}
	if (scope->firstSymbol == NULL)
	{
		n += snprintf(&buf[n], buflen, "\tscope%s_symsnil [%s];\n", scope2id(N, scope), nilFormatString);
		buflen -= n;
	}

	NoisyScope *	childScope  = scope->firstChild;
	while (childScope != NULL)
	{
		n += snprintf(	&buf[n], buflen, "\tscope%s [style=filled,color=\"#FFCC00\",fontname=\"LucidaSans-Typewriter\",fontsize=8,height=0.8,label=\"{%s | {<children> | <syms>}}\",shape=record];\n",
				scope2id(N, childScope), scope2id2(N, childScope));
		buflen -= n;

		childScope = childScope->next;
	}

	NoisySymbol *	childSymbol = scope->firstSymbol;
	while (childSymbol != NULL)
	{
		n += snprintf(&buf[n], buflen, "\tsym%s [%s,label=\"{%s}\",shape=rect];\n",
			symbol2id(N, childSymbol), symbolFormatString, childSymbol->identifier);
		buflen -= n;

		childSymbol = childSymbol->next;
	}


	if (scope->firstChild == NULL)
	{
		n += snprintf(&buf[n], buflen, "\tscope%s:children -> scope%s_kidsnil;\n", scope2id(N, scope), scope2id(N, scope));
		buflen -= n;
	}
	else
	{
		childScope  = scope->firstChild;
		while (childScope != NULL)
		{
			n += snprintf(&buf[n], buflen, "\tscope%s:children -> scope%s;\n", scope2id(N, scope), scope2id(N, childScope));
			buflen -= n;
			
			childScope = childScope->next;
		}
	}

	if (scope->firstSymbol == NULL)
	{
		n += snprintf(&buf[n], buflen, "\tscope%s:syms -> scope%s_symsnil;\n", scope2id(N, scope), scope2id(N, scope));
		buflen -= n;
	}
	else
	{
		childSymbol = scope->firstSymbol;
		n += snprintf(&buf[n], buflen, "\tscope%s:syms -> sym%s;\n", scope2id(N, scope), symbol2id(N, childSymbol));
		buflen -= n;

		while (childSymbol != NULL && childSymbol->next != NULL)
		{
			n += snprintf(&buf[n], buflen, "\tsym%s:syms -> sym%s;\n", symbol2id(N, childSymbol), symbol2id(N, childSymbol->next));
			buflen -= n;
			childSymbol = childSymbol->next;
		}
	}
	USED(buflen);

	return n;
}


int
noisyIrPassDotAstPrintWalk(NoisyState *  N, NoisyIrNode *  p, char *  buf, int buflen)
{
	NoisyTimeStampTraceMacro(kNoisyTimeStampKeyIrPassAstDotPrintWalk);

	int	n0 = 0, n1 = 0, n2 = 0;

	if (p == NULL)
	{
		return 0;
	}

	if (L(p) == p || R(p) == p)
	{
		noisyFatal(N, "Immediate cycle in Ir, seen noisyIrPassAstDotPrintWalk()!!\n");

		/*
		 *	Not reached
		 */
		return 0;
	}

	/*
	 *	For DOT, we walk tree in postorder, though it doesn't matter
	 *	either way.
	 */
	n0 = noisyIrPassDotAstPrintWalk(N, L(p), &buf[0], buflen);
	n1 = noisyIrPassDotAstPrintWalk(N, R(p), &buf[n0], buflen-n0);

	/*
	 *	Only process nodes once.
	 */
	if (p->nodeColor & kNoisyIrNodeColorDotBackendColoring)
	{
		n2 = noisyIrPassDotAstDotFmt(N, &buf[n0+n1], buflen-(n0+n1), p);
		p->nodeColor &= ~kNoisyIrNodeColorDotBackendColoring;
	}

	return (n0+n1+n2);
}


int
noisyIrPassDotSymbolTablePrintWalk(NoisyState *  N, NoisyScope *  p, char *  buf, int buflen)
{
	NoisyTimeStampTraceMacro(kNoisyTimeStampKeyIrPassSymbolTableDotPrintWalk);

	int	n0 = 0, n1 = 0, n2 = 0;

	if (p == NULL)
	{
		return 0;
	}

	//if (L(p) == p || R(p) == p)
	{
		noisyFatal(N, "Immediate cycle in Ir, seen noisyIrPassDotSymbolTablePrintWalk()!!\n");

		/*
		 *	Not reached
		 */
		return 0;
	}

	/*
	 *	For DOT, we walk tree in postorder, though it doesn't matter
	 *	either way.
	 */
	//n0 = noisyIrPassDotSymbolTablePrintWalk(N, L(p), &buf[0], buflen);
	//n1 = noisyIrPassDotSymbolTablePrintWalk(N, R(p), &buf[n0], buflen-n0);

	/*
	 *	Only process nodes once.
	 */
	//if (p->nodeColor & kNoisyIrNodeColorDotBackendColoring)
	{
		//n2 = noisyIrPassDotSymbolTableDotFmt(N, &buf[n0+n1], buflen-(n0+n1), p);
		//p->nodeColor &= ~kNoisyIrNodeColorDotBackendColoring;
	}

	return (n0+n1+n2);
}


char *
noisyIrPassDotBackend(NoisyState *  N)
{
	NoisyTimeStampTraceMacro(kNoisyTimeStampKeyIrPassDotBackend);

	NoisyIrNode *		p;
	int			buflen, treesz, n = 0;
	char *			buf = NULL;
	struct timeval		t;

	/*
	 *	Length is required to be 26 chars by ctime_r.
	 */
	char			dateString[26];


	p = N->noisyIrRoot;

	/*
	 *	Heuristic
	 */
	treesz = noisyIrPassHelperTreeSize(N, p);
	buflen = treesz*kNoisyChunkBufferLength;

	/*
	 *	This buffer is deallocated by our caller
	 */
	buf = calloc(buflen, sizeof(char));
	if (buf == NULL)
	{
		noisyFatal(N, Emalloc);
	}

#ifdef NoisyOsMacOSX
	gettimeofday(&t, NULL);
	ctime_r(&t.tv_sec, dateString);

	/*
	 *	Ctime string always ends in '\n\0'; elide the '\n'
	 */
	dateString[24] = '.';

	n += snprintf(&buf[n], buflen, "digraph Noisy\n{\n");
//TODO: here and elsewhere, should be taking buflen = max(buflen - n, 0)
//n = max(MAX_PRINT_BUF - strlen(buf), 0); like we do for universe_print in sal

	buflen -= n;

	/*
	 *	When rendering bitmapped, don't restrict size, and
	 *	leave dpi reasonable (~150).
	 */
	n += snprintf(&buf[n], buflen, "\tdpi=150;\n");
	buflen -= n;
	n += snprintf(&buf[n], buflen, "\tfontcolor=\"#C0C0C0\";\n");
	buflen -= n;
	n += snprintf(&buf[n], buflen, "\tfontsize=18;\n");
	buflen -= n;

//TODO: take the whole of this following string as one of the arguments, called, e.g., "dotplotlabel",
//so we are not calling gettimeofday() from here, and don't need to have the VM_VERSION symbol here either.
	n += snprintf(&buf[n], buflen, "\tlabel = \"\\nAuto-generated by Noisy compiler, version %s, on %s\";\n",
			kNoisyVersion, dateString);
	buflen -= n;
	n += snprintf(&buf[n], buflen, "\tsplines = true;\n");
	buflen -= n;

	/*
	 *	Don't restrict size when rendering bitmapped
	 */
	//n += snprintf(&buf[n], buflen, "\tsize = \"5,9\";\n");
	//buflen -= n;

	n += snprintf(&buf[n], buflen, "\tmargin = 0.1;\n");
	buflen -= n;

	/*
	 *	Temporarily color the graph, so we can know
	 *	which nodes have been visited, in case when
	 *	the graph is not a tree.
	 */
	noisyIrPassHelperColorTree(N, p, kNoisyIrNodeColorDotBackendColoring, true/* set */, true/* recurse flag */);

	n += noisyIrPassDotAstPrintWalk(N, p, &buf[n], buflen);
	buflen -= n;

//	n += noisyIrPassDotSymbolTablePrintWalk(N, N->noisyIrTopScope, &buf[n], buflen);
//	buflen -= n;

	n += snprintf(&buf[n], buflen, "}\n");
	buflen -= n;
	USED(buflen);

	/*
	 *	TODO: this is not really necessary, since we now use individual
	 *	bitfields for coloring in different passes, and we clear the
	 *	colors of nodes above anyway. If/when we decide to get rid of
	 *	this, be sure to document the associated gain.
	 */
	noisyIrPassHelperColorTree(N, p, ~kNoisyIrNodeColorDotBackendColoring, false/* clear */, true/* recurse flag */);
#endif

	return buf;
}






/*
 *	Local functions.
 */




static bool
isType(NoisyState *  N, NoisyIrNode *  node)
{
	NoisyTimeStampTraceMacro(kNoisyTimeStampKeyIrPassDotIsType);

	switch (node->type)
	{
		case kNoisyIrNodeType_PconstantDeclaration:
		case kNoisyIrNodeType_PtypeDeclaration:
		case kNoisyIrNodeType_PnamegenDeclaration:
		case kNoisyIrNodeType_PfixedType:
		case kNoisyIrNodeType_PanonAggregateType:
		case kNoisyIrNodeType_Tbool:
		case kNoisyIrNodeType_Tnybble:
		case kNoisyIrNodeType_Tbyte:
		case kNoisyIrNodeType_Tint:
		case kNoisyIrNodeType_Tstring:
		case kNoisyIrNodeType_Treal:
 		case kNoisyIrNodeType_Ttype:
		{
			return true;
		}
		
		default:
		{
			return false;
		}
	}

	return false;
}


static char *
scope2id(NoisyState *  N, NoisyScope *  scope)
{
	NoisyTimeStampTraceMacro(kNoisyTimeStampKeyIrPassDotScope2Id);

	if (scope == NULL)
	{
		noisyFatal(N, Esanity);
	}
	if (scope->begin == NULL || scope->end == NULL)
	{
		return "???";
	}

	char *	buf, tmp[kNoisyMaxBufferLength];

	int length = snprintf(tmp, kNoisyMaxBufferLength, "%llu_%llu_%llu_%llu",
			scope->begin->lineNumber, scope->begin->columnNumber,
			scope->end->lineNumber, scope->end->columnNumber);

	buf = (char *)malloc(length);

	sprintf(buf, "%llu_%llu_%llu_%llu",
			scope->begin->lineNumber, scope->begin->columnNumber,
			scope->end->lineNumber, scope->end->columnNumber);

	return buf;
}


static char *
scope2id2(NoisyState *  N, NoisyScope *  scope)
{
	NoisyTimeStampTraceMacro(kNoisyTimeStampKeyIrPassDotScope2Id2);

	if (scope == NULL)
	{
		noisyFatal(N, Esanity);
	}
	if (scope->begin == NULL || scope->end == NULL)
	{
		return "???";
	}

	char *	buf, tmp[kNoisyMaxBufferLength];

	int length = snprintf(tmp, kNoisyMaxBufferLength, "%s:%llu,%llu \\nto\\n %s:%llu,%llu",
		scope->begin->fileName, scope->begin->lineNumber, 
		scope->begin->columnNumber,  scope->begin->fileName,
		scope->end->lineNumber, scope->end->columnNumber);

	buf = (char *)malloc(length);

	sprintf(buf, "%s:%llu,%llu \\nto\\n %s:%llu,%llu",
		scope->begin->fileName, scope->begin->lineNumber, 
		scope->begin->columnNumber,  scope->begin->fileName,
		scope->end->lineNumber, scope->end->columnNumber);

	return buf;
}


static char *
symbol2id(NoisyState *  N, NoisySymbol *  symbol)
{
	NoisyTimeStampTraceMacro(kNoisyTimeStampKeyIrPassDotSymbol2Id);

	if (symbol == NULL)
	{
		noisyFatal(N, Esanity);
	}
	if (symbol->sourceInfo == NULL)
	{
		return "???";
	}

	char *	buf, tmp[kNoisyMaxBufferLength];

	int length = snprintf(tmp, kNoisyMaxBufferLength, "%llu_%llu",
		symbol->sourceInfo->lineNumber, symbol->sourceInfo->columnNumber);

	buf = (char *)malloc(length);

	sprintf(buf, "%llu_%llu",
		symbol->sourceInfo->lineNumber, symbol->sourceInfo->columnNumber);

	return buf;
	
}
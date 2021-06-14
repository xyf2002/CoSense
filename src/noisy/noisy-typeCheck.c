#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "flextypes.h"
#include "flexerror.h"
#include "flex.h"
#include "version.h"
#include "noisy-timeStamps.h"
#include "common-errors.h"
#include "common-timeStamps.h"
#include "common-data-structures.h"
#include "common-symbolTable.h"
#include "common-irHelpers.h"
#include "noisy-typeCheck.h"

NoisyType getNoisyTypeFromExpression(State * N, IrNode * noisyExpressionNode, Scope * currentScope);
bool noisyIsOfType(NoisyType typ1,NoisyBasicType typeSuperSet);



extern char *		gNoisyProductionDescriptions[];
extern const char *	gNoisyTokenDescriptions[];

static char		kNoisyErrorTokenHtmlTagOpen[]	= "<span style=\"background-color:#FFCC00; color:#FF0000;\">";
static char		kNoisyErrorTokenHtmlTagClose[]	= "</span>";
static char		kNoisyErrorDetailHtmlTagOpen[]	= "<span style=\"background-color:#A9E9FF; color:#000000;\">";
static char		kNoisyErrorDetailHtmlTagClose[]	= "</span>";


void
noisySemanticErrorPre(State *  N, IrNode * currentlyParsingNode,
	const char *  string1, const char *  string2, const char *  string3, const char *  string4)
{
	flexprint(N->Fe, N->Fm, N->Fperr, "\n-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --\n");
	if (N->mode & kCommonModeCGI)
	{
		flexprint(N->Fe, N->Fm, N->Fperr, "<b>");
	}

	if (N->mode & kCommonModeCGI)
	{
		flexprint(N->Fe, N->Fm, N->Fperr, "\n\t%s, line %d position %d, %s ",
						string1,
						currentlyParsingNode->sourceInfo->lineNumber,
						currentlyParsingNode->sourceInfo->columnNumber,
						kNoisyErrorTokenHtmlTagOpen);
		flexprint(N->Fe, N->Fm, N->Fperr, " %s %s %s.<br><br>%s%s",
			kNoisyErrorTokenHtmlTagClose,
			string2,
			(currentlyParsingNode->type > kNoisyIrNodeType_TMax ?
				gNoisyProductionDescriptions[currentlyParsingNode->type] :
				gNoisyTokenDescriptions[currentlyParsingNode->type]),
			kNoisyErrorDetailHtmlTagOpen,
			string3);
	}
	else
	{
		flexprint(N->Fe, N->Fm, N->Fperr, "\n\t%s, %s line %d position %d,",
						string1,
						currentlyParsingNode->sourceInfo->fileName,
						currentlyParsingNode->sourceInfo->lineNumber,
						currentlyParsingNode->sourceInfo->columnNumber);
		flexprint(N->Fe, N->Fm, N->Fperr, " %s %s.\n\n\t%s",
			string2,
			(currentlyParsingNode->type > kNoisyIrNodeType_TMax ?
				gNoisyProductionDescriptions[currentlyParsingNode->type] :
				gNoisyTokenDescriptions[currentlyParsingNode->type]),
			string3);
	}	
}

void
noisySemanticErrorPost(State *  N)
{
	if (N->mode & kCommonModeCGI)
	{
		flexprint(N->Fe, N->Fm, N->Fperr, "%s</b>", kNoisyErrorDetailHtmlTagClose);
	}

	flexprint(N->Fe, N->Fm, N->Fperr, "\n-- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - --\n\n");	
}


void
noisySemanticError(State *  N, IrNode * currentlyParsingNode, char *  details)
{
	noisySemanticErrorPre(N, currentlyParsingNode, EsemanticsA, EsemanticsB, details, EsemanticsD);
	noisySemanticErrorPost(N);
}


void
noisySemanticErrorRecovery(State *  N)
{
	TimeStampTraceMacro(kNoisyTimeStampKeyParserErrorRecovery);

	if (N->verbosityLevel & kCommonVerbosityDebugParser)
	{
		flexprint(N->Fe, N->Fm, N->Fperr, "In noisySemanticErrorRecovery(), about to discard tokens...\n");
	}

	if ((N != NULL) && (N->jmpbufIsValid))
	{
		//fprintf(stderr, "doing longjmp");

		/*
		 *	See issue #291.
		 */
		longjmp(N->jmpbuf, 0);
	}

	/*
	 *	Not reached if N->jmpbufIsValid
	 */
	consolePrintBuffers(N);

	exit(EXIT_SUCCESS);
}
/*
*       If we have templated function declaration, returns false.
*       We should invoke code generation with the load operator.
*/
bool
isTypeExprComplete(State * N,IrNode * typeExpr)
{

        if (L(typeExpr)->type == kNoisyIrNodeType_PbasicType)
        {
                return true;
        }
        else if (L(typeExpr)->type == kNoisyIrNodeType_PtypeName)
        {
                /*
                *       TODO; Scope here needs fixing. Typenames need to be resolved.
                */
                Symbol * typeSymbol = commonSymbolTableSymbolForIdentifier(N,N->noisyIrTopScope,LL(typeExpr)->tokenString);
                if (typeSymbol->symbolType == kNoisySymbolTypeModuleParameter)
                {
                        return false;
                }
                else
                {
                        return isTypeExprComplete(N,typeSymbol->typeTree->irRightChild);
                }
        }
        else if (L(typeExpr)->type == kNoisyIrNodeType_PanonAggregateType)
        {
                if (LL(typeExpr)->type == kNoisyIrNodeType_ParrayType)
                {
                        IrNode * iter;
                        for (iter = LL(typeExpr); iter != NULL; iter = R(iter))
                        {
                                if (iter->irLeftChild->type == kNoisyIrNodeType_PtypeExpr)
                                {
                                        break;
                                }
                        }
                        return isTypeExprComplete(N,L(iter));
                }
                return false;
        }
        return false;
}

void
noisyInitNoisyType(NoisyType * typ)
{
        typ->basicType = noisyInitType;
        typ->dimensions = 0;
        typ->sizeOfDimension = NULL;
        typ->arrayType = noisyInitType;
}

bool
noisyTypeEquals(NoisyType typ1, NoisyType typ2)
{
        if (typ1.basicType == typ2.basicType)
        {
                if (typ1.basicType == noisyArrayType)
                {
                        if (typ1.arrayType == typ2.arrayType)
                        {
                                if (typ1.dimensions == typ2.dimensions)
                                {
                                        for (int i = 0; i < typ1.dimensions; i++)
                                        {
                                                if (typ1.sizeOfDimension[i] != typ2.sizeOfDimension[i])
                                                {
                                                        return false;
                                                }
                                        }
                                        return true;
                                }
                                return false;
                        }
                        return false;
                }
                return true;
        }
        else if (typ1.basicType == noisyIntegerConstType)
        {
                return noisyIsOfType(typ2,noisyIntegerConstType);
        }
        else if (typ2.basicType == noisyIntegerConstType)
        {
                return noisyIsOfType(typ1,noisyIntegerConstType);        
        }
        else if (typ1.basicType == noisyRealConstType)
        {
                return noisyIsOfType(typ2,noisyRealConstType);          
        }
        else if (typ2.basicType == noisyRealConstType)
        {
                return noisyIsOfType(typ1,noisyRealConstType);          
        }
        return false;
}

bool
noisyIsOfType(NoisyType typ1,NoisyBasicType typeSuperSet)
{
        if (typeSuperSet == noisyIntegerConstType)
        {
                switch (typ1.basicType)
                {
                case noisyInt4:
                case noisyInt8:
                case noisyInt16:
                case noisyInt32:
                case noisyInt64:
                case noisyInt128:
                case noisyIntegerConstType:
                case noisyNat4:
                case noisyNat8:
                case noisyNat16:
                case noisyNat32:
                case noisyNat64:
                case noisyNat128:
                        return true;
                        break;
                default:
                        return false;
                        break;
                }
        }
        else if (typeSuperSet == noisyRealConstType)
        {
                switch (typ1.basicType)
                {
                case noisyFloat16:
                case noisyFloat32:
                case noisyFloat64:
                case noisyFloat128:
                case noisyRealConstType:
                        return true;
                        break;
                default:
                        return false;
                        break;
                }
        }
        else if (typeSuperSet == noisyArithType)
        {
                return noisyIsOfType(typ1,noisyIntegerConstType) || noisyIsOfType(typ1,noisyRealConstType);
        }
        return false;
}


NoisyType
getNoisyTypeFromBasicType(IrNode * basicType)
{
        NoisyType noisyType;
        noisyType.arrayType = noisyInitType;
        noisyType.dimensions = 0;
        noisyType.sizeOfDimension = NULL;

        if (L(basicType)->type == kNoisyIrNodeType_Tbool)
        {
                noisyType.basicType = noisyBool;
        }
        else if (L(basicType)->type == kNoisyIrNodeType_PintegerType)
        {
                /*
                *       LLVM does not make distintion on signed and unsigned values on its typesystem.
                *       However it can differentiate between them during the operations (e.g signed addition).
                */
                switch (LL(basicType)->type)
                {
                case kNoisyIrNodeType_Tint4:
                        noisyType.basicType = noisyInt4;
                        break;
                case kNoisyIrNodeType_Tnat4:
                        noisyType.basicType = noisyNat4;
                        break;
                case kNoisyIrNodeType_Tint8:
                        noisyType.basicType = noisyInt8;
                        break;
                case kNoisyIrNodeType_Tnat8:
                        noisyType.basicType = noisyNat8;
                        break;
                case kNoisyIrNodeType_Tint16:
                        noisyType.basicType = noisyInt16;
                        break;
                case kNoisyIrNodeType_Tnat16:
                        noisyType.basicType = noisyNat16;
                        break;
                case kNoisyIrNodeType_Tint32:
                        noisyType.basicType = noisyInt32;
                        break;
                case kNoisyIrNodeType_Tnat32:
                        noisyType.basicType = noisyNat32;
                        break;
                case kNoisyIrNodeType_Tint64:
                        noisyType.basicType = noisyInt64;
                        break;
                case kNoisyIrNodeType_Tnat64:
                        noisyType.basicType = noisyNat64;
                        break;
                case kNoisyIrNodeType_Tint128:
                        noisyType.basicType = noisyInt128;
                        break;
                case kNoisyIrNodeType_Tnat128:
                        noisyType.basicType = noisyNat128;
                        break;
                default:
                        break;
                }
        }
        else if (L(basicType)->type == kNoisyIrNodeType_PrealType)
        {
                switch (LL(basicType)->type)
                {
                case kNoisyIrNodeType_Tfloat16:
                        noisyType.basicType = noisyFloat16;
                        break;
                case kNoisyIrNodeType_Tfloat32:
                        noisyType.basicType = noisyFloat32;
                        break;
                case kNoisyIrNodeType_Tfloat64:
                        noisyType.basicType = noisyFloat64;
                        break;
                case kNoisyIrNodeType_Tfloat128:
                        noisyType.basicType = noisyFloat128;
                        break;
                default:
                        break;
                }
        }
        else if(L(basicType)->type == kNoisyIrNodeType_Tstring)
        {
                noisyType.basicType = noisyString;
        }
        return noisyType;
}

/*
*       Takes an arrayType IrNode and returns the corresponding NoisyType
*       Assumes that arrayTypeNode->type == kNoisyIrNodeType_ParrayType.
*/
NoisyType
getNoisyTypeFromArrayNode(State * N,IrNode * arrayTypeNode)
{
        NoisyType noisyType;

        noisyType.basicType = noisyArrayType;
        noisyType.dimensions = 0;

        for (IrNode * iter = arrayTypeNode; iter != NULL; iter = R(iter))
        {
                if (L(iter)->type != kNoisyIrNodeType_PtypeExpr)
                {
                        noisyType.dimensions++;
                }
                else
                {
                        noisyType.arrayType = getNoisyTypeFromTypeExpr(N,L(iter)).basicType;
                }
        }

        noisyType.sizeOfDimension = calloc(noisyType.dimensions,sizeof(int));

        int i = 0;
        for (IrNode * iter = arrayTypeNode; iter != NULL; iter = R(iter))
        {
                if (L(iter)->type != kNoisyIrNodeType_PtypeExpr)
                {
                        noisyType.sizeOfDimension[i] = L(iter)->token->integerConst;
                }
                i++;
        }

        return noisyType;
}

/*
*       Takes the state N (needed for symbolTable search) and a typeNameNode
*       and returns the corresponding NoisyType.
*/
NoisyType
getNoisyTypeFromTypeSymbol(State * N,IrNode * typeNameNode)
{
        NoisyType noisyType;
        Symbol * typeSymbol = commonSymbolTableSymbolForIdentifier(N,NULL,L(typeNameNode)->tokenString);

        if (typeSymbol == NULL)
        {
                /*
                *       Type symbol does not exist error?
                */
                typeSymbol = commonSymbolTableSymbolForIdentifier(N,N->noisyIrTopScope,L(typeNameNode)->tokenString);
                noisyType.basicType = noisyTypeError;
                return noisyType;
        }

        IrNode * typeTree = typeSymbol->typeTree;

        if (RL(typeTree)->type == kNoisyIrNodeType_PbasicType)
        {
                return getNoisyTypeFromBasicType(RL(typeTree));
        }
        else if (RL(typeTree)->type == kNoisyIrNodeType_PanonAggregateType)
        {
                return getNoisyTypeFromArrayNode(N,RL(typeTree));
        }
        else if (RL(typeTree)->type == kNoisyIrNodeType_PtypeName)
        {
                return getNoisyTypeFromTypeSymbol(N,RL(typeTree));
        }
        else
        {
                noisyType.basicType = noisyTypeError;
                return noisyType;
        }
}

/*
*       Takes the state N and a TypeExpr node and returns the corresponding
*       NoisyType. If it fails the returned basic type is noisyTypeError.
*/
NoisyType
getNoisyTypeFromTypeExpr(State * N, IrNode * typeExpr)
{
        NoisyType noisyType;
        if (L(typeExpr)->type == kNoisyIrNodeType_PbasicType)
        {
                return getNoisyTypeFromBasicType(L(typeExpr));
        }
        else if (L(typeExpr)->type == kNoisyIrNodeType_PanonAggregateType)
        {
                IrNode * arrayType = LL(typeExpr);
                if (arrayType->type == kNoisyIrNodeType_ParrayType)
                {
                        return getNoisyTypeFromArrayNode(N,arrayType);
                }
                /*
                *       Lists and other non aggregate types are not supported
                */
                else
                {
                        /*
                        *       Not supported types error.
                        */
                       char *	details;

                        asprintf(&details, "Unsupported non Aggregate Type\n");
                        noisySemanticError(N,L(typeExpr),details);
                        noisySemanticErrorRecovery(N);
                        // noisyType.basicType = noisyTypeError;
                        // return noisyType;
                }
        }
        else if (L(typeExpr)->type == kNoisyIrNodeType_PtypeName)
        {
                return getNoisyTypeFromTypeSymbol(N,L(typeExpr));
        }

        noisyType.basicType = noisyTypeError;
        return noisyType;
}

/*
*       Takes an argument name, an expression and an inputSIgnature of a function. Checks if the argument name and type matches
*       with one of the signature's arguments. In that case it returns true. Otherwise, it returns false.
*/
bool
noisyArgumentMatchesSignature(State * N, IrNode * argName,IrNode * expr,IrNode * inputSignature, Scope * currentScope)
{
        for (IrNode * iter = inputSignature; iter != NULL; iter = RR(iter))
        {
                if (!strcmp(L(iter)->tokenString,argName->tokenString))
                {
                        if (noisyTypeEquals(getNoisyTypeFromTypeExpr(N,RL(iter)),getNoisyTypeFromExpression(N,expr,currentScope)))
                        {
                                return true;
                        }
                }
        }
        /*
        *       If we search all arguments and they dont match then we return false.
        */
        return false;               
}




/*
*       Takes a noisyFactor IrNode and a the currentScope and returns the NoisyType of the factor.
*       If every type check is correct returns the type else it returns noisyTypeError.
*/
NoisyType
getNoisyTypeFromFactor(State * N, IrNode * noisyFactorNode, Scope * currentScope)
{
        NoisyType factorType;

        if (L(noisyFactorNode)->type == kNoisyIrNodeType_TintegerConst)
        {
                factorType.basicType = noisyIntegerConstType;
        }
        else if (L(noisyFactorNode)->type == kNoisyIrNodeType_TrealConst)
        {
                factorType.basicType = noisyRealConstType;
        }
        else if (L(noisyFactorNode)->type == kNoisyIrNodeType_TstringConst)
        {
                factorType.basicType = noisyString;
        }
        else if (L(noisyFactorNode)->type == kNoisyIrNodeType_TboolConst)
        {
                factorType.basicType = noisyBool;
        }
        else if (L(noisyFactorNode)->type == kNoisyIrNodeType_PqualifiedIdentifier)
        {
                Symbol * identifierSymbol = commonSymbolTableSymbolForIdentifier(N, currentScope, LL(noisyFactorNode)->tokenString);

                if (identifierSymbol == NULL)
                {
                        factorType.basicType = noisyTypeError;
                }

                factorType = getNoisyTypeFromTypeExpr(N,identifierSymbol->typeTree);
                if (factorType.basicType == noisyArrayType)
                {
                        int dims = 0;
                        for (IrNode * iter = LR(noisyFactorNode); iter != NULL; iter = R(iter))
                        {
                                if (! noisyIsOfType(getNoisyTypeFromExpression(N,LR(iter),currentScope), noisyIntegerConstType))
                                {
                                        /*
                                        *       Indexes are not integers error.
                                        */
                                        char *	details;

                                        asprintf(&details, "Indexing \"%s\" array with a non-integer expression\n",identifierSymbol->identifier);
                                        noisySemanticError(N,iter,details);
                                        noisySemanticErrorRecovery(N);
                                        // factorType.basicType = noisyTypeError;
                                }
                                dims++;
                        }

                        if (dims != factorType.dimensions)
                        {
                                factorType.dimensions -= dims;
                        }
                        /*
                        *       If there are no type errors on array indexing we the arrayType of the array.
                        *       e.g. when we index an array of int32 the factor we return has type int32.
                        */
                        if (factorType.dimensions == 0)
                        {
                                factorType.basicType = factorType.arrayType;
                        }
                        else if (factorType.dimensions < 0 )
                        {
                                /*
                                *       Indexing dimension error.
                                */
                                char *	details;

                                asprintf(&details, "Dimensions of array \"%s\" dont match\n",identifierSymbol->identifier);
                                noisySemanticError(N,noisyFactorNode,details);
                                noisySemanticErrorRecovery(N);
                        }     
                }
                else
                {
                        if (LR(noisyFactorNode) != NULL)
                        {
                                factorType.basicType = noisyTypeError;
                        }
                }
        }
        else if (L(noisyFactorNode)->type == kNoisyIrNodeType_PnamegenInvokeShorthand)
        {
                /*
                *       We search for functionName in the top scope (local functions) and if it fails it searches on the module scope.
                */
                Symbol * functionNameSymbol = commonSymbolTableSymbolForIdentifier(N, N->noisyIrTopScope, LL(noisyFactorNode)->tokenString);   

                if (functionNameSymbol == NULL)
                {
                        char * details;

                        asprintf(&details, "Unknown namegen invocation \"%s\"\n",LL(noisyFactorNode)->tokenString);
                        noisySemanticError(N,noisyFactorNode,details);
                        noisySemanticErrorRecovery(N);        
                }

                bool paramCorrect = true;
                int paramCount = 0;

                IrNode * inputSignature = L(functionNameSymbol->typeTree);
                /*
                *       Check if inputSignature is nil. Else typeCheck every argument.
                */
                if (L(inputSignature)->type == kNoisyIrNodeType_Tnil)
                {
                        if (LR(noisyFactorNode) == NULL)
                        {
                                factorType.basicType = noisyNilType;        
                        }
                        else
                        {
                                char * details;
                                asprintf(&details, "Using arguments for nil function \"%s\"\n",functionNameSymbol->identifier);
                                noisySemanticError(N,noisyFactorNode,details);
                                noisySemanticErrorRecovery(N);
                        }
                }
                else
                {
                        for (IrNode * iter = LR(noisyFactorNode); iter != NULL; iter = RR(iter))
                        {
                                IrNode * argName = L(iter);
                                IrNode * expr = RL(iter);

                                paramCount++;
                                paramCorrect = paramCorrect && noisyArgumentMatchesSignature(N,argName,expr,inputSignature,currentScope);

                                if (!paramCorrect)
                                {
                                        char * details;
                                        asprintf(&details, "Argument error for argument \"%s\"\n",argName->tokenString);
                                        noisySemanticError(N,noisyFactorNode,details);
                                        noisySemanticErrorRecovery(N);
                                }

                        }

                        if (paramCount != functionNameSymbol->parameterNum)
                        {
                                char * details;
                                asprintf(&details, "Wrong number of arguments for function \"%s\"\n",functionNameSymbol->identifier);
                                noisySemanticError(N,noisyFactorNode,details);
                                noisySemanticErrorRecovery(N);
                        }
                }
                

                IrNode * outputSignature = R(functionNameSymbol->typeTree);

                /*
                *       TypeCheck output signature. The type returned is the return type of the function.
                */
                if (L(outputSignature)->type ==kNoisyIrNodeType_Tnil)
                {
                        factorType.basicType = noisyNilType;
                }
                else
                {
                        factorType = getNoisyTypeFromTypeExpr(N,RL(outputSignature));        
                }
                
        }
        else if (L(noisyFactorNode)->type == kNoisyIrNodeType_Pexpression)
        {
                factorType = getNoisyTypeFromExpression(N,L(noisyFactorNode),currentScope);
        }
        else if (L(noisyFactorNode)->type == kNoisyIrNodeType_PtypeMinExpr
                || L(noisyFactorNode)->type == kNoisyIrNodeType_PtypeMaxExpr)
        {
                factorType = getNoisyTypeFromBasicType(LL(noisyFactorNode));
        }
        else if (L(noisyFactorNode)->type == kNoisyIrNodeType_Tnil)
        {
                factorType.basicType = noisyNilType;
        }
        return factorType;
}

/*
*       TODO; Need to add boolean binary op.
*/
NoisyType
noisyUnaryOpTypeCheck(IrNode * noisyUnaryOpNode,NoisyType factorType)
{
        NoisyType returnType;

        if (L(noisyUnaryOpNode)->type == kNoisyIrNodeType_Tplus ||
            L(noisyUnaryOpNode)->type == kNoisyIrNodeType_Tminus)
        {
                switch (factorType.basicType)
                {
                case noisyBool:
                case noisyString:
                case noisyInitType:
                case noisyArrayType:
                case noisyTypeError:
                        /*
                        *       Unary operator and operand type mismatch.
                        */
                        returnType.basicType = noisyTypeError;
                        break;
                default:
                        returnType = factorType;
                        break;
                }
        }
        /*
        *       "~" and "<-" operator we do not type check so far.
        */
        else
        {
                returnType = factorType;
        }
        return returnType;
}

/*
*       Takes a noisyTerNode typechecks it and everything is correct it returns the NoisyType of the term.
*       TODO; Might need more work.
*/
NoisyType
getNoisyTypeFromTerm(State * N, IrNode * noisyTermNode, Scope * currentScope)
{
        NoisyType basicType,factorType, unaryType;
        IrNode * factorNode = NULL;
        IrNode * unaryOpNode = NULL;
        noisyInitNoisyType(&basicType);
        noisyInitNoisyType(&unaryType);


        if (L(noisyTermNode)->type == kNoisyIrNodeType_PbasicType)
        {
                basicType = getNoisyTypeFromBasicType(noisyTermNode->irLeftChild);
        }

        if (L(noisyTermNode)->type == kNoisyIrNodeType_Pfactor)
        {
                factorNode = L(noisyTermNode);
        }
        else if (RL(noisyTermNode)->type == kNoisyIrNodeType_Pfactor)
        {
                factorNode = RL(noisyTermNode);
                if (L(noisyTermNode)->type == kNoisyIrNodeType_PunaryOp)
                {
                        unaryOpNode = L(noisyTermNode);
                }
        }
        else if (RR(noisyTermNode)->type == kNoisyIrNodeType_Pfactor)
        {

                factorNode = RR(noisyTermNode);
                if (RL(noisyTermNode)->type == kNoisyIrNodeType_PunaryOp)
                {
                        unaryOpNode = RL(noisyTermNode);
                }
        }

        factorType = getNoisyTypeFromFactor(N,factorNode,currentScope);
        if (unaryOpNode != NULL)
        {
                unaryType = noisyUnaryOpTypeCheck(unaryOpNode,factorType);
        }

        if (unaryType.basicType != noisyInitType)
        {
                factorType = unaryType;
        }

        if (basicType.basicType != noisyInitType)
        {
                if (!noisyTypeEquals(basicType,factorType))
                {
                        factorType.basicType = noisyTypeError;
                }
        }

        if (R(noisyTermNode) != NULL)
        {
                if (RL(noisyTermNode)->type == kNoisyIrNodeType_TplusPlus || RL(noisyTermNode)->type == kNoisyIrNodeType_TminusMinus)
                {
                        factorNode = R(factorNode);
                }
        }

        /*
        *       TODO; Need to revisit typecheck on operators.
        */
        for (IrNode * iter = R(noisyTermNode); iter != NULL; iter = RR(iter))
        {
                NoisyType factorIterType;

                factorIterType = getNoisyTypeFromFactor(N,RL(iter),currentScope);

                if (!noisyTypeEquals(factorIterType,factorType))
                {
                        /*
                        *       Operands type mismatch.
                        */
                        char *	details;

                        asprintf(&details, "Operands type mismatch\n");
                        noisySemanticError(N,factorNode,details);
                        noisySemanticErrorRecovery(N);
                        break;
                }

                if (LL(iter)->type == kNoisyIrNodeType_Tasterisk
                || LL(iter)->type == kNoisyIrNodeType_Tdivide
                || LL(iter)->type == kNoisyIrNodeType_Tpercent
                || LL(iter)->type == kNoisyIrNodeType_TarithmeticAnd)
                {
                        if (!noisyIsOfType(factorType,noisyArithType))
                        {
                                /*
                                *       Operator and operand mismatch.
                                */
                               char *	details;

                                asprintf(&details, "Operator and operands type mismatch\n");
                                noisySemanticError(N,factorNode,details);
                                noisySemanticErrorRecovery(N);
                                break;

                        }
                }
                else if (LL(iter)->type == kNoisyIrNodeType_PhighPrecedenceBinaryBoolOp)
                {
                        NoisyType boolType = {noisyBool,0,0};
                        if (!noisyTypeEquals(factorType,boolType))
                        {
                                /*
                                *       Operator and operand mismatch.
                                */
                                char *	details;

                                asprintf(&details, "Operator and operands type mismatch\n");
                                noisySemanticError(N,factorNode,details);
                                noisySemanticErrorRecovery(N);
                        }
                }
                else
                {
                        /*
                        *       Unsupported operators
                        */
                        char *	details;

                        asprintf(&details, "Unsupported binary operator\n");
                        noisySemanticError(N,factorNode,details);
                        noisySemanticErrorRecovery(N);
                }
                deallocateNoisyType(&factorIterType);

        }

        return factorType;
}

/*
*       TODO; Might not be completed.
*/
NoisyType
getNoisyTypeFromExpression(State * N, IrNode * noisyExpressionNode, Scope * currentScope)
{
        NoisyType typ1,returnType;
        noisyInitNoisyType(&returnType);

        if (L(noisyExpressionNode)->type == kNoisyIrNodeType_Pterm)
        {
                typ1 = getNoisyTypeFromTerm(N,L(noisyExpressionNode), currentScope);

                returnType = typ1;

                for (IrNode * iter = R(noisyExpressionNode); iter != NULL; iter = RR(iter))
                {
                        IrNode * operatorNode = L(iter);
                        IrNode * termNode = RL(iter);

                        NoisyType termTyp = getNoisyTypeFromTerm(N,termNode,currentScope);

                        if (noisyTypeEquals(returnType,termTyp))
                        {
                                switch (L(operatorNode)->type)
                                {
                                case kNoisyIrNodeType_Tplus:
                                case kNoisyIrNodeType_Tminus:
                                case kNoisyIrNodeType_TrightShift:
                                case kNoisyIrNodeType_TleftShift:
                                case kNoisyIrNodeType_TbitwiseOr:
                                        if (!noisyIsOfType(returnType,noisyArithType))
                                        {
                                                char *	details;

                                                asprintf(&details, "Operator \"%s\" and operands type mismatch\n",L(operatorNode)->tokenString);
                                                noisySemanticError(N,L(noisyExpressionNode),details);
                                                noisySemanticErrorRecovery(N);        
                                        }
                                        /*
                                        *       returnType = typ1;
                                        */
                                        break;
                                case kNoisyIrNodeType_PcmpOp:
                                        if (LL(operatorNode)->type == kNoisyIrNodeType_Tequals
                                        || LL(operatorNode)->type == kNoisyIrNodeType_TnotEqual)
                                        {
                                                if (returnType.basicType == noisyArrayType)
                                                {
                                                        char *	details;

                                                        asprintf(&details, "Operator \"%s\" and operands type mismatch\n",LL(operatorNode)->tokenString);
                                                        noisySemanticError(N,L(noisyExpressionNode),details);
                                                        noisySemanticErrorRecovery(N);
                                                }
                                        }
                                        else if (LL(operatorNode)->type == kNoisyIrNodeType_TgreaterThan
                                                || LL(operatorNode)-> type == kNoisyIrNodeType_TgreaterThanEqual
                                                || LL(operatorNode)-> type == kNoisyIrNodeType_TlessThan
                                                || LL(operatorNode)-> type == kNoisyIrNodeType_TlessThanEqual)
                                        {
                                                if (noisyIsOfType(returnType,noisyArithType))
                                                {
                                                        returnType.basicType = noisyBool; 
                                                }
                                                else
                                                {
                                                        char *	details;

                                                        asprintf(&details, "Operator \"%s\" and operands type mismatch\n",LL(operatorNode)->tokenString);
                                                        noisySemanticError(N,L(noisyExpressionNode),details);
                                                        noisySemanticErrorRecovery(N);  
                                                }
                                                       
                                        }
                                        else
                                        {
                                                char *	details;

                                                asprintf(&details, "Unsupported CmpOp \"%s\"\n",L(operatorNode)->tokenString);
                                                noisySemanticError(N,L(noisyExpressionNode),details);
                                                noisySemanticErrorRecovery(N);
                                        }
                                        break;
                                case kNoisyIrNodeType_PlowPrecedenceBinaryBoolOp:
                                        if (returnType.basicType != noisyBool)
                                        {
                                                char *	details;

                                                        asprintf(&details, "Operator \"%s\" and operands type mismatch\n",L(operatorNode)->tokenString);
                                                        noisySemanticError(N,L(noisyExpressionNode),details);
                                                        noisySemanticErrorRecovery(N);
                                        }
                                        break;
                                default:
                                        break;
                                }
                        }
                        else
                        {
                                /*
                                *       Operands type mismatch error.
                                */
                                char *	details;

                                asprintf(&details, "Operands type mismatch\n");
                                noisySemanticError(N,L(noisyExpressionNode),details);
                                noisySemanticErrorRecovery(N);
                        }
                }
        }

        return returnType;
}

/*
*       Checks if the function is typeComplete and also counts its parameters as preparation
*       for the code generation.
*/
void
noisyDeclareFunctionTypeCheck(State * N, const char * functionName,IrNode * inputSignature, IrNode * outputSignature,Scope * currentScope)
{
        Symbol * functionSymbol = commonSymbolTableSymbolForIdentifier(N, currentScope, functionName);

        int parameterCount = 0;

        if (L(inputSignature)->type != kNoisyIrNodeType_Tnil)
        {
                for  (IrNode * iter = inputSignature; iter != NULL; iter = RR(iter))
                {
                        parameterCount++;

                        functionSymbol->isTypeComplete = isTypeExprComplete(N,RL(iter));
                        
                }
                /*
                *       We need to save parameterCount so we can allocate memory for the
                *       parameters of the generated function.
                */
        }
        /*
        *       If type == nil then parameterCount = 0
        */

        functionSymbol->parameterNum = parameterCount;

        functionSymbol->isTypeComplete = functionSymbol->isTypeComplete && isTypeExprComplete(N,RL(outputSignature));
}


void
noisyModuleTypeNameDeclTypeCheck(State * N, IrNode * noisyModuleTypeNameDeclNode, Scope * currentScope)
{
        /*
        *       We do not need to typecheck constant definitions.
        */
        if (R(noisyModuleTypeNameDeclNode)->type == kNoisyIrNodeType_PtypeDecl 
                || R(noisyModuleTypeNameDeclNode)->type == kNoisyIrNodeType_PtypeAnnoteDecl )
        {
                /*
                *       Probably we need to type check if type annotations of units and signals match
                *       through Newton API.
                */
                return ;
        }
        else if (R(noisyModuleTypeNameDeclNode)->type == kNoisyIrNodeType_PfunctionDecl)
        {
                IrNode * inputSignature = RLL(noisyModuleTypeNameDeclNode);
                IrNode * outputSignature = RRL(noisyModuleTypeNameDeclNode);
                noisyDeclareFunctionTypeCheck(N,L(noisyModuleTypeNameDeclNode)->tokenString,inputSignature,outputSignature,currentScope);
        }
}

void
noisyModuleDeclBodyTypeCheck(State * N, IrNode * noisyModuleDeclBodyNode,Scope * currentScope)
{
        for (IrNode * currentNode = noisyModuleDeclBodyNode; currentNode != NULL; currentNode = currentNode->irRightChild)
        {
                noisyModuleTypeNameDeclTypeCheck(N, currentNode->irLeftChild,currentScope);
        }
}


/*
*       We need this for every noisyType that we return so we can deallocate its sizeOfDimension array.
*       Otherwise we create garbage. Unfortunately we need accompany each final call of getNoisyType
*       with that function.
*/
void
deallocateNoisyType(NoisyType * typ)
{
        if (typ->basicType == noisyArrayType)
        {
                free(typ->sizeOfDimension);
        }
        return ;
}

bool
noisyMatchTypeExpr(State * N,IrNode * typeExpr1, IrNode * typeExpr2)
{
        NoisyType typ1, typ2;
        typ1 = getNoisyTypeFromTypeExpr(N,typeExpr1);
        typ2 = getNoisyTypeFromTypeExpr(N,typeExpr2);
        bool res = noisyTypeEquals(typ1,typ2);
        deallocateNoisyType(&typ1);
        deallocateNoisyType(&typ2);
        return res;
}

bool
noisySignatureIsMatching(State * N, IrNode * definitionSignature, IrNode * declarationSignature)
{
        if (L(definitionSignature)-> type == kNoisyIrNodeType_Tnil || L(declarationSignature)->type == kNoisyIrNodeType_Tnil)
        {
                if (L(definitionSignature)-> type == L(declarationSignature)-> type)
                {
                        return true;
                }
                else
                {
                        return false;
                }
        }
        else
        {
                IrNode * declIter = declarationSignature;
                for (IrNode * iter = definitionSignature ; iter != NULL; iter = RR(iter))
                {
                        /*
                        *       If one signature has less members than the other.
                        */
                        if (declIter == NULL)
                        {
                                return false;
                        }

                        if (strcmp(L(iter)->tokenString,L(declIter)->tokenString))
                        {
                                return false;
                        }                      

                        if (!noisyMatchTypeExpr(N,RL(iter),RL(declIter)))
                        {
                                return false;
                        }

                        declIter = RR(declIter);
                }

                /*
                *       If one signature has more members than the other.
                */
                if (declIter != NULL)
                {
                        return false;
                }
        }

        return true;
}

/*
*       TODO; Not completed.
*/
void
noisyAssignmentStatementTypeCheck(State * N, IrNode * noisyAssignmentStatementNode, Scope * currentScope)
{
        /*
        *       If type is xseq it means that noisyAssignmentNode is not a declaration but an actual assignment.
        */
        if (R(noisyAssignmentStatementNode)->type == kNoisyIrNodeType_Xseq)
        {
                NoisyType lValuetype, rValueType;
                rValueType = getNoisyTypeFromExpression(N,RRL(noisyAssignmentStatementNode),currentScope);
                for (IrNode * iter = L(noisyAssignmentStatementNode); iter != NULL; iter = R(iter))
                {
                        if (LL(iter)->type == kNoisyIrNodeType_Tnil)
                        {
                                /*
                                *       We do not need to type check lValueType of an assignment to nil.
                                */
                        }
                        else if (LL(iter)->type == kNoisyIrNodeType_PqualifiedIdentifier)
                        {
                                lValuetype = getNoisyTypeFromTypeExpr(N,LLL(iter)->symbol->typeTree);
                                if (!noisyTypeEquals(lValuetype,rValueType))
                                {
                                        char *	details;

                                        asprintf(&details, "Type mismatch on assignment\n");
                                        noisySemanticError(N,LL(iter),details);
                                        noisySemanticErrorRecovery(N);
                                }
                        }
                }
        }
}

void
noisyStatementTypeCheck(State * N, IrNode * noisyStatementNode, Scope * currentScope)
{
        switch (L(noisyStatementNode)->type)
        {
        case kNoisyIrNodeType_PassignmentStatement:
                noisyAssignmentStatementTypeCheck(N,L(noisyStatementNode), currentScope);
                break;
        // case kNoisyIrNodeType_PmatchStatement:
        //         noisyMatchStatementTypeCheck(N,S,L(noisyStatementNode));
        //         break;
        // case kNoisyIrNodeType_PiterateStatement:
        //         noisyIterateStatementTypeCheck(N,S,L(noisyStatementNode));
        //         break;
        // case kNoisyIrNodeType_PsequenceStatement:
        //         noisySequenceStatementTypeCheck(N,S,L(noisyStatementNode));
        //         break;
        // case kNoisyIrNodeType_PscopedStatementList:
        //         noisyStatementListTypeCheck(N,S,LL(noisyStatementNode));
        //         break;
        // case kNoisyIrNodeType_PoperatorToleranceDecl:
        //         noisyOperatorToleranceDeclTypeCheck(N,S,L(noisyStatementNode));
        //         break;
        // case kNoisyIrNodeType_PreturnStatement:
        //         noisyReturnStatementTypeCheck(N,S,L(noisyStatementNode));
        //         break;
        default:
                // flexprint(N->Fe, N->Fm, N->Fperr, "Code generation for that statement is not supported");
                // fatal(N,"Code generation Error\n");
                break;
        }
}

void
noisyStatementListTypeCheck(State * N, IrNode * statementListNode, Scope * currentScope)
{
        for (IrNode * iter = statementListNode; iter != NULL; iter=R(iter))
        {
                if (L(iter) != NULL)
                {
                        noisyStatementTypeCheck(N,L(iter),currentScope);
                }
        }
}


void
noisyFunctionDefnTypeCheck(State * N,IrNode * noisyFunctionDefnNode,Scope * currentScope)
{
        Symbol * functionSymbol = commonSymbolTableSymbolForIdentifier(N, N->moduleScopes, noisyFunctionDefnNode->irLeftChild->tokenString);

        noisySignatureIsMatching(N, RL(noisyFunctionDefnNode),L(functionSymbol->typeTree));
        noisySignatureIsMatching(N, RRL(noisyFunctionDefnNode),R(functionSymbol->typeTree));        

        /*
        *       For local functions we need to typeCheck their declaration.
        *       We cannot skip this step because this function initializes parameterNum and isComplete variables
        *       for function symbols.
        *       If we have a local definition of a function its scope is the TopScope.
        */

        if (functionSymbol->scope == N->noisyIrTopScope)
        {
                noisyDeclareFunctionTypeCheck(N,noisyFunctionDefnNode->irLeftChild->tokenString,RL(noisyFunctionDefnNode),RRL(noisyFunctionDefnNode),currentScope);
        }

        noisyStatementListTypeCheck(N,RR(noisyFunctionDefnNode)->irRightChild->irLeftChild,commonSymbolTableGetScopeWithName(N,currentScope,functionSymbol->identifier));
}

void
noisyModuleDeclTypeCheck(State * N, IrNode * noisyModuleDeclNode,Scope * currentScope)
{
        /*
        *       We do not need to typecheck module parameters.
        */
        noisyModuleDeclBodyTypeCheck(N, RR(noisyModuleDeclNode),currentScope);
}

void
noisyProgramTypeCheck(State * N,IrNode * noisyProgramNode,Scope * currentScope)
{
        for (IrNode * currentNode = noisyProgramNode; currentNode != NULL; currentNode = currentNode->irRightChild)
        {
                if (currentNode->irLeftChild->type == kNoisyIrNodeType_PmoduleDecl)
                {
                        noisyModuleDeclTypeCheck(N, currentNode->irLeftChild,N->moduleScopes);
                }
                else if (currentNode->irLeftChild->type == kNoisyIrNodeType_PfunctionDefn)
                {
                        noisyFunctionDefnTypeCheck(N,currentNode->irLeftChild,currentScope->firstChild);
                }
        }
}

void
noisyTypeCheck(State * N)
{
        noisyProgramTypeCheck(N,N->noisyIrRoot,N->noisyIrTopScope);
}
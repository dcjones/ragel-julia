/*
 *  Copyright 2001-2006 Adrian Thurston <thurston@complang.org>
 *            2004 Erich Ocean <eric.ocean@ampede.com>
 *            2005 Alan West <alan@alanz.com>
 */

/*  This file is part of Ragel.
 *
 *  Ragel is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Ragel is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Ragel; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "codegen.h"
#include "ragel.h"
#include "redfsm.h"
#include "gendata.h"
#include <sstream>
#include <string>
#include <assert.h>


using std::ostream;
using std::ostringstream;
using std::string;
using std::cerr;
using std::endl;
using std::istream;
using std::ifstream;
using std::ostream;
using std::ios;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;

extern int numSplitPartitions;
extern bool noLineDirectives;

#define S8BIT_MIN  -128
#define S8BIT_MAX  127

#define S16BIT_MIN -32768
#define S16BIT_MAX 32767

#define S32BIT_MIN -2147483648l
#define S32BIT_MAX 2147483647l

#define S64BIT_MIN -9223372036854775807LL
#define S64BIT_MAX 9223372036854775807LL

void juliaLineDirective( ostream &out, const char *fileName, int line )
{
    /* Write the line info for to the input file. */
    out << "// line " << line  << " \"";
    for ( const char *pc = fileName; *pc != 0; pc++ ) {
        if ( *pc == '\\' )
            out << "\\\\";
        else
            out << *pc;
    }
    out << '"';

    out << '\n';
}

namespace Julia {

TableArray::TableArray( const char *name, CodeGen &codeGen, bool isIndex )
:
    state(InitialState),
    name(name),
    type("_"),
    width(0),
    isSigned(true),
    values(0),
    min(LLONG_MAX),
    max(LLONG_MIN),
    isIndex(isIndex),
    codeGen(codeGen),
    out(codeGen.out),
    valuesGenerated(0)
{
    codeGen.arrayVector.append( this );
}

std::string TableArray::ref() const
{
    return string("_") + codeGen.DATA_PREFIX() + name;
}

long long TableArray::size()
{
    return width * values;
}

void TableArray::startAnalyze()
{
}

void TableArray::valueAnalyze( long long v )
{
    values += 1;
    if (isIndex) ++v;
    if ( v < min )
        min = v;
    if ( v > max )
        max = v;
}

void TableArray::finishAnalyze()
{
    /* Calculate the type if it is not already set. */
    if ( type == "_" ) {
        if ( min >= S8BIT_MIN && max <= S8BIT_MAX ) {
            type = "Int8";
            width = 1;
        }
        else if ( min >= S16BIT_MIN && max <= S16BIT_MAX ) {
            type = "Int16";
            width = 2;
        }
        else if ( min >= S32BIT_MIN && max <= S32BIT_MAX ) {
            type = "Int32";
            width = 4;
        }
        else if ( min >= S64BIT_MIN && max <= S64BIT_MAX ) {
            type = "Int64";
            width = 8;
        }
        else  {
            throw "Can't fit table value in 64 unsigned integer";
        }
    }
}

void TableArray::startGenerate()
{
    out << "const " <<
        "_" << codeGen.DATA_PREFIX() << name << " = " << type << "[\n\t";
}

void TableArray::valueGenerate( long long v )
{
    if ( ++valuesGenerated % 8 == 0 ) out << "\n";
    out << (isIndex ? v + 1 : v);
    out << ", ";
}

void TableArray::finishGenerate()
{
    out << "0 ]\n\n";
}

void TableArray::start()
{
    switch ( state ) {
        case InitialState:
            break;
        case AnalyzePass:
            startAnalyze();
            break;
        case GeneratePass:
            startGenerate();
            break;
    }
}

void TableArray::value( long long v )
{
    switch ( state ) {
        case InitialState:
            break;
        case AnalyzePass:
            valueAnalyze( v );
            break;
        case GeneratePass:
            valueGenerate( v );
            break;
    }
}

void TableArray::finish()
{
    switch ( state ) {
        case InitialState:
            break;
        case AnalyzePass:
            finishAnalyze();
            break;
        case GeneratePass:
            finishGenerate();
            break;
    }
}

void CodeGen::genLineDirective( ostream &out )
{
    std::streambuf *sbuf = out.rdbuf();
    output_filter *filter = static_cast<output_filter*>(sbuf);
    juliaLineDirective( out, filter->fileName, filter->line + 1 );
}

/* Init code gen with in parameters. */
CodeGen::CodeGen( const CodeGenArgs &args )
:
    CodeGenData(args)
{
}

unsigned int CodeGen::arrayTypeSize( unsigned long maxVal )
{
    long long maxValLL = (long long) maxVal;
    HostType *arrayType = keyOps->typeSubsumes( maxValLL );
    assert( arrayType != 0 );
    return arrayType->size;
}

string CodeGen::ARRAY_TYPE( unsigned long maxVal )
{
    long long maxValLL = (long long) maxVal;
    HostType *arrayType = keyOps->typeSubsumes( maxValLL );
    assert( arrayType != 0 );

    string ret = arrayType->data1;
    if ( arrayType->data2 != 0 ) {
        ret += " ";
        ret += arrayType->data2;
    }
    return ret;
}


/* Write out the fsm name. */
string CodeGen::FSM_NAME()
{
    return fsmName;
}

/* Emit the offset of the start state as a decimal integer. */
string CodeGen::START_STATE_ID()
{
    ostringstream ret;
    ret << redFsm->startState->id;
    return ret.str();
};


string CodeGen::ACCESS()
{
    ostringstream ret;
    if ( accessExpr != 0 )
        INLINE_LIST( ret, accessExpr, 0, false, false );
    return ret.str();
}

string CodeGen::DATA()
{
    ostringstream ret;
    if ( dataExpr == 0 )
        ret << ACCESS() + "data";
    else {
        ret << "(";
        INLINE_LIST( ret, dataExpr, 0, false, false );
        ret << ")";
    }
    return ret.str();
}


string CodeGen::P()
{
    ostringstream ret;
    if ( pExpr == 0 )
        ret << "p";
    else {
        ret << "(";
        INLINE_LIST( ret, pExpr, 0, false, false );
        ret << ")";
    }
    return ret.str();
}

string CodeGen::PE()
{
    ostringstream ret;
    if ( peExpr == 0 )
        ret << "pe";
    else {
        ret << "(";
        INLINE_LIST( ret, peExpr, 0, false, false );
        ret << ")";
    }
    return ret.str();
}

string CodeGen::vEOF()
{
    ostringstream ret;
    if ( eofExpr == 0 )
        ret << "eof";
    else {
        ret << "(";
        INLINE_LIST( ret, eofExpr, 0, false, false );
        ret << ")";
    }
    return ret.str();
}

string CodeGen::vCS()
{
    ostringstream ret;
    if ( csExpr == 0 )
        ret << ACCESS() << "cs";
    else {
        /* Emit the user supplied method of retrieving the key. */
        ret << "(";
        INLINE_LIST( ret, csExpr, 0, false, false );
        ret << ")";
    }
    return ret.str();
}

string CodeGen::TOP()
{
    ostringstream ret;
    if ( topExpr == 0 )
        ret << ACCESS() + "top";
    else {
        ret << "(";
        INLINE_LIST( ret, topExpr, 0, false, false );
        ret << ")";
    }
    return ret.str();
}

string CodeGen::STACK()
{
    ostringstream ret;
    if ( stackExpr == 0 )
        ret << ACCESS() + "stack";
    else {
        ret << "(";
        INLINE_LIST( ret, stackExpr, 0, false, false );
        ret << ")";
    }
    return ret.str();
}

string CodeGen::ACT()
{
    ostringstream ret;
    if ( actExpr == 0 )
        ret << ACCESS() + "act";
    else {
        ret << "(";
        INLINE_LIST( ret, actExpr, 0, false, false );
        ret << ")";
    }
    return ret.str();
}

string CodeGen::TOKSTART()
{
    ostringstream ret;
    if ( tokstartExpr == 0 )
        ret << ACCESS() + "ts";
    else {
        ret << "(";
        INLINE_LIST( ret, tokstartExpr, 0, false, false );
        ret << ")";
    }
    return ret.str();
}

string CodeGen::TOKEND()
{
    ostringstream ret;
    if ( tokendExpr == 0 )
        ret << ACCESS() + "te";
    else {
        ret << "(";
        INLINE_LIST( ret, tokendExpr, 0, false, false );
        ret << ")";
    }
    return ret.str();
}

string CodeGen::GET_KEY()
{
    ostringstream ret;
    if ( getKeyExpr != 0 ) {
        /* Emit the user supplied method of retrieving the key. */
        ret << "(";
        INLINE_LIST( ret, getKeyExpr, 0, false, false );
        ret << ")";
    }
    else {
        /* Expression for retrieving the key, use simple dereference. */
        ret << DATA() << "[" << P() << "]";
    }
    return ret.str();
}

/* Write out level number of tabs. Makes the nested binary search nice
 * looking. */
string CodeGen::TABS( int level )
{
    string result;
    while ( level-- > 0 )
        result += "    ";
    return result;
}

/* Write out a key from the fsm code gen. Depends on wether or not the key is
 * signed. */
string CodeGen::KEY( Key key )
{
    ostringstream ret;
    if ( keyOps->isSigned || !hostLang->explicitUnsigned )
        ret << key.getVal();
    else
        ret << (unsigned long) key.getVal();
    return ret.str();
}

bool CodeGen::isAlphTypeSigned()
{
    return keyOps->isSigned;
}

bool CodeGen::isWideAlphTypeSigned()
{
    string ret;
    if ( keyOps->le( redFsm->maxKey, keyOps->maxKey ) )
        return isAlphTypeSigned();
    else {
        long long maxKeyVal = keyOps->getLongLong( redFsm->maxKey );
        HostType *wideType = keyOps->typeSubsumes( keyOps->isSigned, maxKeyVal );
        return wideType->isSigned;
    }
}

void CodeGen::EXEC( ostream &ret, GenInlineItem *item, int targState, int inFinish )
{
    /* The parser gives fexec two children. The double brackets are for D
     * code. If the inline list is a single word it will get interpreted as a
     * C-style cast by the D compiler. */
    ret << "begin " << P() << " = (";
    INLINE_LIST( ret, item->children, targState, inFinish, false );
    ret << ") - 1; end\n}\n";
}

void CodeGen::LM_SWITCH( ostream &ret, GenInlineItem *item,
        int targState, int inFinish, bool csForced )
{
    int i = 0;
    for ( GenInlineList::Iter lma = *item->children; lma.lte(); lma++ ) {
        if ( lma->lmId < 0 ) {
            ret << "	else\n";
        }
        else {
            ret << i == 0 ? "	if " : "	elseif ";
        }

        ret << ACT() << " == " << lma->lmId << "\n";
        INLINE_LIST( ret, lma->children, targState, inFinish, csForced );
        ret << "\n";
    }

    ret << "	end\n\n";
}

void CodeGen::SET_ACT( ostream &ret, GenInlineItem *item )
{
    ret << ACT() << " = " << item->lmId << "\n";
}

void CodeGen::SET_TOKEND( ostream &ret, GenInlineItem *item )
{
    /* The tokend action sets tokend. */
    ret << TOKEND() << " = " << P();
    if ( item->offset != 0 )
        out << "+" << item->offset;
    out << "\n";
}

void CodeGen::GET_TOKEND( ostream &ret, GenInlineItem *item )
{
    ret << TOKEND();
}

void CodeGen::INIT_TOKSTART( ostream &ret, GenInlineItem *item )
{
    ret << TOKSTART() << " = 0\n";
}

void CodeGen::INIT_ACT( ostream &ret, GenInlineItem *item )
{
    ret << ACT() << " = 0\n";
}

void CodeGen::SET_TOKSTART( ostream &ret, GenInlineItem *item )
{
    ret << TOKSTART() << " = " << P() << "\n";
}

void CodeGen::SUB_ACTION( ostream &ret, GenInlineItem *item,
        int targState, bool inFinish, bool csForced )
{
    if ( item->children->length() > 0 ) {
        /* Write the block and close it off. */
        ret << "begin ";
        INLINE_LIST( ret, item->children, targState, inFinish, csForced );
        ret << " end\n";
    }
}


/* Write out an inline tree structure. Walks the list and possibly calls out
 * to virtual functions than handle language specific items in the tree. */
void CodeGen::INLINE_LIST( ostream &ret, GenInlineList *inlineList,
        int targState, bool inFinish, bool csForced )
{
    for ( GenInlineList::Iter item = *inlineList; item.lte(); item++ ) {
        switch ( item->type ) {
        case GenInlineItem::Text:
            ret << item->data;
            break;
        case GenInlineItem::Goto:
            GOTO( ret, item->targState->id, inFinish );
            break;
        case GenInlineItem::Call:
            CALL( ret, item->targState->id, targState, inFinish );
            break;
        case GenInlineItem::Next:
            NEXT( ret, item->targState->id, inFinish );
            break;
        case GenInlineItem::Ret:
            RET( ret, inFinish );
            break;
        case GenInlineItem::PChar:
            ret << P();
            break;
        case GenInlineItem::Char:
            ret << GET_KEY();
            break;
        case GenInlineItem::Hold:
            ret << P() << "--\n";
            break;
        case GenInlineItem::Exec:
            EXEC( ret, item, targState, inFinish );
            break;
        case GenInlineItem::Curs:
            CURS( ret, inFinish );
            break;
        case GenInlineItem::Targs:
            TARGS( ret, inFinish, targState );
            break;
        case GenInlineItem::Entry:
            ret << item->targState->id;
            break;
        case GenInlineItem::GotoExpr:
            GOTO_EXPR( ret, item, inFinish );
            break;
        case GenInlineItem::CallExpr:
            CALL_EXPR( ret, item, targState, inFinish );
            break;
        case GenInlineItem::NextExpr:
            NEXT_EXPR( ret, item, inFinish );
            break;
        case GenInlineItem::LmSwitch:
            LM_SWITCH( ret, item, targState, inFinish, csForced );
            break;
        case GenInlineItem::LmSetActId:
            SET_ACT( ret, item );
            break;
        case GenInlineItem::LmSetTokEnd:
            SET_TOKEND( ret, item );
            break;
        case GenInlineItem::LmGetTokEnd:
            GET_TOKEND( ret, item );
            break;
        case GenInlineItem::LmInitTokStart:
            INIT_TOKSTART( ret, item );
            break;
        case GenInlineItem::LmInitAct:
            INIT_ACT( ret, item );
            break;
        case GenInlineItem::LmSetTokStart:
            SET_TOKSTART( ret, item );
            break;
        case GenInlineItem::SubAction:
            SUB_ACTION( ret, item, targState, inFinish, csForced );
            break;
        case GenInlineItem::Break:
            BREAK( ret, targState, csForced );
            break;
        }
    }
}
/* Write out paths in line directives. Escapes any special characters. */
string CodeGen::LDIR_PATH( char *path )
{
    ostringstream ret;
    for ( char *pc = path; *pc != 0; pc++ ) {
        if ( *pc == '\\' )
            ret << "\\\\";
        else
            ret << *pc;
    }
    return ret.str();
}

void CodeGen::ACTION( ostream &ret, GenAction *action, int targState,
        bool inFinish, bool csForced )
{
    /* Write the preprocessor line info for going into the source file. */
    juliaLineDirective( ret, action->loc.fileName, action->loc.line );

    /* Write the block and close it off. */
    ret << "\tbegin\n";
    INLINE_LIST( ret, action->inlineList, targState, inFinish, csForced );
    ret << "end\n";
}

void CodeGen::CONDITION( ostream &ret, GenAction *condition )
{
    ret << "\n";
    juliaLineDirective( ret, condition->loc.fileName, condition->loc.line );
    INLINE_LIST( ret, condition->inlineList, 0, false, false );
}

string CodeGen::ERROR_STATE()
{
    ostringstream ret;
    if ( redFsm->errState != 0 )
        ret << redFsm->errState->id;
    else
        ret << "-1";
    return ret.str();
}

string CodeGen::FIRST_FINAL_STATE()
{
    ostringstream ret;
    if ( redFsm->firstFinState != 0 )
        ret << redFsm->firstFinState->id;
    else
        ret << redFsm->nextStateId;
    return ret.str();
}

// TODO: resume work here

void CodeGen::writeInit()
{
    out << "begin\n";

    out << "\t" << P() << " = 0\n";

    if ( !noEnd ) {
        out << "\t" << PE() << " = length(data)\n";
    }

    if ( !noCS ) {
        out << "\t" << vCS() << " = " << START() << "\n";
    }

    /* If there are any calls, then the stack top needs initialization. */
    if ( redFsm->anyActionCalls() || redFsm->anyActionRets() )
        out << "\t" << TOP() << " = 0\n";

    if ( hasLongestMatch ) {
        out <<
            "\t" << TOKSTART() << " = 0\n"
            "\t" << TOKEND() << " = 0\n";

        if ( redFsm->usingAct() ) {
            out << "\t" << ACT() << " = 0\n";
        }
    }
    out << "end\n";
}

string CodeGen::DATA_PREFIX()
{
    if ( !noPrefix )
        return FSM_NAME() + "_";
    return "";
}

/* Emit the alphabet data type. */
string CodeGen::ALPH_TYPE()
{
    string ret = keyOps->alphType->data1;
    if ( keyOps->alphType->data2 != 0 ) {
        ret += " ";
        ret += + keyOps->alphType->data2;
    }
    return ret;
}

void CodeGen::STATE_IDS()
{
    if ( redFsm->startState != 0 )
        out << "const " << START() << " = " << START_STATE_ID() << ";\n";

    if ( !noFinal )
        out << "const " << FIRST_FINAL() << " = " << FIRST_FINAL_STATE() << ";\n";

    if ( !noError )
        out << "const " << ERROR() << " = " << ERROR_STATE() << ";\n";

    out << "\n";

    if ( entryPointNames.length() > 0 ) {
        for ( EntryNameVect::Iter en = entryPointNames; en.lte(); en++ ) {
            out << "const " << DATA_PREFIX() + "en_" + *en <<
                    " = " << entryPointIds[en.pos()] << "\n";
        }
        out << "\n";
    }
}

void CodeGen::writeStart()
{
    out << START_STATE_ID();
}

void CodeGen::writeFirstFinal()
{
    out << FIRST_FINAL_STATE();
}

void CodeGen::writeError()
{
    out << ERROR_STATE();
}

void CodeGen::writeExports()
{
	throw "unreachable code";
    if ( exportList.length() > 0 ) {
        for ( ExportList::Iter ex = exportList; ex.lte(); ex++ ) {
            out << "#define " << DATA_PREFIX() << "ex_" << ex->name << " " <<
                    KEY(ex->key) << "\n";
        }
        out << "\n";
    }
}

ostream &CodeGen::source_warning( const InputLoc &loc )
{
    cerr << sourceFileName << ":" << loc.line << ":" << loc.col << ": warning: ";
    return cerr;
}

ostream &CodeGen::source_error( const InputLoc &loc )
{
    gblErrorCount += 1;
    assert( sourceFileName != 0 );
    cerr << sourceFileName << ":" << loc.line << ":" << loc.col << ": ";
    return cerr;
}

}

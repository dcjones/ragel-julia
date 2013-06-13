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

#include "ragel.h"
#include "binary.h"
#include "redfsm.h"
#include "gendata.h"

#include <assert.h>

namespace Julia {

Binary::Binary( const CodeGenArgs &args )
:
    CodeGen( args ),
    keyOffsets(         "key_offsets",           *this, true ),
    singleLens(         "single_lengths",        *this, false ),
    rangeLens(          "range_lengths",         *this, false ),
    indexOffsets(       "index_offsets",         *this, true ),
    indicies(           "indicies",              *this, true ),
    transCondSpacesWi(  "trans_cond_spaces_wi",  *this, false ),
    transOffsetsWi(     "trans_offsets_wi",      *this, false ),
    transLengthsWi(     "trans_lengths_wi",      *this, false ),
    transCondSpaces(    "trans_cond_spaces",     *this, false ),
    transOffsets(       "trans_offsets",         *this, true ),
    transLengths(       "trans_lengths",         *this, false ),
    condTargs(          "cond_targs",            *this, false ),
    condActions(        "cond_actions",          *this, false ),
    toStateActions(     "to_state_actions",      *this, false ),
    fromStateActions(   "from_state_actions",    *this, false ),
    eofActions(         "eof_actions",           *this, false ),
    eofTransDirect(     "eof_trans_direct",      *this, false ),
    eofTransIndexed(    "eof_trans_indexed",     *this, false ),
    actions(            "actions",               *this, false ),
    keys(               "trans_keys",            *this, false ),
    condKeys(           "cond_keys",             *this, false )
{
}

void Binary::setKeyType()
{
    keys.setType( ALPH_TYPE(), keyOps->alphType->size );
    keys.isSigned = keyOps->isSigned;
}

void Binary::setTableState( TableArray::State state )
{
    for ( ArrayVector::Iter i = arrayVector; i.lte(); i++ ) {
        TableArray *tableArray = *i;
        tableArray->setState( state );
    }
}

void Binary::taKeyOffsets()
{
    keyOffsets.start();

    int curKeyOffset = 0;
    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        keyOffsets.value( curKeyOffset );
        curKeyOffset += st->outSingle.length() + st->outRange.length()*2;
    }

    keyOffsets.finish();
}


void Binary::taSingleLens()
{
    singleLens.start();

    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ )
        singleLens.value( st->outSingle.length() );

    singleLens.finish();
}


void Binary::taRangeLens()
{
    rangeLens.start();

    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ )
        rangeLens.value( st->outRange.length() );

    rangeLens.finish();
}

void Binary::taIndexOffsets()
{
    indexOffsets.start();

    int curIndOffset = 0;

    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        /* Write the index offset. */
        indexOffsets.value( curIndOffset );

        /* Move the index offset ahead. */
        curIndOffset += st->outSingle.length() + st->outRange.length();
        if ( st->defTrans != 0 )
            curIndOffset += 1;
    }

    indexOffsets.finish();
}

void Binary::taToStateActions()
{
    toStateActions.start();

    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ )
        TO_STATE_ACTION(st);

    toStateActions.finish();
}

void Binary::taFromStateActions()
{
    fromStateActions.start();

    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ )
        FROM_STATE_ACTION(st);

    fromStateActions.finish();
}

void Binary::taEofActions()
{
    eofActions.start();

    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ )
        EOF_ACTION( st );

    eofActions.finish();
}

void Binary::taEofTransDirect()
{
    eofTransDirect.start();

    /* Need to compute transition positions. */
    int totalTrans = 0;
    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        totalTrans += st->outSingle.length();
        totalTrans += st->outRange.length();
        if ( st->defTrans != 0 )
            totalTrans += 1;
    }

    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        long trans = 0;
        if ( st->eofTrans != 0 ) {
            trans = totalTrans + 1;
            totalTrans += 1;
        }

        eofTransDirect.value( trans );
    }

    eofTransDirect.finish();
}

void Binary::taEofTransIndexed()
{
    /* Transitions must be written ordered by their id. */
    long t = 0, *transPos = new long[redFsm->transSet.length()];
    for ( TransApSet::Iter trans = redFsm->transSet; trans.lte(); trans++ )
        transPos[trans->id] = t++;

    eofTransIndexed.start();

    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        long trans = 0;
        if ( st->eofTrans != 0 )
            trans = transPos[st->eofTrans->id] + 1;

        eofTransIndexed.value( trans );
    }

    eofTransIndexed.finish();

    delete[] transPos;
}

void Binary::taKeys()
{
    keys.start();

    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        /* Loop the singles. */
        for ( RedTransList::Iter stel = st->outSingle; stel.lte(); stel++ ) {
            keys.value( stel->lowKey.getVal() );
        }

        /* Loop the state's transitions. */
        for ( RedTransList::Iter rtel = st->outRange; rtel.lte(); rtel++ ) {
            /* Lower key. */
            keys.value( rtel->lowKey.getVal() );

            /* Upper key. */
            keys.value( rtel->highKey.getVal() );
        }
    }

    keys.finish();
}

void Binary::taIndicies()
{
    indicies.start();

    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        /* Walk the singles. */
        for ( RedTransList::Iter stel = st->outSingle; stel.lte(); stel++ )
            indicies.value( stel->value->id );

        /* Walk the ranges. */
        for ( RedTransList::Iter rtel = st->outRange; rtel.lte(); rtel++ )
            indicies.value( rtel->value->id );

        /* The state's default index goes next. */
        if ( st->defTrans != 0 )
            indicies.value( st->defTrans->id );
    }

    indicies.finish();
}

void Binary::taTransCondSpaces()
{
    transCondSpaces.start();

    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        /* Walk the singles. */
        for ( RedTransList::Iter stel = st->outSingle; stel.lte(); stel++ ) {
            RedTransAp *trans = stel->value;
            if ( trans->condSpace != 0 )
                transCondSpaces.value( trans->condSpace->condSpaceId );
            else
                transCondSpaces.value( -1 );
        }

        /* Walk the ranges. */
        for ( RedTransList::Iter rtel = st->outRange; rtel.lte(); rtel++ ) {
            RedTransAp *trans = rtel->value;
            if ( trans->condSpace != 0 )
                transCondSpaces.value( trans->condSpace->condSpaceId );
            else
                transCondSpaces.value( -1 );
        }

        /* The state's default index goes next. */
        if ( st->defTrans != 0 ) {
            RedTransAp *trans = st->defTrans;
            if ( trans->condSpace != 0 )
                transCondSpaces.value( trans->condSpace->condSpaceId );
            else
                transCondSpaces.value( -1 );
        }
    }

    /* Add any eof transitions that have not yet been written out above. */
    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        if ( st->eofTrans != 0 ) {
            RedTransAp *trans = st->eofTrans;
            if ( trans->condSpace != 0 )
                transCondSpaces.value( trans->condSpace->condSpaceId );
            else
                transCondSpaces.value( -1 );
        }
    }

    transCondSpaces.finish();
}

void Binary::taTransOffsets()
{
    transOffsets.start();

    int curOffset = 0;
    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        /* Walk the singles. */
        for ( RedTransList::Iter stel = st->outSingle; stel.lte(); stel++ ) {
            RedTransAp *trans = stel->value;
            transOffsets.value( curOffset );
            curOffset += trans->outConds.length();
        }

        /* Walk the ranges. */
        for ( RedTransList::Iter rtel = st->outRange; rtel.lte(); rtel++ ) {
            RedTransAp *trans = rtel->value;
            transOffsets.value( curOffset );
            curOffset += trans->outConds.length();
        }

        /* The state's default index goes next. */
        if ( st->defTrans != 0 ) {
            RedTransAp *trans = st->defTrans;
            transOffsets.value( curOffset );
            curOffset += trans->outConds.length();
        }
    }

    /* Add any eof transitions that have not yet been written out above. */
    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        if ( st->eofTrans != 0 ) {
            RedTransAp *trans = st->eofTrans;
            transOffsets.value( curOffset );
            curOffset += trans->outConds.length();
        }
    }

    transOffsets.finish();
}

void Binary::taTransLengths()
{
    transLengths.start();

    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        /* Walk the singles. */
        for ( RedTransList::Iter stel = st->outSingle; stel.lte(); stel++ ) {
            RedTransAp *trans = stel->value;
            transLengths.value( trans->outConds.length() );
        }

        /* Walk the ranges. */
        for ( RedTransList::Iter rtel = st->outRange; rtel.lte(); rtel++ ) {
            RedTransAp *trans = rtel->value;
            transLengths.value( trans->outConds.length() );
        }

        /* The state's default index goes next. */
        if ( st->defTrans != 0 ) {
            RedTransAp *trans = st->defTrans;
            transLengths.value( trans->outConds.length() );
        }
    }

    /* Add any eof transitions that have not yet been written out above. */
    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        if ( st->eofTrans != 0 ) {
            RedTransAp *trans = st->eofTrans;
            transLengths.value( trans->outConds.length() );
        }
    }

    transLengths.finish();
}

void Binary::taTransCondSpacesWi()
{
    transCondSpacesWi.start();

    for ( TransApSet::Iter trans = redFsm->transSet; trans.lte(); trans++ ) {
        /* Cond Space id. */
        if ( trans->condSpace != 0 )
            transCondSpacesWi.value( trans->condSpace->condSpaceId );
        else
            transCondSpacesWi.value( -1 );
    }

    transCondSpacesWi.finish();
}

void Binary::taTransOffsetsWi()
{
    transOffsetsWi.start();

    int curOffset = 0;
    for ( TransApSet::Iter trans = redFsm->transSet; trans.lte(); trans++ ) {
        transOffsetsWi.value( curOffset );

        TransApSet::Iter next = trans;
        next.increment();

        curOffset += trans->outConds.length();
    }

    transOffsetsWi.finish();
}

void Binary::taTransLengthsWi()
{
    transLengthsWi.start();

    for ( TransApSet::Iter trans = redFsm->transSet; trans.lte(); trans++ ) {
        transLengthsWi.value( trans->outConds.length() );

        TransApSet::Iter next = trans;
        next.increment();
    }

    transLengthsWi.finish();
}

void Binary::taCondKeys()
{
    condKeys.start();

    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        /* Walk the singles. */
        for ( RedTransList::Iter stel = st->outSingle; stel.lte(); stel++ ) {
            RedTransAp *trans = stel->value;
            for ( RedCondList::Iter cond = trans->outConds; cond.lte(); cond++ )
                condKeys.value( cond->key.getVal() );
        }

        /* Walk the ranges. */
        for ( RedTransList::Iter rtel = st->outRange; rtel.lte(); rtel++ ) {
            RedTransAp *trans = rtel->value;
            for ( RedCondList::Iter cond = trans->outConds; cond.lte(); cond++ )
                condKeys.value( cond->key.getVal() );
        }

        /* The state's default index goes next. */
        if ( st->defTrans != 0 ) {
            RedTransAp *trans = st->defTrans;
            for ( RedCondList::Iter cond = trans->outConds; cond.lte(); cond++ )
                condKeys.value( cond->key.getVal() );
        }
    }

    /* Add any eof transitions that have not yet been written out above. */
    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        if ( st->eofTrans != 0 ) {
            RedTransAp *trans = st->eofTrans;
            for ( RedCondList::Iter cond = trans->outConds; cond.lte(); cond++ )
                condKeys.value( cond->key.getVal() );
        }
    }

    condKeys.finish();
}

void Binary::taCondTargs()
{
    condTargs.start();

    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        /* Walk the singles. */
        for ( RedTransList::Iter stel = st->outSingle; stel.lte(); stel++ ) {
            RedTransAp *trans = stel->value;
            for ( RedCondList::Iter cond = trans->outConds; cond.lte(); cond++ ) {
                RedCondAp *c = cond->value;
                condTargs.value( c->targ->id );
            }
        }

        /* Walk the ranges. */
        for ( RedTransList::Iter rtel = st->outRange; rtel.lte(); rtel++ ) {
            RedTransAp *trans = rtel->value;
            for ( RedCondList::Iter cond = trans->outConds; cond.lte(); cond++ ) {
                RedCondAp *c = cond->value;
                condTargs.value( c->targ->id );
            }
        }

        /* The state's default index goes next. */
        if ( st->defTrans != 0 ) {
            RedTransAp *trans = st->defTrans;
            for ( RedCondList::Iter cond = trans->outConds; cond.lte(); cond++ ) {
                RedCondAp *c = cond->value;
                condTargs.value( c->targ->id );
            }
        }
    }

    /* Add any eof transitions that have not yet been written out above. */
    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        if ( st->eofTrans != 0 ) {
            RedTransAp *trans = st->eofTrans;
            for ( RedCondList::Iter cond = trans->outConds; cond.lte(); cond++ ) {
                RedCondAp *c = cond->value;
                condTargs.value( c->targ->id );
            }
        }
    }

    condTargs.finish();
}

void Binary::taCondActions()
{
    condActions.start();

    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        /* Walk the singles. */
        for ( RedTransList::Iter stel = st->outSingle; stel.lte(); stel++ ) {
            RedTransAp *trans = stel->value;
            for ( RedCondList::Iter cond = trans->outConds; cond.lte(); cond++ ) {
                RedCondAp *c = cond->value;
                COND_ACTION( c );
            }
        }

        /* Walk the ranges. */
        for ( RedTransList::Iter rtel = st->outRange; rtel.lte(); rtel++ ) {
            RedTransAp *trans = rtel->value;
            for ( RedCondList::Iter cond = trans->outConds; cond.lte(); cond++ ) {
                RedCondAp *c = cond->value;
                COND_ACTION( c );
            }
        }

        /* The state's default index goes next. */
        if ( st->defTrans != 0 ) {
            RedTransAp *trans = st->defTrans;
            for ( RedCondList::Iter cond = trans->outConds; cond.lte(); cond++ ) {
                RedCondAp *c = cond->value;
                COND_ACTION( c );
            }
        }
    }

    /* Add any eof transitions that have not yet been written out above. */
    for ( RedStateList::Iter st = redFsm->stateList; st.lte(); st++ ) {
        if ( st->eofTrans != 0 ) {
            RedTransAp *trans = st->eofTrans;
            for ( RedCondList::Iter cond = trans->outConds; cond.lte(); cond++ ) {
                RedCondAp *c = cond->value;
                COND_ACTION( c );
            }
        }
    }

    condActions.finish();
}

/* Write out the array of actions. */
std::ostream &Binary::ACTIONS_ARRAY()
{
    out << "\t0, ";
    int totalActions = 1;
    for ( GenActionTableMap::Iter act = redFsm->actionMap; act.lte(); act++ ) {
        /* Write out the length, which will never be the last character. */
        out << act->key.length() << ", ";
        /* Put in a line break every 8 */
        if ( totalActions++ % 8 == 7 )
            out << "\n\t";

        for ( GenActionTable::Iter item = act->key; item.lte(); item++ ) {
            out << item->value->actionId;
            if ( ! (act.last() && item.last()) )
                out << ", ";

            /* Put in a line break every 8 */
            if ( totalActions++ % 8 == 7 )
                out << "\n\t";
        }
    }
    out << "\n";
    return out;
}

void Binary::taActions()
{
    actions.start();

    /* Put "no-action" at the beginning. */
    actions.value( 0 );

    for ( GenActionTableMap::Iter act = redFsm->actionMap; act.lte(); act++ ) {
        /* Write out the length, which will never be the last character. */
        actions.value( act->key.length() );

        for ( GenActionTable::Iter item = act->key; item.lte(); item++ )
            actions.value( item->value->actionId );
    }

    actions.finish();
}


void Binary::LOCATE_TRANS()
{
    out <<
        "    _keys = " << ARR_REF( keyOffsets ) << "[" << vCS() << "]" << "\n" // keys array index
        "    _trans = " << ARR_REF( indexOffsets ) << "[" << vCS() << "]\n"
        "    _klen = " << ARR_REF( singleLens ) << "[" << vCS() << "]\n"
        "    _break_match = false\n"
        "    while true\n"
        "        if _klen > 0\n"
        "            _lower = _keys\n" // ALPH_TYPE array index
        "            _mid = 0\n" // ALPH_TYPE array index
        "            _upper = _keys + _klen - 1\n" // ALPH_TYPE array index
        "            while true\n"
        "                if _upper < _lower\n"
        "                    break\n"
        "                end\n"
        "\n"
        "                _mid = _lower + ((_upper - _lower) >> 1)\n"
        "                if " << GET_KEY() << " < " << ARR_REF( keys ) << "[_mid]\n"
        "                    _upper = _mid - 1\n"
        "                elseif " << GET_KEY() << " > " << ARR_REF( keys ) << "[_mid]\n"
        "                    _lower = _mid + 1\n"
        "                else\n"
        "                    _trans = uint(int(_trans) + (_mid - int(_keys)))\n"
        "                    _break_match = true\n"
        "                    break\n"
        "                end\n"
        "            end\n"
        "            if _break_match\n"
        "                break\n"
        "            end\n"
        "            _key += _klen\n"
        "            _trans += _klen\n"
        "        end\n"
        "\n"
        "        _klen = " << ARR_REF( rangeLens ) << "[" << vCS() << "]\n"
        "        if _klen > 0\n"
        "            _lower = _keys\n" // ALPH_TYPE array index
        "            _mid = 0\n" // ALPH_TYPE array index
        "            _upper = int(_keys) + (_klen << 1) - 2\n" // ALPH_TYPE array index
        "            while true\n"
        "                if _upper < _lower\n"
        "                    break\n"
        "                end\n"
        "\n"
        "                _mid = _lower + (((_upper - _lower) >> 1) & ~uint(1))\n"
        "                if " << GET_KEY() << " < " << ARR_REF( keys ) << "[_mid]\n"
        "                    _upper = _mid - 2\n"
        "                elseif " << GET_KEY() << " > " << ARR_REF( keys ) << "[_mid + 1]\n"
        "                    _lower = _mid + 2\n"
        "                else\n"
        "                    _trans = " << "uint(int(_trans) + ((_mid - int(_keys)) >> 1))\n"
        "                    _break_match = true\n"
        "                    break\n"
        "                end\n"
        "            end\n"
        "            if _break_match\n"
        "                break\n"
        "            end\n"
        "            _trans = uint(int(_trans) + _klen)\n"
        "        end\n"
        "        break\n"
        "    end # while true\n"
        "\n";
}

void Binary::LOCATE_COND()
{
    out <<
        "    _ckeys = " << ARR_REF( transOffsets ) << "[_trans]" << "\n" // condKeys array index
        "    _klen = " << ARR_REF( transLengths ) << "[_trans]" << "\n"
        "    _cond = " << ARR_REF( transOffsets ) << "[_trans]" << "\n"
        "    _break_match = false\n"
        "\n";

    out << "    _cpc = 0\n";
    int i = 0;
    for ( CondSpaceList::Iter csi = condSpaceList; csi.lte(); csi++, i++ ) {
        GenCondSpace *condSpace = csi;
        out << (i == 0 ? "    if " : "    elseif ");
        out << ARR_REF( transCondSpaces ) << "[_trans] == " << condSpace->condSpaceId << "\n";
        for ( GenCondSet::Iter csi = condSpace->condSet; csi.lte(); csi++ ) {
            out << TABS(2) << "if ";
            CONDITION( out, *csi );
            Size condValOffset = (1 << csi.pos());
            out << TABS(3) << "_cpc += " << condValOffset << "\n";
            out << TABS(2) << "end\n";
        }
    }
    if (i > 0) out << "    end\n";

    out <<
        "    begin\n"
        "        _lower = int(_ckeys);\n" // condKeys array index
        "        _mid = 0\n" // condKeys array index
        "        _upper = _ckeys + _klen - 1\n" // condKeys array index
        "        while true\n"
        "            if _upper < _lower\n"
        "                break\n"
        "            end\n"
        "\n"
        "            _mid = _lower + ((_upper - _lower) >> 1)\n"
        "            if " << "_cpc" << " < " << ARR_REF( condKeys ) << "[_mid]\n"
        "                _upper = _mid - 1\n"
        "            elseif " << "_cpc" << " > " << ARR_REF( condKeys ) << "[_mid]\n"
        "                _lower = _mid + 1\n"
        "            else\n"
        "                _cond = " << "_cond + (_mid - _ckeys)\n"
        "                _break_match = true\n"
        "                break\n"
        "            end\n"
        "        end\n"
        "        if !_break_match\n"
        "            " << vCS() << " = " << ERROR_STATE() << "\n"
        "            _goto_level = _again\n"
        "            _trigger_goto = true\n"
        "        end\n"
        "    end\n";
}

void Binary::GOTO( ostream &ret, int gotoDest, bool inFinish )
{
    ret << "    " << vCS() << " = " << gotoDest << "\n";
    ret << "    _trigger_goto = true\n";
    ret << "    _goto_level = _again\n";
    ret << "    break\n";
}

// TODO: everything below this
void Binary::GOTO_EXPR( ostream &ret, GenInlineItem *ilItem, bool inFinish )
{
    ret << "{ " << vCS() << " = (";
    INLINE_LIST( ret, ilItem->children, 0, inFinish, false );
    ret << ")\n";
    ret << "_trigger_goto = true\n";
    ret << "_goto_lever = _again\n";
}

void Binary::CURS( ostream &ret, bool inFinish )
{
    ret << "(_ps)";
}

void Binary::TARGS( ostream &ret, bool inFinish, int targState )
{
    ret << "(" << vCS() << ")";
}

void Binary::NEXT( ostream &ret, int nextDest, bool inFinish )
{
    ret << vCS() << " = " << nextDest << "\n;";
}

void Binary::NEXT_EXPR( ostream &ret, GenInlineItem *ilItem, bool inFinish )
{
    ret << vCS() << " = (";
    INLINE_LIST( ret, ilItem->children, 0, inFinish, false );
    ret << ")\n";
}

void Binary::CALL( ostream &ret, int callDest, int targState, bool inFinish )
{
    if ( prePushExpr != 0 ) {
        ret << "begin\n";
        INLINE_LIST( ret, prePushExpr, 0, false, false );
    }

    ret <<
        "    begin\n"
        "        " << STACK() << "[" << TOP() << "] = " << vCS() << "\n"
        "        " << TOP() << "+= 1\n"
        "        " << vCS() << " = " << callDest << "\n"
        "        _trigger_goto = true\n"
        "        _goto_level = _again\n"
        "        break\n"
        "    end\n";

    if ( prePushExpr != 0 )
        ret << "end\n";
}

void Binary::CALL_EXPR( ostream &ret, GenInlineItem *ilItem, int targState, bool inFinish )
{
    if ( prePushExpr != 0 ) {
        ret << "begin\n";
        INLINE_LIST( ret, prePushExpr, 0, false, false );
    }

    ret <<
        "    begin\n"
        "        " << STACK() << "[" << TOP() << "] = " << vCS() << "\n"
        "        " << TOP() << " += 1\n"
        "        " << vCS() << " = (";
    INLINE_LIST( ret, ilItem->children, targState, inFinish, false );
    ret << ")\n";
    ret <<
        "        _trigger_goto = true\n"
        "        _goto_level = _again\n"
        "        break\n";

    if ( prePushExpr != 0 )
        ret << "end\n";
}

void Binary::RET( ostream &ret, bool inFinish )
{
    ret <<
        "    begin\n"
        "        " << TOP() << " -= 1\n"
        "        " << vCS() << " = " << STACK() << "[" << TOP() << "]\n";

    if ( postPopExpr != 0 ) {
        ret << "begin\n";
        INLINE_LIST( ret, postPopExpr, 0, false, false );
        ret << "end\n";
    }

    ret <<
        "        _trigger_goto = true\n"
        "        _goto_level = _again\n"
        "        break\n"
        "    end\n";
}

void Binary::BREAK( ostream &ret, int targState, bool csForced )
{
    outLabelUsed = true;
    ret <<
        "begin\n"
        "    " << P() << " += 1\n"
        "    _trigger_goto = true\n"
        "    _goto_level = _out\n"
        "    break\n"
        "end\n";
}

}

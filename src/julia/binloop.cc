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
#include "binloop.h"
#include "redfsm.h"
#include "gendata.h"

namespace Julia {

BinaryLooped::BinaryLooped( const CodeGenArgs &args )
:
	Binary( args )
{}

/* Determine if we should use indicies or not. */
void BinaryLooped::calcIndexSize()
{
	long long sizeWithInds =
		indicies.size() +
		transCondSpacesWi.size() +
		transOffsetsWi.size() +
		transLengthsWi.size();

	long long sizeWithoutInds =
		transCondSpaces.size() +
		transOffsets.size() +
		transLengths.size();

	std::cerr << "sizes: " << sizeWithInds << " " << sizeWithoutInds << std::endl;

	///* If using indicies reduces the size, use them. */
	//useIndicies = sizeWithInds < sizeWithoutInds;
	useIndicies = false;
}


void BinaryLooped::tableDataPass()
{
	taActions();
	taKeyOffsets();
	taSingleLens();
	taRangeLens();
	taIndexOffsets();
	taIndicies();

	taTransCondSpacesWi();
	taTransOffsetsWi();
	taTransLengthsWi();

	taTransCondSpaces();
	taTransOffsets();
	taTransLengths();

	taCondTargs();
	taCondActions();

	taToStateActions();
	taFromStateActions();
	taEofActions();

	taEofTransDirect();
	taEofTransIndexed();

	taKeys();
	taCondKeys();
}

void BinaryLooped::genAnalysis()
{
	redFsm->sortByStateId();

	/* Choose default transitions and the single transition. */
	redFsm->chooseDefaultSpan();

	/* Choose the singles. */
	redFsm->chooseSingle();

	/* If any errors have occured in the input file then don't write anything. */
	if ( gblErrorCount > 0 )
		return;

	/* Anlayze Machine will find the final action reference counts, among other
	 * things. We will use these in reporting the usage of fsm directives in
	 * action code. */
	analyzeMachine();

	setKeyType();

	/* Run the analysis pass over the table data. */
	setTableState( TableArray::AnalyzePass );
	tableDataPass();

	/* Determine if we should use indicies. */
	calcIndexSize();

	/* Switch the tables over to the code gen mode. */
	setTableState( TableArray::GeneratePass );
}


void BinaryLooped::COND_ACTION( RedCondAp *cond )
{
	int act = 0;
	if ( cond->action != 0 )
		act = cond->action->location+1;
	condActions.value( act );
}

void BinaryLooped::TO_STATE_ACTION( RedStateAp *state )
{
	int act = 0;
	if ( state->toStateAction != 0 )
		act = state->toStateAction->location+1;
	toStateActions.value( act );
}

void BinaryLooped::FROM_STATE_ACTION( RedStateAp *state )
{
	int act = 0;
	if ( state->fromStateAction != 0 )
		act = state->fromStateAction->location+1;
	fromStateActions.value( act );
}

void BinaryLooped::EOF_ACTION( RedStateAp *state )
{
	int act = 0;
	if ( state->eofAction != 0 )
		act = state->eofAction->location+1;
	eofActions.value( act );
}

std::ostream &BinaryLooped::TO_STATE_ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
    int i = 0;
	for ( GenActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numToStateRefs > 0 ) {
            out << (i == 0 ? "        if " : "        elseif ");
            out << ARR_REF( actions ) << "[_acts - 1] == " << act->actionId <<"\n";
			ACTION( out, act, 0, false, false );
            ++i;
		}
	}
    if (i > 0) out << "        end\n";

	genLineDirective( out );
	return out;
}

std::ostream &BinaryLooped::FROM_STATE_ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
    int i = 0;
	for ( GenActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numFromStateRefs > 0 ) {
            out << (i == 0 ? "        if " : "        elseif ");
            out << ARR_REF( actions ) << "[_acts - 1] == " << act->actionId <<"\n";
			ACTION( out, act, 0, false, false );
            ++i;
		}
	}

	genLineDirective( out );
	return out;
}

std::ostream &BinaryLooped::EOF_ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
	for ( GenActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numEofRefs > 0 ) {
			/* Write the case label, the action and the case break. */
			out << "\tcase " << act->actionId << ":\n";
			ACTION( out, act, 0, true, false );
		}
	}

	genLineDirective( out );
	return out;
}


std::ostream &BinaryLooped::ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
	for ( GenActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numTransRefs > 0 ) {
			/* Write the case label, the action and the case break. */
			out << "\tcase " << act->actionId << ":\n";
			ACTION( out, act, 0, false, false );
		}
	}

	genLineDirective( out );
	return out;
}


void BinaryLooped::writeData()
{
	/* If there are any transtion functions then output the array. If there
	 * are none, don't bother emitting an empty array that won't be used. */
	if ( redFsm->anyActions() )
		taActions();

	taKeyOffsets();
	taKeys();
	taSingleLens();
	taRangeLens();
	taIndexOffsets();

	if ( useIndicies ) {
		taIndicies();
		taTransCondSpacesWi();
		taTransOffsetsWi();
		taTransLengthsWi();
	}
	else {
		taTransCondSpaces();
		taTransOffsets();
		taTransLengths();
	}

	taCondKeys();

	taCondTargs();
	taCondActions();

	if ( redFsm->anyToStateActions() )
		taToStateActions();

	if ( redFsm->anyFromStateActions() )
		taFromStateActions();

	if ( redFsm->anyEofActions() )
		taEofActions();

	if ( redFsm->anyEofTrans() ) {
		taEofTransIndexed();
		taEofTransDirect();
	}

	STATE_IDS();

    // binary search functions
    out <<
        "function " << FSM_NAME() << "_matchsingle(_keys::Int, _klen::Int, _trans::Int, p::Int, data)\n"
        "    _lower = _keys\n" // ALPH_TYPE array index
        "    _mid = 0\n" // ALPH_TYPE array index
        "    _upper = _keys + _klen - 1\n" // ALPH_TYPE array index
        "    while true\n"
        "        if _upper < _lower\n"
        "            break\n"
        "        end\n"
        "\n"
        "        _mid = _lower + ((_upper - _lower) >> 1)\n"
        "        if " << GET_KEY() << " < " << ARR_REF( keys ) << "[_mid]\n"
        "            _upper = _mid - 1\n"
        "        elseif " << GET_KEY() << " > " << ARR_REF( keys ) << "[_mid]\n"
        "            _lower = _mid + 1\n"
        "        else\n"
        "            return true, _trans + (_mid - _keys)\n"
        "        end\n"
        "    end\n"
        "    return false, _trans\n"
        "end\n\n";

    out <<
        "function " << FSM_NAME() << "_matchrange(_keys::Int, _klen::Int, _trans::Int, p::Int, data)\n"
        "    _lower = _keys\n" // ALPH_TYPE array index
        "    _mid = 0\n" // ALPH_TYPE array index
        "    _upper = _keys + (_klen << 1) - 2\n" // ALPH_TYPE array index
        "    while true\n"
        "        if _upper < _lower\n"
        "            break\n"
        "        end\n"
        "\n"
        "        _mid = _lower + (((_upper - _lower) >> 1) & ~1)\n"
        "        if " << GET_KEY() << " < " << ARR_REF( keys ) << "[_mid]\n"
        "            _upper = _mid - 2\n"
        "        elseif " << GET_KEY() << " > " << ARR_REF( keys ) << "[_mid + 1]\n"
        "            _lower = _mid + 2\n"
        "        else\n"
        "            return true, _trans + ((_mid - _keys) >> 1)\n"
        "        end\n"
        "    end\n"
        "    return false, _trans\n"
        "end\n\n";

    out <<
        "function " << FSM_NAME() << "_matchcond(_ckeys::Int, _klen::Int, _cpc::Int, _cond::Int)\n"
        "    _lower = _ckeys;\n" // condKeys array index
        "    _mid = 0\n" // condKeys array index
        "    _upper = _ckeys + _klen - 1\n" // condKeys array index
        "    while true\n"
        "        if _upper < _lower\n"
        "            break\n"
        "        end\n"
        "\n"
        "        _mid = _lower + ((_upper - _lower) >> 1)\n"
        "        if " << "_cpc" << " < " << ARR_REF( condKeys ) << "[_mid]\n"
        "            _upper = _mid - 1\n"
        "        elseif " << "_cpc" << " > " << ARR_REF( condKeys ) << "[_mid]\n"
        "            _lower = _mid + 1\n"
        "        else\n"
        "            return true, _cond + (_mid - _ckeys)\n"
        "        end\n"
        "    end\n"
        "    return false, _cond\n"
        "end\n\n";
}

void BinaryLooped::writeExec()
{
	testEofUsed = false;
	outLabelUsed = false;

    out << "begin\n";
    out <<
        "    _goto_level = 0\n"
        "    _resume = 10\n"
        "    _eof_trans = 15\n"
        "    _again = 20\n"
        "    _test_eof = 30\n"
        "    _out = 40\n";

    out <<
        "    while true\n"
        "    _trigger_goto = false\n"
        "    if _goto_level <= 0\n";

    if ( !noEnd ) {
		testEofUsed = true;
        out <<
            "    if " << P() << " == " << PE() << "\n"
            "        _goto_level = _test_eof\n"
            "        continue\n"
            "    end\n";
    }

    if ( redFsm->errState != 0 ) {
		outLabelUsed = true;
        out <<
            "    if " << vCS() << " == " << redFsm->errState->id << "\n"
            "        _goto_level = _test_eof\n"
            "        continue\n"
            "    end\n";
    }

    // resume label
    out <<
        "    end\n"
        "    if _goto_level <= _resume\n";

	if ( redFsm->anyFromStateActions() ) {
        out <<
            "    _acts = " << ARR_REF( fromStateActions ) << "[" << vCS() << "]" << ")\n" // actions array endx
            "    _nacts = " << ARR_REF( actions ) << "[_acts]\n"
            "    _acts += 1\n"
            "    while _nacts > 0\n"
            "        _acts += 1\n";
        FROM_STATE_ACTION_SWITCH() <<
            "        _nacts -= 1\n"
            "    end\n"
            "    if _trigger_goto\n"
            "        continue\n"
            "    end\n";
    }

    // match label
	LOCATE_TRANS();

    if ( useIndicies ) {
        out << "    _trans = " << ARR_REF( indicies ) << "[_trans]\n";
    }

	LOCATE_COND();

    // match_cond label
    if ( redFsm->anyEofTrans() ) {
        out <<
            "    end\n"
            "    if _goto_level <= _eof_trans\n";
    }

    if ( redFsm->anyRegCurStateRef() ) {
        out << "    _ps = " << vCS() << "\n";
    }

    out << "    " << vCS() << " = " << ARR_REF( condTargs ) << "[_cond]\n";

    if ( redFsm->anyRegActions() ) {
        out <<
            "    if " << ARR_REF( condActions ) << "[_cond] == 0"
            "        _goto_level = _again\n"
            "        continue\n"
            "    end\n"
            "\n"
            "    _acts = " << ARR_REF( condActions ) << "[_cond]\n"
            "    _nacts = " << ARR_REF( actions ) << "[_acts]\n"
            "    _acts += 1\n"
            "    while _nacts > 0\n"
            "        _acts += 1\n"
            "        _nacts -= 1\n";
            ACTION_SWITCH() <<
            "    end\n"
            "\n";
    }


    // again label
    out <<
        "    end\n"
        "    if _goto_level <= _again\n";

	if ( redFsm->anyToStateActions() ) {
        out <<
            "    _acts = " << ARR_REF( toStateActions ) << "[" << vCS() << "]\n" // actions array index
            "    _nacts = " << ARR_REF( actions ) << "[_acts]\n"
            "    _acts += 1\n"
            "    while _nacts > 0\n"
            "        _acts += 1\n"
            "        _nacts -= 1\n";
            TO_STATE_ACTION_SWITCH() <<
            "    end\n"
            "\n";
    }

    if ( redFsm->errState != 0 ) {
		outLabelUsed = true;
        out <<
            "    if " << vCS() << " == " << redFsm->errState->id << "\n"
            "        _goto_level = _out\n"
            "        continue\n"
            "    end\n";
    }

    out << "    " << P() << " += 1\n";

    if ( !noEnd ) {
        out <<
            "    if " << P() << " != " << PE() << "\n"
            "        _goto_level = _resume\n"
            "        continue\n"
            "    end\n";
    }
    else {
        out <<
            "    _goto_level = _resume\n"
            "    continue\n";
    }

    // test_eof label
    out <<
        "    end\n"
        "    if _goto_level <= _test_eof\n";

	if ( redFsm->anyEofTrans() || redFsm->anyEofActions() ) {
        out <<
            "    if " << P() << " == " << vEOF() << "\n";

		if ( redFsm->anyEofTrans() ) {
			TableArray &eofTrans = useIndicies ? eofTransIndexed : eofTransDirect;
			out <<
                "    if " << ARR_REF( eofTrans ) << "[" << vCS() << "] > 0\n"
                "        _trans = " << ARR_REF( eofTrans ) << "[" << vCS() << "] - 1\n"
                "        _cond = " << ARR_REF( transOffsets ) << "[_trans]\n"
                "        _goto_level = _eof_trans\n"
                "        continue\n"
                "    end\n";
        }

        if ( redFsm->anyEofActions() ) {
            out <<
                "    __acts = " << ARR_REF( eofActions ) << "[" << vCS() << "]\n" // actions array index
                "    __nacts = " << ARR_REF( actions ) << "[__acts]\n"
                "    __acts += 1\n"
                "    while __nacts > 0\n"
                "        __acts += 1\n"
                "        __nacts -= 1\n";
                EOF_ACTION_SWITCH() <<
                "    end\n"
                "    if _trigger_goto\n"
                "        continue\n"
                "    end\n";
        }

        out <<
            "end\n";
    }

	if ( outLabelUsed ) {
        out <<
            "    end\n"
            "    if _goto_level <= _out\n"
            "        break\n"
            "    end\n";
    }

    // end loop for next
    out <<
        "    end\n";

	// wrapping the execute block.
	out <<
		"	end\n";
}

}

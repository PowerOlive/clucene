/*------------------------------------------------------------------------------
* Copyright (C) 2003-2006 Ben van Klinken and the CLucene Team
* 
* Distributable under the terms of either the Apache License (Version 2.0) or 
* the GNU Lesser General Public License, as specified in the COPYING file.
------------------------------------------------------------------------------*/
#include "test.h"

class QPTestFilter: public TokenFilter {
public:

	bool inPhrase;
	int32_t savedStart, savedEnd;

	/**
	* Filter which discards the token 'stop' and which expands the
	* token 'phrase' into 'phrase1 phrase2'
	*/
	QPTestFilter(TokenStream* in):
		TokenFilter(in,true),
		inPhrase(false),
		savedStart(0),
		savedEnd(0)
	{
	}

	bool next(Token* token) {
		if (inPhrase) {
			inPhrase = false;
			token->set( _T("phrase2"), savedStart, savedEnd);
			return true;
		}else{
			while( input->next(token) ){
				if ( _tcscmp(token->termBuffer(), _T("phrase")) == 0 ) {
					inPhrase = true;
					savedStart = token->startOffset();
					savedEnd = token->endOffset();
					token->set( _T("phrase1"), savedStart, savedEnd);
					return true;
				}else if ( _tcscmp(token->termBuffer(), _T("stop") ) !=0 ){
					return true;
				}
			}
		}
		return false;
	}
};

class QPTestAnalyzer: public Analyzer {
public:
	QPTestAnalyzer() {
	}

	/** Filters LowerCaseTokenizer with StopFilter. */
	TokenStream* tokenStream(const TCHAR* fieldName, Reader* reader) {
		return _CLNEW QPTestFilter(_CLNEW LowerCaseTokenizer(reader));
	}
};

void assertEquals(CuTest *tc,const TCHAR* result, Query* q) {
	if ( q == NULL )
		return;

	const TCHAR* s = q->toString();
	int ret = _tcscmp(s,result);
	_CLDELETE_LCARRAY(s);
	_CLDELETE(q);
	if ( ret != 0 ) {
		CuFail(tc, _T("FAILED Query yielded /%s/, expecting /%s/\n"), s, result);
	}
}

// verify parsing of query using a stopping analyzer  
void assertStopQueryEquals(CuTest *tc, TCHAR* qtxt, TCHAR* expectedRes) {
	const TCHAR* fields[] = {_T("b"), _T("t"), NULL };
	//Occur occur[] = {Occur.SHOULD, Occur.SHOULD};
	QPTestAnalyzer *a = _CLNEW QPTestAnalyzer();
	MultiFieldQueryParser mfqp(fields, a);

	Query *q = mfqp.parse(qtxt);
	assertEquals(tc, expectedRes, q);

	q = MultiFieldQueryParser::parse(qtxt, fields, a);
	assertEquals(tc, expectedRes, q);
	_CLDELETE(a);
}

/** test stop words arsing for both the non static form, and for the 
* corresponding static form (qtxt, fields[]). */
void tesStopwordsParsing(CuTest *tc) {
	assertStopQueryEquals(tc, _T("one"), _T("b:one t:one"));
	assertStopQueryEquals(tc, _T("one stop"), _T("b:one t:one"));
	assertStopQueryEquals(tc, _T("one (stop)"), _T("b:one t:one"));
	assertStopQueryEquals(tc, _T("one ((stop))"), _T("b:one t:one"));
	assertStopQueryEquals(tc, _T("stop"), _T(""));
	assertStopQueryEquals(tc, _T("(stop)"), _T(""));
	assertStopQueryEquals(tc, _T("((stop))"), _T(""));
}

void testMFQPSimple(CuTest *tc) {
	const TCHAR* fields[] = {_T("b"), _T("t"), NULL};
	Analyzer* a = _CLNEW StandardAnalyzer();
	MultiFieldQueryParser mfqp(fields, a);

	Query *q = mfqp.parse(_T("one"));
	assertEquals(tc, _T("b:one t:one"), q);

	q = mfqp.parse(_T("one two"));
	assertEquals(tc, _T("(b:one t:one) (b:two t:two)"),q);

	q = mfqp.parse(_T("+one +two"));
	assertEquals(tc, _T("+(b:one t:one) +(b:two t:two)"), q);

	q = mfqp.parse(_T("+one -two -three"));
	assertEquals(tc, _T("+(b:one t:one) -(b:two t:two) -(b:three t:three)"), q);

	q = mfqp.parse(_T("one^2 two"));
	assertEquals(tc, _T("((b:one t:one)^2.0) (b:two t:two)"), q);

	q = mfqp.parse(_T("one~ two"));
	assertEquals(tc, _T("(b:one~0.5 t:one~0.5) (b:two t:two)"), q);

	q = mfqp.parse(_T("one~0.8 two^2"));
	assertEquals(tc, _T("(b:one~0.8 t:one~0.8) ((b:two t:two)^2.0)"), q);

	q = mfqp.parse(_T("one* two*"));
	assertEquals(tc, _T("(b:one* t:one*) (b:two* t:two*)"), q);

	q = mfqp.parse(_T("[a TO c] two"));
	assertEquals(tc, _T("(b:[a TO c] t:[a TO c]) (b:two t:two)"), q);

	q = mfqp.parse(_T("w?ldcard"));
	assertEquals(tc, _T("b:w?ldcard t:w?ldcard"), q);

	q = mfqp.parse(_T("\"foo bar\""));
	assertEquals(tc, _T("b:\"foo bar\" t:\"foo bar\""), q);

	q = mfqp.parse(_T("\"aa bb cc\" \"dd ee\""));
	assertEquals(tc, _T("(b:\"aa bb cc\" t:\"aa bb cc\") (b:\"dd ee\" t:\"dd ee\")"), q);

	q = mfqp.parse(_T("\"foo bar\"~4"));
	assertEquals(tc, _T("b:\"foo bar\"~4 t:\"foo bar\"~4"), q);

	// make sure that terms which have a field are not touched:
	q = mfqp.parse(_T("one f:two"));
	assertEquals(tc, _T("(b:one t:one) f:two"), q);

	// AND mode:
	mfqp.setDefaultOperator(QueryParser::AND_OPERATOR);
	q = mfqp.parse(_T("one two"));
	assertEquals(tc, _T("+(b:one t:one) +(b:two t:two)"), q);
	q = mfqp.parse(_T("\"aa bb cc\" \"dd ee\""));
	assertEquals(tc, _T("+(b:\"aa bb cc\" t:\"aa bb cc\") +(b:\"dd ee\" t:\"dd ee\")"), q);

	_CLDELETE(a);
}

CuSuite *testMultiFieldQueryParser(void)
{
	CuSuite *suite = CuSuiteNew(_T("CLucene Multi-Field QP Test"));

    SUITE_ADD_TEST(suite, tesStopwordsParsing);
	SUITE_ADD_TEST(suite, testMFQPSimple);

    return suite; 
}
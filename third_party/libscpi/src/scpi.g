grammar scpi;

terminatedProgramMessage 
	:	programMessage NL? EOF
	;
	
programMessage
	:	programMessageUnit (SEMICOLON programMessageUnit)*
	;
	
	
programMessageUnit 
	:	WS* programHeader (WS programData (COMMA programData)*)?
	;

programHeader
	:	compoundProgramHeader
	|	commonProgramHeader
	;

compoundProgramHeader
	:	COLON? PROGRAM_MNEMONIC (COLON PROGRAM_MNEMONIC)* QUESTION?
	;
	
commonProgramHeader 
	:	STAR PROGRAM_MNEMONIC QUESTION?
	;
	
programDataSeparator 
	:	  WS*
	;

programData
	:	WS* programDataType WS*
	;
	
programDataType
	:	nondecimalNumericProgramData
	|	characterProgramData
	|	decimalNumericProgramData
	|	stringProgramData
	|	arbitraryBlockProgramData
	|	expressionProgramData
//	|	suffixProgramData
	;

nondecimalNumericProgramData
	:	HEXNUM
	|	OCTNUM
	|	BINNUM
	;
	
characterProgramData
	:	PROGRAM_MNEMONIC
	;

decimalNumericProgramData
	:	DECIMAL_NUMERIC_PROGRAM_DATA_WITH_SUFFIX
	;
	
//suffixProgramData
//	:	PROGRAM_MNEMONIC//SUFFIX_PROGRAM_DATA
//	;
	
stringProgramData
	:	SINGLE_QUOTE_PROGRAM_DATA
	|	DOUBLE_QUOTE_PROGRAM_DATA
	;	

expressionProgramData
	: 	PROGRAM_EXPRESSION
	;

// support only nonzero prefix
arbitraryBlockProgramData
	:	SHARP NONZERO_DIGIT NUMBER .*
	;
		
PROGRAM_MNEMONIC	: 	ALPHA (ALPHA | DIGIT | UNDERSCORE)*;
HEXNUM			:	SHARP H HEXDIGIT*;
BINNUM			:	SHARP Q OCTDIGIT*;
OCTNUM			:	SHARP B BINDIGIT*;
UNDERSCORE		:	'_';
SEMICOLON 		:	';';
QUESTION		:	'?';
COLON	 		:	':';
COMMA			:	',';
STAR			:	'*';
NL			:	'\r'? '\n' ;
WS  			:   	(SPACE | TAB);

DECIMAL_NUMERIC_PROGRAM_DATA_WITH_SUFFIX	:	DECIMAL_NUMERIC_PROGRAM_DATA WS* (SUFFIX_PROGRAM_DATA)?;
fragment DECIMAL_NUMERIC_PROGRAM_DATA	:	MANTISA WS* (EXPONENT)?;
SINGLE_QUOTE_PROGRAM_DATA	:	SINGLE_QUOTE ( (NON_SINGLE_QUOTE) | (SINGLE_QUOTE SINGLE_QUOTE))* SINGLE_QUOTE;
DOUBLE_QUOTE_PROGRAM_DATA	:	DOUBLE_QUOTE ( (NON_DOUBLE_QUOTE) | (DOUBLE_QUOTE DOUBLE_QUOTE))* DOUBLE_QUOTE;
//SUFFIX_PROGRAM_DATA	:	SLASH? (ALPHA+ (MINUS? DIGIT)?) ((SLASH | DOT) (ALPHA+ (MINUS? DIGIT)?))*;	
fragment SUFFIX_PROGRAM_DATA	:	SLASH? ALPHA+ ((SLASH | DOT) ALPHA+)*;	
//fragment SUFFIX_PROGRAM_DATA	:	ALPHA+;	

fragment PROGRAM_EXPRESSION_CHARACTER	: 	(SPACE | '!' | '$'..'&' | '*'..':' | '<' ..'~');
PROGRAM_EXPRESSION	:	LBRACKET PROGRAM_EXPRESSION_CHARACTER RBRACKET;
	
fragment PLUSMN		:	(PLUS | MINUS);
fragment MANTISA	:	PLUSMN? ( (NUMBER) | (NUMBER DOT NUMBER?) | (DOT NUMBER));
	
//fragment EXPONENT	:	WS* E WS* PLUSMN? NUMBER;
fragment EXPONENT	:	E WS* PLUSMN? NUMBER;

fragment NUMBER		:	DIGIT+;

fragment LBRACKET		:	'(';
fragment RBRACKET		:	')';

fragment ALPHA		:	('a'..'z'|'A'..'Z');
fragment DIGIT		:	('0'..'9');
fragment NONZERO_DIGIT	:	('1'..'9');

fragment HEXDIGIT	:	(DIGIT | 'a'..'f' | 'A'..'F');
fragment OCTDIGIT	:	('0'..'7');
fragment BINDIGIT	:	('0' | '1');

fragment SHARP			:	'#';

fragment E		:	('E'|'e');
fragment H		:	('H'|'h');
fragment Q		:	('Q'|'q');
fragment B		:	('B'|'b');

fragment SPACE		:	' ';
fragment TAB		:	'\t';

fragment PLUS		:	'+';
fragment MINUS		:	'-';
fragment DOT		:	'.';
fragment SLASH		:	'/';
fragment SINGLE_QUOTE	:	'\'';
fragment DOUBLE_QUOTE	:	'"';
fragment NON_SINGLE_QUOTE 	:	~SINGLE_QUOTE;
fragment NON_DOUBLE_QUOTE 	:	~DOUBLE_QUOTE;



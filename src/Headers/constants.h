/*
** Copyright (C) University of Virginia, Massachusetts Institue of Technology 1994-2000.
** See ../LICENSE for license information.
**
*/
/*
**  constants.h
*/

/*
 * This is constants.h from Mike Smith's Win32 port of lclint.
 * Modified by Herbert 04/19/97:
 * added conditional 'OS2' to conditional 'MSDOS'
 */

# ifndef CONSTANTS_H
# define CONSTANTS_H

# if defined(MSDOS) || defined(OS2)
/*@constant observer char *RCFILE; @*/
# define RCFILE         "lclint.rc"
# else
/*@constant observer char *RCFILE; @*/
# define RCFILE         ".lclintrc"
# endif

/*@constant observer char *C_SUFFIX; @*/
# define C_SUFFIX               ".c"

/*@constant observer char *LARCH_PATH; @*/
# define LARCH_PATH		"LARCH_PATH"

/*@constant observer char *LCLIMPORTDIR; @*/
# define LCLIMPORTDIR           "LCLIMPORTDIR"

/*@constant observer char *LLSTDLIBS_NAME; @*/
# define LLSTDLIBS_NAME          "ansi"

/*@constant observer char *LLSTRICTLIBS_NAME; @*/
# define LLSTRICTLIBS_NAME      "ansistrict"

/*@constant observer char *LLUNIXLIBS_NAME; @*/
# define LLUNIXLIBS_NAME        "unix"

/*@constant observer char *LLUNIXSTRICTLIBS_NAME; @*/
# define LLUNIXSTRICTLIBS_NAME  "unixstrict"

/*@constant observer char *LLPOSIXLIBS_NAME; @*/
# define LLPOSIXLIBS_NAME       "posix"

/*@constant observer char *LLPOSIXSTRICTLIBS_NAME; @*/
# define LLPOSIXSTRICTLIBS_NAME "posixstrict"

/*@constant observer cstring REFSNAME; @*/
# define REFSNAME               cstring_makeLiteralTemp ("refs")

/*
** Used to be .lldmp.  Truncated to .lcd to fix within
** MS-DOS filename limitations.
*/

/*@constant observer char *DUMP_SUFFIX; @*/
# define DUMP_SUFFIX ".lcd"

/*@constant int MAX_NAME_LENGTH; @*/
# define MAX_NAME_LENGTH 256

/*@constant int MAX_LINE_LENGTH; @*/
# define MAX_LINE_LENGTH 1024

/*@constant int MAX_DUMP_LINE_LENGTH; @*/
# define MAX_DUMP_LINE_LENGTH 8192

/*@constant int MINLINELEN; @*/
# define MINLINELEN 20

/*
** WARNING: Can't use macros in token for cgrammar.l -->
**   must keep these consistent!
*/

/*@constant observer char *LLMRCODE; @*/
# define LLMRCODE "@QLMR"  
/* MUST be 5 chars = defin[e]. The number of spaces between the
   # and the d is the sixth tag. 
*/

/*@constant observer char *PPMRCODE; @*/
# define PPMRCODE "@@MR@@"  

/*@constant observer char *DEFAULT_SYSTEMDIR; @*/
# define DEFAULT_SYSTEMDIR "/usr/"

/*@constant char DEFAULT_COMMENTCHAR; @*/
# define DEFAULT_COMMENTCHAR '@'

/*@constant int DEFAULT_LINELEN; @*/
# define DEFAULT_LINELEN 80

/*
** These constants are based on implementation limits in ANSI standard,
** Section 3.1. 
*/

/*@constant int DEFAULT_EXTERNALNAMELEN; @*/
# define DEFAULT_EXTERNALNAMELEN 6

/*@constant int DEFAULT_INTERNALNAMELEN; @*/
# define DEFAULT_INTERNALNAMELEN 31

/*@constant int DEFAULT_CONTROLNESTDEPTH; @*/
# define DEFAULT_CONTROLNESTDEPTH 15

/*@constant int DEFAULT_STRINGLITERALLEN; @*/
# define DEFAULT_STRINGLITERALLEN 509

/*@constant int DEFAULT_INCLUDENEST; @*/
# define DEFAULT_INCLUDENEST 8

/*@constant int DEFAULT_NUMSTRUCTFIELDS; @*/
# define DEFAULT_NUMSTRUCTFIELDS 127

/*@constant int DEFAULT_NUMENUMMEMBERS; @*/
# define DEFAULT_NUMENUMMEMBERS 127

/*@constant int DEFAULT_LIMIT; @*/
# define DEFAULT_LIMIT -1    /* unlimited messages */

/*@constant char PFX_UPPERCASE; @*/
# define PFX_UPPERCASE '^'

/*@constant char PFX_LOWERCASE; @*/
# define PFX_LOWERCASE '&'

/*@constant char PFX_ANY; @*/
# define PFX_ANY '?'

/*@constant char PFX_DIGIT; @*/
# define PFX_DIGIT '#'

/*@constant char PFX_NOTUPPER; @*/
# define PFX_NOTUPPER '%'

/*@constant char PFX_NOTLOWER; @*/
# define PFX_NOTLOWER '~'

/*@constant char PFX_ANYLETTER; @*/
# define PFX_ANYLETTER '$'

/*@constant char PFX_ANYLETTERDIGIT; @*/
# define PFX_ANYLETTERDIGIT '/'

/*
** Note: this name is wired into ansi.h!
*/

/*@constant observer char *DEFAULT_BOOLTYPE;@*/
# define DEFAULT_BOOLTYPE "lltX_bool"

/*@constant observer char *PRAGMA_EXPAND; @*/
# define PRAGMA_EXPAND "expand"

/*@constant int PRAGMA_LEN_EXPAND; @*/
# define PRAGMA_LEN_EXPAND 6

/*@constant int MAX_PRAGMA_LEN; @*/
# define MAX_PRAGMA_LEN PRAGMA_LEN_EXPAND

/*
** Minimum version with compatible libraries.
*/

/*@constant float LCL_MIN_VERSION; @*/
# define LCL_MIN_VERSION 2.2

/*
** Flex doesn't pre-process input, so remember to copy these manually
** to cscanner.l.
*/

/*@constant observer char *BEFORE_COMMENT_MARKER@*/
# define BEFORE_COMMENT_MARKER "%{"

/*@constant observer char *AFTER_COMMENT_MARKER@*/
# define AFTER_COMMENT_MARKER "%}"

# else
# error "Multiple include"
# endif
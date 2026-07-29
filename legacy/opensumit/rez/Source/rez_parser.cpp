/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 1 "Source/rez_parser.y"


/*	$Id: rez_parser.y,v 1.1.1.1 2000/03/05 06:23:12 tpv Exp $
	
	Copyright 1996, 1997, 1998
	        Hekkelman Programmatuur B.V.  All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.
	2. Redistributions in binary form must reproduce the above copyright notice,
	   this list of conditions and the following disclaimer in the documentation
	   and/or other materials provided with the distribution.
	3. All advertising materials mentioning features or use of this software
	   must display the following acknowledgement:
	   
	    This product includes software developed by Hekkelman Programmatuur B.V.
	
	4. The name of Hekkelman Programmatuur B.V. may not be used to endorse or
	   promote products derived from this software without specific prior
	   written permission.
	
	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
	FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
	AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
	EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
	OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
	WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
	OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 	

	Created: 12/02/98 15:37:42
*/

#include "rez.h"
#include "RTypes.h"
#include "RState.h"
#include "REval.h"
#include <cstring>
#include <cstdio>
#include <List.h>
#include <ByteOrder.h>
#include <arpa/inet.h>

//#define alloca malloc
#define YYDEBUG 1

#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL

extern FILE *yyin;

extern int yylex();

#define yyerror(s)	rez_error(s)

static RState *sState;
static RElem *head;

#define RS(s)	((RState *)(s))
#define RSV(s)	((RSValue *)(s))
#define LST(s)	((BList *)(s))
#define RE(s)	((REval *)(s))

#line 139 "Source/rez_parser.cpp"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "rez_parser.hpp"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_ALIGN = 3,                      /* ALIGN  */
  YYSYMBOL_ARRAY = 4,                      /* ARRAY  */
  YYSYMBOL_AS = 5,                         /* AS  */
  YYSYMBOL_BINARY = 6,                     /* BINARY  */
  YYSYMBOL_BITSTRING = 7,                  /* BITSTRING  */
  YYSYMBOL_BOOLEAN = 8,                    /* BOOLEAN  */
  YYSYMBOL_BYTE = 9,                       /* BYTE  */
  YYSYMBOL_CASE = 10,                      /* CASE  */
  YYSYMBOL_CHANGE = 11,                    /* CHANGE  */
  YYSYMBOL_CHAR = 12,                      /* CHAR  */
  YYSYMBOL_CSTRING = 13,                   /* CSTRING  */
  YYSYMBOL_DECIMAL = 14,                   /* DECIMAL  */
  YYSYMBOL_DELETE = 15,                    /* DELETE  */
  YYSYMBOL_DEREZ = 16,                     /* DEREZ  */
  YYSYMBOL_FILL = 17,                      /* FILL  */
  YYSYMBOL_HEX = 18,                       /* HEX  */
  YYSYMBOL_INTEGER = 19,                   /* INTEGER  */
  YYSYMBOL_LITERAL = 20,                   /* LITERAL  */
  YYSYMBOL_LONGINT = 21,                   /* LONGINT  */
  YYSYMBOL_OCTAL = 22,                     /* OCTAL  */
  YYSYMBOL_INCLUDE = 23,                   /* INCLUDE  */
  YYSYMBOL_POINT = 24,                     /* POINT  */
  YYSYMBOL_PSTRING = 25,                   /* PSTRING  */
  YYSYMBOL_READ = 26,                      /* READ  */
  YYSYMBOL_RECT = 27,                      /* RECT  */
  YYSYMBOL_RES = 28,                       /* RES  */
  YYSYMBOL_REZ = 29,                       /* REZ  */
  YYSYMBOL_STRING = 30,                    /* STRING  */
  YYSYMBOL_SWITCH = 31,                    /* SWITCH  */
  YYSYMBOL_rTYPE = 32,                     /* rTYPE  */
  YYSYMBOL_WSTRING = 33,                   /* WSTRING  */
  YYSYMBOL_LABEL = 34,                     /* LABEL  */
  YYSYMBOL_IDENT = 35,                     /* IDENT  */
  YYSYMBOL_REZVAR = 36,                    /* REZVAR  */
  YYSYMBOL_NUMBER = 37,                    /* NUMBER  */
  YYSYMBOL_AND = 38,                       /* AND  */
  YYSYMBOL_OR = 39,                        /* OR  */
  YYSYMBOL_EQ = 40,                        /* EQ  */
  YYSYMBOL_NE = 41,                        /* NE  */
  YYSYMBOL_LT = 42,                        /* LT  */
  YYSYMBOL_LE = 43,                        /* LE  */
  YYSYMBOL_GT = 44,                        /* GT  */
  YYSYMBOL_GE = 45,                        /* GE  */
  YYSYMBOL_SHR = 46,                       /* SHR  */
  YYSYMBOL_SHL = 47,                       /* SHL  */
  YYSYMBOL_STR_CONST = 48,                 /* STR_CONST  */
  YYSYMBOL_WIDE = 49,                      /* WIDE  */
  YYSYMBOL_BIT = 50,                       /* BIT  */
  YYSYMBOL_NIBBLE = 51,                    /* NIBBLE  */
  YYSYMBOL_WORD = 52,                      /* WORD  */
  YYSYMBOL_LONG = 53,                      /* LONG  */
  YYSYMBOL_KEY = 54,                       /* KEY  */
  YYSYMBOL_HEXSTRING = 55,                 /* HEXSTRING  */
  YYSYMBOL_UNSIGNED = 56,                  /* UNSIGNED  */
  YYSYMBOL_rEOF = 57,                      /* rEOF  */
  YYSYMBOL_COUNTOF = 58,                   /* COUNTOF  */
  YYSYMBOL_ARRAYINDEX = 59,                /* ARRAYINDEX  */
  YYSYMBOL_LBITFIELD = 60,                 /* LBITFIELD  */
  YYSYMBOL_LBYTE = 61,                     /* LBYTE  */
  YYSYMBOL_FORMAT = 62,                    /* FORMAT  */
  YYSYMBOL_RESSIZE = 63,                   /* RESSIZE  */
  YYSYMBOL_RESOURCE = 64,                  /* RESOURCE  */
  YYSYMBOL_LLONG = 65,                     /* LLONG  */
  YYSYMBOL_SHELL = 66,                     /* SHELL  */
  YYSYMBOL_LWORD = 67,                     /* LWORD  */
  YYSYMBOL_HEX_CONST = 68,                 /* HEX_CONST  */
  YYSYMBOL_69_ = 69,                       /* '|'  */
  YYSYMBOL_70_ = 70,                       /* '^'  */
  YYSYMBOL_71_ = 71,                       /* '&'  */
  YYSYMBOL_72_ = 72,                       /* '+'  */
  YYSYMBOL_73_ = 73,                       /* '-'  */
  YYSYMBOL_74_ = 74,                       /* '*'  */
  YYSYMBOL_75_ = 75,                       /* '/'  */
  YYSYMBOL_76_ = 76,                       /* '%'  */
  YYSYMBOL_NEGATE = 77,                    /* NEGATE  */
  YYSYMBOL_FLIP = 78,                      /* FLIP  */
  YYSYMBOL_NOT = 79,                       /* NOT  */
  YYSYMBOL_80_ = 80,                       /* '{'  */
  YYSYMBOL_81_ = 81,                       /* '}'  */
  YYSYMBOL_82_ = 82,                       /* ';'  */
  YYSYMBOL_83_ = 83,                       /* '='  */
  YYSYMBOL_84_ = 84,                       /* '['  */
  YYSYMBOL_85_ = 85,                       /* ']'  */
  YYSYMBOL_86_ = 86,                       /* ':'  */
  YYSYMBOL_87_ = 87,                       /* ','  */
  YYSYMBOL_88_ = 88,                       /* '('  */
  YYSYMBOL_89_ = 89,                       /* ')'  */
  YYSYMBOL_90_ = 90,                       /* '~'  */
  YYSYMBOL_91_ = 91,                       /* '!'  */
  YYSYMBOL_YYACCEPT = 92,                  /* $accept  */
  YYSYMBOL_s = 93,                         /* s  */
  YYSYMBOL_incl = 94,                      /* incl  */
  YYSYMBOL_type = 95,                      /* type  */
  YYSYMBOL_datadecl = 96,                  /* datadecl  */
  YYSYMBOL_datatype = 97,                  /* datatype  */
  YYSYMBOL_booleantype = 98,               /* booleantype  */
  YYSYMBOL_numerictype = 99,               /* numerictype  */
  YYSYMBOL_radix = 100,                    /* radix  */
  YYSYMBOL_numericsize = 101,              /* numericsize  */
  YYSYMBOL_chartype = 102,                 /* chartype  */
  YYSYMBOL_stringtype = 103,               /* stringtype  */
  YYSYMBOL_stringspecifier = 104,          /* stringspecifier  */
  YYSYMBOL_pointtype = 105,                /* pointtype  */
  YYSYMBOL_recttype = 106,                 /* recttype  */
  YYSYMBOL_arraytype = 107,                /* arraytype  */
  YYSYMBOL_switchtype = 108,               /* switchtype  */
  YYSYMBOL_casestmts = 109,                /* casestmts  */
  YYSYMBOL_casestmt = 110,                 /* casestmt  */
  YYSYMBOL_casebody = 111,                 /* casebody  */
  YYSYMBOL_caseline = 112,                 /* caseline  */
  YYSYMBOL_keytype = 113,                  /* keytype  */
  YYSYMBOL_filltype = 114,                 /* filltype  */
  YYSYMBOL_fillsize = 115,                 /* fillsize  */
  YYSYMBOL_aligntype = 116,                /* aligntype  */
  YYSYMBOL_alignsize = 117,                /* alignsize  */
  YYSYMBOL_symboliclist = 118,             /* symboliclist  */
  YYSYMBOL_symbolicvalue = 119,            /* symbolicvalue  */
  YYSYMBOL_e = 120,                        /* e  */
  YYSYMBOL_f = 121,                        /* f  */
  YYSYMBOL_resheader = 122,                /* resheader  */
  YYSYMBOL_readheader = 123,               /* readheader  */
  YYSYMBOL_datalist = 124,                 /* datalist  */
  YYSYMBOL_data = 125,                     /* data  */
  YYSYMBOL_dataarray = 126,                /* dataarray  */
  YYSYMBOL_switchdata = 127,               /* switchdata  */
  YYSYMBOL_hexconst = 128,                 /* hexconst  */
  YYSYMBOL_strconst = 129,                 /* strconst  */
  YYSYMBOL_fmt = 130,                      /* fmt  */
  YYSYMBOL_farg = 131                      /* farg  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  19
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   781

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  92
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  40
/* YYNRULES -- Number of rules.  */
#define YYNRULES  170
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  328

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   326


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    91,     2,     2,     2,    76,    71,     2,
      88,    89,    74,    72,    87,    73,     2,    75,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    86,    82,
       2,    83,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    84,     2,    85,    70,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    80,    69,    81,    90,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    77,    78,    79
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,    91,    91,    92,    93,    94,    95,    96,    99,   102,
     103,   106,   107,   108,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   124,   125,   126,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
     143,   144,   145,   146,   147,   150,   151,   152,   153,   154,
     157,   158,   159,   162,   163,   164,   165,   166,   167,   170,
     171,   172,   173,   174,   177,   178,   179,   182,   183,   184,
     187,   188,   189,   190,   191,   192,   195,   198,   199,   202,
     205,   206,   209,   210,   211,   214,   215,   216,   217,   218,
     219,   222,   223,   238,   239,   240,   241,   242,   245,   248,
     249,   250,   251,   254,   255,   258,   259,   260,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   290,   291,   293,   294,   295,   298,   309,
     322,   324,   328,   329,   330,   333,   335,   336,   337,   338,
     339,   342,   345,   348,   349,   366,   367,   376,   377,   380,
     381,   382,   383,   384,   385,   386,   387,   388,   389,   392,
     393
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "ALIGN", "ARRAY", "AS",
  "BINARY", "BITSTRING", "BOOLEAN", "BYTE", "CASE", "CHANGE", "CHAR",
  "CSTRING", "DECIMAL", "DELETE", "DEREZ", "FILL", "HEX", "INTEGER",
  "LITERAL", "LONGINT", "OCTAL", "INCLUDE", "POINT", "PSTRING", "READ",
  "RECT", "RES", "REZ", "STRING", "SWITCH", "rTYPE", "WSTRING", "LABEL",
  "IDENT", "REZVAR", "NUMBER", "AND", "OR", "EQ", "NE", "LT", "LE", "GT",
  "GE", "SHR", "SHL", "STR_CONST", "WIDE", "BIT", "NIBBLE", "WORD", "LONG",
  "KEY", "HEXSTRING", "UNSIGNED", "rEOF", "COUNTOF", "ARRAYINDEX",
  "LBITFIELD", "LBYTE", "FORMAT", "RESSIZE", "RESOURCE", "LLONG", "SHELL",
  "LWORD", "HEX_CONST", "'|'", "'^'", "'&'", "'+'", "'-'", "'*'", "'/'",
  "'%'", "NEGATE", "FLIP", "NOT", "'{'", "'}'", "';'", "'='", "'['", "']'",
  "':'", "','", "'('", "')'", "'~'", "'!'", "$accept", "s", "incl", "type",
  "datadecl", "datatype", "booleantype", "numerictype", "radix",
  "numericsize", "chartype", "stringtype", "stringspecifier", "pointtype",
  "recttype", "arraytype", "switchtype", "casestmts", "casestmt",
  "casebody", "caseline", "keytype", "filltype", "fillsize", "aligntype",
  "alignsize", "symboliclist", "symbolicvalue", "e", "f", "resheader",
  "readheader", "datalist", "data", "dataarray", "switchdata", "hexconst",
  "strconst", "fmt", "farg", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-160)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-25)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      96,  -160,    61,    -6,    -3,     3,  -160,    46,  -160,  -160,
      28,  -160,  -160,    37,    42,   -19,    81,    85,    11,  -160,
     -23,    64,   101,  -160,  -160,   141,   143,   151,   375,   107,
    -160,   104,   111,   118,   133,   138,  -160,   232,  -160,   232,
     232,   232,   611,  -160,    39,  -160,   -23,   -23,   171,   183,
     131,   159,   166,    74,    78,   182,   106,   -33,  -160,   184,
     -14,  -160,    13,  -160,  -160,    90,  -160,  -160,  -160,  -160,
    -160,    17,  -160,    18,  -160,   186,  -160,  -160,   274,  -160,
     407,    55,  -160,  -160,  -160,   196,    19,  -160,  -160,   -22,
    -160,  -160,  -160,  -160,  -160,  -160,  -160,   248,   249,   250,
     252,   254,  -160,  -160,   425,  -160,  -160,   232,   232,   232,
     232,   232,   232,   232,   232,   232,   232,   232,   232,   232,
     232,   232,   232,   232,   232,   209,   -23,   -23,    63,    65,
    -160,    -7,  -160,  -160,    61,    61,    61,  -160,  -160,  -160,
    -160,  -160,  -160,  -160,   215,   375,   232,   232,   213,   232,
     217,  -160,   232,   217,  -160,  -160,  -160,  -160,  -160,   218,
     220,   217,   220,   217,   293,    14,   196,    20,   224,   429,
      21,   232,   217,    61,   232,   217,   219,   223,   225,   226,
     227,  -160,   631,   650,   535,   535,   285,   285,   285,   285,
     187,   187,   668,   687,   705,   -48,   -48,  -160,  -160,  -160,
    -160,  -160,  -160,  -160,  -160,  -160,  -160,   230,   -29,   -16,
     -25,   375,    95,   464,   481,   221,   611,   272,   611,   232,
     -23,   -23,   278,    12,  -160,   238,   375,   232,    23,   232,
     217,  -160,  -160,   232,   217,   611,   183,   529,  -160,   282,
    -160,  -160,  -160,    -7,    61,  -160,  -160,   127,  -160,   241,
    -160,   611,   -23,  -160,   546,    73,   113,   240,  -160,  -160,
     375,   130,   594,   232,   217,   611,   611,    24,   257,   260,
      47,  -160,   375,   170,  -160,  -160,  -160,   321,   163,  -160,
     256,   611,    61,   217,   300,    -7,  -160,   172,  -160,  -160,
     177,  -160,   267,  -160,  -160,   375,   183,   261,   266,  -160,
    -160,  -160,  -160,  -160,  -160,  -160,   273,   216,   192,  -160,
      -7,   232,  -160,  -160,   275,   611,    -7,   276,    -7,   277,
      -7,   279,    -7,   280,    -7,   281,    -7,  -160
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     6,     0,     0,     0,     0,     7,     0,     5,     2,
       0,     4,   155,     0,     0,     0,     0,     0,     0,     1,
     150,     0,     0,   156,     8,     0,     0,     0,    24,   131,
     130,     0,     0,     0,     0,     0,   153,     0,   151,     0,
       0,     0,   145,   132,     0,   144,   150,   150,   147,   146,
       0,     0,     0,     0,     0,     0,     0,     0,    43,    45,
      25,    47,    50,    63,    41,     0,    40,    48,    44,    49,
      42,    64,    61,    67,    59,     0,    62,    12,     0,    60,
       0,     0,    11,    14,    16,     0,    28,    15,    17,    53,
      18,    19,    20,    21,    22,    23,   152,     0,     0,     0,
       0,     0,   131,   121,     0,   119,   120,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   150,   150,     0,     0,
     154,     0,   158,   157,     0,     0,     0,   139,    10,   100,
      99,   101,   102,    98,     0,    24,     0,     0,   105,     0,
      27,   103,     0,    52,    95,    93,    94,    96,    97,    91,
       0,    66,     0,    69,     0,     0,     0,    29,     0,    24,
      30,     0,    36,     0,     0,    57,     0,     0,     0,     0,
       0,   108,   122,   123,   128,   129,   124,   126,   125,   127,
     115,   114,   117,   118,   116,   109,   110,   111,   112,   113,
       3,   143,   142,   149,   148,   170,   169,   159,     0,     0,
       0,    24,     0,     0,     0,     0,    26,     0,    51,     0,
     150,   150,     0,     0,    78,     0,    24,     0,    31,     0,
      37,     9,    13,     0,    38,    32,    55,     0,   133,     0,
     135,   137,   136,     0,     0,   141,   138,     0,    70,     0,
      46,   106,   150,   104,     0,     0,     0,     0,    76,    77,
      24,     0,     0,     0,    39,    33,    34,    54,     0,   160,
       0,    72,    24,     0,    92,    65,    68,    24,     0,    71,
       0,    35,     0,    58,     0,     0,   140,     0,   107,    83,
       0,    82,     0,    81,    73,    24,    56,     0,   161,    74,
      85,    86,    87,    88,    89,    90,     0,    79,     0,   134,
       0,     0,    80,    75,   162,    84,     0,   163,     0,   164,
       0,   165,     0,   166,     0,   167,     0,   168
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -160,  -160,  -160,  -160,  -121,  -159,  -160,  -160,   289,   -74,
    -160,  -160,    75,  -160,  -160,  -160,  -160,  -160,   148,  -160,
      66,  -160,  -160,  -160,  -160,  -160,   -53,   155,   -36,  -160,
    -160,  -160,   -39,   150,  -145,  -160,  -160,    -2,  -160,  -154
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,     7,     8,     9,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,   223,   224,   292,
     293,   306,    94,   159,    95,   143,   150,   151,    42,    43,
      10,    11,    44,    45,    46,    47,    48,    49,    51,   207
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      15,   103,   144,   104,   105,   106,   167,   128,   129,   153,
     232,   170,    29,   148,    30,   220,    27,   221,   161,    23,
     163,   148,   222,    23,   212,    12,   122,   123,   124,    23,
     205,    16,    23,   172,    17,    31,   175,    32,    33,    13,
      18,   206,    34,    14,    35,    36,    19,   145,   148,   225,
      37,   146,   148,   148,   148,   148,   148,    38,   148,   148,
     244,   173,   174,    24,   246,    39,   245,    40,    41,   149,
     252,   182,   183,   184,   185,   186,   187,   188,   189,   190,
     191,   192,   193,   194,   195,   196,   197,   198,   199,   269,
     247,    28,   228,   258,   226,    23,   152,     1,   227,   154,
     160,   162,   171,   229,   233,   261,   263,   282,    20,    12,
     213,   214,    50,   216,   230,   139,   218,   234,   291,     2,
     125,   126,     3,    13,     4,    21,   127,    14,     5,   286,
      22,   298,   208,   209,   210,   235,   168,   169,   237,   278,
     155,   156,   157,   158,   203,   126,   204,   126,   291,    52,
     127,   287,   127,     6,   275,   126,   314,   140,   141,   142,
     127,   134,   317,   135,   319,   136,   321,   137,   323,    25,
     325,   236,   327,    26,   308,   264,   248,   169,    53,   251,
      54,   255,   256,   254,    59,   300,    61,    96,    55,   301,
      63,   262,    97,   265,   276,   126,    67,   266,    69,    98,
     127,   302,    72,    59,   303,    61,    99,    74,   271,   169,
      76,   279,   169,   273,   283,    67,   304,    69,   131,    56,
      57,   100,    58,    59,    60,    61,   101,   281,    62,    63,
      64,    23,    79,    65,    66,    67,    68,    69,    70,   130,
      71,    72,   270,    73,   294,   169,    74,    75,   132,    76,
     289,   288,   126,   299,   169,   133,   102,   127,    30,   120,
     121,   122,   123,   124,   138,    78,   164,   102,   147,    30,
     290,    79,    80,   313,   169,   315,   201,   202,   165,    31,
     296,    32,    33,   176,   177,   178,    34,   179,    35,   180,
      31,   200,    32,    33,    37,   211,   215,    34,   -24,    35,
      38,    38,   219,   222,   217,    37,   231,   148,   238,    39,
     239,    40,    41,   257,   240,   241,   242,   243,   260,   268,
      39,   272,    40,    41,    56,    57,   277,    58,    59,    60,
      61,   115,   116,    62,    63,    64,   295,   297,    65,    66,
      67,    68,    69,    70,   284,    71,    72,   285,    73,   307,
     309,    74,    75,   310,    76,   289,   311,   120,   121,   122,
     123,   124,   316,   318,   320,   305,   322,   324,   326,   166,
      78,   259,   253,   312,     0,   290,    79,    80,    56,    57,
       0,    58,    59,    60,    61,     0,     0,    62,    63,    64,
       0,     0,    65,    66,    67,    68,    69,    70,     0,    71,
      72,     0,    73,     0,     0,    74,    75,     0,    76,    77,
       0,     0,     0,    58,    59,     0,    61,     0,     0,     0,
       0,    64,     0,     0,    78,    66,    67,    68,    69,    70,
      79,    80,    56,    57,     0,    58,    59,    60,    61,     0,
       0,    62,    63,    64,     0,     0,    65,    66,    67,    68,
      69,    70,     0,    71,    72,     0,    73,     0,     0,    74,
      75,     0,    76,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,     0,     0,     0,     0,     0,    78,     0,
       0,     0,     0,     0,    79,    80,     0,     0,     0,     0,
       0,     0,     0,     0,   117,   118,   119,   120,   121,   122,
     123,   124,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,     0,     0,   181,     0,     0,     0,     0,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,     0,
       0,     0,     0,   117,   118,   119,   120,   121,   122,   123,
     124,     0,     0,     0,     0,     0,     0,     0,     0,   249,
     117,   118,   119,   120,   121,   122,   123,   124,     0,     0,
       0,     0,     0,     0,     0,     0,   250,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   111,   112,   113,
     114,   115,   116,     0,   107,   108,   109,   110,   111,   112,
     113,   114,   115,   116,     0,     0,     0,     0,   117,   118,
     119,   120,   121,   122,   123,   124,     0,   120,   121,   122,
     123,   124,     0,     0,   267,   117,   118,   119,   120,   121,
     122,   123,   124,     0,     0,     0,     0,     0,     0,     0,
       0,   274,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,     0,     0,     0,     0,     0,     0,     0,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,     0,
       0,     0,     0,   117,   118,   119,   120,   121,   122,   123,
     124,   109,   110,   111,   112,   113,   114,   115,   116,   280,
     117,   118,   119,   120,   121,   122,   123,   124,   107,     0,
     109,   110,   111,   112,   113,   114,   115,   116,     0,     0,
     117,   118,   119,   120,   121,   122,   123,   124,   109,   110,
     111,   112,   113,   114,   115,   116,     0,     0,     0,   117,
     118,   119,   120,   121,   122,   123,   124,   109,   110,   111,
     112,   113,   114,   115,   116,     0,     0,     0,   118,   119,
     120,   121,   122,   123,   124,   109,   110,   111,   112,   113,
     114,   115,   116,     0,     0,     0,     0,     0,   119,   120,
     121,   122,   123,   124,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   120,   121,   122,
     123,   124
};

static const yytype_int16 yycheck[] =
{
       2,    37,    35,    39,    40,    41,    80,    46,    47,    62,
     169,    85,    35,    35,    37,   160,     5,   162,    71,    48,
      73,    35,    10,    48,   145,    48,    74,    75,    76,    48,
      37,    37,    48,    86,    37,    58,    89,    60,    61,    62,
      37,    48,    65,    66,    67,    68,     0,    80,    35,    35,
      73,    84,    35,    35,    35,    35,    35,    80,    35,    35,
      89,    83,    84,    82,    89,    88,    82,    90,    91,    83,
     215,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   243,
     211,    80,   166,    81,    80,    48,    83,     1,    84,     9,
      83,    83,    83,    83,    83,   226,    83,    83,    80,    48,
     146,   147,    48,   149,   167,     9,   152,   170,   277,    23,
      81,    82,    26,    62,    28,    88,    87,    66,    32,    82,
      88,   285,   134,   135,   136,   171,    81,    82,   174,   260,
      50,    51,    52,    53,    81,    82,    81,    82,   307,    48,
      87,   272,    87,    57,    81,    82,   310,    51,    52,    53,
      87,    87,   316,    89,   318,    87,   320,    89,   322,    88,
     324,   173,   326,    88,   295,   228,    81,    82,    37,   215,
      37,   220,   221,   219,     7,     8,     9,    80,    37,    12,
      13,   227,    88,   229,    81,    82,    19,   233,    21,    88,
      87,    24,    25,     7,    27,     9,    88,    30,    81,    82,
      33,    81,    82,   252,   267,    19,   290,    21,    87,     3,
       4,    88,     6,     7,     8,     9,    88,   263,    12,    13,
      14,    48,    55,    17,    18,    19,    20,    21,    22,    68,
      24,    25,   244,    27,    81,    82,    30,    31,    89,    33,
      34,    81,    82,    81,    82,    89,    35,    87,    37,    72,
      73,    74,    75,    76,    82,    49,    80,    35,    84,    37,
      54,    55,    56,    81,    82,   311,   126,   127,     4,    58,
     282,    60,    61,    35,    35,    35,    65,    35,    67,    35,
      58,    82,    60,    61,    73,    80,    83,    65,    82,    67,
      80,    80,    84,    10,    87,    73,    82,    35,    89,    88,
      87,    90,    91,    35,    89,    89,    89,    87,    80,    37,
      88,    80,    90,    91,     3,     4,    86,     6,     7,     8,
       9,    46,    47,    12,    13,    14,    80,    37,    17,    18,
      19,    20,    21,    22,    87,    24,    25,    87,    27,    82,
      89,    30,    31,    87,    33,    34,    83,    72,    73,    74,
      75,    76,    87,    87,    87,   290,    87,    87,    87,    80,
      49,   223,   217,   307,    -1,    54,    55,    56,     3,     4,
      -1,     6,     7,     8,     9,    -1,    -1,    12,    13,    14,
      -1,    -1,    17,    18,    19,    20,    21,    22,    -1,    24,
      25,    -1,    27,    -1,    -1,    30,    31,    -1,    33,    34,
      -1,    -1,    -1,     6,     7,    -1,     9,    -1,    -1,    -1,
      -1,    14,    -1,    -1,    49,    18,    19,    20,    21,    22,
      55,    56,     3,     4,    -1,     6,     7,     8,     9,    -1,
      -1,    12,    13,    14,    -1,    -1,    17,    18,    19,    20,
      21,    22,    -1,    24,    25,    -1,    27,    -1,    -1,    30,
      31,    -1,    33,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    -1,    -1,    -1,    -1,    -1,    49,    -1,
      -1,    -1,    -1,    -1,    55,    56,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    69,    70,    71,    72,    73,    74,
      75,    76,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    -1,    -1,    89,    -1,    -1,    -1,    -1,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    -1,
      -1,    -1,    -1,    69,    70,    71,    72,    73,    74,    75,
      76,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    85,
      69,    70,    71,    72,    73,    74,    75,    76,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    85,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    42,    43,    44,
      45,    46,    47,    -1,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    -1,    -1,    -1,    -1,    69,    70,
      71,    72,    73,    74,    75,    76,    -1,    72,    73,    74,
      75,    76,    -1,    -1,    85,    69,    70,    71,    72,    73,
      74,    75,    76,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    85,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    -1,
      -1,    -1,    -1,    69,    70,    71,    72,    73,    74,    75,
      76,    40,    41,    42,    43,    44,    45,    46,    47,    85,
      69,    70,    71,    72,    73,    74,    75,    76,    38,    -1,
      40,    41,    42,    43,    44,    45,    46,    47,    -1,    -1,
      69,    70,    71,    72,    73,    74,    75,    76,    40,    41,
      42,    43,    44,    45,    46,    47,    -1,    -1,    -1,    69,
      70,    71,    72,    73,    74,    75,    76,    40,    41,    42,
      43,    44,    45,    46,    47,    -1,    -1,    -1,    70,    71,
      72,    73,    74,    75,    76,    40,    41,    42,    43,    44,
      45,    46,    47,    -1,    -1,    -1,    -1,    -1,    71,    72,
      73,    74,    75,    76,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    72,    73,    74,
      75,    76
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,    23,    26,    28,    32,    57,    93,    94,    95,
     122,   123,    48,    62,    66,   129,    37,    37,    37,     0,
      80,    88,    88,    48,    82,    88,    88,     5,    80,    35,
      37,    58,    60,    61,    65,    67,    68,    73,    80,    88,
      90,    91,   120,   121,   124,   125,   126,   127,   128,   129,
      48,   130,    48,    37,    37,    37,     3,     4,     6,     7,
       8,     9,    12,    13,    14,    17,    18,    19,    20,    21,
      22,    24,    25,    27,    30,    31,    33,    34,    49,    55,
      56,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   114,   116,    80,    88,    88,    88,
      88,    88,    35,   120,   120,   120,   120,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    69,    70,    71,
      72,    73,    74,    75,    76,    81,    82,    87,   124,   124,
      68,    87,    89,    89,    87,    89,    87,    89,    82,     9,
      51,    52,    53,   117,    35,    80,    84,    84,    35,    83,
     118,   119,    83,   118,     9,    50,    51,    52,    53,   115,
      83,   118,    83,   118,    80,     4,   100,   101,    81,    82,
     101,    83,   118,    83,    84,   118,    35,    35,    35,    35,
      35,    89,   120,   120,   120,   120,   120,   120,   120,   120,
     120,   120,   120,   120,   120,   120,   120,   120,   120,   120,
      82,   125,   125,    81,    81,    37,    48,   131,   129,   129,
     129,    80,    96,   120,   120,    83,   120,    87,   120,    84,
     126,   126,    10,   109,   110,    35,    80,    84,   101,    83,
     118,    82,    97,    83,   118,   120,   129,   120,    89,    87,
      89,    89,    89,    87,    89,    82,    89,    96,    81,    85,
      85,   120,   126,   119,   120,   124,   124,    35,    81,   110,
      80,    96,   120,    83,   118,   120,   120,    85,    37,   131,
     129,    81,    80,   124,    85,    81,    81,    86,    96,    81,
      85,   120,    83,   118,    87,    87,    82,    96,    81,    34,
      54,    97,   111,   112,    81,    80,   129,    37,   131,    81,
       8,    12,    24,    27,   101,   104,   113,    82,    96,    89,
      87,    83,   112,    81,   131,   120,    87,   131,    87,   131,
      87,   131,    87,   131,    87,   131,    87,   131
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,    92,    93,    93,    93,    93,    93,    93,    94,    95,
      95,    96,    96,    96,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    98,    98,    98,    99,    99,
      99,    99,    99,    99,    99,    99,    99,    99,    99,    99,
     100,   100,   100,   100,   100,   101,   101,   101,   101,   101,
     102,   102,   102,   103,   103,   103,   103,   103,   103,   104,
     104,   104,   104,   104,   105,   105,   105,   106,   106,   106,
     107,   107,   107,   107,   107,   107,   108,   109,   109,   110,
     111,   111,   112,   112,   112,   113,   113,   113,   113,   113,
     113,   114,   114,   115,   115,   115,   115,   115,   116,   117,
     117,   117,   117,   118,   118,   119,   119,   119,   120,   120,
     120,   120,   120,   120,   120,   120,   120,   120,   120,   120,
     120,   120,   120,   120,   120,   120,   120,   120,   120,   120,
     120,   120,   120,   121,   121,   121,   121,   121,   122,   122,
     123,   123,   124,   124,   124,   125,   125,   125,   125,   125,
     125,   126,   127,   128,   128,   129,   129,   129,   129,   130,
     130,   130,   130,   130,   130,   130,   130,   130,   130,   131,
     131
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     5,     1,     1,     1,     1,     3,     6,
       5,     1,     1,     3,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     0,     1,     3,     2,     1,     2,
       2,     3,     3,     4,     4,     5,     2,     3,     3,     4,
       1,     1,     1,     1,     1,     1,     4,     1,     1,     1,
       1,     3,     2,     1,     4,     3,     6,     2,     5,     1,
       1,     1,     1,     1,     1,     5,     2,     1,     5,     2,
       4,     5,     5,     6,     7,     8,     4,     2,     1,     5,
       3,     1,     1,     1,     4,     1,     1,     1,     1,     1,
       1,     2,     5,     1,     1,     1,     1,     1,     2,     1,
       1,     1,     1,     1,     3,     1,     3,     5,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     2,
       2,     2,     3,     3,     3,     3,     3,     3,     3,     3,
       1,     1,     1,     4,     8,     4,     4,     4,     7,     5,
       9,     7,     3,     3,     1,     1,     1,     1,     3,     3,
       0,     1,     2,     1,     2,     1,     2,     4,     4,     3,
       5,     7,     9,    11,    13,    15,    17,    19,    21,     1,
       1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* s: type  */
#line 91 "Source/rez_parser.y"
                                                                                                        { YYACCEPT; }
#line 1576 "Source/rez_parser.cpp"
    break;

  case 3: /* s: resheader '{' datalist '}' ';'  */
#line 92 "Source/rez_parser.y"
                                                                                { if (head) head->Write(); WriteResource(yyvsp[-4]); YYACCEPT; }
#line 1582 "Source/rez_parser.cpp"
    break;

  case 4: /* s: readheader  */
#line 93 "Source/rez_parser.y"
                                                                                                        { YYACCEPT; }
#line 1588 "Source/rez_parser.cpp"
    break;

  case 5: /* s: incl  */
#line 94 "Source/rez_parser.y"
                                                                                                        { YYACCEPT; }
#line 1594 "Source/rez_parser.cpp"
    break;

  case 6: /* s: error  */
#line 95 "Source/rez_parser.y"
                                                                                                        { return 1; }
#line 1600 "Source/rez_parser.cpp"
    break;

  case 7: /* s: rEOF  */
#line 96 "Source/rez_parser.y"
                                                                                                        { return 1; }
#line 1606 "Source/rez_parser.cpp"
    break;

  case 8: /* incl: INCLUDE strconst ';'  */
#line 99 "Source/rez_parser.y"
                                                                                        { Include((char *)yyvsp[-1]); }
#line 1612 "Source/rez_parser.cpp"
    break;

  case 9: /* type: rTYPE NUMBER '{' datadecl '}' ';'  */
#line 102 "Source/rez_parser.y"
                                                                                { RState::FinishType(yyvsp[-4], RS(yyvsp[-2])); }
#line 1618 "Source/rez_parser.cpp"
    break;

  case 10: /* type: rTYPE NUMBER AS NUMBER ';'  */
#line 103 "Source/rez_parser.y"
                                                                                                { RState::CopyType(yyvsp[-3], yyvsp[-1]); }
#line 1624 "Source/rez_parser.cpp"
    break;

  case 13: /* datadecl: datadecl ';' datatype  */
#line 108 "Source/rez_parser.y"
                                                                                                { yyval = yyvsp[-2]; if (yyvsp[0]) RS(yyvsp[-2])->SetNext(RS(yyvsp[0])); }
#line 1630 "Source/rez_parser.cpp"
    break;

  case 24: /* datatype: %empty  */
#line 121 "Source/rez_parser.y"
                                                                                                                        { yyval = 0; }
#line 1636 "Source/rez_parser.cpp"
    break;

  case 25: /* booleantype: BOOLEAN  */
#line 124 "Source/rez_parser.y"
                                                                                                        { yyval = (long)new RSNrValue(8); }
#line 1642 "Source/rez_parser.cpp"
    break;

  case 26: /* booleantype: BOOLEAN '=' e  */
#line 125 "Source/rez_parser.y"
                                                                                                                { yyval = (long)new RSNrValue(8); RSV(yyval)->SetDefaultValue(yyvsp[0]); }
#line 1648 "Source/rez_parser.cpp"
    break;

  case 27: /* booleantype: BOOLEAN symboliclist  */
#line 126 "Source/rez_parser.y"
                                                                                                { yyval = (long)new RSNrValue(8); RSV(yyval)->AddIdentifiers(LST(yyvsp[0])); }
#line 1654 "Source/rez_parser.cpp"
    break;

  case 29: /* numerictype: UNSIGNED numericsize  */
#line 130 "Source/rez_parser.y"
                                                                                                { yyval = yyvsp[0]; }
#line 1660 "Source/rez_parser.cpp"
    break;

  case 30: /* numerictype: radix numericsize  */
#line 131 "Source/rez_parser.y"
                                                                                                        { yyval = yyvsp[0]; }
#line 1666 "Source/rez_parser.cpp"
    break;

  case 31: /* numerictype: UNSIGNED radix numericsize  */
#line 132 "Source/rez_parser.y"
                                                                                                { yyval = yyvsp[0]; }
#line 1672 "Source/rez_parser.cpp"
    break;

  case 32: /* numerictype: numericsize '=' e  */
#line 133 "Source/rez_parser.y"
                                                                                                        { yyval = yyvsp[-2]; RSV(yyval)->SetDefaultValue(yyvsp[0]); }
#line 1678 "Source/rez_parser.cpp"
    break;

  case 33: /* numerictype: UNSIGNED numericsize '=' e  */
#line 134 "Source/rez_parser.y"
                                                                                                { yyval = yyvsp[-2]; RSV(yyval)->SetDefaultValue(yyvsp[0]); }
#line 1684 "Source/rez_parser.cpp"
    break;

  case 34: /* numerictype: radix numericsize '=' e  */
#line 135 "Source/rez_parser.y"
                                                                                                { yyval = yyvsp[-2]; RSV(yyval)->SetDefaultValue(yyvsp[0]); }
#line 1690 "Source/rez_parser.cpp"
    break;

  case 35: /* numerictype: UNSIGNED radix numericsize '=' e  */
#line 136 "Source/rez_parser.y"
                                                                                        { yyval = yyvsp[-2]; RSV(yyval)->SetDefaultValue(yyvsp[0]); }
#line 1696 "Source/rez_parser.cpp"
    break;

  case 36: /* numerictype: numericsize symboliclist  */
#line 137 "Source/rez_parser.y"
                                                                                                { yyval = yyvsp[-1]; RSV(yyval)->AddIdentifiers(LST(yyvsp[0])); }
#line 1702 "Source/rez_parser.cpp"
    break;

  case 37: /* numerictype: UNSIGNED numericsize symboliclist  */
#line 138 "Source/rez_parser.y"
                                                                                        { yyval = yyvsp[-1]; RSV(yyval)->AddIdentifiers(LST(yyvsp[0])); }
#line 1708 "Source/rez_parser.cpp"
    break;

  case 38: /* numerictype: radix numericsize symboliclist  */
#line 139 "Source/rez_parser.y"
                                                                                        { yyval = yyvsp[-1]; RSV(yyval)->AddIdentifiers(LST(yyvsp[0])); }
#line 1714 "Source/rez_parser.cpp"
    break;

  case 39: /* numerictype: UNSIGNED radix numericsize symboliclist  */
#line 140 "Source/rez_parser.y"
                                                                                { yyval = yyvsp[-1]; RSV(yyval)->AddIdentifiers(LST(yyvsp[0])); }
#line 1720 "Source/rez_parser.cpp"
    break;

  case 45: /* numericsize: BITSTRING  */
#line 150 "Source/rez_parser.y"
                                                                                                        { yyval = (long)new RSNrValue(32); }
#line 1726 "Source/rez_parser.cpp"
    break;

  case 46: /* numericsize: BITSTRING '[' e ']'  */
#line 151 "Source/rez_parser.y"
                                                                                                        { yyval = (long)new RSNrValue(REvaluate(RE(yyvsp[-1]), head)); }
#line 1732 "Source/rez_parser.cpp"
    break;

  case 47: /* numericsize: BYTE  */
#line 152 "Source/rez_parser.y"
                                                                                                                { yyval = (long)new RSNrValue(8); }
#line 1738 "Source/rez_parser.cpp"
    break;

  case 48: /* numericsize: INTEGER  */
#line 153 "Source/rez_parser.y"
                                                                                                                { yyval = (long)new RSNrValue(16); }
#line 1744 "Source/rez_parser.cpp"
    break;

  case 49: /* numericsize: LONGINT  */
#line 154 "Source/rez_parser.y"
                                                                                                                { yyval = (long)new RSNrValue(32); }
#line 1750 "Source/rez_parser.cpp"
    break;

  case 50: /* chartype: CHAR  */
#line 157 "Source/rez_parser.y"
                                                                                                        { yyval = (long)new RSNrValue(8); }
#line 1756 "Source/rez_parser.cpp"
    break;

  case 51: /* chartype: CHAR '=' e  */
#line 158 "Source/rez_parser.y"
                                                                                                                { yyval = (long)new RSNrValue(8); RSV(yyval)->SetDefaultValue(yyvsp[0]); }
#line 1762 "Source/rez_parser.cpp"
    break;

  case 52: /* chartype: CHAR symboliclist  */
#line 159 "Source/rez_parser.y"
                                                                                                        { yyval = (long)new RSNrValue(8); RSV(yyval)->AddIdentifiers(LST(yyvsp[0])); }
#line 1768 "Source/rez_parser.cpp"
    break;

  case 53: /* stringtype: stringspecifier  */
#line 162 "Source/rez_parser.y"
                                                                                                { yyval = (long)new RSStringValue(yyvsp[0], 0); }
#line 1774 "Source/rez_parser.cpp"
    break;

  case 54: /* stringtype: stringspecifier '[' e ']'  */
#line 163 "Source/rez_parser.y"
                                                                                                { yyval = (long)new RSStringValue(yyvsp[-3], REvaluate(RE(yyvsp[-1]), head)); }
#line 1780 "Source/rez_parser.cpp"
    break;

  case 55: /* stringtype: stringspecifier '=' strconst  */
#line 164 "Source/rez_parser.y"
                                                                                        { yyval = (long)new RSStringValue(yyvsp[-2], 0); RSV(yyval)->SetDefaultValue(yyvsp[0]); }
#line 1786 "Source/rez_parser.cpp"
    break;

  case 56: /* stringtype: stringspecifier '[' e ']' '=' strconst  */
#line 165 "Source/rez_parser.y"
                                                                                { yyval = (long)new RSStringValue(yyvsp[-5], REvaluate(RE(yyvsp[-3]), head)); RSV(yyval)->SetDefaultValue(yyvsp[0]); }
#line 1792 "Source/rez_parser.cpp"
    break;

  case 57: /* stringtype: stringspecifier symboliclist  */
#line 166 "Source/rez_parser.y"
                                                                                        { yyval = (long)new RSStringValue(yyvsp[-1], 0); RSV(yyval)->AddIdentifiers(LST(yyvsp[0])); }
#line 1798 "Source/rez_parser.cpp"
    break;

  case 58: /* stringtype: stringspecifier '[' e ']' symboliclist  */
#line 167 "Source/rez_parser.y"
                                                                                { yyval = (long)new RSStringValue(yyvsp[-4], REvaluate(RE(yyvsp[-2]), head)); RSV(yyval)->AddIdentifiers(LST(yyvsp[0])); }
#line 1804 "Source/rez_parser.cpp"
    break;

  case 59: /* stringspecifier: STRING  */
#line 170 "Source/rez_parser.y"
                                                                                                { yyval = RSStringValue::skStr; }
#line 1810 "Source/rez_parser.cpp"
    break;

  case 60: /* stringspecifier: HEXSTRING  */
#line 171 "Source/rez_parser.y"
                                                                                                                { yyval = RSStringValue::skHex; }
#line 1816 "Source/rez_parser.cpp"
    break;

  case 61: /* stringspecifier: PSTRING  */
#line 172 "Source/rez_parser.y"
                                                                                                                { yyval = RSStringValue::skPStr; }
#line 1822 "Source/rez_parser.cpp"
    break;

  case 62: /* stringspecifier: WSTRING  */
#line 173 "Source/rez_parser.y"
                                                                                                                { yyval = RSStringValue::skWStr; }
#line 1828 "Source/rez_parser.cpp"
    break;

  case 63: /* stringspecifier: CSTRING  */
#line 174 "Source/rez_parser.y"
                                                                                                                { yyval = RSStringValue::skCStr; }
#line 1834 "Source/rez_parser.cpp"
    break;

  case 64: /* pointtype: POINT  */
#line 177 "Source/rez_parser.y"
                                                                                                        { yyval = (long)new RSArray(new RSNrValue(16), 0, 2); }
#line 1840 "Source/rez_parser.cpp"
    break;

  case 65: /* pointtype: POINT '=' dataarray datalist '}'  */
#line 178 "Source/rez_parser.y"
                                                                                        { rez_error("Unimplemented constant declaration"); }
#line 1846 "Source/rez_parser.cpp"
    break;

  case 66: /* pointtype: POINT symboliclist  */
#line 179 "Source/rez_parser.y"
                                                                                        { rez_error("Unimplemented constant declaration"); }
#line 1852 "Source/rez_parser.cpp"
    break;

  case 67: /* recttype: RECT  */
#line 182 "Source/rez_parser.y"
                                                                                                        { yyval = (long)new RSArray(new RSNrValue(16), 0, 4); }
#line 1858 "Source/rez_parser.cpp"
    break;

  case 68: /* recttype: RECT '=' dataarray datalist '}'  */
#line 183 "Source/rez_parser.y"
                                                                                        { rez_error("Unimplemented constant declaration"); }
#line 1864 "Source/rez_parser.cpp"
    break;

  case 69: /* recttype: RECT symboliclist  */
#line 184 "Source/rez_parser.y"
                                                                                        { rez_error("Unimplemented constant declaration"); }
#line 1870 "Source/rez_parser.cpp"
    break;

  case 70: /* arraytype: ARRAY '{' datadecl '}'  */
#line 187 "Source/rez_parser.y"
                                                                                        { yyval = (long)new RSArray(RS(yyvsp[-1])); }
#line 1876 "Source/rez_parser.cpp"
    break;

  case 71: /* arraytype: WIDE ARRAY '{' datadecl '}'  */
#line 188 "Source/rez_parser.y"
                                                                                                { yyval = (long)new RSArray(RS(yyvsp[-1])); }
#line 1882 "Source/rez_parser.cpp"
    break;

  case 72: /* arraytype: ARRAY IDENT '{' datadecl '}'  */
#line 189 "Source/rez_parser.y"
                                                                                        { yyval = (long)new RSArray(RS(yyvsp[-1]), yyvsp[-3]); }
#line 1888 "Source/rez_parser.cpp"
    break;

  case 73: /* arraytype: WIDE ARRAY IDENT '{' datadecl '}'  */
#line 190 "Source/rez_parser.y"
                                                                                        { yyval = (long)new RSArray(RS(yyvsp[-1]), yyvsp[-3]); }
#line 1894 "Source/rez_parser.cpp"
    break;

  case 74: /* arraytype: ARRAY '[' e ']' '{' datadecl '}'  */
#line 191 "Source/rez_parser.y"
                                                                                        { yyval = (long)new RSArray(RS(yyvsp[-1]), 0, yyvsp[-4]); }
#line 1900 "Source/rez_parser.cpp"
    break;

  case 75: /* arraytype: WIDE ARRAY '[' e ']' '{' datadecl '}'  */
#line 192 "Source/rez_parser.y"
                                                                                { yyval = (long)new RSArray(RS(yyvsp[-1]), 0, yyvsp[-5]); }
#line 1906 "Source/rez_parser.cpp"
    break;

  case 76: /* switchtype: SWITCH '{' casestmts '}'  */
#line 195 "Source/rez_parser.y"
                                                                                        { yyval = (long)new RSSwitch(LST(yyvsp[-1])); }
#line 1912 "Source/rez_parser.cpp"
    break;

  case 77: /* casestmts: casestmts casestmt  */
#line 198 "Source/rez_parser.y"
                                                                                                { yyval = yyvsp[-1]; LST(yyvsp[-1])->AddItem(RS(yyvsp[0])); }
#line 1918 "Source/rez_parser.cpp"
    break;

  case 78: /* casestmts: casestmt  */
#line 199 "Source/rez_parser.y"
                                                                                                                { yyval = (long)new BList; LST(yyval)->AddItem(RS(yyvsp[0])); }
#line 1924 "Source/rez_parser.cpp"
    break;

  case 79: /* casestmt: CASE IDENT ':' casebody ';'  */
#line 202 "Source/rez_parser.y"
                                                                                        { yyval = (long)new RCase(yyvsp[-3], RS(yyvsp[-1])); }
#line 1930 "Source/rez_parser.cpp"
    break;

  case 80: /* casebody: casebody ';' caseline  */
#line 205 "Source/rez_parser.y"
                                                                                        { yyval = yyvsp[-2]; RS(yyvsp[-2])->SetNext(RS(yyvsp[0])); }
#line 1936 "Source/rez_parser.cpp"
    break;

  case 84: /* caseline: KEY keytype '=' e  */
#line 211 "Source/rez_parser.y"
                                                                                                        { yyval = yyvsp[-2]; RSV(yyval)->SetDefaultValue(yyvsp[0]); }
#line 1942 "Source/rez_parser.cpp"
    break;

  case 91: /* filltype: FILL fillsize  */
#line 222 "Source/rez_parser.y"
                                                                                                { yyval = (long)new RSNrValue(yyvsp[0]); RSV(yyval)->SetDefaultValue((long)RValue(0)); }
#line 1948 "Source/rez_parser.cpp"
    break;

  case 92: /* filltype: FILL fillsize '[' e ']'  */
#line 223 "Source/rez_parser.y"
                                                                                                {
																int cnt = REvaluate(RE(yyvsp[-1]), head);
																RSNrValue *s, *t = NULL;

																while (cnt--)
																{
																	s = new RSNrValue(yyvsp[-3]);
																	s->SetDefaultValue((long)RValue(0));
																	s->SetNext(t);
																	t = s;
																}
																yyval = (long)s;
															}
#line 1966 "Source/rez_parser.cpp"
    break;

  case 93: /* fillsize: BIT  */
#line 238 "Source/rez_parser.y"
                                                                                                                { yyval = 1; }
#line 1972 "Source/rez_parser.cpp"
    break;

  case 94: /* fillsize: NIBBLE  */
#line 239 "Source/rez_parser.y"
                                                                                                                { yyval = 4; }
#line 1978 "Source/rez_parser.cpp"
    break;

  case 95: /* fillsize: BYTE  */
#line 240 "Source/rez_parser.y"
                                                                                                                { yyval = 8; }
#line 1984 "Source/rez_parser.cpp"
    break;

  case 96: /* fillsize: WORD  */
#line 241 "Source/rez_parser.y"
                                                                                                                { yyval = 16; }
#line 1990 "Source/rez_parser.cpp"
    break;

  case 97: /* fillsize: LONG  */
#line 242 "Source/rez_parser.y"
                                                                                                                { yyval = 32; }
#line 1996 "Source/rez_parser.cpp"
    break;

  case 103: /* symboliclist: symbolicvalue  */
#line 254 "Source/rez_parser.y"
                                                                                        { yyval = (long)new BList; LST(yyval)->AddItem(RS(yyvsp[0])); }
#line 2002 "Source/rez_parser.cpp"
    break;

  case 104: /* symboliclist: symboliclist ',' symbolicvalue  */
#line 255 "Source/rez_parser.y"
                                                                                        { yyval = yyvsp[-2]; LST(yyvsp[-2])->AddItem(RS(yyvsp[0])); }
#line 2008 "Source/rez_parser.cpp"
    break;

  case 105: /* symbolicvalue: IDENT  */
#line 258 "Source/rez_parser.y"
                                                                                                { yyval = (long)new RSymbol(yyvsp[0], 0); }
#line 2014 "Source/rez_parser.cpp"
    break;

  case 106: /* symbolicvalue: IDENT '=' e  */
#line 259 "Source/rez_parser.y"
                                                                                                                { yyval = (long)new RSymbol(yyvsp[-2], REvaluate(RE(yyvsp[0]), head)); }
#line 2020 "Source/rez_parser.cpp"
    break;

  case 107: /* symbolicvalue: IDENT '=' dataarray datalist '}'  */
#line 260 "Source/rez_parser.y"
                                                                                        { yyval = (long)new RSymbol(yyvsp[-4], 0); }
#line 2026 "Source/rez_parser.cpp"
    break;

  case 108: /* e: '(' e ')'  */
#line 263 "Source/rez_parser.y"
                                                                                                                { yyval = yyvsp[-1]; }
#line 2032 "Source/rez_parser.cpp"
    break;

  case 109: /* e: e '+' e  */
#line 264 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoPlus); }
#line 2038 "Source/rez_parser.cpp"
    break;

  case 110: /* e: e '-' e  */
#line 265 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoMinus); }
#line 2044 "Source/rez_parser.cpp"
    break;

  case 111: /* e: e '*' e  */
#line 266 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoMultiply); }
#line 2050 "Source/rez_parser.cpp"
    break;

  case 112: /* e: e '/' e  */
#line 267 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoDivide); }
#line 2056 "Source/rez_parser.cpp"
    break;

  case 113: /* e: e '%' e  */
#line 268 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoModulus); }
#line 2062 "Source/rez_parser.cpp"
    break;

  case 114: /* e: e SHL e  */
#line 269 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoSHL); }
#line 2068 "Source/rez_parser.cpp"
    break;

  case 115: /* e: e SHR e  */
#line 270 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoSHR); }
#line 2074 "Source/rez_parser.cpp"
    break;

  case 116: /* e: e '&' e  */
#line 271 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoBitAnd); }
#line 2080 "Source/rez_parser.cpp"
    break;

  case 117: /* e: e '|' e  */
#line 272 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoBitOr); }
#line 2086 "Source/rez_parser.cpp"
    break;

  case 118: /* e: e '^' e  */
#line 273 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoXPwrY); }
#line 2092 "Source/rez_parser.cpp"
    break;

  case 119: /* e: '~' e  */
#line 274 "Source/rez_parser.y"
                                                                                                        { yyval = (long)RUnaryOp(RE(yyvsp[0]), reoFlip); }
#line 2098 "Source/rez_parser.cpp"
    break;

  case 120: /* e: '!' e  */
#line 275 "Source/rez_parser.y"
                                                                                                        { yyval = (long)RUnaryOp(RE(yyvsp[0]), reoNot); }
#line 2104 "Source/rez_parser.cpp"
    break;

  case 121: /* e: '-' e  */
#line 276 "Source/rez_parser.y"
                                                                                                        { yyval = (long)RUnaryOp(RE(yyvsp[0]), reoNegate); }
#line 2110 "Source/rez_parser.cpp"
    break;

  case 122: /* e: e AND e  */
#line 277 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoAnd); }
#line 2116 "Source/rez_parser.cpp"
    break;

  case 123: /* e: e OR e  */
#line 278 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoOr); }
#line 2122 "Source/rez_parser.cpp"
    break;

  case 124: /* e: e LT e  */
#line 279 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoLT); }
#line 2128 "Source/rez_parser.cpp"
    break;

  case 125: /* e: e GT e  */
#line 280 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoGT); }
#line 2134 "Source/rez_parser.cpp"
    break;

  case 126: /* e: e LE e  */
#line 281 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoLE); }
#line 2140 "Source/rez_parser.cpp"
    break;

  case 127: /* e: e GE e  */
#line 282 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoGE); }
#line 2146 "Source/rez_parser.cpp"
    break;

  case 128: /* e: e EQ e  */
#line 283 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoEQ); }
#line 2152 "Source/rez_parser.cpp"
    break;

  case 129: /* e: e NE e  */
#line 284 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RBinaryOp(RE(yyvsp[-2]), RE(yyvsp[0]), reoNE); }
#line 2158 "Source/rez_parser.cpp"
    break;

  case 130: /* e: NUMBER  */
#line 285 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RValue(yyvsp[0]); }
#line 2164 "Source/rez_parser.cpp"
    break;

  case 131: /* e: IDENT  */
#line 286 "Source/rez_parser.y"
                                                                                                                { yyval = (long)RIdentifier(yyvsp[0]); }
#line 2170 "Source/rez_parser.cpp"
    break;

  case 133: /* f: COUNTOF '(' IDENT ')'  */
#line 290 "Source/rez_parser.y"
                                                                                                { yyval = (long)RFunction(refCountOf, yyvsp[-1]); }
#line 2176 "Source/rez_parser.cpp"
    break;

  case 134: /* f: LBITFIELD '(' IDENT ',' NUMBER ',' NUMBER ')'  */
#line 292 "Source/rez_parser.y"
                                                                                                                        { yyval = (long)RFunction(refCopyBits, yyvsp[-5], yyvsp[-3], yyvsp[-1]); }
#line 2182 "Source/rez_parser.cpp"
    break;

  case 135: /* f: LBYTE '(' IDENT ')'  */
#line 293 "Source/rez_parser.y"
                                                                                                        { yyval = (long)RFunction(refCopyBits, yyvsp[-1], 0, 8); }
#line 2188 "Source/rez_parser.cpp"
    break;

  case 136: /* f: LWORD '(' IDENT ')'  */
#line 294 "Source/rez_parser.y"
                                                                                                        { yyval = (long)RFunction(refCopyBits, yyvsp[-1], 0, 16); }
#line 2194 "Source/rez_parser.cpp"
    break;

  case 137: /* f: LLONG '(' IDENT ')'  */
#line 295 "Source/rez_parser.y"
                                                                                                        { yyval = (long)RFunction(refCopyBits, yyvsp[-1], 0, 32); }
#line 2200 "Source/rez_parser.cpp"
    break;

  case 138: /* resheader: RES NUMBER '(' NUMBER ',' strconst ')'  */
#line 299 "Source/rez_parser.y"
                                                                                                                        {
																sState = RState::FirstState(yyvsp[-5]);
																if (!sState)
																{
																	int t = ntohl(yyvsp[-5]);
																	rez_error("Undefined resource type: %4.4s", &t);
																}
																head = NULL;
																yyval = (long)new ResHeader(yyvsp[-5], yyvsp[-3], yyvsp[-1]);
															}
#line 2215 "Source/rez_parser.cpp"
    break;

  case 139: /* resheader: RES NUMBER '(' NUMBER ')'  */
#line 310 "Source/rez_parser.y"
                                                                                                                        {
																sState = RState::FirstState(yyvsp[-3]);
																if (!sState)
																{
																	int t = ntohl(yyvsp[-3]);
																	rez_error("Undefined resource type: %4.4s", &t);
																}
																head = NULL;
																yyval = (long)new ResHeader((long)yyvsp[-3], (long)yyvsp[-1], (long)NULL);
															}
#line 2230 "Source/rez_parser.cpp"
    break;

  case 140: /* readheader: READ NUMBER '(' NUMBER ',' strconst ')' strconst ';'  */
#line 323 "Source/rez_parser.y"
                                                                                                                        { WriteResource((char *)yyvsp[-1], yyvsp[-7], yyvsp[-5], (char *)yyvsp[-3]); }
#line 2236 "Source/rez_parser.cpp"
    break;

  case 141: /* readheader: READ NUMBER '(' NUMBER ')' strconst ';'  */
#line 325 "Source/rez_parser.y"
                                                                                                                        { WriteResource((char *)yyvsp[-1], yyvsp[-5], yyvsp[-3], NULL); }
#line 2242 "Source/rez_parser.cpp"
    break;

  case 145: /* data: e  */
#line 333 "Source/rez_parser.y"
                                                                                                                { sState = sState->Shift(yyvsp[0], tInt, &head); }
#line 2248 "Source/rez_parser.cpp"
    break;

  case 146: /* data: strconst  */
#line 335 "Source/rez_parser.y"
                                                                                                                { sState = sState->Shift(yyvsp[0], tString, &head); free((char *)yyvsp[0]); }
#line 2254 "Source/rez_parser.cpp"
    break;

  case 147: /* data: hexconst  */
#line 336 "Source/rez_parser.y"
                                                                                                                { sState = sState->Shift(yyvsp[0], tRaw, &head); free((char *)yyvsp[0]); }
#line 2260 "Source/rez_parser.cpp"
    break;

  case 149: /* data: dataarray datalist '}'  */
#line 338 "Source/rez_parser.y"
                                                                                                { sState = sState->Shift(0, tArrayEnd, &head); }
#line 2266 "Source/rez_parser.cpp"
    break;

  case 151: /* dataarray: '{'  */
#line 342 "Source/rez_parser.y"
                                                                                                                { sState = sState->Shift(0, tArray, &head); }
#line 2272 "Source/rez_parser.cpp"
    break;

  case 152: /* switchdata: IDENT '{'  */
#line 345 "Source/rez_parser.y"
                                                                                                        { sState = sState->Shift(yyvsp[-1], tCase, &head); }
#line 2278 "Source/rez_parser.cpp"
    break;

  case 154: /* hexconst: hexconst HEX_CONST  */
#line 349 "Source/rez_parser.y"
                                                                                                        { char *t, *a, *b;
															  long sa, sb;

															  a = (char *)yyvsp[-1];	sa = *(long *)a;
															  b = (char *)yyvsp[0];	sb = *(long *)b;

															  t = (char *)malloc(sa + sb + sizeof(long));
															  if (!t) rez_error("insufficient memory");

															  memcpy(t + sizeof(long), a + sizeof(long), sa);
															  memcpy(t + sizeof(long) + sa, b + sizeof(long), sb);
															  *(long*)t = sa + sb;

															  free(a); free(b);
															  yyval = (long)t; }
#line 2298 "Source/rez_parser.cpp"
    break;

  case 156: /* strconst: strconst STR_CONST  */
#line 367 "Source/rez_parser.y"
                                                                                                        { char *t, *a, *b;
															  a = (char *)yyvsp[-1];
															  b = (char *)yyvsp[0];
															  t = (char *)malloc(strlen(a) + strlen(b) + 1);
															  if (!t) rez_error("insufficient memory");
															  strcpy(t, a);
															  strcat(t, b);
															  free(a); free(b);
															  yyval = (long)t; }
#line 2312 "Source/rez_parser.cpp"
    break;

  case 157: /* strconst: SHELL '(' STR_CONST ')'  */
#line 376 "Source/rez_parser.y"
                                                                                                { yyval = (long)strdup(getenv((char *)yyvsp[-1])); }
#line 2318 "Source/rez_parser.cpp"
    break;

  case 158: /* strconst: FORMAT '(' fmt ')'  */
#line 377 "Source/rez_parser.y"
                                                                                                        { yyval = yyvsp[-1]; }
#line 2324 "Source/rez_parser.cpp"
    break;

  case 159: /* fmt: STR_CONST ',' farg  */
#line 380 "Source/rez_parser.y"
                                                                                                        { char b[1024]; sprintf(b, (char *)yyvsp[-2], yyvsp[0]); yyval = (long)strdup(b); }
#line 2330 "Source/rez_parser.cpp"
    break;

  case 160: /* fmt: STR_CONST ',' farg ',' farg  */
#line 381 "Source/rez_parser.y"
                                                                                                                { char b[1024]; sprintf(b, (char *)yyvsp[-4], yyvsp[-2], yyvsp[0]); yyval = (long)strdup(b); }
#line 2336 "Source/rez_parser.cpp"
    break;

  case 161: /* fmt: STR_CONST ',' farg ',' farg ',' farg  */
#line 382 "Source/rez_parser.y"
                                                                                                                        { char b[1024]; sprintf(b, (char *)yyvsp[-6], yyvsp[-4], yyvsp[-2], yyvsp[0]); yyval = (long)strdup(b); }
#line 2342 "Source/rez_parser.cpp"
    break;

  case 162: /* fmt: STR_CONST ',' farg ',' farg ',' farg ',' farg  */
#line 383 "Source/rez_parser.y"
                                                                                                                                { char b[1024]; sprintf(b, (char *)yyvsp[-8], yyvsp[-6], yyvsp[-4], yyvsp[-2], yyvsp[0]); yyval = (long)strdup(b); }
#line 2348 "Source/rez_parser.cpp"
    break;

  case 163: /* fmt: STR_CONST ',' farg ',' farg ',' farg ',' farg ',' farg  */
#line 384 "Source/rez_parser.y"
                                                                                                                                        { char b[1024]; sprintf(b, (char *)yyvsp[-10], yyvsp[-8], yyvsp[-6], yyvsp[-4], yyvsp[-2], yyvsp[0]); yyval = (long)strdup(b); }
#line 2354 "Source/rez_parser.cpp"
    break;

  case 164: /* fmt: STR_CONST ',' farg ',' farg ',' farg ',' farg ',' farg ',' farg  */
#line 385 "Source/rez_parser.y"
                                                                                                                                                { char b[1024]; sprintf(b, (char *)yyvsp[-12], yyvsp[-10], yyvsp[-8], yyvsp[-6], yyvsp[-4], yyvsp[-2], yyvsp[0]); yyval = (long)strdup(b); }
#line 2360 "Source/rez_parser.cpp"
    break;

  case 165: /* fmt: STR_CONST ',' farg ',' farg ',' farg ',' farg ',' farg ',' farg ',' farg  */
#line 386 "Source/rez_parser.y"
                                                                                                                                                                { char b[1024]; sprintf(b, (char *)yyvsp[-14], yyvsp[-12], yyvsp[-10], yyvsp[-8], yyvsp[-6], yyvsp[-4], yyvsp[-2], yyvsp[0]); yyval = (long)strdup(b); }
#line 2366 "Source/rez_parser.cpp"
    break;

  case 166: /* fmt: STR_CONST ',' farg ',' farg ',' farg ',' farg ',' farg ',' farg ',' farg ',' farg  */
#line 387 "Source/rez_parser.y"
                                                                                                                                                                        { char b[1024]; sprintf(b, (char *)yyvsp[-16], yyvsp[-14], yyvsp[-12], yyvsp[-10], yyvsp[-8], yyvsp[-6], yyvsp[-4], yyvsp[-2], yyvsp[0]); yyval = (long)strdup(b); }
#line 2372 "Source/rez_parser.cpp"
    break;

  case 167: /* fmt: STR_CONST ',' farg ',' farg ',' farg ',' farg ',' farg ',' farg ',' farg ',' farg ',' farg  */
#line 388 "Source/rez_parser.y"
                                                                                                                                                                                { char b[1024]; sprintf(b, (char *)yyvsp[-18], yyvsp[-16], yyvsp[-14], yyvsp[-12], yyvsp[-10], yyvsp[-8], yyvsp[-6], yyvsp[-4], yyvsp[-2], yyvsp[0]); yyval = (long)strdup(b); }
#line 2378 "Source/rez_parser.cpp"
    break;

  case 168: /* fmt: STR_CONST ',' farg ',' farg ',' farg ',' farg ',' farg ',' farg ',' farg ',' farg ',' farg ',' farg  */
#line 389 "Source/rez_parser.y"
                                                                                                                                                                                        { char b[1024]; sprintf(b, (char *)yyvsp[-20], yyvsp[-18], yyvsp[-16], yyvsp[-14], yyvsp[-12], yyvsp[-10], yyvsp[-8], yyvsp[-6], yyvsp[-4], yyvsp[-2], yyvsp[0]); yyval = (long)strdup(b); }
#line 2384 "Source/rez_parser.cpp"
    break;


#line 2388 "Source/rez_parser.cpp"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 396 "Source/rez_parser.y"



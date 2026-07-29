/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_SOURCE_REZ_PARSER_HPP_INCLUDED
# define YY_YY_SOURCE_REZ_PARSER_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    ALIGN = 258,                   /* ALIGN  */
    ARRAY = 259,                   /* ARRAY  */
    AS = 260,                      /* AS  */
    BINARY = 261,                  /* BINARY  */
    BITSTRING = 262,               /* BITSTRING  */
    BOOLEAN = 263,                 /* BOOLEAN  */
    BYTE = 264,                    /* BYTE  */
    CASE = 265,                    /* CASE  */
    CHANGE = 266,                  /* CHANGE  */
    CHAR = 267,                    /* CHAR  */
    CSTRING = 268,                 /* CSTRING  */
    DECIMAL = 269,                 /* DECIMAL  */
    DELETE = 270,                  /* DELETE  */
    DEREZ = 271,                   /* DEREZ  */
    FILL = 272,                    /* FILL  */
    HEX = 273,                     /* HEX  */
    INTEGER = 274,                 /* INTEGER  */
    LITERAL = 275,                 /* LITERAL  */
    LONGINT = 276,                 /* LONGINT  */
    OCTAL = 277,                   /* OCTAL  */
    INCLUDE = 278,                 /* INCLUDE  */
    POINT = 279,                   /* POINT  */
    PSTRING = 280,                 /* PSTRING  */
    READ = 281,                    /* READ  */
    RECT = 282,                    /* RECT  */
    RES = 283,                     /* RES  */
    REZ = 284,                     /* REZ  */
    STRING = 285,                  /* STRING  */
    SWITCH = 286,                  /* SWITCH  */
    rTYPE = 287,                   /* rTYPE  */
    WSTRING = 288,                 /* WSTRING  */
    LABEL = 289,                   /* LABEL  */
    IDENT = 290,                   /* IDENT  */
    REZVAR = 291,                  /* REZVAR  */
    NUMBER = 292,                  /* NUMBER  */
    AND = 293,                     /* AND  */
    OR = 294,                      /* OR  */
    EQ = 295,                      /* EQ  */
    NE = 296,                      /* NE  */
    LT = 297,                      /* LT  */
    LE = 298,                      /* LE  */
    GT = 299,                      /* GT  */
    GE = 300,                      /* GE  */
    SHR = 301,                     /* SHR  */
    SHL = 302,                     /* SHL  */
    STR_CONST = 303,               /* STR_CONST  */
    WIDE = 304,                    /* WIDE  */
    BIT = 305,                     /* BIT  */
    NIBBLE = 306,                  /* NIBBLE  */
    WORD = 307,                    /* WORD  */
    LONG = 308,                    /* LONG  */
    KEY = 309,                     /* KEY  */
    HEXSTRING = 310,               /* HEXSTRING  */
    UNSIGNED = 311,                /* UNSIGNED  */
    rEOF = 312,                    /* rEOF  */
    COUNTOF = 313,                 /* COUNTOF  */
    ARRAYINDEX = 314,              /* ARRAYINDEX  */
    LBITFIELD = 315,               /* LBITFIELD  */
    LBYTE = 316,                   /* LBYTE  */
    FORMAT = 317,                  /* FORMAT  */
    RESSIZE = 318,                 /* RESSIZE  */
    RESOURCE = 319,                /* RESOURCE  */
    LLONG = 320,                   /* LLONG  */
    SHELL = 321,                   /* SHELL  */
    LWORD = 322,                   /* LWORD  */
    HEX_CONST = 323,               /* HEX_CONST  */
    NEGATE = 324,                  /* NEGATE  */
    FLIP = 325,                    /* FLIP  */
    NOT = 326                      /* NOT  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_SOURCE_REZ_PARSER_HPP_INCLUDED  */

/*==============================================================
 * pl0x.h  --  扩展版 PL/0 编译程序头文件
 * 在清华大学出版社《编译原理》(王生原等) 附录 A C 版基础上改写
 * 第 5 组  三级项目
 *==============================================================*/
#ifndef PL0X_H
#define PL0X_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

typedef enum { false, true } bool;

/*---------------- 容量参数 ----------------*/
#define norw      27        /* 保留字个数 */
#define txmax    200        /* 名字表容量 */
#define nmax       9        /* 无符号整数的最大位数 */
#define al        16        /* 标识符最大长度 */
#define amax    4095        /* 地址上界 */
#define levmax     4        /* 最大过程嵌套层数 */
#define cxmax   2000        /* 目标代码区容量 */
#define stacksize 2000      /* 运行栈容量 */
#define MAXTEMP    8        /* 每个分程序预留的循环上界临时单元数 */
#define MAXSTR    64        /* 字符串常量池容量 */
#define MAXSTRLEN 128       /* 单个字符串常量最大长度 */
#define MAXLINE  512        /* 源程序单行最大长度 */

/*---------------- 单词符号 ----------------*/
enum symbol {
    nul,        ident,      number,     string,
    plus,       minus,      times,      slash,      percent,
    eql,        neq,        lss,        leq,        gtr,        geq,
    becomes,    pluseq,     minuseq,    timeseq,    slasheq,    percenteq,
    plusplus,   minusminus,
    andsym,     orsym,      notsym,
    lparen,     rparen,     lbracket,   rbracket,
    comma,      semicolon,  period,     colon,
    beginsym,   endsym,     ifsym,      thensym,    elsesym,
    whilesym,   dosym,      forsym,     tosym,      downtosym,
    repeatsym,  untilsym,
    readsym,    writesym,   callsym,
    constsym,   varsym,     arraysym,   procsym,    oddsym,
    sqrtSym,    printsym
};
#define symnum (printsym + 1)

/*---------------- 名字表项类别 ----------------*/
enum object { constant, variable, array, procedur };

/*---------------- 虚拟机指令 ----------------*/
enum fct {
    lit,    /* 0  取常数 a 压栈                    */
    opr,    /* 1  执行 a 号运算                     */
    lod,    /* 2  取变量 l,a 的值压栈               */
    sto,    /* 3  栈顶存入变量 l,a                  */
    cal,    /* 4  调用 l 层 a 号过程                */
    Int,    /* 5  栈顶指针增加 a（开辟数据区）       */
    jmp,    /* 6  无条件转移到 a                    */
    jpc,    /* 7  栈顶为 0 则转移到 a               */
    lda,    /* 8  取变量 l,a 的地址压栈             */
    ldi,    /* 9  栈顶为地址，取出该地址的内容      */
    sti,    /* 10 次栈顶为地址，栈顶为值，间接存入  */
    chk,    /* 11 数组下标越界检查，a 为上界        */
    wrs     /* 12 输出 a 号字符串常量               */
};
#define fctnum 13

/* opr 指令的运算编码 */
#define OPR_RET      0
#define OPR_NEG      1
#define OPR_ADD      2
#define OPR_SUB      3
#define OPR_MUL      4
#define OPR_DIV      5
#define OPR_ODD      6
#define OPR_EQU      8
#define OPR_NEQ      9
#define OPR_LES     10
#define OPR_GEQ     11
#define OPR_GTR     12
#define OPR_LEQ     13
#define OPR_WRITE   14
#define OPR_WRITELN 15
#define OPR_READ    16
#define OPR_MOD     17
#define OPR_AND     18
#define OPR_OR      19
#define OPR_NOT     20
#define OPR_SQRT    21
#define OPR_DUP     22

struct instruction {
    int f;      /* 操作码 */
    int l;      /* 层次差 */
    int a;      /* 操作数 */
};

struct tablestruct {
    char name[al + 1];  /* 名字 */
    int  kind;          /* 类别 */
    int  val;           /* 常量值 */
    int  level;         /* 所在层次 */
    int  adr;           /* 相对地址 */
    int  size;          /* 占用单元数（数组/过程） */
};

typedef bool BoolSet[symnum];

/*---------------- 全局变量 ----------------*/
extern FILE *fa;    /* 目标代码清单 */
extern FILE *fas;   /* 名字表清单 */
extern FILE *fa2;   /* 运行结果清单 */
extern FILE *flist; /* 源程序清单 */

extern char  ch;            /* 当前字符 */
extern int   sym;           /* 当前单词符号 */
extern char  id[al + 1];    /* 当前标识符 */
extern int   num;           /* 当前整数 */
extern char  strconst[MAXSTRLEN]; /* 当前字符串常量 */

extern int   cc, ll;        /* 列号、行号 */
extern int   cx;            /* 目标代码指针 */
extern int   err;           /* 错误计数 */
extern int   tx, lev, dx;   /* 名字表指针、层次、数据区指针 */
extern int   tempbase, tempcnt;  /* 循环上界临时单元 */

extern struct instruction code[cxmax];
extern struct tablestruct  table[txmax];
extern char  mnemonic[fctnum][5];
extern char  strpool[MAXSTR][MAXSTRLEN];
extern int   strcnt;

extern bool  declbegsys[symnum];
extern bool  statbegsys[symnum];
extern bool  facbegsys[symnum];
extern bool  relset[symnum];

/*---------------- 函数原型 ----------------*/
void error(int n);
void fatal(const char *msg);
void getch(void);
char peekch(void);
void getsym(void);
void init(void);
void gen(int f, int l, int a);
void test(BoolSet s1, BoolSet s2, int n);
int  inset(int e, BoolSet s);
void setclr(BoolSet s);
void setadd(BoolSet s, int e);
void setcopy(BoolSet r, BoolSet s);
void setunion(BoolSet r, BoolSet s1, BoolSet s2);
void block(int lev, BoolSet fsys);
int  position(char *idt);
void enter(int k);
void constdeclaration(void);
void vardeclaration(void);
void statement(BoolSet fsys);
void expression(BoolSet fsys);
void term(BoolSet fsys);
void factor(BoolSet fsys);
void condition(BoolSet fsys, int needRel);
void condand(BoolSet fsys, int needRel);
void condrel(BoolSet fsys, int needRel);
void listcode(int cx0, int cx1);
void interpret(void);
int  base(int l, int *s, int b);
void dumptable(void);

#endif

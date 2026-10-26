/*==============================================================
 * pl0x.c  --  扩展版 PL/0 编译程序（编译 + 解释执行）
 * 基线：王生原等《编译原理》(第3版) 附录 A C 版 PL/0 源程序
 * 扩充：注释、下划线标识符、字符串常量、一维数组、if-else、
 *       for-to/downto、repeat-until、复合赋值、++/--、%、逻辑
 *       运算、开方、中文报错与行列出错定位、名字表/代码表输出
 * 第 5 组  三级项目
 *==============================================================*/
#include "pl0x.h"

/*---------------- 全局变量定义 ----------------*/
FILE *fa, *fas, *fa2, *flist;

char  ch;
int   sym;
char  id[al + 1];
int   num;
char  strconst[MAXSTRLEN];
int   strinx;

int   cc, ll;
int   tkline = 1, tkcol = 1;   /* 当前单词的起始行列 */
int   cx;
int   err;
int   tx, lev, dx;
int   tempbase, tempcnt;
int   gsize;            /* 数组声明时暂存元素个数 */

struct instruction code[cxmax];
struct tablestruct  table[txmax];
char  mnemonic[fctnum][5];
char  strpool[MAXSTR][MAXSTRLEN];
int   strcnt = 0;

bool  declbegsys[symnum];
bool  statbegsys[symnum];
bool  facbegsys[symnum];
bool  relset[symnum];

/* 源程序缓冲区 */
static char *srcbuf = NULL;
static int   srclen = 0, srcpos = 0;

/*---------------- 错误信息表（中文） ----------------*/
static const char *err_msg[] = {
    "",
    "常数说明中应为“=”",                       /* 1  */
    "应为无符号整数",                           /* 2  */
    "常数说明中应为“=”或“:=”",                 /* 3  */
    "应为标识符",                               /* 4  */
    "缺少“,”或“;”",                            /* 5  */
    "过程说明有误：应为 begin",                 /* 6  */
    "应为语句",                                 /* 7  */
    "语句之后出现非法符号",                     /* 8  */
    "程序应以“.”结束",                          /* 9  */
    "应为“;”",                                  /* 10 */
    "标识符未定义",                             /* 11 */
    "不能给常量或过程名赋值",                   /* 12 */
    "应为赋值号“:=”",                           /* 13 */
    "call 之后应为标识符",                      /* 14 */
    "call 之后不能是常量或变量",                /* 15 */
    "应为“then”",                               /* 16 */
    "应为“;”或“end”",                           /* 17 */
    "应为“do”",                                 /* 18 */
    "非法符号",                                 /* 19 */
    "应为关系运算符",                           /* 20 */
    "表达式中不能出现过程名",                   /* 21 */
    "应为“)”",                                  /* 22 */
    "因子之后出现非法符号",                     /* 23 */
    "该符号不能作为表达式的开始",               /* 24 */
    "无符号整数位数过多",                       /* 25 */
    "应为“]”",                                  /* 26 */
    "数组元素个数必须为正整数",                 /* 27 */
    "下标只能用于数组名",                       /* 28 */
    "数组下标必须为整型表达式",                 /* 29 */
    "名字表溢出（标识符过多）",                 /* 30 */
    "数值超出允许范围",                         /* 31 */
    "过程嵌套层数超过 levmax",                  /* 32 */
    "应为“(”",                                  /* 33 */
    "read 语句中应为变量",                      /* 34 */
    "for 语句控制变量必须为整型变量",           /* 35 */
    "循环嵌套过深，临时单元不足",               /* 36 */
    "应为“:=”或复合赋值运算符",                 /* 37 */
    "缺少“until”",                              /* 38 */
    "字符串常量过长或过多",                     /* 39 */
    "目标代码区溢出（程序过长）",               /* 40 */
    "该语法成分尚未在本版本中实现",             /* 41 */
    "应为“to”或“downto”"                        /* 42 */
};

/*---------------- 致命错误出口 ----------------*/
void fatal(const char *msg)
{
    printf("\n[致命错误] 第 %d 行：%s\n", ll, msg);
    if (flist) fprintf(flist, "\n[致命错误] 第 %d 行：%s\n", ll, msg);
    if (fa)  fclose(fa);
    if (fas) fclose(fas);
    if (fa2) fclose(fa2);
    if (flist) fclose(flist);
    exit(1);
}

/*---------------- 错误处理 ----------------*/
void error(int n)
{
    if (n < 1 || n > (int)(sizeof(err_msg) / sizeof(err_msg[0])) - 1)
        n = 19;
    printf("  *** 第 %d 行 第 %d 列：错误 %d —— %s\n", tkline, tkcol, n, err_msg[n]);
    if (flist)
        fprintf(flist, "  *** 第 %d 行 第 %d 列：错误 %d —— %s\n", tkline, tkcol, n, err_msg[n]);
    err++;
}

/*---------------- 字符输入 ----------------*/
void getch(void)
{
    if (srcpos < srclen) {
        ch = srcbuf[srcpos++];
        if (ch == '\n') { ll++; cc = 0; }
        else cc++;
    } else {
        ch = 0;
    }
}

char peekch(void)
{
    return (srcpos < srclen) ? srcbuf[srcpos] : 0;
}

/*---------------- 跳过空白与注释 ----------------*/
static void skipspace(void)
{
    for (;;) {
        while (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') getch();
        if (ch == '/' && peekch() == '/') {          /* 单行注释 */
            while (ch && ch != '\n') getch();
            continue;
        }
        if (ch == '/' && peekch() == '*') {          /* 块注释 */
            getch(); getch();
            while (ch && !(ch == '*' && peekch() == '/')) getch();
            if (ch) { getch(); getch(); }
            else error(19);
            continue;
        }
        break;
    }
}

/*---------------- 词法分析 ----------------*/
typedef struct { const char *w; int s; } KeyWord;

static const KeyWord keywords[norw] = {
    {"and", andsym},   {"array", arraysym}, {"begin", beginsym}, {"call", callsym},
    {"const", constsym},{"div", slash},     {"do", dosym},       {"downto", downtosym},
    {"else", elsesym}, {"end", endsym},     {"for", forsym},     {"if", ifsym},
    {"mod", percent},  {"not", notsym},     {"odd", oddsym},     {"or", orsym},
    {"print", printsym}, {"procedure", procsym}, {"read", readsym}, {"repeat", repeatsym}, {"sqrt", sqrtSym},
    {"then", thensym}, {"to", tosym},       {"until", untilsym}, {"var", varsym},
    {"while", whilesym}, {"write", writesym}
};

void getsym(void)
{
    int k, i;

    skipspace();
    tkline = ll; tkcol = cc;      /* 记录单词起点，供报错定位 */

    if (isalpha((unsigned char)ch) || ch == '_') {          /* 标识符或保留字 */
        k = 0;
        while (isalnum((unsigned char)ch) || ch == '_') {
            if (k < al) { id[k++] = (char)tolower((unsigned char)ch); }
            getch();
        }
        id[k] = '\0';
        sym = ident;
        for (i = 0; i < norw; i++) {
            if (strcmp(id, keywords[i].w) == 0) { sym = keywords[i].s; break; }
        }
        return;
    }

    if (isdigit((unsigned char)ch)) {                        /* 无符号整数 */
        k = 0; num = 0; sym = number;
        while (isdigit((unsigned char)ch)) {
            if (k < nmax) num = num * 10 + (ch - '0');
            k++;
            getch();
        }
        if (k > nmax) error(25);
        return;
    }

    if (ch == '\'') {                                        /* 字符串常量 */
        k = 0; getch();
        while (ch && ch != '\'' && ch != '\n') {
            if (k < MAXSTRLEN - 1) strconst[k++] = ch;
            getch();
        }
        strconst[k] = '\0';
        if (ch == '\'') getch(); else error(39);
        sym = string;
        for (i = 0; i < strcnt; i++)
            if (strcmp(strpool[i], strconst) == 0) { strinx = i; return; }
        if (strcnt >= MAXSTR) { error(39); strinx = 0; return; }
        strcpy(strpool[strcnt], strconst);
        strinx = strcnt++;
        return;
    }

    switch (ch) {
    case ':':
        getch();
        if (ch == '=') { sym = becomes; getch(); } else sym = colon;
        break;
    case '<':
        getch();
        if (ch == '=')      { sym = leq;  getch(); }
        else if (ch == '>') { sym = neq;  getch(); }
        else                  sym = lss;
        break;
    case '>':
        getch();
        if (ch == '=') { sym = geq; getch(); } else sym = gtr;
        break;
    case '=':
        getch();
        if (ch == '=') getch();
        sym = eql;
        break;
    case '!':
        getch();
        if (ch == '=') { sym = neq; getch(); } else sym = notsym;
        break;
    case '+':
        getch();
        if (ch == '=')      { sym = pluseq;    getch(); }
        else if (ch == '+') { sym = plusplus;  getch(); }
        else                  sym = plus;
        break;
    case '-':
        getch();
        if (ch == '=')      { sym = minuseq;   getch(); }
        else if (ch == '-') { sym = minusminus;getch(); }
        else                  sym = minus;
        break;
    case '*':
        getch();
        if (ch == '=') { sym = timeseq; getch(); } else sym = times;
        break;
    case '/':
        getch();
        if (ch == '=') { sym = slasheq; getch(); } else sym = slash;
        break;
    case '%':
        getch();
        if (ch == '=') { sym = percenteq; getch(); } else sym = percent;
        break;
    case '&':
        getch(); if (ch == '&') getch();
        sym = andsym;
        break;
    case '|':
        getch(); if (ch == '|') getch();
        sym = orsym;
        break;
    case '(': sym = lparen;    getch(); break;
    case ')': sym = rparen;    getch(); break;
    case '[': sym = lbracket;  getch(); break;
    case ']': sym = rbracket;  getch(); break;
    case ',': sym = comma;     getch(); break;
    case ';': sym = semicolon; getch(); break;
    case '.': sym = period;    getch(); break;
    default:
        error(19);
        getch();
        sym = nul;
        break;
    }
}

/*---------------- 目标代码生成 ----------------*/
void gen(int f, int l, int a)
{
    if (cx >= cxmax) fatal("目标代码区溢出，程序过长");
    code[cx].f = f;
    code[cx].l = l;
    code[cx].a = a;
    cx++;
}

/*---------------- 集合运算 ----------------*/
int inset(int e, BoolSet s) { return s[e]; }

void setclr(BoolSet s) { memset(s, 0, sizeof(bool) * symnum); }

void setadd(BoolSet s, int e) { s[e] = true; }

void setcopy(BoolSet r, BoolSet s) { memcpy(r, s, sizeof(bool) * symnum); }

void setunion(BoolSet r, BoolSet s1, BoolSet s2)
{
    int i;
    for (i = 0; i < symnum; i++) r[i] = s1[i] || s2[i];
}

void test(BoolSet s1, BoolSet s2, int n)
{
    BoolSet u;
    int guard = 0;

    if (!inset(sym, s1)) {
        error(n);
        setunion(u, s1, s2);
        while (!inset(sym, u) && sym != nul) {
            getsym();
            if (++guard > 4096) break;      /* 防止死循环 */
        }
    }
}

/*---------------- 名字表操作 ----------------*/
int position(char *idt)
{
    int i = tx;
    strcpy(table[0].name, idt);
    while (strcmp(table[i].name, idt) != 0) i--;
    return i;
}

void enter(int k)
{
    tx++;
    if (tx >= txmax) { error(30); tx--; return; }
    strcpy(table[tx].name, id);
    table[tx].kind = k;
    switch (k) {
    case constant:
        if (num > amax) { error(31); num = 0; }
        table[tx].val = num;
        break;
    case variable:
        table[tx].level = lev;
        table[tx].adr   = dx;
        table[tx].size  = 1;
        dx++;
        break;
    case array:
        if (gsize <= 0) gsize = 1;
        table[tx].level = lev;
        table[tx].adr   = dx;
        table[tx].size  = gsize;
        dx += gsize;
        break;
    case procedur:
        table[tx].level = lev;
        break;
    }
}

/*---------------- 说明部分的处理 ----------------*/
void constdeclaration(void)
{
    if (sym == ident) {
        getsym();
        if (sym == eql || sym == becomes) {
            if (sym == becomes) error(1);
            getsym();
            if (sym == number) { enter(constant); getsym(); }
            else error(2);
        } else error(3);
    } else error(4);
}

void vardeclaration(void)
{
    char nm[al + 1];

    if (sym != ident) { error(4); return; }
    strcpy(nm, id);
    getsym();
    if (sym == lbracket) {              /* 一维数组说明 */
        getsym();
        if (sym == number) { gsize = num; getsym(); }
        else { error(2); gsize = 1; }
        if (sym == rbracket) getsym(); else error(26);
        if (gsize <= 0) { error(27); gsize = 1; }
        strcpy(id, nm);
        enter(array);
    } else {
        strcpy(id, nm);
        enter(variable);
    }
}

/*---------------- 表达式分析（后三层递归） ----------------*/
void factor(BoolSet fsys)
{
    BoolSet nxtlev;
    int i;

    test(facbegsys, fsys, 24);
    if (inset(sym, facbegsys)) {
        if (sym == ident) {
            i = position(id);
            if (i == 0) { error(11); getsym(); }
            else if (table[i].kind == procedur) { error(21); getsym(); }
            else if (table[i].kind == constant) { gen(lit, 0, table[i].val); getsym(); }
            else if (table[i].kind == variable) {
                gen(lod, lev - table[i].level, table[i].adr); getsym();
            } else {                                   /* 数组元素 */
                int adr = table[i].adr, lv = table[i].level, sz = table[i].size;
                getsym();
                if (sym != lbracket) { error(26); gen(lod, lev - lv, adr); }
                else {
                    getsym();
                    setcopy(nxtlev, fsys); setadd(nxtlev, rbracket);
                    expression(nxtlev);
                    if (sym == rbracket) getsym(); else error(26);
                    gen(chk, 0, sz);
                    gen(lda, lev - lv, adr);
                    gen(opr, 0, OPR_ADD);
                    gen(ldi, 0, 0);
                }
            }
        } else if (sym == number) {
            if (num > amax) { error(31); num = 0; }
            gen(lit, 0, num);
            getsym();
        } else if (sym == lparen) {
            getsym();
            setcopy(nxtlev, fsys); setadd(nxtlev, rparen);
            condition(nxtlev, 0);
            if (sym == rparen) getsym(); else error(22);
        } else if (sym == notsym) {
            getsym();
            setcopy(nxtlev, fsys);
            factor(nxtlev);
            gen(opr, 0, OPR_NOT);
        } else if (sym == sqrtSym) {
            getsym();
            if (sym == lparen) {
                getsym();
                setcopy(nxtlev, fsys); setadd(nxtlev, rparen);
                expression(nxtlev);
                if (sym == rparen) getsym(); else error(22);
            } else {
                setcopy(nxtlev, fsys);
                factor(nxtlev);
            }
            gen(opr, 0, OPR_SQRT);
        }
        test(fsys, facbegsys, 23);
    }
}

void term(BoolSet fsys)
{
    BoolSet nxtlev;
    int mulop;

    setcopy(nxtlev, fsys);
    setadd(nxtlev, times);  setadd(nxtlev, slash); setadd(nxtlev, percent);
    factor(nxtlev);

    while (sym == times || sym == slash || sym == percent) {
        mulop = sym;
        getsym();
        setcopy(nxtlev, fsys);
        setadd(nxtlev, times); setadd(nxtlev, slash); setadd(nxtlev, percent);
        factor(nxtlev);
        if (mulop == times)        gen(opr, 0, OPR_MUL);
        else if (mulop == slash)   gen(opr, 0, OPR_DIV);
        else                       gen(opr, 0, OPR_MOD);
    }
}

void expression(BoolSet fsys)
{
    BoolSet nxtlev;
    int addop;

    setcopy(nxtlev, fsys);
    setadd(nxtlev, plus); setadd(nxtlev, minus);

    if (sym == plus || sym == minus) {
        addop = sym;
        getsym();
        term(nxtlev);
        if (addop == minus) gen(opr, 0, OPR_NEG);
    } else {
        term(nxtlev);
    }
    while (sym == plus || sym == minus) {
        addop = sym;
        getsym();
        term(nxtlev);
        if (addop == plus) gen(opr, 0, OPR_ADD);
        else               gen(opr, 0, OPR_SUB);
    }
}

/*---------------- 条件分析（或 / 与 / 关系 三级） ----------------*/
void condrel(BoolSet fsys, int needRel)
{
    BoolSet nxtlev;
    int relop;

    if (sym == notsym) {
        getsym();
        condrel(fsys, needRel);
        gen(opr, 0, OPR_NOT);
        return;
    }
    if (sym == oddsym) {
        getsym();
        expression(fsys);
        gen(opr, 0, OPR_ODD);
        return;
    }
    setcopy(nxtlev, fsys);
    setunion(nxtlev, nxtlev, relset);
    setadd(nxtlev, andsym); setadd(nxtlev, orsym);
    expression(nxtlev);
    if (inset(sym, relset)) {
        relop = sym;
        getsym();
        expression(fsys);
        switch (relop) {
        case eql: gen(opr, 0, OPR_EQU); break;
        case neq: gen(opr, 0, OPR_NEQ); break;
        case lss: gen(opr, 0, OPR_LES); break;
        case leq: gen(opr, 0, OPR_LEQ); break;
        case gtr: gen(opr, 0, OPR_GTR); break;
        case geq: gen(opr, 0, OPR_GEQ); break;
        }
    } else if (needRel) {
        error(20);
    }
}

void condand(BoolSet fsys, int needRel)
{
    condrel(fsys, needRel);
    while (sym == andsym) {
        getsym();
        condrel(fsys, needRel);
        gen(opr, 0, OPR_AND);
    }
}

void condition(BoolSet fsys, int needRel)
{
    condand(fsys, needRel);
    while (sym == orsym) {
        getsym();
        condand(fsys, needRel);
        gen(opr, 0, OPR_OR);
    }
}

/*---------------- 语句分析 ----------------*/
void statement(BoolSet fsys)
{
    BoolSet nxtlev, nxt2;
    int i, cx1, cx2, tslot, op;

    if (sym == ident) {                              /* 赋值语句 */
        i = position(id);
        if (i == 0) error(11);
        else if (table[i].kind == procedur) { error(12); i = 0; }
        getsym();
        if (sym == lbracket) {                       /* 数组元素赋值 */
            int lv = 0, adr = 0, sz = 0;
            if (i && table[i].kind != array) error(28);
            if (i) { lv = table[i].level; adr = table[i].adr; sz = table[i].size; }
            getsym();
            setcopy(nxtlev, fsys); setadd(nxtlev, rbracket);
            expression(nxtlev);
            if (sym == rbracket) getsym(); else error(26);
            if (i) { gen(chk, 0, sz); gen(lda, lev - lv, adr); gen(opr, 0, OPR_ADD); }

            if (sym == becomes) {
                getsym();
                condition(fsys, 0);
                if (i) gen(sti, 0, 0);
            } else if (sym == pluseq || sym == minuseq ||
                       sym == timeseq || sym == slasheq || sym == percenteq) {
                op = (sym == pluseq) ? OPR_ADD : (sym == minuseq) ? OPR_SUB :
                     (sym == timeseq) ? OPR_MUL : (sym == slasheq) ? OPR_DIV : OPR_MOD;
                getsym();
                if (i) { gen(opr, 0, OPR_DUP); gen(ldi, 0, 0); }
                expression(fsys);
                if (i) { gen(opr, 0, op); gen(sti, 0, 0); }
            } else if (sym == plusplus || sym == minusminus) {
                op = (sym == plusplus) ? OPR_ADD : OPR_SUB;
                getsym();
                if (i) {
                    gen(opr, 0, OPR_DUP); gen(ldi, 0, 0);
                    gen(lit, 0, 1); gen(opr, 0, op); gen(sti, 0, 0);
                }
            } else {
                error(37);
                expression(fsys);
            }
        } else {                                     /* 简单变量赋值 */
            if (i) gen(lod, lev - table[i].level, table[i].adr);
            if (sym == becomes) {
                getsym();
                condition(fsys, 0);
                if (i) gen(sto, lev - table[i].level, table[i].adr);
            } else if (sym == pluseq || sym == minuseq ||
                       sym == timeseq || sym == slasheq || sym == percenteq) {
                op = (sym == pluseq) ? OPR_ADD : (sym == minuseq) ? OPR_SUB :
                     (sym == timeseq) ? OPR_MUL : (sym == slasheq) ? OPR_DIV : OPR_MOD;
                getsym();
                expression(fsys);
                if (i) { gen(opr, 0, op); gen(sto, lev - table[i].level, table[i].adr); }
            } else if (sym == plusplus || sym == minusminus) {
                op = (sym == plusplus) ? OPR_ADD : OPR_SUB;
                getsym();
                if (i) {
                    gen(lit, 0, 1); gen(opr, 0, op);
                    gen(sto, lev - table[i].level, table[i].adr);
                }
            } else {
                error(37);
            }
        }
    } else if (sym == readsym) {                     /* 读语句 */
        getsym();
        if (sym != lparen) error(33);
        else {
            do {
                getsym();
                if (sym == ident) {
                    i = position(id);
                    if (i == 0) error(11);
                    else if (table[i].kind == procedur) { error(34); i = 0; }
                    getsym();
                    if (sym == lbracket) {
                        int lv = 0, adr = 0, sz = 0;
                        if (i && table[i].kind != array) error(28);
                        if (i) { lv = table[i].level; adr = table[i].adr; sz = table[i].size; }
                        getsym();
                        setcopy(nxtlev, fsys); setadd(nxtlev, rbracket);
                        expression(nxtlev);
                        if (sym == rbracket) getsym(); else error(26);
                        if (i) { gen(chk, 0, sz); gen(lda, lev - lv, adr); gen(opr, 0, OPR_ADD); }
                        gen(opr, 0, OPR_READ);
                        if (i) gen(sti, 0, 0);
                    } else if (i) {
                        gen(opr, 0, OPR_READ);
                        gen(sto, lev - table[i].level, table[i].adr);
                    } else {
                        gen(opr, 0, OPR_READ);
                    }
                } else error(34);
            } while (sym == comma);
            if (sym == rparen) getsym(); else error(22);
        }
    } else if (sym == writesym || sym == printsym) { /* 写语句：write 换行，print 不换行 */
        int nl = (sym == writesym);
        getsym();
        if (sym == lparen) {
            do {
                getsym();
                if (sym == string) {
                    gen(wrs, 0, strinx);
                    getsym();
                } else {
                    setcopy(nxtlev, fsys);
                    setadd(nxtlev, rparen); setadd(nxtlev, comma);
                    condition(nxtlev, 0);
                    gen(opr, 0, OPR_WRITE);
                }
            } while (sym == comma);
            if (sym == rparen) getsym(); else error(22);
        } else error(33);
        if (nl) gen(opr, 0, OPR_WRITELN);
    } else if (sym == callsym) {                     /* 过程调用 */
        getsym();
        if (sym != ident) error(14);
        else {
            i = position(id);
            if (i == 0) error(11);
            else if (table[i].kind == procedur)
                gen(cal, lev - table[i].level, table[i].adr);
            else error(15);
            getsym();
        }
    } else if (sym == ifsym) {                       /* 条件语句（含 else） */
        getsym();
        setcopy(nxtlev, fsys);
        setadd(nxtlev, thensym); setadd(nxtlev, dosym); setadd(nxtlev, elsesym);
        condition(nxtlev, 1);
        if (sym == thensym) getsym(); else error(16);
        cx1 = cx;
        gen(jpc, 0, 0);
        setcopy(nxtlev, fsys); setadd(nxtlev, elsesym);
        statement(nxtlev);
        if (sym == elsesym) {
            cx2 = cx;
            gen(jmp, 0, 0);
            code[cx1].a = cx;
            getsym();
            setcopy(nxtlev, fsys);
            statement(nxtlev);
            code[cx2].a = cx;
        } else {
            code[cx1].a = cx;
        }
    } else if (sym == beginsym) {                    /* 复合语句 */
        getsym();
        setcopy(nxtlev, fsys);
        setadd(nxtlev, semicolon); setadd(nxtlev, endsym);
        statement(nxtlev);
        while (sym == semicolon || inset(sym, statbegsys) || sym == ident) {
            if (sym == semicolon) getsym(); else error(10);
            statement(nxtlev);
        }
        if (sym == endsym) getsym(); else error(17);
    } else if (sym == whilesym) {                    /* 当型循环 */
        cx1 = cx;
        getsym();
        setcopy(nxtlev, fsys);
        setadd(nxtlev, dosym);
        condition(nxtlev, 1);
        cx2 = cx;
        gen(jpc, 0, 0);
        if (sym == dosym) getsym(); else error(18);
        statement(fsys);
        gen(jmp, 0, cx1);
        code[cx2].a = cx;
    } else if (sym == forsym) {                      /* for 循环 */
        int vlev = 0, vadr = 0, cnt;
        getsym();
        if (sym != ident) { error(4); }
        i = position(id);
        if (i == 0) error(11);
        else if (table[i].kind != variable) { error(35); i = 0; }
        if (i) { vlev = table[i].level; vadr = table[i].adr; }
        getsym();
        if (sym != becomes) error(13); else getsym();
        setcopy(nxtlev, fsys);
        setadd(nxtlev, tosym); setadd(nxtlev, downtosym);
        expression(nxtlev);                           /* 初值 */
        if (i) gen(sto, lev - vlev, vadr);

        if (sym == tosym || sym == downtosym) cnt = sym; else { error(42); cnt = tosym; }
        if (sym == tosym || sym == downtosym) getsym();

        if (tempcnt >= MAXTEMP) error(36);
        tslot = tempbase + tempcnt; tempcnt++;
        setcopy(nxtlev, fsys); setadd(nxtlev, dosym);
        expression(nxtlev);                           /* 上界，只计算一次 */
        gen(sto, 0, tslot);

        cx1 = cx;
        if (i) gen(lod, lev - vlev, vadr);
        gen(lod, 0, tslot);
        gen(opr, 0, (cnt == tosym) ? OPR_LEQ : OPR_GEQ);
        cx2 = cx;
        gen(jpc, 0, 0);
        if (sym == dosym) getsym(); else error(18);
        statement(fsys);
        if (i) gen(lod, lev - vlev, vadr);
        gen(lit, 0, 1);
        gen(opr, 0, (cnt == tosym) ? OPR_ADD : OPR_SUB);
        if (i) gen(sto, lev - vlev, vadr);
        gen(jmp, 0, cx1);
        code[cx2].a = cx;
        tempcnt--;
    } else if (sym == repeatsym) {                   /* 直到型循环 */
        cx1 = cx;
        getsym();
        setcopy(nxtlev, fsys);
        setadd(nxtlev, untilsym);
        statement(nxtlev);
        if (sym == untilsym) getsym(); else error(38);
        condition(fsys, 1);
        gen(jpc, 0, cx1);                             /* 条件为假则继续循环 */
    }

    setclr(nxt2);
    test(fsys, nxt2, 19);
}

/*---------------- 分程序分析 ----------------*/
void block(int lev0, BoolSet fsys)
{
    int tx0, cxs, lev0s = lev, tb0 = tempbase;
    int dx0 = dx;     /* 保存父层数据区指针 */
    BoolSet nxtlev, nxt2;

    dx = 3;
    tx0 = tx;
    lev = lev0;
    cxs = cx;
    table[tx].adr = cx;
    gen(jmp, 0, 0);
    if (lev > levmax) error(32);

    do {
        if (sym == constsym) {                       /* 常数说明 */
            getsym();
            do {
                constdeclaration();
                while (sym == comma) { getsym(); constdeclaration(); }
                if (sym == semicolon) getsym(); else error(5);
            } while (sym == ident);
        }
        if (sym == varsym) {                         /* 变量与数组说明 */
            getsym();
            do {
                vardeclaration();
                while (sym == comma) { getsym(); vardeclaration(); }
                if (sym == semicolon) getsym(); else error(5);
            } while (sym == ident);
        }
        while (sym == procsym) {                     /* 过程说明 */
            getsym();
            if (sym == ident) { enter(procedur); getsym(); }
            else error(4);
            if (sym == semicolon) getsym(); else error(5);
            setcopy(nxtlev, fsys);
            setadd(nxtlev, semicolon);
            block(lev + 1, nxtlev);
            if (sym == semicolon) {
                getsym();
                setcopy(nxtlev, statbegsys);
                setadd(nxtlev, ident); setadd(nxtlev, procsym);
                test(nxtlev, fsys, 6);
            } else error(5);
        }
        setcopy(nxtlev, statbegsys);
        setadd(nxtlev, ident); setadd(nxtlev, period);
        test(nxtlev, declbegsys, 7);
    } while (inset(sym, declbegsys));

    code[table[tx0].adr].a = cx;
    table[tx0].adr = cx;
    table[tx0].size = dx;
    gen(Int, 0, dx + MAXTEMP);                       /* 数据区 + 循环上界临时单元 */
    tempbase = dx;
    tempcnt = 0;
    dumptable();

    setcopy(nxtlev, statbegsys);
    setadd(nxtlev, ident);
    setadd(nxtlev, semicolon); setadd(nxtlev, endsym);
    setunion(nxtlev, nxtlev, fsys);
    statement(nxtlev);
    gen(opr, 0, OPR_RET);
    setclr(nxt2);
    test(fsys, nxt2, 8);
    listcode(cxs, cx);

    lev = lev0s;
    tempbase = tb0;
    dx = dx0;         /* 恢复父层的数据区指针 */
}

/*---------------- 清单输出 ----------------*/
void listcode(int cx0, int cx1)
{
    int i;
    printf("\n------ 目标代码（P-代码）%d 条 ------\n", cx1 - cx0);
    if (fa) fprintf(fa, "\n------ 目标代码（P-代码）%d 条 ------\n", cx1 - cx0);
    printf(" 地址  指令   层次  操作数\n");
    for (i = cx0; i < cx1; i++) {
        printf("%5d  %-5s %5d %6d\n", i, mnemonic[code[i].f], code[i].l, code[i].a);
        if (fa) fprintf(fa, "%5d  %-5s %5d %6d\n", i, mnemonic[code[i].f], code[i].l, code[i].a);
    }
}

void dumptable(void)
{
    int i;
    printf("\n------ 名字表（当前层次 lev=%d，表长 %d）------\n", lev, tx);
    if (fas) fprintf(fas, "\n------ 名字表（lev=%d，表长 %d）------\n", lev, tx);
    printf(" 序号 名字            类别     值/层次  地址  大小\n");
    for (i = 1; i <= tx; i++) {
        const char *k = (table[i].kind == constant) ? "常量" :
                        (table[i].kind == variable) ? "变量" :
                        (table[i].kind == array)    ? "数组" : "过程";
        if (table[i].kind == constant) {
            printf("%5d %-14s %-8s %6d\n", i, table[i].name, k, table[i].val);
            if (fas) fprintf(fas, "%5d %-14s %-8s %6d\n", i, table[i].name, k, table[i].val);
        } else {
            printf("%5d %-14s %-8s %6d %5d %5d\n", i, table[i].name, k,
                   table[i].level, table[i].adr, table[i].size);
            if (fas) fprintf(fas, "%5d %-14s %-8s %6d %5d %5d\n", i, table[i].name, k,
                   table[i].level, table[i].adr, table[i].size);
        }
    }
}

/*---------------- 解释执行 ----------------*/
int base(int l, int *s, int b)
{
    int bl = b;
    while (l > 0) { bl = s[bl]; l--; }
    return bl;
}

void interpret(void)
{
    static int s[stacksize + 1];
    int p = 0, b = 1, t = 0;
    struct instruction i;
    int n;

    printf("\n======== 开始解释执行（start pl0）========\n");
    if (fa2) fprintf(fa2, "\n======== 运行结果 ========\n");
    s[1] = s[2] = s[3] = 0;

    do {
        if (p < 0 || p >= cx) { printf("\n[运行错误] 指令指针越界\n"); break; }
        i = code[p++];
        switch (i.f) {
        case lit:
            t++; s[t] = i.a;
            break;
        case opr:
            switch (i.a) {
            case OPR_RET:
                t = b - 1; p = s[t + 3]; b = s[t + 2];
                break;
            case OPR_NEG: s[t] = -s[t]; break;
            case OPR_ADD: t--; s[t] = s[t] + s[t + 1]; break;
            case OPR_SUB: t--; s[t] = s[t] - s[t + 1]; break;
            case OPR_MUL: t--; s[t] = s[t] * s[t + 1]; break;
            case OPR_DIV:
                if (s[t] == 0) { printf("\n[运行错误] 除数为零\n"); return; }
                t--; s[t] = s[t] / s[t + 1];
                break;
            case OPR_MOD:
                if (s[t] == 0) { printf("\n[运行错误] 取余运算除数为零\n"); return; }
                t--; s[t] = s[t] % s[t + 1];
                break;
            case OPR_ODD: s[t] = s[t] % 2 != 0; break;
            case OPR_EQU: t--; s[t] = (s[t] == s[t + 1]); break;
            case OPR_NEQ: t--; s[t] = (s[t] != s[t + 1]); break;
            case OPR_LES: t--; s[t] = (s[t] <  s[t + 1]); break;
            case OPR_GEQ: t--; s[t] = (s[t] >= s[t + 1]); break;
            case OPR_GTR: t--; s[t] = (s[t] >  s[t + 1]); break;
            case OPR_LEQ: t--; s[t] = (s[t] <= s[t + 1]); break;
            case OPR_AND: t--; s[t] = (s[t] != 0) && (s[t + 1] != 0); break;
            case OPR_OR:  t--; s[t] = (s[t] != 0) || (s[t + 1] != 0); break;
            case OPR_NOT: s[t] = (s[t] == 0); break;
            case OPR_SQRT:
                if (s[t] < 0) { printf("\n[运行错误] 负数不能开方\n"); return; }
                { int r = 0; while ((r + 1) * (r + 1) <= s[t]) r++; s[t] = r; }
                break;
            case OPR_DUP: t++; s[t] = s[t - 1]; break;
            case OPR_WRITE:
                printf("%d", s[t]);
                if (fa2) fprintf(fa2, "%d", s[t]);
                t--;
                break;
            case OPR_WRITELN:
                printf("\n");
                if (fa2) fprintf(fa2, "\n");
                break;
            case OPR_READ:
                printf("? ");
                if (fa2) fprintf(fa2, "? ");
                if (scanf("%d", &n) != 1) n = 0;
                t++; s[t] = n;
                if (fa2) fprintf(fa2, "%d\n", n);
                break;
            default:
                printf("\n[运行错误] 非法运算码 %d\n", i.a);
                return;
            }
            break;
        case lod: t++; s[t] = s[base(i.l, s, b) + i.a]; break;
        case sto: s[base(i.l, s, b) + i.a] = s[t]; t--; break;
        case cal:
            s[t + 1] = base(i.l, s, b);
            s[t + 2] = b;
            s[t + 3] = p;
            b = t + 1;
            p = i.a;
            break;
        case Int: t += i.a; break;
        case jmp: p = i.a; break;
        case jpc: if (s[t] == 0) p = i.a; t--; break;   /* 先判后弹 */
        case lda: t++; s[t] = base(i.l, s, b) + i.a; break;
        case ldi: s[t] = s[s[t]]; break;
        case sti: s[s[t - 1]] = s[t]; t -= 2; break;   /* [addr,val] → mem[addr]=val */
        case chk:
            if (s[t] < 0 || s[t] >= i.a) {
                printf("\n[运行错误] 数组下标 %d 越界（合法范围 0..%d）\n", s[t], i.a - 1);
                if (fa2) fprintf(fa2, "\n[运行错误] 数组下标 %d 越界\n", s[t]);
                return;
            }
            break;
        case wrs:
            printf("%s", strpool[i.a]);
            if (fa2) fprintf(fa2, "%s", strpool[i.a]);
            break;
        }
    } while (p != 0);
    printf("\n======== 解释执行结束 ========\n");
}

/*---------------- 初始化 ----------------*/
void init(void)
{
    int i;

    for (i = 0; i < fctnum; i++) memset(mnemonic[i], 0, 5);
    strcpy(mnemonic[lit], "lit");
    strcpy(mnemonic[opr], "opr");
    strcpy(mnemonic[lod], "lod");
    strcpy(mnemonic[sto], "sto");
    strcpy(mnemonic[cal], "cal");
    strcpy(mnemonic[Int], "int");
    strcpy(mnemonic[jmp], "jmp");
    strcpy(mnemonic[jpc], "jpc");
    strcpy(mnemonic[lda], "lda");
    strcpy(mnemonic[ldi], "ldi");
    strcpy(mnemonic[sti], "sti");
    strcpy(mnemonic[chk], "chk");
    strcpy(mnemonic[wrs], "wrs");

    setclr(declbegsys); setclr(statbegsys); setclr(facbegsys); setclr(relset);
    setadd(declbegsys, constsym); setadd(declbegsys, varsym); setadd(declbegsys, procsym);
    setadd(statbegsys, beginsym); setadd(statbegsys, callsym);
    setadd(statbegsys, ifsym);    setadd(statbegsys, whilesym);
    setadd(statbegsys, forsym);   setadd(statbegsys, repeatsym);
    setadd(statbegsys, readsym);  setadd(statbegsys, writesym);
    setadd(statbegsys, printsym);
    setadd(facbegsys, ident); setadd(facbegsys, number);
    setadd(facbegsys, lparen); setadd(facbegsys, notsym); setadd(facbegsys, sqrtSym);
    setadd(relset, eql); setadd(relset, neq); setadd(relset, lss);
    setadd(relset, leq); setadd(relset, gtr); setadd(relset, geq);
}

/*---------------- 读入源程序 ----------------*/
static void loadfile(const char *name)
{
    FILE *f = fopen(name, "rb");
    int cap = 65536;

    if (!f) { printf("无法打开源程序文件：%s\n", name); exit(1); }
    srcbuf = (char *)malloc(cap);
    srclen = 0;
    for (;;) {
        int n = (int)fread(srcbuf + srclen, 1, cap - srclen - 1, f);
        if (n <= 0) break;
        srclen += n;
        if (srclen >= cap - 1) { cap *= 2; srcbuf = (char *)realloc(srcbuf, cap); }
    }
    srcbuf[srclen] = '\0';
    fclose(f);
}

static void listsource(const char *name)
{
    int i, line = 1;
    printf("------ 源程序 %s ------\n", name);
    if (flist) fprintf(flist, "------ 源程序 %s ------\n", name);
    for (i = 0; i < srclen; i++) {
        if (i == 0 || srcbuf[i - 1] == '\n') {
            printf("%3d| ", line++);
            if (flist) fprintf(flist, "%3d| ", line - 1);
        }
        putchar(srcbuf[i]);
        if (flist) fputc(srcbuf[i], flist);
    }
    if (srclen && srcbuf[srclen - 1] != '\n') { printf("\n"); if (flist) fprintf(flist, "\n"); }
}

int main(int argc, char *argv[])
{
    char fname[256];

    setvbuf(stdout, NULL, _IONBF, 0);   /* 关闭缓冲，便于交互与录屏 */
    if (argc > 1) strncpy(fname, argv[1], 255), fname[255] = 0;
    else { printf("请输入 PL/0 源程序文件名："); if (scanf("%255s", fname) != 1) return 1; }

    loadfile(fname);
    fa    = fopen("fa.tmp", "w");
    fas   = fopen("fas.tmp", "w");
    fa2   = fopen("fa2.tmp", "w");
    flist = fopen("f1.tmp", "w");

    init();
    err = 0; cx = 0; tx = 0; lev = 0; dx = 3; cc = 0; ll = 1;
    tempbase = 3; tempcnt = 0;

    listsource(fname);

    getch();
    getsym();
    {
        BoolSet nxtlev, nxt2;
        setclr(nxt2);
        setcopy(nxtlev, declbegsys);
        setunion(nxtlev, nxtlev, statbegsys);
        setadd(nxtlev, period);
        block(0, nxtlev);
    }
    if (sym != period) error(9);

    printf("\n------ 编译统计 ------\n");
    printf("源程序行数：%d    目标代码：%d 条    名字表：%d 项    错误：%d 个\n",
           ll, cx, tx, err);
    if (flist)
        fprintf(flist, "\n------ 编译统计 ------\n源程序行数：%d  目标代码：%d 条  名字表：%d 项  错误：%d 个\n",
                ll, cx, tx, err);

    if (err == 0) interpret();
    else printf("\n编译存在 %d 个错误，跳过解释执行。\n", err);

    fclose(fa); fclose(fas); fclose(fa2); fclose(flist);
    return 0;
}

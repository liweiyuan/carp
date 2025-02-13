#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "instructions.h"
#include "lib/types.h"
#include "registers.h"
#include "tokenizer.h"

char carp_reverse_type[][6] = {"undef", "num", "reg",  "lbl",
                               "func",  "var", "instr"};

carp_reg in_reg_set(register const char *str);
carp_instr in_instr_set(register const char *str);

static char *file_read(const char *);
static carp_bool is_sign(char);
static carp_bool is_num(const char *);
static carp_bool is_number(const char *);
static carp_bool is_reg(const char *);
static carp_bool is_label(const char *);
static carp_bool is_var(const char *);
static carp_bool is_instr(const char *);

/* Reads a whole file into memory (really should change to
 * line-by-line), then goes through and copies lexemes, types, and
 * program positions in.
 * 将整个文件读入内存（真的应该改为逐行），然后遍历并复制词素、类型和程序位置。
 */
carp_tok *carp_lex_tokenize(const char *fn) {
    assert(fn != NULL);

    char *str = file_read(fn);

    assert(str != NULL);

    const char *delim = " ,\t\n";
    char *toks = strtok(str, delim);

    /* empty file, so skip the pain and halt with success there must be
     * a cleaner way to do this
     */
    if (toks == NULL) {
        free(str);
        exit(EXIT_SUCCESS);
    }

    int toks_len = 0;

    carp_id type;
    carp_tok *parsed = malloc(sizeof *parsed);
    carp_tok *head = parsed;
    carp_tok *lookbehind = NULL;
    carp_value i = 0;

    if (parsed == NULL) {
        free(str);
        fprintf(stderr,
                "Could not allocate memory (%lu bytes) for a token: %s\n",
                sizeof *parsed, strerror(errno));
        return NULL;
    }

    while (toks != NULL) {
        toks_len = strlen(toks);

        if (is_num(toks)) {
            type = CARP_TOK_NUM;
        } else if (is_reg(toks)) {
            type = CARP_TOK_REG;
        } else if (is_label(toks)) {
            type = CARP_TOK_LBL;
        } else if (lookbehind != NULL &&
                   strcmp(lookbehind->lexeme, "call") == 0) {
            type = CARP_TOK_FUNC;
        } else if (is_var(toks)) {
            type = CARP_TOK_VAR;
        } else if (is_instr(toks)) {
            type = CARP_TOK_INSTR;
        } else {
            type = CARP_TOK_UNDEF;
        }

        if (type == CARP_TOK_LBL) {
            /* don't copy colon at end */
            memcpy(parsed->lexeme, toks, toks_len - 1);
            parsed->lexeme[toks_len - 1] = 0;
        } else if (type == CARP_TOK_VAR) {
            /* don't copy proposed $ at start */
            memcpy(parsed->lexeme, toks + 1, toks_len - 1);
            parsed->lexeme[toks_len - 1] = 0;
        } else {
            /* nothing to avoid */
            memcpy(parsed->lexeme, toks, toks_len);
            parsed->lexeme[toks_len] = 0;
        }

        parsed->type = type;
        parsed->pos = i++;
        parsed->next = NULL;

        toks = strtok(NULL, delim);

        if (toks != NULL) {
            parsed->next = malloc(sizeof *parsed->next);
            lookbehind = parsed;
            parsed = parsed->next;
        }
    }

    free(str);

    return head;
}
/**
 *
 * 按行读取，生成tokens(是一个carp_tok链表)
 */
carp_tok *carp_lex_tokenize_by_line(const char *fn) {
    assert(fn != NULL);
    FILE *fp = fopen(fn, "r");
    assert(fp != NULL);

    const char *delim = " ,\t\n";
    /*假设每行最大256*/
    char line[256]; 
    carp_tok *head = NULL, *tail= NULL;
    carp_value i =0;

    while(fgets(line, sizeof(line), fp)){
        //去掉换行符
        line[strcspn(line, "\n")] = '\0';
        char *toks = strtok(line, delim);
        while(toks != NULL){
            carp_tok *parsed = malloc(sizeof(carp_tok));
            if(parsed == NULL){
                fclose(fp);
                fprintf(stderr, "Could not allocate memory for token\n");
                return NULL;
            }
            // 识别 token 类型
            if (is_number(toks)) {
                parsed->type = CARP_TOK_NUM;
            } else if (is_reg(toks)) {
                parsed->type = CARP_TOK_REG;
            } else if (is_label(toks)) {
                parsed->type = CARP_TOK_LBL;
            } else if (is_var(toks)) {
                parsed->type = CARP_TOK_VAR;
            } else if (is_instr(toks)) {
                parsed->type = CARP_TOK_INSTR;
            } else {
                parsed->type = CARP_TOK_UNDEF;
            }

            // 处理 lexeme
            size_t toks_len = strlen(toks);
            if (parsed->type == CARP_TOK_LBL) {
                memcpy(parsed->lexeme, toks, toks_len - 1);
                parsed->lexeme[toks_len - 1] = 0;  // 去掉冒号
            } else if (parsed->type == CARP_TOK_VAR) {
                memcpy(parsed->lexeme, toks + 1, toks_len - 1);
                parsed->lexeme[toks_len - 1] = 0;  // 去掉前缀 $
            } else {
                memcpy(parsed->lexeme, toks, toks_len);
                parsed->lexeme[toks_len] = 0;  // 添加字符串结束符
            }

            parsed->pos = i++;
            parsed->next = NULL;

            // 将 token 添加到链表
            if (head == NULL) {
                head = parsed;  // 第一个 token
                tail = parsed;
            } else {
                tail->next = parsed;  // 将新 token 连接到链表尾部
                tail = parsed;
            }

            toks = strtok(NULL, delim);
        }
    }
    fclose(fp);
    return head;
}

/* Reads a whole file and returns a pointer to its contents. */
char *file_read(const char *fn) {
    assert(fn != NULL);

    FILE *fp;
    /* 指向一个char类型的指针*/
    char *contents;  
    size_t fsize;
    size_t nread;

    printf("Attempting to open file: %s\n", fn);
    fp = fopen(fn, "r");
    if (!fp) {
        fprintf(stderr, "Could not open file `%s' for reading: %s\n", fn,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Figure out file size */
    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

/* 这行代码实际上是将 fsize 乘以一个字符的大小,用于后续申请内存的大小*/
    fsize *= sizeof *contents;

    /* + 1 is for the NULL terminator */
    contents = malloc(fsize + 1);
    if (!contents) {
        fprintf(stderr, "Could not malloc %lu bytes for file contents: %s\n",
                fsize + 1, strerror(errno));
        exit(EXIT_FAILURE);
    }

    nread = fread(contents, sizeof *contents, fsize, fp);
    if (nread != fsize) {
        fprintf(stderr, "WARNING: There was a problem reading `%s': %s\n", fn,
                strerror(errno));
    }

    contents[nread] = '\0';

    if (fclose(fp) == EOF) {
        fprintf(stderr, "WARNING: There was a problem closing `%s': %s\n", fn,
                strerror(errno));
        // clearerr(fp);
    }

    return contents;
}

/* Returns true if the character is a numeric sign. */
carp_bool is_sign(char c) { return c == '+' || c == '-'; }

/* Returns true if the string contains all numbers (can start with a sign). */
carp_bool is_num(const char *s) {
    assert(s != NULL);

    if (!(is_sign(s[0]) || isdigit((unsigned char)s[0])))
        return 0;

    for (size_t i = 1; i < strlen(s); i++)
        if (!isdigit((unsigned char)s[i]))
            return 0;

    return 1;
}

/*使用标准库去判断是否全是数字 */
carp_bool is_number(const char *str) {
    assert(str != NULL);
    // 检查是否有符号
    if (*str == '+' || *str == '-') {
        str++;
    }
    while (*str) {
        if (!isdigit((unsigned char)*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

/* Returns true if the string is in the registers list. Uses gperf. */
carp_bool is_reg(const char *s) {
    assert(s != NULL);

    return in_reg_set(s) != CARP_REG_UNDEF;
}

/* Returns true if the string has a : in it. */
carp_bool is_label(const char *s) {
    assert(s != NULL);

    return strchr(s, ':') != NULL;
}

/* Returns true if the string has a $ in it. */
carp_bool is_var(const char *s) {
    assert(s != NULL);

    return strchr(s, '$') != NULL;
}

/* Returns true if the string is in the instructions list. Uses gperf. */
carp_bool is_instr(const char *s) {
    assert(s != NULL);

    return in_instr_set(s) != CARP_INSTR_UNDEF;
}

/* Uses strcmp to look up regs. Could probably use a hashtable. */
carp_reg carp_reg_lookup(const char *s) {
    assert(s != NULL);

    for (int i = 0; i < CARP_NUM_REGS; i++)
        if (!strcmp(carp_reverse_reg[i], s))
            return i;

    return CARP_REG_UNDEF;
}

/* Uses strcmp to look up instrs. Could probably use a hashtable. */
carp_instr carp_instr_lookup(const char *s) {
    assert(s != NULL);

    for (int i = 0; i < CARP_NUM_INSTRS; i++)
        if (!strcmp(carp_reverse_instr[i], s))
            return i;

    return CARP_INSTR_UNDEF;
}

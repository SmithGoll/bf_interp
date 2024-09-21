/*
 * Brainf**k Interpreter ARM64 JIT Version (BFINTERP JIT)
 *
 * Copyright (c) 2024 SilentTalk <gollsm@foxmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __aarch64__
    #error "Unsupported architecture"
#endif

#include <stdio.h>          // putchar, getchar, fprintf, fopen, fgetc, fclose, rewind
#include <stdlib.h>         // exit
#include <errno.h>          // strerror, errno
#include <unistd.h>         // isatty
#include <sys/mman.h>       // mmap, munmap

// scratch registers
#define x0      0
#define x1      1
#define x2      2
#define x3      3
#define x4      4
#define x5      5
#define x6      6
#define x7      7
// 32bits version of scratch
#define w0      x0
#define w1      x1
#define w2      x2
#define w3      x3
#define w4      x4
#define w5      x5
#define w6      x6
#define w7      x7
// xZR regs is 31
#define xZR     31
#define wZR     xZR

#define EMIT(x) instr_emitter(x, 0)

#define LOGIC_REG_gen(sf, opc, shift, N, Rm, imm6, Rn, Rd)    ((sf)<<31 | (opc)<<29 | 0b01010<<24 | (shift)<<22 | (N)<<21 | (Rm)<<16 | (imm6)<<10 | (Rn)<<5 | (Rd))
#define ORRx_REG(Rd, Rn, Rm)            EMIT(LOGIC_REG_gen(1, 0b01, 0b00, 0, Rm, 0, Rn, Rd))
#define ORRw_REG(Rd, Rn, Rm)            EMIT(LOGIC_REG_gen(0, 0b01, 0b00, 0, Rm, 0, Rn, Rd))
#define MOVx_REG(Rd, Rm)                ORRx_REG(Rd, xZR, Rm)
#define MOVw_REG(Rd, Rm)                ORRw_REG(Rd, xZR, Rm)

#define ADDSUB_IMM_gen(sf, op, S, shift, imm12, Rn, Rd)    ((sf)<<31 | (op)<<30 | (S)<<29 | 0b10001<<24 | (shift)<<22 | (imm12)<<10 | (Rn)<<5 | (Rd))
#define ADDx_U12(Rd, Rn, imm12)     EMIT(ADDSUB_IMM_gen(1, 0, 0, 0b00, (imm12)&0xfff, Rn, Rd))
#define ADDw_U12(Rd, Rn, imm12)     EMIT(ADDSUB_IMM_gen(0, 0, 0, 0b00, (imm12)&0xfff, Rn, Rd))
#define SUBx_U12(Rd, Rn, imm12)     EMIT(ADDSUB_IMM_gen(1, 1, 0, 0b00, (imm12)&0xfff, Rn, Rd))
#define SUBw_U12(Rd, Rn, imm12)     EMIT(ADDSUB_IMM_gen(0, 1, 0, 0b00, (imm12)&0xfff, Rn, Rd))

#define LD_gen(size, op1, imm12, Rn, Rt)        ((size)<<30 | 0b111<<27 | (op1)<<24 | 0b01<<22 | (imm12)<<10 | (Rn)<<5 | (Rt))
#define LDRB_U12(Rt, Rn, imm12)           EMIT(LD_gen(0b00, 0b01, ((uint32_t)((imm12)))&0xfff, Rn, Rt))

#define ST_gen(size, op1, imm12, Rn, Rt)        ((size)<<30 | 0b111<<27 | (op1)<<24 | 0b00<<22 | (imm12)<<10 | (Rn)<<5 | (Rt))
#define STRB_U12(Rt, Rn, imm12)           EMIT(ST_gen(0b00, 0b01, ((uint32_t)((imm12)))&0xfff, Rn, Rt))

#define BR_gen(Z, op, A, M, Rn, Rm)       (0b1101011<<25 | (Z)<<24 | (op)<<21 | 0b11111<<16 | (A)<<11 | (M)<<10 | (Rn)<<5 | (Rm))
#define BLR(Rn)                           EMIT(BR_gen(0, 0b01, 0, 0, Rn, 0))

#define B_gen(imm26)                    (0b000101<<26 | (imm26))
#define B(imm26)                        B_gen(((imm26)>>2)&0x3ffffff)

#define CB_gen(sf, op, imm19, Rt)       ((sf)<<31 | 0b011010<<25 | (op)<<24 | (imm19)<<5 | (Rt))
#define CBZw(Rt, imm19)                 CB_gen(0, 0, ((imm19)>>2)&0x7FFFF, Rt)

#define INSTR_SIZE 4
#define JMP(x) EMIT(B((x) * INSTR_SIZE))
#define JMP_IF(x) instr_emitter(CBZw(w0, (x) * INSTR_SIZE), stack[sp])

#define ABOUT \
    "BFINTERP JIT(aarch64) v3.8 built on " __DATE__ " " __TIME__ ".\n" \
    "Copyright (c) 2024 - Brainf**k Interpreter written by SilentTalk.\n" \
    "Licensed under MIT. See source distribution for detailed\n" \
    "copyright notices.\n\n"

#define DATA_SIZE 65535
#define PROG_SIZE 1024*1024

int bf_load();
int bf_exec();
int bf_unmap();

void instr_emitter(unsigned int, int);

int sp = 0;
int bf_size = 0;
FILE *fp = NULL;
char data[DATA_SIZE] = {};
int *prog = NULL;
int stack[PROG_SIZE/2] = {};
int (*bf_func[])() = { bf_load, bf_exec, bf_unmap };

static const unsigned int inst[] = {
    0xa9bf7bfd,        // stp x29, x30, [sp, #-16]!
    0x910003fd,        // mov x29, sp
    0xa8c17bfd,        // ldp x29, x30, [sp], #16
    0xd65f03c0         // ret
};

static inline size_t align(size_t size) {
    int page_size = getpagesize();
    return (size + (page_size - 1)) & ~(page_size - 1);
}

static inline void arm64_addw(int reg, unsigned int count)
{
    for(int j = (count / 0xfff); j; j--)
        ADDw_U12(reg, reg, 0xfff);
    ADDw_U12(reg, reg, (count % 0xfff));
    return;
}

static inline void arm64_addx(int reg, unsigned int count)
{
    for(int j = (count / 0xfff); j; j--)
        ADDx_U12(reg, reg, 0xfff);
    ADDx_U12(reg, reg, (count % 0xfff));
    return;
}

static inline void arm64_subw(int reg, unsigned int count)
{
    for(int j = (count / 0xfff); j; j--)
        SUBw_U12(reg, reg, 0xfff);
    SUBw_U12(reg, reg, (count % 0xfff));
    return;
}

static inline void arm64_subx(int reg, unsigned int count)
{
    for(int j = (count / 0xfff); j; j--)
        SUBx_U12(reg, reg, 0xfff);
    SUBx_U12(reg, reg, (count % 0xfff));
    return;
}

void reg_rec(int, void*, void*, void*);
asm(
    ".text\n\t"
    ".align 4\n\t"
    ".globl reg_rec\n\t"
    ".type reg_rec, @function\n"
    "reg_rec:\n\t"
    "ret\n\t"
    ".size reg_rec,.-reg_rec\n"
);

void instr_emitter(unsigned int instr, int pos)
{
    if(bf_size >= PROG_SIZE)
    {
        fprintf(stderr, "Error: file is too large!\n");
        bf_unmap();
        exit(1);
    }

    if(pos)
        prog[pos] = instr;
    else
        prog[bf_size++] = instr;

    return;
}

int getop()
{
    int c;
    for(;;)
    {
        switch((c = fgetc(fp)))
        {
            case '+':
            case '-':
            case '>':
            case '<':
            case '.':
            case ',':
            case '[':
            case ']':
            case EOF:
                return c;
        }
    }
}

void bf_putchar(char a, char *d, void *put_func, void *get_func)
{
#ifdef DEBUG
    fprintf(stderr, "[%p]  %02x %c\n", d, *d, *d);
#endif
    putchar(*d);
    reg_rec(a, d, put_func, get_func);
    return;
}

void bf_getchar(char a, char *d, void *put_func, void *get_func)
{
    int ch = getchar();
    *d = ((ch != EOF) ? ch : 0);
    reg_rec(a, d, put_func, get_func);
    return;
}

int bf_load()
{
    prog = mmap(NULL, align(PROG_SIZE * sizeof(*prog)),
                PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if(prog == MAP_FAILED)
    {
        fprintf(stderr, "mmap: %s\n", strerror(errno));
        return -1;
    }

    for(int i = 0; i < 2; i++, bf_size++)
        prog[i] = inst[i];

    int c;
    int tmp = EOF;
    while(1)
    {
        if(tmp != EOF)
        {
            c = tmp;
            tmp = EOF;
        }
        else if((c = getop()) == EOF)
        {
            break;
        }

        int count = 1;
        switch (c)
        {
            case '+':
            case '-':
                while((tmp = getop()) != EOF && (tmp == '+' || tmp == '-'))
                {
                    if(tmp == c)
                        count++;
                    else
                        count--;
                }

                if(count == 0)
                    break;
                count = ((c == '+') ? count : -count);
                LDRB_U12(w0, x1, 0);
                if(count < 0)
                    arm64_subw(w0, (-count));
                else
                    arm64_addw(w0, count);
                STRB_U12(w0, x1, 0);
                break;
            case '>':
            case '<':
                while((tmp = getop()) != EOF && (tmp == '>' || tmp == '<'))
                {
                    if(tmp == c)
                        count++;
                    else
                        count--;
                }

                if(count == 0)
                    break;
                count = ((c == '>') ? count : -count);
                if(count < 0)
                    arm64_subx(x1, (-count));
                else
                    arm64_addx(x1, count);
                break;
            case '.':
                BLR(x2);
                break;
            case ',':
                BLR(x3);
                break;
            case '[':
                if(sp >= sizeof(stack)/sizeof(stack[0]))
                {
                    bf_unmap();
                    return -1;
                }
                LDRB_U12(w0, x1, 0);
                stack[sp++] = bf_size;
                EMIT(0);
                break;
            case ']':
                sp--;
                if(sp < 0)
                {
                    bf_unmap();
                    return -1;
                }
                JMP_IF(bf_size - stack[sp] + 1);
                JMP(stack[sp] - bf_size - 1);
                break;
        }
    }
    if(fp != stdin)
        fclose(fp);

    if(bf_size >= PROG_SIZE - 2 || sp)
    {
        return -2;
    }
    for(int i = 2; i < 4; i++)
        prog[bf_size++] = inst[i];

#ifdef GEN_BIN_FILE
    for(int i = 0; prog[i]; i++)
    {
        char *p = (void*)&prog[i];
        putchar(p[0]);
        putchar(p[1]);
        putchar(p[2]);
        putchar(p[3]);
    }
    bf_unmap();
    exit(0);
#endif

    return 0;
}

int bf_exec()
{
#ifdef DEBUG
    fprintf(stderr, "data pointer: %p\n", data);
#endif
    void (*bf_prog)(int, void*, void*, void*) = (void*)prog;
    bf_prog(0, data, bf_putchar, bf_getchar);
    return 0;
}

int bf_unmap()
{
    if(munmap(prog, align(PROG_SIZE * sizeof(*prog))))
    {
        fprintf(stderr, "munmap: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void help(const char *name)
{
    fprintf(stderr, ABOUT);
    fprintf(stderr, "Usage: %s bf-file\n", name);
    exit(-1);
}

int main(int argc, const char * argv[])
{
    if(argc == 1)
    {
        if(isatty(STDIN_FILENO))
            help(argv[0]);
        fp = stdin;
    }
    else if(argc != 2)
        help(argv[0]);
    else if((fp = fopen(argv[1], "r")) == NULL)
    {
    	fprintf(stderr, "BFINTERP: %s (%s)\n", argv[1], strerror(errno));
    	return -1;
    }

    if(fgetc(fp) == '#' && fgetc(fp) =='!')
    {
        int i;
        do {
            i = fgetc(fp);
        } while(i != '\n' && i != EOF);
    } else
        rewind(fp);

    for(int i = 0; i < sizeof(bf_func)/sizeof(bf_func[0]); i++)
    {
        int status = bf_func[i]();
        if(status)
        {
            fprintf(stderr, "Error: bf_func[%d] returned %d\n", i, status);
            return status;
        }
    }
    return 0;
}

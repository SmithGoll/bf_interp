/*
 * Brainf**k Interpreter (BFINTERP)
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

#include <stdio.h>          // putchar, getchar, fprintf, fopen, fgetc, fclose, rewind
#include <stdlib.h>         // exit
#include <errno.h>          // strerror, errno
#include <unistd.h>         // isatty

#define ABOUT \
    "BFINTERP v3.8 built on " __DATE__ " " __TIME__ ".\n" \
    "Copyright (c) 2024 - Brainf**k Interpreter written by SilentTalk.\n" \
    "Licensed under MIT. See source distribution for detailed\n" \
    "copyright notices.\n\n"

#define DATA_SIZE 65535
#define PROG_SIZE 1024*1024

int load_bf();
int exec_bf();

int sp = 0;
int bf_size = 0;
FILE *fp = NULL;
char data[DATA_SIZE] = {};
int prog[PROG_SIZE] = {};
int stack[PROG_SIZE/2] = {};
int (*bf_func[])() = { load_bf, exec_bf };

enum
{
    OP_STOP = 0,
    OP_JMP_FWD,
    OP_JMP_BACK,
    OP_GETCHAR,
    OP_PUTCHAR,
    OP_VAL_ADD,
    OP_VAL_INC,
    OP_VAL_DEC,
    OP_POS_ADD,
    OP_POS_INC,
    OP_POS_DEC
};

void op_emitter(unsigned int op)
{
    if(bf_size >= PROG_SIZE)
    {
        fprintf(stderr, "Error: file is too large!\n");
        if(fp != stdin)
            fclose(fp);
        exit(1);
    }

    prog[bf_size++] = op;
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

int load_bf()
{
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
        switch(c)
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
                count = ((c == '+') ? count : -count);
                if(count)
                {
                    if(count == 1)
                        op_emitter(OP_VAL_INC);
                    else if(count == -1)
                        op_emitter(OP_VAL_DEC);
                    else
                    {
                        op_emitter(OP_VAL_ADD);
                        op_emitter(count);
                    }
                }
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
                count = ((c == '>') ? count : -count);
                if(count)
                {
                    if(count == 1)
                        op_emitter(OP_POS_INC);
                    else if(count == -1)
                        op_emitter(OP_POS_DEC);
                    else
                    {
                        op_emitter(OP_POS_ADD);
                        op_emitter(count);
                    }
                }
                break;
            case '.':
                op_emitter(OP_PUTCHAR);
                break;
            case ',':
                op_emitter(OP_GETCHAR);
                break;
            case '[':
                if(sp >= sizeof(stack)/sizeof(stack[0]))
                    return -1;
                op_emitter(OP_JMP_FWD);
                stack[sp++] = bf_size++;
                break;
            case ']':
                if(sp <= 0)
                    return -1;
                op_emitter(OP_JMP_BACK);
                op_emitter(stack[--sp] - bf_size);
                prog[(stack[sp])] = bf_size - 1;
                break;
        }
    }
    if(fp != stdin)
        fclose(fp);

    if(sp)
        return -2;
    prog[bf_size] = OP_STOP;
    return 0;
}

int exec_bf()
{
    unsigned int pos = 0;
    for(int i = 0; prog[i]; i++)
    {
        switch(prog[i])
        {
            case OP_JMP_FWD:
                i++;
                if(!data[pos])
                    i = prog[i];
                break;
            case OP_JMP_BACK:
                i++;
                if(data[pos])
                    i += prog[i];
                break;
            case OP_GETCHAR:
            {
                int c = getchar();
                data[pos] = ((c != EOF) ? c : 0);
                break;
            }
            case OP_PUTCHAR:
                putchar(data[pos]);
                break;
            case OP_VAL_ADD:
                i++;
                data[pos] += prog[i];
                break;
            case OP_VAL_INC:
                data[pos]++;
                break;
            case OP_VAL_DEC:
                data[pos]--;
                break;
            case OP_POS_ADD:
                i++;
                pos += prog[i];
                break;
            case OP_POS_INC:
                pos++;
                break;
            case OP_POS_DEC:
                pos--;
                break;
            default:
                fprintf(stderr, "Unknown op[0x%08x] at: %d\n", prog[i], i);
                return i;
        }
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

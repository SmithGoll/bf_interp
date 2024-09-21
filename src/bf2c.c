/*
 * Brainf**k to C
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

#include <stdio.h>          // putchar, getchar, vprintf, fprintf, fopen, fgetc, fclose
#include <stdlib.h>         // exit
#include <errno.h>          // strerror, errno
#include <unistd.h>         // isatty
#include <stdarg.h>         // va_start, va_end

#define ABOUT \
    "BF2C v2.2 built on " __DATE__ " " __TIME__ ".\n" \
    "Copyright (c) 2024 - Brainf**k Tool written by SilentTalk.\n" \
    "Licensed under MIT. See source distribution for detailed\n" \
    "copyright notices.\n\n"

#define DATA_SIZE 65535

int getop(void);
void bf2c(void);
void depth_printf(int, const char*, ...);

FILE *fp = NULL;

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

void bf2c()
{
    printf("#include <stdio.h>\n\n"
        "extern int bf_getchar();\n"
        "extern int bf_putchar(int);\n\n"
        "char data[%d] = {};\n"
        "int pos = 0;\n\n"
        "int main()\n"
        "{\n", DATA_SIZE);

    int c;
    int depth = 0;
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

                if(!count)
                    break;
                if(count < 0)
                {
                    count = -count;
                    c = (c == '+') ? '-' : '+';
                }
                if(count == 1)
                    depth_printf(depth, "data[pos]%c%c;\n", c, c);
                else
                    depth_printf(depth, "data[pos] %c= %d;\n", c, count);
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

                if(!count)
                    break;
                if(count < 0)
                {
                    count = -count;
                    c = (c == '>') ? '<' : '>';
                }
                if(count == 1)
                {
                    char op = ((c == '>') ? '+' : '-');
                    depth_printf(depth, "pos%c%c;\n", op, op);
                }
                else
                    depth_printf(depth, "pos %c= %d;\n", ((c == '>') ? '+' : '-'), count);
                break;
            case '.':
            case ',':
                if(c == '.')
                {
                    depth_printf(depth, "bf_putchar(data[pos]);\n");
                } else {
                    depth_printf(depth, "data[pos] = bf_getchar();\n");
                }
                break;
            case '[':
                depth_printf(depth, "while(data[pos])\n");
                depth_printf(depth, "{\n");
                depth++;
                break;
            case ']':
                depth--;
                depth_printf(depth, "}\n");
                break;
        }
    }
    depth_printf(depth, "return 0;\n}\n");

    fclose(fp);
    return;
}

void depth_printf(int depth, const char *fmt, ...)
{
    if(depth < 0)
    {
        fprintf(stderr, "Wrong depth!\n");
        exit(1);
    }

    do
    {
        printf("    ");
    }
    while(depth--);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

int main(int argc, const char * argv[])
{
    if(argc != 2)
    {
        fprintf(stderr, ABOUT);
        fprintf(stderr, "Usage: %s bf-file\n", argv[0]);
        return -1;
    }

    if((fp = fopen(argv[1], "r")) == NULL)
    {
    	fprintf(stderr, "BF2C: %s (%s)\n", argv[1], strerror(errno));
    	return -1;
    }

    bf2c();

    return 0;
}

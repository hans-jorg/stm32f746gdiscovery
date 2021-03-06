/**
 * @file  ministdio.c
 * @brief Routines for minimal stdio support
 *
 * @note  Very simple and short routines for input/output
 *
 * @note  Uses getchar and putchar routines for input/output
 *
 * @note  Does not accept width and precision specification
 * @note  Does not return character count
 *
 * @note  Output routines: printf, puts, fputs
 *
 * @note  Input routines: fgets
 *
 */

#include <stdarg.h>
#include "ministdio.h"

/*
 * Use division in conversion
 */
#define USE_DIV

/*
 * Low level input/output
 */
extern int      miniputchar(int c);
extern int      minigetchar(void);

#define BS  '\x08'
#define DEL '\x7F'
#define CR  '\x0D'
#define LF  '\x0A'

static void
printbit(unsigned n,int size) {
unsigned int m = 1<<(size-1);
int flag = 0;

    while(m) {
        if( n&m ) {
            putchar('1');
            flag = 1;
        } else {
            if( flag ) putchar('0');
        }
        m >>= 1;
    }
}

static void
printuint(unsigned n) {
int d;
int k;
unsigned n10;

    d = 1;
    n10 = n/10;
    while( d < n10 ) d *= 10;

    while( d > 1 ) {
#ifdef USE_DIV
        k = n/d;
        n = n % d;
#else
        k = 0;
        while( n > d ) {
            n = n-d;
            k++;
        }
#endif
        putchar(k+'0');
        d /= 10;
    };

    putchar(n+'0');
}

static void
printhex(int n, int d) {
int pos = sizeof(int)*8-4;
int ch;

    while( pos >= 0 ) {
        ch = (n>>pos)&0x0F;
        if( ch < 10 )
            putchar(ch+'0');
        else
            putchar(ch-10+d);
        pos -= 4;
    }
}

static void
printint( int n) {
int d,n10,k;

    if( n < 0 ) {
        n = -n;
        putchar('-');
    }
    d = 1;
    n10 = n/10;
    while( d <= n10 ) d *= 10;
    while( d > 1 ) {
#ifdef USE_DIV
        k = n/d;
        n = n%d;
#else
        k = 0;
        while( (n = n-d) >= 0 ) k++;
        n += d;
#endif
        putchar(k+'0');
        d /= 10;
    };

    putchar(n+'0');
}

int
miniprintf(const char *fmt, ... ) {
va_list ap;
int n;
char ch;
char *p;

    va_start(ap,fmt);
    while( (ch = *fmt++) != 0 ) {
        if( ch != '%' ) {
            putchar(ch);
            if ( ch == '\n' ) putchar('\r');
        } else {
            ch = *fmt++;
            switch(ch) {
            case '%':
                putchar('%');
                break;
            case 'i':
            case 'd':
                n = va_arg(ap,int);
                printint(n);
                break;
            case 'u':
                n = va_arg(ap,int);
                printuint(n);
                break;
            case 'x':
                n = va_arg(ap,unsigned int);
                printhex(n,'a');
                break;
            case 'X':
                n = va_arg(ap,unsigned int);
                printhex(n,'A');
                break;
            case 'c':
                n = va_arg(ap,int);
                putchar(n);
                break;
            case 's':
                p = va_arg(ap,char *);
                while (*p) putchar(*p++);
                break;
            case 'b':
                n = va_arg(ap,unsigned int);
                printbit(n,sizeof(int)*8);
                break;
            default:
                putchar(ch);
                break;
            }
        }

    }
    va_end(ap);
    return 1;
}

int
miniputs(const char *s) {

    while( *s ) putchar(*s++);
    return 1;
}

int
minifputs(const char *s, void *ignored ) {

    while( *s ) putchar(*s++);
    return 1;
}

char *
minifgets(char *s, int n, void *ignored) {
char *p = s;
int c;

    n--; // make space for ending 0
    while( ((c=getchar()) != CR) && (c!=LF) ) {
        if( (c==BS) || (c==DEL) ) {
            if(n) {
                p--;
                n--;
            }
            putchar(BS);
            continue;
        }
        putchar(c);
        *p++ = c;
        n--;
    }
    *p = 0;
    return s;

}

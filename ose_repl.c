/*
  Copyright (c) 2019-22 John MacCallum Permission is hereby granted,
  free of charge, to any person obtaining a copy of this software
  and associated documentation files (the "Software"), to deal in
  the Software without restriction, including without limitation the
  rights to use, copy, modify, merge, publish, distribute,
  sublicense, and/or sell copies of the Software, and to permit
  persons to whom the Software is furnished to do so, subject to the
  following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <poll.h>
/* #include <arpa/inet.h> */
/* #include <netinet/in.h> */
/* #include <sys/socket.h> */
/* #include <sys/time.h> */
#include <sys/types.h>
#include <sys/ioctl.h>

#include "ose.h"
#include "ose_context.h"
#include "ose_util.h"
#include "ose_stackops.h"
#include "ose_assert.h"
#include "ose_vm.h"
#include "ose_symtab.h"
#include "ose_print.h"
#include "sys/ose_load.h"
/* #include "sys/ose_time.h" */

#include "ose_term.h"

#if defined(WIN32) || defined(__MINGW32__)
#define sigsetjmp(jb,s) setjmp(jb)
#define siglongjmp(jb,v) longjmp(jb,v)
#define sigjmp_buf jmp_buf
#endif

#ifndef MAX_BUNDLE_LEN
#define MAX_BUNDLE_LEN 1000000
#endif

#define OSEREPL_HOOK_INIT_POST "/hook/init/post"

/* erase line */
#define ANSI_CSI_EL0 "\033[0K"
#define ANSI_CSI_EL1 "\033[1K"
#define ANSI_CSI_EL2 "\033[2K"
/* erase display */
#define ANSI_CSI_ED0 "\033[0J"
#define ANSI_CSI_ED1 "\033[1J"
#define ANSI_CSI_ED2 "\033[2J"
#define ANSI_CSI_ED3 "\033[3J"
/* colors */
#define ANSI_COLOR_BLACK "\033[30m"
#define ANSI_COLOR_RED "\033[31m"
#define ANSI_COLOR_GREEN "\033[32m"
#define ANSI_COLOR_YELLOW "\033[33m"
#define ANSI_COLOR_BLUE "\033[34m"
#define ANSI_COLOR_MAGENTA "\033[35m"
#define ANSI_COLOR_CYAN "\033[36m"
#define ANSI_COLOR_WHITE "\033[37m"

#define ANSI_COLOR_BRIGHT_BLACK "\033[1;30m"
#define ANSI_COLOR_BRIGHT_RED "\033[1;31m"
#define ANSI_COLOR_BRIGHT_GREEN "\033[1;32m"
#define ANSI_COLOR_BRIGHT_YELLOW "\033[1;33m"
#define ANSI_COLOR_BRIGHT_BLUE "\033[1;34m"
#define ANSI_COLOR_BRIGHT_MAGENTA "\033[1;35m"
#define ANSI_COLOR_BRIGHT_CYAN "\033[1;36m"
#define ANSI_COLOR_BRIGHT_WHITE "\033[1;37m"

#define ANSI_COLOR_RESET "\033[0m"
                                      
/* For debugging */
#ifdef OSE_DEBUG
__attribute__((used))
static void pbndl(ose_bundle bundle, const char * const str)
{
    char buf[8192];
    memset(buf, 0, 8192);
    ose_pprintBundle(bundle, buf, 8192);
    fprintf(stderr, "\n\r%s>>>>>\n\r%s\n\r%s<<<<<\n\r",
            str, buf, str);
}

__attribute__((used))
static void pbytes(ose_bundle bundle, int32_t start, int32_t end)
{
    char *b = ose_getBundlePtr(bundle);
    for(int32_t i = start; i < end; i++){
        fprintf(stderr, "%d: %c %d\n\r", i,
                (unsigned char)b[i],
                (unsigned char)b[i]);
    }
}
void oserepl_debug(ose_bundle osevm){}
#endif

static struct pollfd fds[256];
static short nfd = 0;

static void oserepl_sigHandler(int signo);

/* Data and pointers for the main bundle and vm */
static char *bytes;
static ose_bundle bundle, osevm, vm_i, vm_s, vm_e, vm_c,
    vm_d, vm_o, vm_x;

void oserepl_lookup(ose_bundle osevm)
{
    ose_bundle vm_s = OSEVM_STACK(osevm);
    ose_bundle vm_e = OSEVM_ENV(osevm);
    ose_bundle vm_x = ose_enter(osevm, "/_x");

    const char * const address = ose_peekString(vm_s);
    int32_t mo = ose_getFirstOffsetForMatch(vm_e, address);
    if(mo >= OSE_BUNDLE_HEADER_LEN)
    {
        ose_drop(vm_s);
        ose_copyElemAtOffset(mo, vm_e, vm_s);
        return;
    }
    /* if it wasn't present in env, lookup in _x */
    mo = ose_getFirstOffsetForMatch(vm_x, address);
    if(mo >= OSE_BUNDLE_HEADER_LEN)
    {
        ose_drop(vm_s);
        ose_copyElemAtOffset(mo, vm_x, vm_s);
        return;
    }
    /* if it wasn't present in _x, lookup in the symtab */
    {
        ose_fn f = ose_symtab_lookup_fn(address);
        if(f)
        {
            ose_drop(vm_s);               
            ose_pushAlignedPtr(vm_s, f);
        }else
        {
            ;
        }
    }
}

static void oserepl_run(ose_bundle osevm)
{
    osevm_run(osevm);
}

static void oserepl_runHook(ose_bundle osevm,
                            const char * const hookaddr,
                            int32_t hookaddrlen)
{
    /* ose_copyBundle(vm_e, vm_s); */
    /* ose_pushString(vm_s, hookaddr); */
    /* OSEVM_LOOKUP(osevm); */
    /* if(ose_peekType(vm_s) == OSETT_MESSAGE */
    /*    && ose_peekMessageArgType(vm_s) == OSETT_BLOB) */
    /* { */
    /*     ose_blobToElem(vm_s); */
    /*     ose_pushMessage(vm_i, "/!/exec", strlen("/!/exec"), 0); */
    /*     oserepl_run(osevm); */
    /*     ose_replaceBundle(vm_s, vm_e); */
    /*     ose_clear(vm_s); */
    /* } */
    /* else */
    /* { */
    /*     ose_2drop(vm_s); */
    /* } */
}

static void oserepl_init(int32_t nbytes, char *bytes)
{
    memset(bytes, 0, nbytes);
    bundle = ose_newBundleFromCBytes(nbytes, bytes);
    osevm = osevm_init(bundle);
    vm_i = OSEVM_INPUT(osevm);
    vm_s = OSEVM_STACK(osevm);
    vm_e = OSEVM_ENV(osevm);
    vm_c = OSEVM_CONTROL(osevm);
    vm_d = OSEVM_DUMP(osevm);
    vm_o = OSEVM_OUTPUT(osevm);
    ose_pushContextMessage(osevm, 65536, "/_x");
    vm_x = ose_enter(osevm, "/_x");
}

static void oserepl_load(ose_bundle osevm)
{
    ose_bundle vm_s = OSEVM_STACK(osevm);
    ose_bundle vm_i = OSEVM_INPUT(osevm);
    if(ose_isStringType(ose_peekMessageArgType(vm_s)))
    {
        const char * const str = ose_peekString(vm_s);
        const size_t len = strlen(str);
        if((len > 3 && !strcmp(str + (len - 3), ".so"))
           || (len > 4 && !strcmp(str + (len - 4), ".dll"))
           || (len > 6 && !strcmp(str + (len - 6), ".dylib")))
        {
            ose_loadLib(osevm, str);
        }
        else if((len > 4 && !strcmp(str + (len - 4), ".ose")))
        {
            ose_readFileLines(vm_i, str);
            ose_popAllDrop(vm_i);
            ose_drop(vm_s);
        }
    }
    else
    {
        fprintf(stderr, "/!/load requires a path on the stack");
    }
}

static void oserepl_read(ose_bundle osevm)
{
    ose_bundle vm_s = OSEVM_STACK(osevm);
    if(ose_isStringType(ose_peekMessageArgType(vm_s)))
    {
        const char * const str = ose_peekString(vm_s);
        ose_readFile(vm_s, str);
    }
    else
    {
        fprintf(stderr, "/!/read requires a path on the stack");
    }
}

static void oserepl_readFromFileDescriptor(ose_bundle osevm)
{
    ose_bundle vm_s = OSEVM_STACK(osevm);
    int32_t fd;
    if(ose_bundleHasAtLeastNElems(vm_s, 1)
       && ose_peekType(vm_s) == OSETT_MESSAGE
       && ose_peekMessageArgType(vm_s) == OSETT_INT32)
    {
        fd = ose_popInt32(vm_s);
    }
    else
    {
        fprintf(stderr, "/!/readfd requires a file descriptor "
                "(int32) on the stack\n");
        return;
    }
    char buf[32];
    int32_t n = ose_termRead(fd, 32, buf);
    int32_t i;
    for(i = n - 1; i >= 0; --i)
    {
        ose_pushInt32(vm_s, buf[i]);
    }
    ose_pushInt32(vm_s, n);
}

static void oserepl_format(ose_bundle osevm)
{
    ose_bundle vm_s = OSEVM_STACK(osevm);
    int32_t n = ose_pprintBundle(vm_s, NULL, 0);
    int32_t pn = ose_pnbytes(n);
    ose_pushBlob(vm_s, pn, NULL);
    char *p = ose_peekBlob(vm_s) + 4;
    ose_pprintBundle(vm_s, p, pn);
    ose_pushInt32(vm_s, OSETT_STRING);
    ose_blobToType(vm_s);
}

static void oserepl_print(ose_bundle osevm)
{
    ose_bundle vm_s = OSEVM_STACK(osevm);
    if(ose_bundleHasAtLeastNElems(vm_s, 1)
       && ose_peekType(vm_s) == OSETT_MESSAGE
       && ose_peekMessageArgType(vm_s) == OSETT_STRING)
    {
        const char * const str = ose_peekString(vm_s);
        printf(ANSI_CSI_EL2"\r%s", str);
        fflush(stdout);
        ose_drop(vm_s);
    }
}

static void oserepl_println(ose_bundle osevm)
{
    ose_bundle vm_s = OSEVM_STACK(osevm);
    if(ose_bundleHasAtLeastNElems(vm_s, 1)
       && ose_peekType(vm_s) == OSETT_MESSAGE
       && ose_peekMessageArgType(vm_s) == OSETT_STRING)
    {
        const char * const str = ose_peekString(vm_s);
        printf(ANSI_CSI_EL2"\r%s\n", str);
        ose_drop(vm_s);
    }
}

static void oserepl_listen(ose_bundle osevm)
{
    ose_bundle vm_s = OSEVM_STACK(osevm);
    ose_assert(ose_bundleHasAtLeastNElems(vm_s, 1));
    ose_assert(ose_peekType(vm_s) == OSETT_MESSAGE);
    ose_assert(ose_peekMessageArgType(vm_s) == OSETT_INT32);
    int32_t fileno = ose_popInt32(vm_s);
    fds[nfd++] = (struct pollfd)
        {
            fileno, POLLIN, 0
        };
}

static void oserepl_quit(ose_bundle osevm)
{
    exit(0);
}

/* 
   usage
*/
#define USAGE_INDENT_4 "    "
#define USAGE_INDENT_2 "  "
#define USAGE_INDENT_WIDTH 4
#define USAGE_INDENT USAGE_INDENT_ ## 4

#define USAGE_EXPAND_EXPAND(a, b) a # b
#define USAGE_EXPAND(a, b) USAGE_EXPAND_EXPAND(a, b)
#define USAGE_OPT_FLAG_WIDTH 12
#define USAGE_OPT_FLAG_WIDTH_STR \
    USAGE_EXPAND("", USAGE_OPT_FLAG_WIDTH)
#define USAGE_OPT_FMT \
    USAGE_INDENT"%-"USAGE_OPT_FLAG_WIDTH_STR"s: %s\n"
#define USAGE_DESC_INDENT_COUNT \
    (USAGE_INDENT_WIDTH + USAGE_OPT_FLAG_WIDTH + 2)
char usage_desc_indent[USAGE_DESC_INDENT_COUNT + 1];

void usage_print_ldesc_impl(char *ldesc)
{
    int len = strlen(ldesc);
    const int n = 70 - USAGE_DESC_INDENT_COUNT;
    if(len < n)
    {
        printf("%s%s\n", usage_desc_indent, ldesc);
    }
    else
    {
        int nn = n;
        while(ldesc[nn] != ' ' && nn > 0)
        {
            --nn;
        }
        if(nn == 0)
        {
            /* this string has like 50 characters and none of them
               are spaces! */
            nn = n;
        }
        char c = ldesc[nn];
        ldesc[nn] = 0;
        printf("%s%s\n", usage_desc_indent, ldesc);
        ldesc[nn] = c;
        usage_print_ldesc_impl(ldesc + nn + 1);
    }
}

void usage_print_ldesc(const char * const ldesc)
{
    int ldesclen = strlen(ldesc);
    char myldesc[ldesclen + 1];
    memcpy(myldesc, ldesc, ldesclen + 1);
    usage_print_ldesc_impl(myldesc);
}

static void usage_print_opt(const char * const flag,
                            const char * const sdesc)
{   
    printf(USAGE_OPT_FMT, flag, sdesc);
}

static void usage_setup(void)
{
    memset(usage_desc_indent, ' ', USAGE_DESC_INDENT_COUNT);
    usage_desc_indent[USAGE_DESC_INDENT_COUNT] = 0;
}

static void usage(const char * const progname)
{
    usage_setup();
    printf("\n");
    printf("%s ["
           ANSI_COLOR_BRIGHT_WHITE"OPTIONS"ANSI_COLOR_RESET
           "] ["
           ANSI_COLOR_BRIGHT_WHITE"OSC ADDRESSES"ANSI_COLOR_RESET
           "]\n", progname);

    printf("\n");
    
    printf(ANSI_COLOR_BRIGHT_WHITE"OPTIONS:\n"ANSI_COLOR_RESET);
    usage_print_opt("-f <file>", ".ose file to load at startup.");
    usage_print_ldesc("If used, this flag must be the first option.");
    usage_print_opt("-h", "Print this help message.");
    
    printf("\n");
    
    printf(ANSI_COLOR_BRIGHT_WHITE"OSC ADDRESSES:\n"ANSI_COLOR_RESET);
    printf(USAGE_INDENT"0 or more OSC addresses that will be read and\n"
           USAGE_INDENT"executed in sequence from left to right.\n");

    printf("\n");
    
    printf(ANSI_COLOR_BRIGHT_WHITE"Examples:\n"ANSI_COLOR_RESET);
    printf(USAGE_INDENT"./o.se -f repl.ose '/s/Hello World!' '/!/println'\n");
    
    printf("\n");
}

sigjmp_buf jmp_buffer;

int main(int ac, char **av)
{
    if(ac == 1)
    {
        /* when o.se is run without any initial commands, not only
           does it not do anything, you can't even quit it because
           it's not listening to stdin! */
        usage(*av);
        exit(0);
    }
    int caught;
    /* install signal handler */
    if(signal(SIGINT, oserepl_sigHandler) == SIG_ERR)
    {
        fprintf(stderr,
                "error installing signal handler to catch SIGINT\n");
        return 0;
    }
    if(signal(SIGABRT, oserepl_sigHandler) == SIG_ERR)
    {
        fprintf(stderr,
                "error installing signal handler to catch SIGABRT\n");
        return 0;
    }
    if(signal(SIGTERM, oserepl_sigHandler) == SIG_ERR)
    {
        fprintf(stderr,
                "error installing signal handler to catch SIGTERM\n");
        return 0;
    }

    /* set up ose environment and vm */
    bytes = (char *)malloc(MAX_BUNDLE_LEN);
    if(!bytes)
    {
        fprintf(stderr, "couldn't allocate %d bytes for bundle\n",
                MAX_BUNDLE_LEN);
        return 1;
    }

    oserepl_init(MAX_BUNDLE_LEN, bytes);

    ose_pushMessage(vm_x, "/name", strlen("/name"), 1,
                    OSETT_STRING, av[0]);
    const char * const homedir = getenv("HOME");
    ose_pushMessage(vm_x, "/homedir", strlen("/homedir"), 1,
                    OSETT_STRING, homedir);

    ose_pushMessage(vm_x, "/load", strlen("/load"), 1,
                    OSETT_ALIGNEDPTR, oserepl_load);
    ose_pushMessage(vm_x, "/read", strlen("/read"), 1,
                    OSETT_ALIGNEDPTR, oserepl_read);
    ose_pushMessage(vm_x, "/readfd", strlen("/readfd"), 1,
                    OSETT_ALIGNEDPTR,
                    oserepl_readFromFileDescriptor);
    ose_pushMessage(vm_x, "/format", strlen("/format"), 1,
                    OSETT_ALIGNEDPTR, oserepl_format);
    ose_pushMessage(vm_x, "/print", strlen("/print"), 1,
                    OSETT_ALIGNEDPTR, oserepl_print);
    ose_pushMessage(vm_x, "/println", strlen("/println"), 1,
                    OSETT_ALIGNEDPTR, oserepl_println);
    ose_pushMessage(vm_x, "/listen", strlen("/listen"), 1,
                    OSETT_ALIGNEDPTR, oserepl_listen);
    ose_pushMessage(vm_x, "/quit", strlen("/quit"), 1,
                    OSETT_ALIGNEDPTR, oserepl_quit);
    ose_pushMessage(vm_x, "/stdin", strlen("/stdin"), 1,
                    OSETT_INT32, STDIN_FILENO);
    ose_pushMessage(vm_x, "/stdout", strlen("/stdout"), 1,
                    OSETT_INT32, STDOUT_FILENO);
    ose_pushMessage(vm_x, "/stderr", strlen("/stderr"), 1,
                    OSETT_INT32, STDERR_FILENO);
#ifdef OSE_DEBUG
    ose_pushMessage(vm_x, "/debug", strlen("/debug"), 1,
                    OSETT_ALIGNEDPTR, oserepl_debug);
#endif

    int32_t i = 1;
    if(ac > 1 && !strcmp(av[1], "-f"))
    {
        ose_pushString(vm_s, av[2]);
        oserepl_load(osevm);
        oserepl_run(osevm);
        i = 3;
    }

    if((caught = sigsetjmp(jmp_buffer, 1)) != 0)
    {
        switch(caught)
        {
        case SIGABRT:
            /* oserepl_init(bytes); */
            exit(0);
            break;
        case SIGTERM:
        case SIGINT:
            exit(0);
            break;
        }
        if(signal(caught, oserepl_sigHandler) == SIG_ERR)
        {
            fprintf(stderr,
                    "error installing signal handler\n");
            return 0;
        }
    }

    {
        ose_pushBundle(vm_s);       /* Stack */
        ose_pushBundle(vm_s);       /* Env */
    }

    if(ac > i)
    {
        /* eval anything passed on the command line */
        ose_pushBundle(vm_s);
        for( ; i < ac; i++)
        {
            ose_pushString(vm_s, av[i]);
            ose_push(vm_s);
        }
        ose_pushMessage(vm_i, "/!/exec3", strlen("/!/exec3"), 0);
        oserepl_run(osevm);
        ose_bundleAll(vm_s);
        ose_pop(vm_s);
        oserepl_runHook(osevm,
                        OSEREPL_HOOK_INIT_POST,
                        strlen(OSEREPL_HOOK_INIT_POST));
    }
    ose_termRaw();
    while(1)
    {
        if(poll(fds, nfd, -1))
        {
            short fdi;
            for(fdi = 0; fdi < nfd; fdi++)
            {
                if(fds[fdi].revents & POLLIN)
                {
                    ose_pushBundle(vm_s);
                    ose_pushInt32(vm_s, fds[fdi].fd);
                    ose_push(vm_s);
                    ose_pushString(vm_s, "/!/repl/run");
                    ose_push(vm_s);
                    ose_pushString(vm_i, "/!/exec3");
                    oserepl_run(osevm);
                    ose_bundleAll(vm_s);
                    ose_pop(vm_s);
                }
            }
        }
    }
    exit(0);
    return 0;
}

static void oserepl_sigHandler(int signo)
{
    siglongjmp(jmp_buffer, signo);
}

/*
  Copyright (c) 2019-21 John MacCallum Permission is hereby granted,
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

#include "ose.h"
#include "ose_context.h"
#include "ose_util.h"
#include "ose_stackops.h"
#include "ose_assert.h"
#include "ose_vm.h"
#include "ose_symtab.h"
#include "ose_print.h"
#include "sys/ose_load.h"
#include "sys/ose_term.h"
/* #include "sys/ose_time.h" */

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
    int32_t mo = ose_getFirstOffsetForPMatch(vm_e, address);
    if(mo >= OSE_BUNDLE_HEADER_LEN)
    {
        ose_drop(vm_s);
        ose_copyElemAtOffset(mo, vm_e, vm_s);
        return;
    }
    /* if it wasn't present in env, lookup in _x */
    mo = ose_getFirstOffsetForPMatch(vm_x, address);
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

static void oserepl_init(char *bytes)
{
    memset(bytes, 0, MAX_BUNDLE_LEN);
    bundle = ose_newBundleFromCBytes(MAX_BUNDLE_LEN, bytes);
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
    if(ose_isStringType(ose_peekMessageArgType(vm_s))
       == OSETT_TRUE)
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
    if(ose_isStringType(ose_peekMessageArgType(vm_s))
       == OSETT_TRUE)
    {
        const char * const str = ose_peekString(vm_s);
        ose_readFile(vm_s, str);
    }
    else
    {
        fprintf(stderr, "/!/read requires a path on the stack");
    }
}

static void oserepl_listen(ose_bundle osevm)
{
    ose_bundle vm_s = OSEVM_STACK(osevm);
    assert(ose_bundleHasAtLeastNElems(vm_s, 1) == OSETT_TRUE);
    assert(ose_peekType(vm_s) == OSETT_MESSAGE);
    assert(ose_peekMessageArgType(vm_s) == OSETT_INT32);
    int32_t fileno = ose_popInt32(vm_s);
    fds[nfd++] = (struct pollfd)
        {
            fileno, POLLIN, 0
        };
}

sigjmp_buf jmp_buffer;

int main(int ac, char **av)
{
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

    oserepl_init(bytes);

    ose_pushMessage(vm_x, "/name", strlen("/name"), 1,
                    OSETT_STRING, av[0]);
    const char * const homedir = getenv("HOME");
    ose_pushMessage(vm_x, "/homedir", strlen("/homedir"), 1,
                    OSETT_STRING, homedir);

    ose_pushMessage(vm_x, "/load", strlen("/load"), 1,
                    OSETT_ALIGNEDPTR, oserepl_load);
    ose_pushMessage(vm_x, "/read", strlen("/read"), 1,
                    OSETT_ALIGNEDPTR, oserepl_read);
    ose_pushMessage(vm_x, "/listen", strlen("/listen"), 1,
                    OSETT_ALIGNEDPTR, oserepl_listen);
    ose_pushMessage(vm_x, "/stdin", strlen("/stdin"), 1,
                    OSETT_INT32, STDIN_FILENO);
#ifdef OSE_DEBUG
    ose_pushMessage(vm_x, "/debug", strlen("/debug"), 1,
                    OSETT_ALIGNEDPTR, oserepl_debug);
#endif

    int32_t i = 1;
    if(!strcmp(av[1], "-f"))
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
        if(ose_bundleHasAtLeastNElems(vm_s, 1) == OSETT_TRUE)
        {
            if(ose_peekType(vm_s) == OSETT_MESSAGE
               && ose_peekMessageArgType(vm_s) == OSETT_STRING)
            {
                const char * const str = ose_peekString(vm_s);
                fprintf(stdout, ANSI_CSI_EL2"\r%s", str);
                fflush(stdout);
            }
            ose_clear(vm_s);
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
                    ose_pushInt32(vm_s, fds[fdi].fd);
                    {
                        ose_pushString(vm_s, "/repl/fd/cb/read");
                        OSEVM_LOOKUP(osevm);
                        ose_pushInt32(vm_s, fdi);
                        ose_nth(vm_s);
                    }
                    {
                        ose_pushString(vm_s, "/repl/fd/cb/print");
                        OSEVM_LOOKUP(osevm);
                        ose_pushInt32(vm_s, fdi);
                        ose_nth(vm_s);
                    }
                    ose_pushInt32(vm_s, 3);
                    ose_bundleFromTop(vm_s);
                    ose_pushString(vm_i, "/!/exec3");
                    oserepl_run(osevm);
                    if(ose_bundleHasAtLeastNElems(vm_s, 2)
                       == OSETT_TRUE)
                    {
                        ose_swap(vm_s);
                        if((ose_peekType(vm_s) == OSETT_MESSAGE)
                           && (ose_peekMessageArgType(vm_s)
                               == OSETT_STRING))
                        {
                            const char * const str =
                                ose_peekString(vm_s);
                            fprintf(stdout, ANSI_CSI_EL2"\r%s", str);
                            fflush(stdout);
                            ose_drop(vm_s);
                        }
                        else
                        {
                            ose_swap(vm_s);
                        }
                    }
                    /* { */
                    /*     ose_swap(vm_s); */
                    /*     const char * const str = ose_peekString(vm_s); */
                    /*     fprintf(stdout, ANSI_CSI_EL2"\r%s", str); */
                    /*     fflush(stdout); */
                    /*     ose_drop(vm_s); */
                    /* } */
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

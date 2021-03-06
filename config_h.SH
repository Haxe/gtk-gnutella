case "$CONFIG_SH" in
'') CONFIG_SH=config.sh;;
esac
case "$CONFIG_H" in
'') CONFIG_H=config.h;;
esac
case $CONFIG in
'')
	if test -f $CONFIG_SH; then TOP=.;
	elif test -f ../$CONFIG_SH; then TOP=..;
	elif test -f ../../$CONFIG_SH; then TOP=../..;
	elif test -f ../../../$CONFIG_SH; then TOP=../../..;
	elif test -f ../../../../$CONFIG_SH; then TOP=../../../..;
	else
		echo "Can't find $CONFIG_SH."; exit 1
	fi
	. $TOP/$CONFIG_SH
	;;
esac
case "$0" in
*/*) cd `expr X$0 : 'X\(.*\)/'` ;;
esac
echo "Extracting $CONFIG_H (with variable substitutions)"
sed <<!GROK!THIS! >$CONFIG_H -e 's!^#undef\(.*/\)\*!/\*#define\1 \*!' -e 's!^#un-def!#undef!'
/*
 * This file was produced by running the config_h.SH script, which
 * gets its values from $CONFIG_SH, which is generally produced by
 * running Configure.
 *
 * Feel free to modify any of this as the need arises.  Note, however,
 * that running config_h.SH again will wipe out any changes you've made.
 * For a more permanent change edit $CONFIG_SH and rerun config_h.SH.
 *
 * \$Id$
 */

/*
 * Package name      : $package
 * Source directory  : $src
 * Configuration time: $cf_time
 * Configured by     : $cf_by
 * Target system     : $myuname
 */

#ifndef _config_h_
#define _config_h_

/* VMS_SYSTEM:
 *	This symbol, if defined, indicates that the program is running under
 *	VMS.  It is currently only set in conjunction with the EUNICE symbol.
 */
/* LINUX_SYSTEM:
 *	This symbol, if defined, indicates that the program is running under
 *	a Linux system.
 */
#$d_eunice VMS_SYSTEM		/**/
#$d_linux LINUX_SYSTEM		/**/

/* MEM_ALIGNBYTES:
 *	This symbol contains the number of bytes required to align a
 *	double. Usual values are 2, 4 and 8.
 */
#define MEM_ALIGNBYTES $alignbytes	/**/

/* ARCHLIB_EXP:
 *	This symbol contains the ~name expanded version of ARCHLIB, to be used
 *	in programs that are not prepared to deal with ~ expansion at run-time.
 */
#$d_archlib ARCHLIB_EXP "$archlibexp"		/**/

/* BIN:
 *	This symbol holds the path of the bin directory where the package will
 *	be installed. Program must be prepared to deal with ~name substitution.
 */
#define BIN "$bin"	/**/

/* CAT2:
 *	This macro catenates 2 tokens together.
 */
/* CAT3:
 *	This macro catenates 3 tokens together.
 */
/* STRINGIFY:
 *	This macro surrounds its token with double quotes.
 */
#if $cpp_stuff == 1
#define CAT2(a,b)a/**/b
#define CAT3(a,b,c)a/**/b/**/c
#define STRINGIFY(a)"a"
#define SQuoTe(a)${cpp_quote}a
#define EQuoTe(a)a${cpp_quote}
#endif
#if $cpp_stuff == 42
#define CaTiFy(a,b) a ## b
#define CAT2(a,b) CaTiFy(a,b)
#define CAT3(a,b,c) CAT2(CaTiFy(a,b),c)
#define StGiFy(a)# a
#define STRINGIFY(a)StGiFy(a)
#endif
#if $cpp_stuff != 1 && $cpp_stuff != 42
#include "Bletch: How does this C preprocessor catenate tokens?"
#endif

/* getdtablesize:
 *	This catches use of the getdtablesize() subroutine, and remaps it
 *	to either ulimit(4,0) or NOFILE, if getdtablesize() isn't available.
 */
#$d_gettblsz getdtablesize() $tablesize	/**/

/* HAS_ARC4RANDOM:
 *	This symbol, if defined, indicates that the arc4random routine is
 *	available.
 */
#$d_arc4random HAS_ARC4RANDOM		/**/

/* HASATTRIBUTE:
 *	This symbol indicates the C compiler can check for function attributes,
 *	such as printf formats. This is normally only supported by GNU cc.
 */
#$d_attribut HASATTRIBUTE 	/**/
#ifndef HASATTRIBUTE
#define __attribute__(_arg_)
#endif

/* HAS_BACKTRACE:
 *	This symbol, if defined, indicates that the backtrace() routine is
 *	available to get a stack trace.  The <execinfo.h> header must be
 *	included to use this routine.
 */
#$d_backtrace HAS_BACKTRACE	/**/

/* HAS_BCOPY:
 *	This symbol is defined if the bcopy() routine is available to
 *	copy blocks of memory.
 */
#$d_bcopy HAS_BCOPY	/**/

/* HAS_BIND_TEXTDOMAIN_CODESET:
 *	This symbol, if defined, indicates that the bind_textdomain_codeset routine
 *	is available.
 */
#$d_bindtxtcode HAS_BIND_TEXTDOMAIN_CODESET		/**/

/* HAS_BUILTIN_POPCOUNT:
 *	This symbol, if defined, indicates that __builtin_popcount routine is
 *	available to compute the amount of '1' bits in a word.
 */
#$d_built_popcount HAS_BUILTIN_POPCOUNT	/**/

/* HAS_CLOSEFROM:
 *	This symbol is defined when closefrom() can be used.
 */
#$d_closefrom HAS_CLOSEFROM

/* HASCONST:
 *	This symbol, if defined, indicates that this C compiler knows about
 *	the const type. There is no need to actually test for that symbol
 *	within your programs. The mere use of the "const" keyword will
 *	trigger the necessary tests.
 */
#$d_const HASCONST	/**/
#ifndef HASCONST
#define const
#endif

/* HAS_DEV_POLL:
 *	This symbol is defined when /dev/poll can be used.
 */
#$d_dev_poll HAS_DEV_POLL

/* HAS_DIRENT_D_TYPE:
 *	This symbol, if defined, indicates that struct dirent has an
 *	integer member d_type.
 */
#$d_dirent_d_type HAS_DIRENT_D_TYPE		/**/

/* HAS_EPOLL:
 *	This symbol is defined when epoll() can be used.
 */
#$d_epoll HAS_EPOLL

/* FAST_ASSERTIONS:
 *	This symbol, when defined, indicates that the program should make
 *	use of its own asserting and failure reporting code, instead of
 *	the one from GLib.
 */
#$d_fast_assert FAST_ASSERTIONS	/**/

/* HAS_FORK:
 *	This symbol, if defined, indicates that the fork routine is
 *	available.
 */
#$d_fork HAS_FORK		/**/

/* HAS_GETADDRINFO:
 *	This symbol is defined when getaddrinfo() can be used.
 */
#$d_getaddrinfo HAS_GETADDRINFO

/* HAS_GETEUID:
 *	This symbol is defined when geteuid() can be used.
 */
#$d_geteuid HAS_GETEUID

/* HAS_UNAME:
 *	This symbol, if defined, indicates that the C program may use the
 *	uname() routine to derive the host name.  See also HAS_GETHOSTNAME
 *	and PHOSTNAME.
 */
#$d_uname HAS_UNAME		/**/

/* HAS_GETIFADDRS:
 *	This symbol is defined when getifaddrs() and freeifaddrs() can be used.
 */
#$d_getifaddrs HAS_GETIFADDRS

/* HAS_GETINVENT:
 *    This symbol, if defined, indicates that the getinvent() routine is
 *    available.
 */
#$d_getinvent       HAS_GETINVENT           /**/

/* HAS_GETLOGIN:
 *	This symbol, if defined, indicates that the getlogin routine is
 *	available to get the login name.
 */
#$d_getlogin HAS_GETLOGIN		/**/

/* HAS_GETPPID:
 *	This symbol, if defined, indicates that the getppid routine is
 *	available to get the parent process ID.
 */
#$d_getppid HAS_GETPPID		/**/

/* HAS_GETUID:
 *	This symbol is defined when getuid() can be used.
 */
#$d_getuid HAS_GETUID

/* HAS_GNULIBC:
 *	This symbol, if defined, indicates to the C program that 
 *	the GNU C library is being used.  A better check is to use
 *	the __GLIBC__ and __GLIBC_MINOR__ symbols supplied with glibc.
 */
#$d_gnulibc HAS_GNULIBC  	/**/
#if defined(HAS_GNULIBC) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

/* HAS_HERROR:
 *	This symbol, if defined, indicates that the herror routine is
 *	available.
 */
#$d_herror HAS_HERROR		/**/

/* HAS_HSTRERROR:
 *	This symbol, if defined, indicates that the hstrerror routine is
 *	available.
 */
#$d_hstrerror HAS_HSTRERROR		/**/

/* USE_IP_TOS:
 *	This symbol, if defined, indicates that the IP TOS services are
 *	available and can be used.  Be prepared to include <sys/socket.h>,
 * either <netinet/in.h> or <sys/in.h>, and <netinet/ip.h> when
 *	I_NETINET_IP is defined.
 */
#$d_iptos USE_IP_TOS		/**/

/* HAS_IPV6:
 *  This symbol is defined when IPv6 can be used
 */
#$d_ipv6 HAS_IPV6	/**/

/* HAS_ISASCII:
 *	This manifest constant lets the C program know that isascii 
 *	is available.
 */
#$d_isascii HAS_ISASCII		/**/

/* HAS_KEVENT_INT_UDATA:
 *	This symbol, if defined, indicates that struct kevent has an
 *	integer member udata.
 */
#$d_kevent_int_udata HAS_KEVENT_INT_UDATA		/**/

/* HAS_KQUEUE:
 *	This symbol is defined when kqueue() can be used.
 */
#$d_kqueue HAS_KQUEUE

/* HAS_LOCALE_CHARSET:
 *	This symbol is defined when locale_charset() can be used.
 */
#$d_locale_charset HAS_LOCALE_CHARSET

/* HAS_LSTAT:
 *	This symbol, if defined, indicates that the lstat routine is
 *	available to do file stats on symbolic links.
 */
#$d_lstat HAS_LSTAT		/**/

/* HAS_MADVISE:
 *	This symbol, if defined, indicates that the madvise routine is
 *	available.
 */
#$d_madvise HAS_MADVISE		/**/

/* HAS_MEMALIGN:
 *	This symbol, if defined, indicates that the memalign routine is
 *	available to allocate aligned memory.
 */
#$d_memalign HAS_MEMALIGN		/**/

/* HAS_MEMCPY:
 *	This symbol, if defined, indicates that the memcpy routine is available
 *	to copy blocks of memory.
 */
#$d_memcpy HAS_MEMCPY	/**/

/* HAS_MEMMOVE:
 *	This symbol, if defined, indicates that the memmove routine is available
 *	to copy potentially overlapping blocks of memory. This should be used
 *	only when HAS_SAFE_BCOPY is not defined. If neither is there, roll your
 *	own version.
 */
#$d_memmove HAS_MEMMOVE	/**/

/* HAS_MMAP:
 *	This symbol, if defined, indicates that the mmap system call is
 *	available to map a file into memory.
 */
#$d_mmap HAS_MMAP		/**/

/* HAS_MSGHDR_MSG_FLAGS:
 *	This symbol, if defined, indicates that struct msghdr has a
 *	member msg_flags.
 */
#$d_msghdr_msg_flags HAS_MSGHDR_MSG_FLAGS		/**/

/* HAS_NANOSLEEP:
 *	This symbol, if defined, indicates that the nanosleep() system call
 *	is available.
 */
#$d_nanosleep HAS_NANOSLEEP		/**/

/* HAS_PAUSE:
 *	This symbol, if defined, indicates that the pause routine is
 *	available to suspend a process until a signal is received.
 */
#$d_pause HAS_PAUSE		/**/

/* HAS_POLL:
 *	This symbol, if defined, indicates that the poll routine is
 *	available to poll active file descriptors.  Please check I_POLL and
 *	I_SYS_POLL to know which header should be included as well.
 */
#$d_poll HAS_POLL		/**/

/* HAS_POPEN:
 *	This symbol, if defined, indicates that the popen routine is
 *	available to open a pipe from a process.
 */
#$d_popen HAS_POPEN		/**/

/* HAS_POSIX_FADVISE:
 *	This symbol is defined when posix_fadvise() can be used.
 */
#$d_posix_fadvise HAS_POSIX_FADVISE

/* HAS_POSIX_MEMALIGN:
 *	This symbol is defined when posix_memalign() can be used.
 */
#$d_posix_memalign HAS_POSIX_MEMALIGN

/* HAS_PREAD:
 *	This symbol, if defined, indicates that the pread routine is
 *	available to perform reads on a file descriptor at a given offset.
 */
#$d_pread HAS_PREAD		/**/

/* HAS_PREADV:
 *	This symbol, if defined, indicates that the preadv routine is
 *	available to perform vectored reads on a file descriptor at a
 * given offset.
 */
#$d_preadv HAS_PREADV		/**/

/* HAS_PWRITE:
 *	This symbol, if defined, indicates that the pwrite routine is
 *	available to perform writes on a file descriptor at a given offset.
 */
#$d_pwrite HAS_PWRITE		/**/

/* HAS_PWRITEV:
 *	This symbol, if defined, indicates that the pwritev routine is
 *	available to perform vectored writes on a file descriptor at a
 * given offset.
 */
#$d_pwritev HAS_PWRITEV		/**/

/* HAS_RECVMSG:
 *	This symbol, if defined, indicates that the recvmsg() function
 *	is available.
 */
#$d_recvmsg HAS_RECVMSG		/**/

/* HAS_REGPARM:
 *	This symbol is defined when __attribute__((__regparm__(n))) can be used.
 */
#$d_regparm HAS_REGPARM

/* HAS_GETRUSAGE:
 *	This symbol, if defined, indicates that the getrusage() routine is
 *	available to get process statistics with a sub-second accuracy.
 *	Inclusion of <sys/resource.h> and <sys/time.h> may be necessary.
 */
#$d_rusage HAS_GETRUSAGE		/**/

/* HAS_SELECT:
 *	This symbol, if defined, indicates that the select routine is
 *	available to select active file descriptors. If the timeout field
 *	is used, <sys/time.h> may need to be included.
 */
#$d_select HAS_SELECT	/**/

/* HAS_SENDFILE:
 *	This symbol, if defined, indicates that the sendfile routine is
 *	available.
 */
#$d_sendfile HAS_SENDFILE		/**/

/* HAS_SETPROCTITLE:
 *	This symbol is defined when setproctitle() can be used and takes a
 *	format string.
 */
#$d_setproctitle HAS_SETPROCTITLE

/* HAS_SETSID:
 *	This symbol, if defined, indicates that the setsid routine is
 *	available to set the process group ID.
 */
#$d_setsid HAS_SETSID	/**/

/* HAS_SIGACTION:
 *	This symbol, if defined, indicates that Vr4's sigaction() routine
 *	is available.
 */
#$d_sigaction HAS_SIGACTION	/**/

/* HAS_SIGPROCMASK:
 *	This symbol, if defined, indicates that the sigprocmask() routine is
 *	available to examine and change blocked signals.
 */
#$d_sigprocmask HAS_SIGPROCMASK	/**/

/* Sigjmp_buf:
 *	This is the buffer type to be used with Sigsetjmp and Siglongjmp.
 */
/* Sigsetjmp:
 *	This macro is used in the same way as sigsetjmp(), but will invoke
 *	traditional setjmp() if sigsetjmp isn't available.
 *	See HAS_SIGSETJMP.
 */
/* Siglongjmp:
 *	This macro is used in the same way as siglongjmp(), but will invoke
 *	traditional longjmp() if siglongjmp isn't available.
 *	See HAS_SIGSETJMP.
 */
#$d_sigsetjmp HAS_SIGSETJMP	/**/
#ifdef HAS_SIGSETJMP
#define Sigjmp_buf sigjmp_buf
#define Sigsetjmp(buf,save_mask) sigsetjmp((buf),(save_mask))
#define Siglongjmp(buf,retval) siglongjmp((buf),(retval))
#else
#define Sigjmp_buf jmp_buf
#define Sigsetjmp(buf,save_mask) setjmp((buf))
#define Siglongjmp(buf,retval) longjmp((buf),(retval))
#endif

/* HAS_SOCKADDR_IN_SIN_LEN:
 *	This symbol is defined if struct sockaddr_in has sin_len.
 */
#$d_sockaddr_in_sin_len HAS_SOCKADDR_IN_SIN_LEN

/* HAS_SOCKADDR_UN:
 *	This symbol is defined if struct sockaddr_un exists, allowing the
 *	creation of file sockets.
 */
#$d_sockaddr_un HAS_SOCKADDR_UN

/* HAS_SOCKER_GET:
 *	This symbol is defined when socker_get() can be used to get sockets
 *	bound to privileged ports.
 */
#$d_socker_get HAS_SOCKER_GET

/* HAS_SOCKET:
 *	This symbol, if defined, indicates that the BSD socket interface is
 *	supported.
 */
#$d_socket HAS_SOCKET		/**/

/* HAS_STATFS:
 *	This symbol, if defined, indicates that the Linux statfs() system call
 *	is available to get filesystem statistics.
 */
#$d_statfs HAS_STATFS		/**/

/* HAS_STATVFS:
 *	This symbol, if defined, indicates that the POSIX statvfs() system call
 *	is available to get filesystem statistics.
 */
#$d_statvfs HAS_STATVFS		/**/

/* HAS_STRLCAT:
 *	This symbol, if defined, indicates that the strlcat routine is
 *	available.
 */
#$d_strlcat HAS_STRLCAT		/**/

/* HAS_STRLCPY:
 *	This symbol, if defined, indicates that the strlcpy routine is
 *	available.
 */
#$d_strlcpy HAS_STRLCPY		/**/

/* HAS_SYSCTL:
 *	This symbol, if defined, indicates that the sysctl routine is
 *	available.
 */
#$d_sysctl HAS_SYSCTL		/**/

/* HAS_SYSTEM:
 *	This symbol, if defined, indicates that the system routine is
 *	available to issue a shell command.
 */
#$d_system HAS_SYSTEM	/**/

/* HAS_TIMES:
 *	This symbol, if defined, indicates that the times() routine exists.
 *	Note that this became obsolete on some systems (SUNOS), which now
 * use getrusage(). It may be necessary to include <sys/times.h>.
 */
#$d_times HAS_TIMES		/**/

/* HAS_TTYNAME:
 *	This symbol, if defined, indicates that the ttyname() routine is
 *	available to determine the pathname of the terminal associated with
 *	a file descriptor.  The <unistd.h> header must be included to use
 *	this routine.
 */
#$d_ttyname HAS_TTYNAME	/**/

/* HAS_USLEEP:
 *	This symbol, if defined, indicates that the usleep routine is
 *	available to let the process sleep on a sub-second accuracy.
 */
#$d_usleep HAS_USLEEP		/**/

/* HASVOLATILE:
 *	This symbol, if defined, indicates that this C compiler knows about
 *	the volatile declaration.
 */
#$d_volatile	HASVOLATILE	/**/
#ifndef HASVOLATILE
#define volatile
#endif

/* HAS_VSNPRINTF:
 *	This symbol, if defined, indicates that the vsnprintf routine is
 *	available.
 */
#$d_vsnprintf HAS_VSNPRINTF		/**/

/* HAS_WAITPID:
 *	This symbol, if defined, indicates that the waitpid routine is
 *	available to wait for child process.
 */
#$d_waitpid HAS_WAITPID	/**/

/* EBCDIC:
 *     This symbol, if defined, indicates that this system uses
 *	EBCDIC encoding.
 */
#$ebcdic	EBCDIC 		/**/

/* ENABLE_NLS:
 *	This symbol, when defines, indicates that the program should enable
 *	Native Language Support (NLS) for internationalization (I18N).
 */
#$d_enablenls ENABLE_NLS	/**/

/* I_ARPA_INET:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <arpa/inet.h> to get inet_addr and friends declarations.
 */
#$i_arpainet	I_ARPA_INET		/**/

/* I_DIRENT:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <dirent.h>. Using this symbol also triggers the definition
 *	of the Direntry_t define which ends up being 'struct dirent' or
 *	'struct direct' depending on the availability of <dirent.h>.
 */
#$i_dirent I_DIRENT		/**/

/* I_EXECINFO:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <execinfo.h> for backtrace() support.
 */
#$i_execinfo I_EXECINFO		/**/

/* I_FCNTL:
 *	This manifest constant tells the C program to include <fcntl.h>.
 */
#$i_fcntl I_FCNTL	/**/

/* I_ICONV:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <iconv.h>.
 */
#$i_iconv I_ICONV	/**/

/* I_IFADDRS:
 *    This symbol, if defined, indicates to the C program that it should
 *    include <ifaddrs.h>.
 */
#$i_ifaddrs I_IFADDRS         /**/

/* I_INTTYPES:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <inttypes.h> to get format conversions of integer types.
 */
#$i_inttypes I_INTTYPES		/**/

/* I_INVENT:
 *    This symbol, if defined, indicates to the C program that it should
 *    include <invent.h>.
 */
#$i_invent I_INVENT         /**/

/* I_LANGINFO:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <langinfo.h>.
 */
#$i_langinfo I_LANGINFO		/**/

/* I_LIBCHARSET:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <libcharset.h>.
 */
#$i_libcharset I_LIBCHARSET		/**/

/* I_LINUX_NETLINK:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <linux/netlink.h> to get definitions for the NLMSG_DATA() and
 *	other NLMSG_NEXT() macros.
 */
#$i_linux_netlink I_LINUX_NETLINK		/**/

/* I_LINUX_RTNETLINK:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <linux/rtnetlink.h> to get definitions for the RTA_DATA() and
 *	other RTA_NEXT() macros.
 */
#$i_linux_rtnetlink I_LINUX_RTNETLINK		/**/

/* I_MATH:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <math.h>.
 */
#$i_math I_MATH		/**/

/* I_NETDB:
 *    This symbol, if defined, indicates to the C program that it should
 *    include <netdb.h>.
 */
#$i_netdb I_NETDB         /**/

/* I_NET_IF:
 *    This symbol, if defined, indicates to the C program that it should
 *    include <net/if.h>.
 */
#$i_netif I_NET_IF         /**/

/* I_NET_ROUTE:
 *	This symbol, if defined, indicates to the C program that it can
 *	include <net/route.h>.
 */
#$i_netroute I_NET_ROUTE	/**/

/* I_NETINET_IN:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <netinet/in.h>. Otherwise, you may try <sys/in.h>.
 */
#$i_niin I_NETINET_IN	/**/

/* I_NETINET_IP:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <netinet/ip.h>.
 */
#$i_niip I_NETINET_IP	/**/

/* I_POLL:
 *	This symbol, if defined, indicates that the program may include
 *	<poll.h>.  If there is no <poll.h>, then you may check I_SYS_POLL
 *	instead to see whether there is a <sys/poll.h> to include.
 */
#$i_poll I_POLL	/**/

/* I_PWD:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <pwd.h>.
 */
#$i_pwd I_PWD		/**/

/* I_REGEX:
 *	This symbol, if defined, indicates that <regex.h> exists and should
 *	be included.
 */
#$i_regex I_REGEX	/**/

/* I_STDLIB:
 *	This symbol, if defined, indicates that <stdlib.h> exists and should
 *	be included.
 */
#$i_stdlib I_STDLIB		/**/

/* I_STRING:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <string.h> (USG systems) instead of <strings.h> (BSD systems).
 */
#$i_string I_STRING		/**/

/* I_SYS_FILE:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/file.h> to get definition of R_OK and friends.
 */
#$i_sysfile I_SYS_FILE		/**/

/* I_SYS_MMAN:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/mman.h>.
 */
#$i_sysmman	I_SYS_MMAN		/**/

/* I_SYS_MOUNT:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/mount.h> to get the struct statfs definition for the
 *	statfs() system call.
 */
#$i_sysmount I_SYS_MOUNT		/**/

/* I_SYS_PARAM:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/param.h>.
 */
#$i_sysparam I_SYS_PARAM		/**/

/* I_SYS_POLL:
 *	This symbol, if defined, indicates that the program may include
 *	<sys/poll.h>.  When I_POLL is also defined, it's probably safest
 *	to only include <poll.h>.
 */
#$i_syspoll I_SYS_POLL	/**/

/* I_SYS_RESOURCE:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/resource.h>.
 */
#$i_sysresrc I_SYS_RESOURCE		/**/

/* I_SYS_SELECT:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/select.h> in order to get definition of struct timeval.
 */
#$i_sysselct I_SYS_SELECT	/**/

/* I_SYS_SENDFILE:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/sendfile.h>.
 */
#$i_syssendfile I_SYS_SENDFILE		/**/

/* I_SYS_SOCKET:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/socket.h> before performing socket calls.
 */
#$i_syssock I_SYS_SOCKET		/**/

/* I_SYS_STAT:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/stat.h>.
 */
#$i_sysstat	I_SYS_STAT		/**/

/* I_SYS_STATVFS:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/statvfs.h> to get the struct statvfs definition for the
 *	statvfs() system call.
 */
#$i_sysstatvfs I_SYS_STATVFS		/**/

/* I_SYS_SYSCTL:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/sysctl.h>.
 */
#$i_syssysctl I_SYS_SYSCTL		/**/

/* I_SYS_TIMES:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/times.h>.
 */
#$i_systimes	I_SYS_TIMES		/**/

/* I_SYS_TYPES:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/types.h>.
 */
#$i_systypes	I_SYS_TYPES		/**/

/* I_SYS_UN:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/un.h> to get UNIX domain socket definitions.
 */
#$i_sysun I_SYS_UN		/**/

/* I_SYS_UTSNAME:
 *    This symbol, if defined, indicates to the C program that it should
 *    include <sys/utsname.h>.
 */
#$i_sysutsname I_SYS_UTSNAME         /**/

/* I_SYS_VFS:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/vfs.h> to get the struct statfs definition for the
 *	statfs() system call.
 */
#$i_sysvfs I_SYS_VFS		/**/

/* I_TIME:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <time.h>.
 */
/* I_SYS_TIME:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/time.h>.
 */
/* I_SYS_TIME_KERNEL:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <sys/time.h> with KERNEL defined.
 */
#$i_time I_TIME		/**/
#$i_systime I_SYS_TIME		/**/
#$i_systimek I_SYS_TIME_KERNEL		/**/

/* I_UNISTD:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <unistd.h>.
 */
#$i_unistd I_UNISTD		/**/

/* I_STDARG:
 *	This symbol, if defined, indicates that <stdarg.h> exists and should
 *	be included.
 */
#$i_stdarg I_STDARG		/**/

/* I_WINSOCK2:
 *	This symbol, if defined, indicates to the C program that it should
 *	include <Winsock2.h>.
 */
#$i_winsock2 I_WINSOCK2		/**/

/* LOCALE_EXP:
 *	This symbol holds the possibly ~name expanded place where localization
 *	files for the package are expected to be found.
 *	Only meaningful when ENABLE_NLS is defined.
 */
#define LOCALE_EXP "$localeexp"

/* VAL_O_NONBLOCK:
 *	This symbol is to be used during open() or fcntl(F_SETFL) to turn on
 *	non-blocking I/O for the file descriptor. Note that there is no way
 *	back, i.e. you cannot turn it blocking again this way. If you wish to
 *	alternatively switch between blocking and non-blocking, use the
 *	ioctl(FIOSNBIO) call instead, but that is not supported by all devices.
 */
/* VAL_EAGAIN:
 *	This symbol holds the errno error code set by read() when no data was
 *	present on the non-blocking file descriptor.
 *
 * FIXME: And who guarantees this isn't e.g. device-dependent?
 *	   If EAGAIN is defined one should expect it.
 *	   If EWOULDBLOCK is defined one should expect it.
 *	   If both are defined one should expect both.
 * 	   -- cbiere, 2011-01-18
 */
#define VAL_O_NONBLOCK $o_nonblock
#define VAL_EAGAIN $eagain

/* OFFICIAL_BUILD:
 * When defined, the build is "official".  Programs generated for an
 *	official build MUST NOT peek into the place where the sources lie, via
 *	PACKAGE_SOURCE_DIR or any other means.
 */
#$d_official OFFICIAL_BUILD	/**/

/* PACKAGE:
 *	This variable contains the name of the package being built.
 */
#define PACKAGE "$package"

/* PACKAGE_SOURCE_DIR:
 * Holds the directory name holding the package source.
 */
#define PACKAGE_SOURCE_DIR "$pkgsrc"

/* PRIVLIB_EXP:
 *	This symbol contains the ~name expanded version of PRIVLIB, to be used
 *	in programs that are not prepared to deal with ~ expansion at run-time.
 */
#define PRIVLIB_EXP "$privlibexp"		/**/

/* SIG_NAME:
 *	This symbol contains a list of signal names in order of
 *	signal number. This is intended
 *	to be used as a static array initialization, like this:
 *		char *sig_name[] = { SIG_NAME };
 *	The signals in the list are separated with commas, and each signal
 *	is surrounded by double quotes. There is no leading SIG in the signal
 *	name, i.e. SIGQUIT is known as "QUIT".
 *	Gaps in the signal numbers (up to NSIG) are filled in with NUMnn,
 *	etc., where nn is the actual signal number (e.g. NUM37).
 *	The signal number for sig_name[i] is stored in sig_num[i].
 *	The last element is 0 to terminate the list with a NULL.  This
 *	corresponds to the 0 at the end of the sig_num list.
 */
/* SIG_COUNT:
 *	This variable contains a number larger than the largest
 *	signal number.  This is usually the same as the NSIG macro.
 */
#define SIG_NAME $sig_name_init		/**/
#define SIG_COUNT $sig_count			/**/

/* VOIDFLAGS:
 *	This symbol indicates how much support of the void type is given by this
 *	compiler.  What various bits mean:
 *
 *	    1 = supports declaration of void
 *	    2 = supports arrays of pointers to functions returning void
 *	    4 = supports comparisons between pointers to void functions and
 *		    addresses of void functions
 *	    8 = suports declaration of generic void pointers
 *
 *	The package designer should define VOIDUSED to indicate the requirements
 *	of the package.  This can be done either by #defining VOIDUSED before
 *	including config.h, or by defining defvoidused in Myinit.U.  If the
 *	latter approach is taken, only those flags will be tested.  If the
 *	level of void support necessary is not present, defines void to int.
 */
#ifndef VOIDUSED
#define VOIDUSED $defvoidused
#endif
#define VOIDFLAGS $voidflags
#if (VOIDFLAGS & VOIDUSED) != VOIDUSED
#define void int		/* is void to be avoided? */
#define M_VOID			/* Xenix strikes again */
#endif

/* CAN_PROTOTYPE:
 *	If defined, this macro indicates that the C compiler can handle
 *	function prototypes.
 */
/* P:
 *	This macro is used to declare function parameters for folks who want
 *	to make declarations with prototypes using a different style than
 *	the above macros.  Use double parentheses.  For example:
 *
 *		int main P((int argc, char *argv[]));
 */
#$prototype	CAN_PROTOTYPE	/**/
#ifdef CAN_PROTOTYPE
#define	P(args) args
#else
#define	P(args) ()
#endif

/* HAS_DBUS:
 * This symbol is defined when dbus is present
 */
#$d_dbus HAS_DBUS  /**/

/* USE_GLIB1:
 * This symbol is defined when compiling for GLib 1.x.
 */
/* USE_GLIB2:
 * This symbol is defined when compiling for GLib 2.x.
 */
#$d_useglib1 USE_GLIB1    /**/
#$d_useglib2 USE_GLIB2    /**/

/* HAS_GNUTLS:
 * This symbol is defined when GnuTLS is present
 */
#$d_gnutls HAS_GNUTLS  /**/

/* USE_GTK1:
 * This symbol is defined when compiling for the GTK1 toolkit.
 */
/* USE_GTK2:
 * This symbol is defined when compiling for the GTK2 toolkit.
 */
#$d_usegtk1 USE_GTK1    /**/
#$d_usegtk2 USE_GTK2    /**/

/* USE_REMOTE_CTRL:
 * This symbol is defined when they want remote control support.
 */
#$d_remotectrl USE_REMOTE_CTRL    /**/

/* USE_TOPLESS:
 *	This symbol, when defined, indicates that there is no GUI interface.
 */
#$d_headless USE_TOPLESS	/**/

#endif
!GROK!THIS!

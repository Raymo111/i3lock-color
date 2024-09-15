#                                               -*- Autoconf -*-
# Run autoreconf -fi to generate a configure script from this file.

AC_PREREQ([2.69])
AC_INIT([i3lock-color],[m4_esyscmd_s([cat I3LOCK_VERSION])],[https://github.com/Raymo11/i3lock-color/issues])
# For AX_EXTEND_SRCDIR
AX_ENABLE_BUILDDIR
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall no-dist-gzip dist-bzip2])
# Default to silent rules, use V=1 to get verbose compilation output.
AM_SILENT_RULES([yes])
# Make it possible to disable maintainer mode to disable re-generation of build
# system files.
AM_MAINTAINER_MODE([enable])
AC_CONFIG_SRCDIR([i3lock.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

dnl Verify macros defined in m4/ such as AX_SANITIZERS are not present in the
dnl output, i.e. are replaced as expected. This line results in a better error
dnl message when using aclocal < 1.13 (which does not understand
dnl AC_CONFIG_MACRO_DIR) without passing the -I m4 parameter.
m4_pattern_forbid([AX_SANITIZERS])

# Verify we are using GNU make because we use '%'-style pattern rules in
# Makefile.am, which are a GNU make extension. Pull requests to replace
# '%'-style pattern rules with a more portable alternative are welcome.
AX_CHECK_GNU_MAKE
AS_VAR_IF([_cv_gnu_make_command], [""], [AC_MSG_ERROR([the i3lock Makefile.am requires GNU make])])

AX_EXTEND_SRCDIR

AS_IF([test -d ${srcdir}/.git],
      [
        VERSION="$(git -C ${srcdir} describe --tags --abbrev=0)"
        I3LOCK_VERSION="$(git -C ${srcdir} describe --tags --always) ($(git -C ${srcdir} rev-list --pretty=format:%cd --date=short -n1 HEAD | tail -n1))"
        # Mirrors what libi3/is_debug_build.c does:
        is_release=$(test $(echo "${I3LOCK_VERSION}" | cut -d ' ' -f 1 | wc -m) -lt 13 && echo yes || echo no)
      ],
      [
        VERSION="$(cut -d '-' -f 1 ${srcdir}/I3LOCK_VERSION | cut -d ' ' -f 1)"
        I3LOCK_VERSION="$(sed -e 's/@<:@\"?\\@:>@/\\&/g' ${srcdir}/I3LOCK_VERSION)"
        is_release="$(grep -q non-git ${srcdir}/I3LOCK_VERSION && echo no || echo yes)"
      ])
AC_SUBST([I3LOCK_VERSION], [$I3LOCK_VERSION])
AC_DEFINE_UNQUOTED([I3LOCK_VERSION], ["${I3LOCK_VERSION}"], [i3lock version])

AX_CODE_COVERAGE

dnl is_release must be lowercase because AX_CHECK_ENABLE_DEBUG calls m4_tolower
dnl on its fourth argument.
AX_CHECK_ENABLE_DEBUG([no], , , [$is_release])

AC_PROG_CC

# For strnlen() and vasprintf().
AC_USE_SYSTEM_EXTENSIONS

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
dnl The error message should include the specific type which could not be
dnl found, but I do not see a way to achieve that.
AC_CHECK_TYPES([mode_t, off_t, pid_t, size_t, ssize_t], , [AC_MSG_FAILURE([cannot find required type])])

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_STRNLEN
AC_CHECK_FUNCS([atexit dup2 ftruncate getcwd gettimeofday localtime_r memchr memset mkdir rmdir setlocale socket strcasecmp strchr strdup strerror strncasecmp strndup strrchr strspn strstr strtol strtoul], , [AC_MSG_FAILURE([cannot find the $ac_func function, which i3lock requires])])
AC_CHECK_FUNCS([explicit_bzero])

# Check for libraries
AC_SEARCH_LIBS([floor], [m], , [AC_MSG_FAILURE([cannot find the required floor() function despite trying to link with -lm])])

# libev does not ship with a pkg-config file :(.
AC_SEARCH_LIBS([ev_run], [ev], , [AC_MSG_FAILURE([cannot find the required ev_run() function despite trying to link with -lev])])

AC_SEARCH_LIBS([shm_open], [rt])

AC_SEARCH_LIBS([DGifOpenFileName], [gif])
# Use system-local-login instead of login on Arch and Gentoo
if [[ -f /etc/arch-release ]] || [[ -f /etc/gentoo-release ]]; then
    echo "Using PAM for Arch/Gentoo"
    sed -i "s/^#auth include system-local-login/auth include system-local-login/g" ../pam/i3lock
    sed -i "s/^auth include login/#auth include login/g" ../pam/i3lock
fi

# Only disable PAM on OpenBSD where i3lock uses BSD Auth instead
case "$host" in
	*-openbsd*)
	# Nothing yet.
	;;
	*)
	AC_SEARCH_LIBS([pam_authenticate], [pam])
	;;
esac

AC_SEARCH_LIBS([iconv_open], [iconv], , [AC_MSG_FAILURE([cannot find the required iconv_open() function despite trying to link with -liconv])])

dnl Each prefix corresponds to a source tarball which users might have
dnl downloaded in a newer version and would like to overwrite.
PKG_CHECK_MODULES([XCB], [xcb xcb-xkb xcb-xinerama xcb-randr xcb-composite])
PKG_CHECK_MODULES([XCB_IMAGE], [xcb-image])
PKG_CHECK_MODULES([XCB_UTIL], [xcb-event xcb-util xcb-atom])
PKG_CHECK_MODULES([XCB_UTIL_XRM], [xcb-xrm])
PKG_CHECK_MODULES([XKBCOMMON], [xkbcommon xkbcommon-x11])
PKG_CHECK_MODULES([CAIRO], [cairo])
PKG_CHECK_MODULES([JPEG], [libjpeg])
PKG_CHECK_MODULES([FONTCONFIG], [fontconfig])


# Checks for programs.
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_LN_S

AM_PROG_AR

AX_FLAGS_WARN_ALL
AX_APPEND_FLAG([-O2], [AM_CFLAGS])
AX_APPEND_FLAG([-funroll-loops], [AM_CFLAGS])
AX_APPEND_FLAG([-pthread], [AM_CFLAGS])
AX_CHECK_COMPILE_FLAG([-Wunused-value], [AX_APPEND_FLAG([-Wunused-value], [AM_CFLAGS])])
AC_SUBST(AM_CFLAGS)

AX_CHECK_BASH_COMPLETION
AX_CHECK_ZSH_COMPLETION

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h float.h inttypes.h limits.h locale.h netinet/in.h paths.h stddef.h stdint.h stdlib.h string.h sys/param.h sys/socket.h sys/time.h unistd.h gif_lib.h], , [AC_MSG_FAILURE([cannot find the $ac_header header, which i3lock requires])])

AC_CONFIG_FILES([Makefile])

# Enable address sanitizer for debug builds. The performance hit is a
# 50% increase of wallclock time for the testsuite on my machine.
if test "x$ax_enable_debug" = "xyes"; then
	default_sanitizers=address
else
	default_sanitizers=
fi
AX_SANITIZERS(, [$default_sanitizers])

AC_OUTPUT

in_git_worktree=`git rev-parse --is-inside-work-tree 2>/dev/null`
if [[ "$in_git_worktree" = "true" ]]; then
	git_dir=`git rev-parse --git-dir 2>/dev/null`
	srcdir=`dirname "$git_dir"`
	exclude_dir=`pwd | sed "s,^$srcdir,,g"`
	if ! grep -q "^$exclude_dir" "$git_dir/info/exclude"; then
		echo "$exclude_dir" >> "$git_dir/info/exclude"
	fi
fi

echo \
"--------------------------------------------------------------------------------
build configured:

AS_HELP_STRING([i3lock version:], [`echo ${I3LOCK_VERSION} | sed 's,\\\\,,g'`])
AS_HELP_STRING([is release version:], [${is_release}])

AS_HELP_STRING([enable debug flags:], [${ax_enable_debug}])
AS_HELP_STRING([code coverage:], [${CODE_COVERAGE_ENABLED}])
AS_HELP_STRING([enabled sanitizers:], [${ax_enabled_sanitizers}])"

#To compile, run:
#
#  cd `pwd` && make
echo \
"--------------------------------------------------------------------------------"

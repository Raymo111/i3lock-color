# SYNOPSIS
#
#   AX_CHECK_ZSH_COMPLETION()
#
# DESCRIPTION
#
#   Checks for the presence of an --with-zsh-completion-dir option to set a
#   custom path to zsh completions. If no specified, it will use the default
#   path as long as zsh is available.
#

#serial 1

AC_DEFUN([AX_CHECK_ZSH_COMPLETION], [
  AC_ARG_WITH([zsh-completion-dir],
      AS_HELP_STRING([--with-zsh-completion-dir[=PATH]],
          [Install the zsh auto-completion script in this directory. @<:@default=yes@:>@]),
      [],
      [with_zsh_completion_dir=yes])

# Check zsh-completion
  if test "x$with_zsh_completion_dir" = "xyes"; then
      AC_ARG_VAR([ZSH_AVAILABLE], [])
      AC_CHECK_PROG([ZSH_AVAILABLE], [zsh], [yes], [no])

      AS_IF([test "$ZSH_AVAILABLE" = "yes"], [
          ZSH_COMPLETION_DIR="$datadir/zsh/vendor-completions"
          ], [
          ZSH_COMPLETION_DIR=""
          ])
  else
      ZSH_COMPLETION_DIR="$with_zsh_completion_dir"
  fi
  AC_SUBST([ZSH_COMPLETION_DIR])
  AM_CONDITIONAL([ENABLE_ZSH_COMPLETION], [test "x$with_zsh_completion_dir" != "xno" && test "$ZSH_COMPLETION_DIR" != ""])
])


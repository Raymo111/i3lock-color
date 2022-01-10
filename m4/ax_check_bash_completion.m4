# SYNOPSIS
#
#   AX_CHECK_BASH_COMPLETION()
#
# DESCRIPTION
#
#   Checks for the presence of an --with-bash-completion-dir option to set a
#   custom path to bash completions. If no specified, it will use the default
#   path as long as bash-completion is available.
#

#serial 1

AC_DEFUN([AX_CHECK_BASH_COMPLETION], [
  AC_ARG_WITH([bash-completion-dir],
      AS_HELP_STRING([--with-bash-completion-dir[=PATH]],
          [Install the bash auto-completion script in this directory. @<:@default=yes@:>@]),
      [],
      [with_bash_completion_dir=yes])

# Check bash-completion
  if test "x$with_bash_completion_dir" = "xyes"; then
      PKG_CHECK_MODULES([BASH_COMPLETION], [bash-completion >= 2.0], [
          BASH_COMPLETION_DIR="$datadir/bash-completion/completions"
          ], [
          BASH_COMPLETION_DIR=""
          ])
  else
      BASH_COMPLETION_DIR="$with_bash_completion_dir"
  fi
  AC_SUBST([BASH_COMPLETION_DIR])
  AM_CONDITIONAL([ENABLE_BASH_COMPLETION], [test "x$with_bash_completion_dir" != "xno" && test "$BASH_COMPLETION_DIR" != ""])
])


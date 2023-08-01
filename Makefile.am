@CODE_COVERAGE_RULES@

echo-version:
	@echo "@I3LOCK_VERSION@"

bin_PROGRAMS = i3lock

dist_man1_MANS = i3lock.1

DISTCLEANFILES = i3lock-color-*.tar.bz2

pamddir = $(sysconfdir)/pam.d
pamd_files = pam/i3lock
pamd_DATA = $(pamd_files)

if ENABLE_BASH_COMPLETION
bashcompletiondir = $(BASH_COMPLETION_DIR)
dist_bashcompletion_DATA = i3lock-bash
endif

if ENABLE_ZSH_COMPLETION
zshcompletiondir = $(ZSH_COMPLETION_DIR)
dist_zshcompletion_DATA = i3lock-zsh
endif

install-data-hook:
if ENABLE_BASH_COMPLETION
	test -f "${BASH_COMPLETION_DIR}/i3lock-bash" && mv -f "${BASH_COMPLETION_DIR}/i3lock-bash" "${BASH_COMPLETION_DIR}/i3lock" || true
endif
if ENABLE_ZSH_COMPLETION
	test -f "${ZSH_COMPLETION_DIR}/i3lock-zsh" && mv -f "${ZSH_COMPLETION_DIR}/i3lock-zsh" "${ZSH_COMPLETION_DIR}/_i3lock" || true
endif

uninstall-hook:
if ENABLE_BASH_COMPLETION
	rm -f ${BASH_COMPLETION_DIR}/i3lock
endif
if ENABLE_ZSH_COMPLETION
	rm -f ${ZSH_COMPLETION_DIR}/_i3lock
endif


AM_CPPFLAGS = \
	@AX_EXTEND_SRCDIR_CPPFLAGS@

i3lock_CFLAGS = \
	$(AM_CFLAGS) \
	$(XCB_CFLAGS) \
	$(XCB_IMAGE_CFLAGS) \
	$(XCB_UTIL_CFLAGS) \
	$(XCB_UTIL_XRM_CFLAGS) \
	$(XKBCOMMON_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(FONTCONFIG_CFLAGS) \
	$(JPEG_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

i3lock_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(CODE_COVERAGE_CPPFLAGS)

i3lock_LDADD = \
	$(XCB_LIBS) \
	$(XCB_IMAGE_LIBS) \
	$(XCB_UTIL_LIBS) \
	$(XCB_UTIL_XRM_LIBS) \
	$(XKBCOMMON_LIBS) \
	$(CAIRO_LIBS) \
	$(JPEG_LIBS) \
	$(FONTCONFIG_LIBS) \
	$(CODE_COVERAGE_LDFLAGS)

i3lock_SOURCES = \
	blur.c \
	blur.h \
	blur_simd.c \
	cursors.h \
	dpi.c \
	dpi.h \
	fonts.h \
	jpg.c \
	jpg.h \
	i3lock.c \
	i3lock.h \
	randr.c \
	randr.h \
	rgba.h \
	tinyexpr.c \
	tinyexpr.h \
	unlock_indicator.c \
	unlock_indicator.h \
	xcb.c \
	xcb.h

EXTRA_DIST = \
	$(pamd_files) \
	CHANGELOG \
	LICENSE \
	README.md \
	I3LOCK_VERSION

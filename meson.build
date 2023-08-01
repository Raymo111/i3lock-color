# -*- mode: meson -*-

# Style objective: be consistent with what mesonbuild.com documents/uses, and/or
# the meson book: https://meson-manual.com/

project(
  'i3lock',
  'c',
  version: '2.13',
  default_options: [
    'c_std=c11',
    'warning_level=1',  # enable all warnings (-Wall)
    # TODO(https://github.com/i3/i3/issues/4087): switch to
    # 'buildtype=debugoptimized',
  ],
  # Ubuntu 18.04 (supported until 2023) has meson 0.45.
  # We can revisit our minimum supported meson version
  # if it turns out to be too hard to maintain.
  meson_version: '>=0.45.0',
)

cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments(['-Wunused-value']), language: 'c')

if meson.version().version_compare('>=0.48.0')
  # https://github.com/mesonbuild/meson/issues/2166#issuecomment-629696911
  meson.add_dist_script('meson/meson-dist-script')
else
  message('meson <0.48.0 detected, dist tarballs will not be filtered')
endif

################################################################################
# Version handling
################################################################################

cdata = configuration_data()

version_array = meson.project_version().split('.')
cdata.set_quoted('I3LOCK_VERSION', '@VCS_TAG@')
cdata.set_quoted('SYSCONFDIR', join_paths(get_option('prefix'), get_option('sysconfdir')))

if get_option('b_sanitize').split(',').contains('address')
  cdata.set('I3LOCK_ASAN_ENABLED', 1)
endif

cdata.set('HAVE_STRNDUP', cc.has_function('strndup'))
cdata.set('HAVE_MKDIRP', cc.has_function('mkdirp'))

# Instead of generating config.h directly, make vcs_tag generate it so that
# @VCS_TAG@ is replaced.
config_h_in = configure_file(
  output: 'config.h.in',
  configuration: cdata,
)
config_h = declare_dependency(
  sources: vcs_tag(
    input: config_h_in,
    output: 'config.h',
    fallback: meson.project_version() + '-non-git',
  )
)

################################################################################
# manpages
################################################################################

install_man('i3lock.1')

# Required for e.g. struct ucred to be defined as per unix(7).
add_project_arguments('-D_GNU_SOURCE', language: 'c')


# https://mesonbuild.com/howtox.html#add-math-library-lm-portably
m_dep = cc.find_library('m', required: false)
rt_dep = cc.find_library('rt', required: false)

xcb_dep = dependency('xcb', method: 'pkg-config')
xcb_xkb_dep = dependency('xcb-xkb', method: 'pkg-config')
xcb_xinerama_dep = dependency('xcb-xinerama', method: 'pkg-config')
xcb_randr_dep = dependency('xcb-randr', method: 'pkg-config')
xcb_image_dep = dependency('xcb-image', method: 'pkg-config')
xcb_util_dep = dependency('xcb-util', method: 'pkg-config')
xcb_util_xrm_dep = dependency('xcb-xrm', method: 'pkg-config')
xcb_composite = dependency('xcb-composite', method: 'pkg-config')
xkbcommon_dep = dependency('xkbcommon', method: 'pkg-config')
xkbcommon_x11_dep = dependency('xkbcommon-x11', method: 'pkg-config')
cairo_dep = dependency('cairo', version: '>=1.14.4', method: 'pkg-config')
fontconfig_dep = dependency('fontconfig', method: 'pkg-config')
libjpeg_dep = dependency('libjpeg', method: 'pkg-config')

i3lock_srcs = [
  'dpi.c',
  'i3lock.c',
  'randr.c',
  'unlock_indicator.c',
  'xcb.c',
  'jpg.c',
  'tinyexpr.c',
  'blur.c',
  'blur_simd.c'
]

ev_dep = cc.find_library('ev')

thread_dep = dependency('threads')

i3lock_deps = [
  thread_dep,
  m_dep,
  rt_dep,
  ev_dep,
  config_h,
  cairo_dep,
  xcb_dep,
  xcb_xkb_dep,
  xcb_xinerama_dep,
  xcb_randr_dep,
  xcb_image_dep,
  xcb_util_dep,
  xcb_util_xrm_dep,
  xcb_composite,
  xkbcommon_dep,
  xkbcommon_x11_dep,
  fontconfig_dep,
  libjpeg_dep
]

host_os = host_machine.system()
if host_os != 'openbsd'
  pam_dep = cc.find_library('pam', required: true)
  i3lock_deps += [pam_dep]
endif

inc = include_directories('include')

executable(
  'i3lock',
  i3lock_srcs,
  install: true,
  include_directories: inc,
  dependencies: i3lock_deps,
)

install_subdir(
  'pam',
  strip_directory: true,
  install_dir: join_paths(get_option('sysconfdir'), 'pam.d'),
)

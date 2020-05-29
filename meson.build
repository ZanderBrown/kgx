project('kgx', 'c', version: '0.2.1',
  meson_version: '>= 0.49.0',
  default_options: [
    'werror=true',
    'buildtype=debugoptimized',
    'c_std=gnu11',
    'cpp_std=gnu++17',
  ],
)

i18n = import('i18n')

prefix = get_option('prefix')
libdir = prefix / get_option('libdir')
datadir = prefix / get_option('datadir')
pkglibdir = prefix / libdir / meson.project_name()
pkgdatadir = datadir / meson.project_name()
pkggirdir =  pkgdatadir / 'gir-1.0'
pkgtypelibdir = pkglibdir / 'girepository-1.0'

conf = configuration_data()
conf.set_quoted('PACKAGE_VERSION', '@VCS_TAG@')
conf.set_quoted('GETTEXT_PACKAGE', 'kgx')
conf.set_quoted('LOCALEDIR', prefix / get_option('localedir'))
conf.set_quoted('RES_PATH', '/org/gnome/zbrown/KingsCross/')
conf.set('BINDIR', prefix / get_option('bindir'))
if get_option('generic')
  conf.set('IS_GENERIC', 1)
else
  conf.set('IS_GENERIC', 0)
endif
config_h_in = configure_file(
  output: 'kgx-config.h.in',
  configuration: conf,
)
config_h = vcs_tag(input: config_h_in, output: 'kgx-config.h')

gtk_ver = 'GDK_VERSION_3_24'
glib_ver = 'GLIB_VERSION_2_58'
add_project_arguments([
  '-I' + meson.build_root(),
  '-DGDK_VERSION_MIN_REQUIRED=@0@'.format(gtk_ver),
  '-DGDK_VERSION_MAX_ALLOWED=@0@'.format(gtk_ver),
  '-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_ver),
  '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_ver),
], language: 'c')

cc = meson.get_compiler('c')

global_c_args = []
test_c_args = [
  '-Wdeclaration-after-statement',
  ['-Werror=format-security', '-Werror=format=2'],
  '-Werror=missing-declarations',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Winit-self',
  '-Wmaybe-uninitialized',
  '-Wno-strict-aliasing',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Wshadow',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wunused-function',
]
if get_option('buildtype') != 'plain'
  test_c_args += '-fstack-protector-strong'
endif

foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    global_c_args += arg
  endif
endforeach
add_project_arguments(
  global_c_args,
  language: 'c'
)

gi_runner = configure_file(
  input: 'gi.in',
  output: 'gi',
  configuration: {
    'BUILD_DIR': join_paths(meson.current_build_dir(), 'src')
  }
)

subdir('data')
subdir('src')
subdir('po')
subdir('doc')
if get_option('tests')
  if not get_option('gir')
    error('Tests require GIR')
  endif
  subdir('tests')
endif

meson.add_install_script('build-aux/meson/postinstall.py')

project('gnome-console', 'c', version: '42.beta',
  meson_version: '>= 0.59.0',
  default_options: [
    'werror=true',
    'buildtype=debugoptimized',
    'c_std=c17',
    'vte:c_std=gnu11',
    'vte:cpp_std=gnu++17',
    'vte:werror=false',
    'vte:gir=false',
    'vte:vapi=false',
    'vte:gtk3=false',
    'vte:gtk4=true',
    'adwaita:werror=false',
    'adwaita:vapi=false',
    'adwaita:introspection=disabled',
    'adwaita:tests=false',
    'libsass:werror=false',
  ],
)

i18n = import('i18n')
gnome = import('gnome')


if get_option('devel')
  app_id = 'org.gnome.Console.Devel'
  app_path = '/org/gnome/Console/Devel/'
  bin_name = 'kgx-devel'
  log_domain = 'KgxDevel'
  package_version = meson.project_version() + '-@VCS_TAG@'
else
  app_id = 'org.gnome.Console'
  app_path = '/org/gnome/Console/'
  bin_name = 'kgx'
  log_domain = 'Kgx'
  package_version = meson.project_version()
endif

prefix = get_option('prefix')
bindir = prefix / get_option('bindir')
libdir = prefix / get_option('libdir')
datadir = prefix / get_option('datadir')
pkglibdir = prefix / libdir / bin_name
pkgdatadir = datadir / bin_name

conf = configuration_data()
conf.set_quoted('PACKAGE_VERSION', package_version)
conf.set_quoted('GETTEXT_PACKAGE', bin_name)
conf.set_quoted('KGX_APPLICATION_ID', app_id)
conf.set('KGX_APPLICATION_ID_RAW', app_id)
conf.set_quoted('KGX_APPLICATION_PATH', app_path)
conf.set('KGX_APPLICATION_PATH_RAW', app_path)
conf.set_quoted('LOCALEDIR', prefix / get_option('localedir'))
conf.set('BIN_DIR', prefix / bindir)
conf.set('BIN_NAME', bin_name)
conf.set('IS_DEVEL', get_option('devel'))

config_h_in = configure_file(
  output: 'kgx-config.h.in',
  configuration: conf,
)
config_h = vcs_tag( input: config_h_in,
                   output: 'kgx-config.h',
                  command: ['git', 'rev-parse', '--short', 'HEAD'],
                 fallback: 'Devel')

gtk_ver = 'GDK_VERSION_4_0'
glib_ver = 'GLIB_VERSION_2_66'
add_project_arguments([
  '-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_ver),
  '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_ver),
  '-DG_LOG_USE_STRUCTURED',
  '-DG_LOG_DOMAIN="@0@"'.format(log_domain),
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

subdir('data')
subdir('src')
subdir('po')

gnome.post_install(
     glib_compile_schemas: true,
    gtk_update_icon_cache: true,
  update_desktop_database: true,
)

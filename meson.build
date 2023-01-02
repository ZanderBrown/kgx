project('gnome-console', 'c',
                  version: '44.alpha',
            meson_version: '>= 0.59.0',
                  license: 'GPL-3.0-or-later',
          default_options: [
                             'buildtype=debugoptimized',
                             'c_std=c17',
                             # So, apparently these two don’t currently do
                             # anything — Yet somehow vte works anyway?
                             # Mysterious
                             # https://github.com/mesonbuild/meson/pull/7521
                             'vte:c_std=gnu11',
                             'vte:cpp_std=gnu++17',
                             'vte:werror=false',
                             'vte:gir=false',
                             'vte:vapi=false',
                             'vte:gtk3=false',
                             'vte:gtk4=true',
                             'adwaita:vapi=false',
                             'adwaita:introspection=disabled',
                             'adwaita:tests=false',
                             # libsass may be pulled in via adwaita
                             'libsass:werror=false',
                             'gtk4:media-gstreamer=disabled',
                             'gtk4:print-cups=disabled',
                             'gtk4:introspection=disabled',
                             'gtk4:demos=false',
                             'gtk4:build-examples=false',
                             'gtk4:build-tests=false',
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

gtk_ver = 'GDK_VERSION_4_8'
glib_ver = 'GLIB_VERSION_2_74'

kgx_cargs = [
  '-DG_LOG_DOMAIN="@0@"'.format(log_domain),
  '-DG_LOG_USE_STRUCTURED',
  '-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_ver),
  '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_ver),
  '-DGDK_VERSION_MIN_REQUIRED=@0@'.format(gtk_ver),
  '-DGDK_VERSION_MAX_ALLOWED=@0@'.format(gtk_ver)
]

cc = meson.get_compiler('c')

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

foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    kgx_cargs += arg
  endif
endforeach

global_cargs = []

if get_option('buildtype') != 'plain'
  foreach arg: ['-fstack-protector-strong', '-fno-omit-frame-pointer']
    if cc.has_multi_arguments(arg)
      global_cargs += arg
    endif
  endforeach
endif

add_project_arguments(global_cargs, language: 'c')

adw_dep = dependency('libadwaita-1', version: '>= 1.2.beta')
gtk_dep = dependency('gtk4', version: '>= 4.8')
vte_dep = dependency('vte-2.91-gtk4', version: '>= 0.70.2')
gio_dep = dependency('gio-2.0', version: '>= 2.74')
gio_unix_dep = dependency('gio-unix-2.0', version: '>= 2.74')
gtop_dep = dependency('libgtop-2.0')
pcre_dep = dependency('libpcre2-8', version: '>= 10.32')
schemas_dep = dependency('gsettings-desktop-schemas')

subdir('data')
subdir('src')
subdir('po')

gnome.post_install(
     glib_compile_schemas: true,
    gtk_update_icon_cache: true,
  update_desktop_database: true,
)

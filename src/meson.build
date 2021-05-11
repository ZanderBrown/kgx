subdir('styles')

kgx_sources = [
  'kgx-application.c',
  'kgx-application.h',
  'fp-vte-util.c',
  'fp-vte-util.h',
  'kgx-terminal.c',
  'kgx-terminal.h',
  'kgx-tab.c',
  'kgx-tab.h',
  'kgx-simple-tab.c',
  'kgx-simple-tab.h',
  'kgx-pages.c',
  'kgx-pages.h',
  'kgx-close-dialog.c',
  'kgx-close-dialog.h',
  'kgx-window.c',
  'kgx-window.h',
  'kgx-process.c',
  'kgx-process.h',
]

kgx_deps = [
  dependency('gio-2.0', version: '>= 2.62'),
  dependency('gtk+-3.0', version: '>= 3.24'),
  dependency('libhandy-1', version: '>= 1.1', fallback: ['handy', 'libhandy_dep']),
  dependency('vte-2.91', version: '>= 0.65', fallback: ['vte', 'libvte_gtk3_dep']),
  dependency('libgtop-2.0'),
  cc.find_library('m', required: false),
]

kgx_sources += config_h

gnome = import('gnome')

resources_file = configure_file(
                       input: 'kgx.gresource.xml.in',
                      output: 'kgx.gresource.xml',
               configuration: {
                                'COLOUR_ICON': icon_filename,
                                'SYMBOLIC_ICON': icon_symbolic_filename,
                              }
)

kgx_sources += gnome.compile_resources('kgx-resources',
  resources_file,
  c_name: 'kgx',
  dependencies: style_deps
)

kgx_sources += gnome.gdbus_codegen('xdg-term1',
           sources: 'org.freedesktop.Terminal1.xml',
  interface_prefix: 'org.freedesktop.',
         namespace: 'Xdg',
)

kgx_sources += gnome.mkenums_simple('kgx-enums',
                                    sources: [
                                      'kgx-close-dialog.h',
                                      'kgx-terminal.h',
                                      'kgx-window.h',
                                      'kgx-tab.h'
                                    ])

kgx_lib = library ('kgx', kgx_sources,
                   dependencies: kgx_deps,
                   install: true,
                   install_dir: pkglibdir,
                   install_rpath: pkglibdir + ':' + libdir,
                  )
kgx_inc = include_directories('.')

kgx_dep = declare_dependency (sources: [config_h],
                              include_directories: kgx_inc,
                              link_with: kgx_lib,
                              dependencies: kgx_deps)

if get_option('gir')
  kgx_gir = gnome.generate_gir(
      kgx_lib,
      sources: kgx_sources,
      header: 'kgx.h',
      nsversion: '1.0',
      namespace: 'Kgx',
      symbol_prefix: 'kgx',
      identifier_prefix: 'Kgx',
      includes: [
        'GObject-2.0',
        'Gio-2.0',
        'Atk-1.0',
        'Gdk-3.0',
        'Gtk-3.0',
        'Pango-1.0',
        'Vte-2.91',
      ],
      dependencies: [kgx_dep],
      install: false,
      install_dir_gir: pkggirdir,
      install_dir_typelib: pkgtypelibdir,
      build_by_default: true,
      # This is bad but we aren't generating a public GIR
      extra_args: '--accept-unprefixed',
  )
  if get_option('vapi')
    kgx_vala_dep = gnome.generate_vapi('Kgx-1.0',
                                       sources: kgx_gir[0],
                                       packages: [
                                          'gio-2.0',
                                          'atk',
                                          'gtk+-3.0',
                                          'pango',
                                          'vte-2.91',
                                       ],
                                       install: false,
    )
  endif
else
  if get_option('vapi')
    error('Can\'t generate VAPI without GIR')
  endif
endif

executable('kgx', 'main.c',
  dependencies: kgx_dep,
  install: true,
  install_rpath: pkglibdir,
  gui_app: true,
)


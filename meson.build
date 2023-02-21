project('lumina', 'c')
cc = meson.get_compiler('c')

lumina_dep = [dependency('mpc'), dependency('libedit'), cc.find_library('m', required : true)]

add_global_arguments('-lm', language : 'c')
executable('main', 'parser.c', dependencies : lumina_dep)
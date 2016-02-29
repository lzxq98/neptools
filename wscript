# -*- mode: python -*-

# the following two variables are used by the target "waf dist"
VERSION='0.2.4'
APPNAME='stcm-editor'

# these variables are mandatory ('/' are converted automatically)
top = '.'
out = 'build'

try:
    with open('wscript_user.py', 'r') as f:
        exec(f.read())
except IOError:
    pass

def options(opt):
    opt.load('compiler_c compiler_cxx boost')
    opt.add_option('--msvc-hack', action='store_true', default=False,
                   help='Read COMPILE.md...')
    opt.add_option('--release', action='store_true', default=False,
                   help='Enable some flags for release builds')

def configure(cfg):
    if cfg.options.msvc_hack:
        cfg.env.DEST_OS = 'win32'
        cfg.find_program('link', var='LINK_CXX')
        cfg.find_program('lib', var='AR')
        cfg.load('msvc', funs='no_autodetect')

        from waflib.Tools.compiler_cxx import cxx_compiler
        from waflib import Utils
        cxx_compiler[Utils.unversioned_sys_platform()] = ['msvc']
        from waflib.Tools.compiler_c import c_compiler
        c_compiler[Utils.unversioned_sys_platform()] = ['msvc']

    cfg.load('compiler_c compiler_cxx boost clang_compilation_database')

    if cfg.env['COMPILER_CXX'] == 'msvc':
        if cfg.env.DEST_OS == 'win32' and not \
           cfg.check_cxx(fragment='int main() { return _M_IX86; }',
                         msg='Checking for i386', features='cxx',
                         mandatory=False):
            cfg.env.DEST_OS = 'winwhatever'


        cfg.env.append_value('CXXFLAGS', [
            '/EHsc', '/MD', '/Zc:rvalueCast', '/Zc:strictStrings', '/Zc:inline'])

        if cfg.options.release:
            cfg.env.prepend_value('CFLAGS', ['/O1', '/GS-', '/Gs9999999'])
            cfg.env.prepend_value('CXXFLAGS', [
                '/O2', '/Gv', '/GL', '/Gw', '/Gy'])
            cfg.env.prepend_value('LINKFLAGS', ['/LTCG', '/OPT:REF', '/OPT:ICF'])
            cfg.env.prepend_value('ARFLAGS', ['/LTCG'])
        cfg.check_boost()
    else:
        cfg.check_cxx(cxxflags='-std=c++14')
        cfg.env.append_value('CXXFLAGS', ['-std=c++14'])
        cfg.env.append_value('CXXFLAGS', cfg.filter_flags([
            '-fcolor-diagnostics', '-Wall', '-Wextra', '-pedantic',
            '-Wno-parentheses']))
        cfg.check_boost(lib='filesystem system')

        if cfg.options.release:
            opt=['-Ofast', '-flto', '-fno-fat-lto-objects',
                 '-fomit-frame-pointer']
            cfg.check_cxx(cxxflags=opt)

            cfg.env.prepend_value('CFLAGS', ['-Os', '-fomit-frame-pointer'])
            cfg.env.prepend_value('CXXFLAGS', opt)
            cfg.env.prepend_value('LINKFLAGS', opt + ['-Wl,-O1'])

    if cfg.options.release:
        cfg.define('NDEBUG', 1)
    if cfg.env.DEST_OS[0:3] == 'win':
        cfg.define('WINDOWS', 1)
        cfg.define('UNICODE', 1)
        cfg.define('_UNICODE', 1)

        cfg.check_cxx(lib='kernel32')
        cfg.check_cxx(lib='shell32')
        cfg.check_cxx(lib='user32')

def build(bld):
    src = [
        'src/dumpable.cpp',
        'src/source.cpp',
        'src/utils.cpp',
        'src/format/context.cpp',
        'src/format/gbnl.cpp',
        'src/format/item.cpp',
        'src/format/raw_item.cpp',
        'src/format/cl3.cpp',
        'src/format/stcm/collection_link.cpp',
        'src/format/stcm/data.cpp',
        'src/format/stcm/exports.cpp',
        'src/format/stcm/file.cpp',
        'src/format/stcm/gbnl.cpp',
        'src/format/stcm/header.cpp',
        'src/format/stcm/instruction.cpp',
    ]

    bld.stlib(source = src,
              uselib = 'BOOST',
              target = 'common')

    bld.program(source = 'src/programs/stcm-editor.cpp',
                uselib = 'BOOST',
                use = 'common',
                target = APPNAME)

    if bld.env['COMPILER_CXX'] == 'msvc' and bld.env.DEST_OS == 'win32':
        # technically launcher can be compiled for 64bits, but it makes no sense
        ld = ['/LTCG:OFF', '/FIXED', '/NXCOMPAT:NO', '/IGNORE:4254']
        bld.program(source = 'src/programs/launcher.c',
                    target = 'launcher',
                    uselib = 'KERNEL32 SHELL32 USER32',
                    linkflags = ld)

        # server.dll has no implib (doesn't export anything)
        from waflib.TaskGen import taskgen_method
        @taskgen_method
        def apply_implib(self):
            pass

        src_inject = [
            'src/injected/cpk.cpp',
            'src/injected/hook.cpp',
            'src/programs/server.cpp',
        ]
        bld.shlib(source = src_inject,
                  target = 'server',
                  uselib = 'BOOST USER32')

from waflib.Configure import conf
@conf
def filter_flags(cfg, flags):
    ret = []

    for flag in flags:
        try:
            cfg.check_cxx(cxxflags=flag)
            ret.append(flag)
        except:
            pass

    return ret

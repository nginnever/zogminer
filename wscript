import Options, os

srcdir = '.'
blddir = 'build-cc'
VERSION = '0.0.1'


def set_options(opt):
  opt.tool_options('compiler_cxx')

def configure(conf):
  conf.check_tool('compiler_cxx')
  conf.check_tool('node_addon')

  conf.check_cfg(atleast_pkgconfig_version='0.0.0', mandatory=True, errmsg='Couldn\'t find pkg-config tool')

  if conf.check_cfg(package='openssl',
                    args='--cflags --libs',
                    uselib_store='OPENSSL'):
    Options.options.use_openssl = conf.env["USE_OPENSSL"] = True
    conf.env.append_value("CPPFLAGS", "-DHAVE_OPENSSL=1")
  else:
    libssl = conf.check_cc(lib=['ssl', 'crypto'],
                           header_name='openssl/ssl.h',
                           function_name='SSL_library_init',
                           libpath=['/usr/lib', '/usr/local/lib', '/opt/local/lib', '/usr/sfw/lib'],
                           uselib_store='OPENSSL')
    libcrypto = conf.check_cc(lib='crypto',
                              header_name='openssl/crypto.h',
                              uselib_store='OPENSSL')
    if libcrypto and libssl:
      conf.env["USE_OPENSSL"] = Options.options.use_openssl = True
      conf.env.append_value("CPPFLAGS", "-DHAVE_OPENSSL=1")
    else:
      conf.fatal("Couldn't find OpenSSL!")

def build_post(bld):
  module_path = bld.path.find_resource('native.node').abspath(bld.env)
  os.system('cp %r native.node' % module_path)

def build(bld):
  obj = bld.new_task_gen('cxx', 'shlib', 'node_addon')
  obj.target = 'native'
  obj.source = 'src/main.cc src/eckey.cc'
bld.add_post_fun(build_post)

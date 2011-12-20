import Options, Utils
from os import unlink, symlink, chdir, environ
from os.path import exists

#srcdir = "."
#blddir = "build"
#VERSION = "0.2.2"

def set_options(opt):
  opt.tool_options("compiler_cxx")

def configure(conf):
  conf.check_tool("compiler_cxx")
  conf.check_tool("node_addon")

  # Enables all the warnings that are easy to avoid
  conf.env.append_unique('CXXFLAGS', ["-Wall"])
  conf.env.append_unique('CXXFLAGS', ['-Isrc/'])
  conf.env.append_unique('CXXFLAGS', ['-g'])
  conf.env.append_unique('CXXFLAGS', ['-D_FILE_OFFSET_BITS=64'])
  conf.env.append_unique('CXXFLAGS', ['-D_LARGEFILE_SOURCE'])

  oci_include = environ.get("OCI_INCLUDE_DIR", "/usr/include/oracle/11.2/client")
  if oci_include:
      conf.env.append_unique('CXXFLAGS', [ '-I' + oci_include ])

  oci_lib = environ.get("OCI_LIB_DIR", "/usr/lib/oracle/11.2/client/lib")
  if oci_lib:
      conf.env.append_unique('LINKFLAGS', [ '-L' + oci_lib ])

  conf.env.append_unique('LINKFLAGS', ['-locci', '-lclntsh', '-lnnz11'])
  conf.check(header_name="occi.h", errmsg="Missing include files for OCI", mandatory=True)
  conf.check_cxx(lib="occi", errmsg="Missing libocci", mandatory=True)

def build(bld):
  obj = bld.new_task_gen("cxx", "shlib", "node_addon")
  obj.target = "oracle_bindings"
  obj.source = "src/connection.cpp src/outParam.cpp src/oracle_bindings.cpp"
  obj.includes = "src/"

{
  "targets": [
    {
      "target_name": "oracle_bindings",
      "sources": [ "src/connection.cpp", 
                   "src/oracle_bindings.cpp", 
                   "src/executeBaton.cpp",
                   "src/outParam.cpp" ],
      "conditions": [
        ["OS=='mac'", {
          "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES"
          },
          "variables": {
			 "oci_include_dir%": "<!(if [ -z $OCI_INCLUDE_DIR ]; then echo \"/opt/instantclient/sdk/include/\"; else echo $OCI_INCLUDE_DIR; fi)",
			 "oci_lib_dir%": "<!(if [ -z $OCI_LIB_DIR ]; then echo \"/opt/instantclient/\"; else echo $OCI_LIB_DIR; fi)",
          },
          "libraries": [ "-locci", "-lclntsh", "-lnnz11" ],
          "link_settings": {"libraries": [ '-L<(oci_lib_dir)'] }
        }],
        ["OS=='linux'", {
          "variables": {
			 "oci_include_dir%": "<!(if [ -z $OCI_INCLUDE_DIR ]; then echo \"/opt/instantclient/sdk/include/\"; else echo $OCI_INCLUDE_DIR; fi)",
			 "oci_lib_dir%": "<!(if [ -z $OCI_LIB_DIR ]; then echo \"/opt/instantclient/\"; else echo $OCI_LIB_DIR; fi)",
          },
          "libraries": [ "-locci", "-lclntsh", "-lnnz12" ],
          "link_settings": {"libraries": [ '-L<(oci_lib_dir)'] }
        }],
        ["OS=='win'", {
	  "configurations": {
          "Release": {
            "msvs_settings": {
              "VCCLCompilerTool": {
                "RuntimeLibrary": "2"
              }
            },
	  },   
          "Debug": {
            "msvs_settings": {
              "VCCLCompilerTool": {
              "RuntimeLibrary": "3"
              }
            }, 
	  }
	  },
          "variables": {
            "oci_include_dir%": "<!(IF DEFINED OCI_INCLUDE_DIR (echo %OCI_INCLUDE_DIR%) ELSE (echo C:\oracle\instantclient\sdk\include))",
            "oci_lib_dir%": "<!(IF DEFINED OCI_LIB_DIR (echo %OCI_LIB_DIR%) ELSE (echo C:\oracle\instantclient\sdk\lib\msvc))",
         },
         # "libraries": [ "-loci" ],
         "link_settings": {"libraries": [ '<(oci_lib_dir)\oraocci12.lib'] }
        }]
      ],
      "include_dirs": [ "<(oci_include_dir)" ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ]
    }
  ]
}



{
  "targets": [
    {
      "target_name": "oracle_bindings",
      "variables": {
         "oci_include_dir%": "<!(echo $OCI_INCLUDE_DIR)",
         "oci_lib_dir%": "<!(echo $OCI_LIB_DIR)"
      },
      "sources": [ "src/connection.cpp", 
                   "src/oracle_bindings.cpp", 
                   "src/executeBaton.cpp",
                   "src/outParam.cpp" ],
      "include_dirs": [ "<(oci_include_dir)" ],
      "libraries": [ "-locci", "-lclntsh", "-lnnz11" ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "link_settings": {"libraries": [ "-L<(oci_lib_dir)"] },
      "conditions": [
        ["OS=='mac'", {
          "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES"
          }
        }]
      ]
    }
  ]
}

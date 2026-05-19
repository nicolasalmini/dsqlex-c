{
  "targets": [
    {
      "target_name": "dsqlex_napi",
      "sources": ["src/dsqlex_napi.cpp"],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "<!@(node -p \"require('path').resolve(__dirname, '../../include')\")",
        "<!@(pkg-config --cflags-only-I libmpdec libmpdec++ | sed 's/-I//g')"
      ],
      "libraries": [
        "-L<!@(node -p \"require('path').resolve(__dirname, '../../build')\")",
        "-ldsqlex",
        "-Wl,-rpath,<!@(node -p \"require('path').resolve(__dirname, '../../build')\")"
      ],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "cflags_cc": ["-std=c++17"],
      "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"],
      "conditions": [
        ["OS=='mac'", {
          "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
            "CLANG_CXX_LIBRARY": "libc++",
            "MACOSX_DEPLOYMENT_TARGET": "10.15",
            "OTHER_CPLUSPLUSFLAGS": ["-std=c++17"],
            "OTHER_LDFLAGS": [
              "-Wl,-rpath,@loader_path/../../../../build"
            ]
          }
        }]
      ]
    }
  ]
}

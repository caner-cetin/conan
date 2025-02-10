#!/usr/bin/env just --justfile

cmake := '/opt/homebrew/bin/cmake'
exe := 'Conan'

run: 
  build/Conan

build config="release":
    #!/usr/bin/env sh
    if [ "{{config}}" = "debug" ]; then
        {{cmake}} --build {{justfile_directory()}}/build --config Debug --target all
        just debugger
    else
        {{cmake}} --build {{justfile_directory()}}/build --config Release --target all
        just run
    fi

debugger:
  lldb -- {{justfile_directory()}}/build/{{exe}}

configure:
  {{cmake}} -S {{justfile_directory()}} -B {{justfile_directory()}}/build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang

clean:
  rm -rf {{justfile_directory()}}/build 
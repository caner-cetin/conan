build: 
  /opt/homebrew/bin/cmake --build /Users/canercetin/Git/conan/build --config Debug --target all
run: 
  build/Conan
debug:
  lldb build/Conan
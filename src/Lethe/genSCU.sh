find Core -name '*.cpp' | awk '{print "#include \"" $0 "\""}' > Lethe_SCU_Core.cpp
find Script -name '*.cpp' | awk '{print "#include \"" $0 "\""}' > Lethe_SCU_Script.cpp

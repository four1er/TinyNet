macro(target name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} tinynet)
endmacro()

target(echotest echotest.cpp)

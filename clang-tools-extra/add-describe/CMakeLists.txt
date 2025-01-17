set(LLVM_LINK_COMPONENTS support)

add_clang_executable(add-describe
  AddDescribe.cpp
  )
target_link_libraries(add-describe
  PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangFrontend
  clangSerialization
  clangTooling
  )

# HACK: to provide dependet func to tint cmake file
function(common_compile_options TARGET)
  if (COMPILER_IS_LIKE_GNU)
    target_compile_options(${TARGET} PRIVATE
      -fno-exceptions
      -fno-rtti

      -Wno-deprecated-builtins
      -Wno-unknown-warning-option
    )

    if (${DAWN_ENABLE_MSAN})
      target_compile_options(${TARGET} PUBLIC -fsanitize=memory)
      target_link_options(${TARGET} PUBLIC -fsanitize=memory)
    elseif (${DAWN_ENABLE_ASAN})
      target_compile_options(${TARGET} PUBLIC -fsanitize=address)
      target_link_options(${TARGET} PUBLIC -fsanitize=address)
    elseif (${DAWN_ENABLE_TSAN})
      target_compile_options(${TARGET} PUBLIC -fsanitize=thread)
      target_link_options(${TARGET} PUBLIC -fsanitize=thread)
    elseif (${DAWN_ENABLE_UBSAN})
      target_compile_options(${TARGET} PUBLIC -fsanitize=undefined -fsanitize=float-divide-by-zero)
      target_link_options(${TARGET} PUBLIC -fsanitize=undefined -fsanitize=float-divide-by-zero)
    endif()
  endif(COMPILER_IS_LIKE_GNU)

  if(MSVC)
      target_compile_options(${TARGET} PUBLIC /utf-8)
  endif()

  if (DAWN_EMIT_COVERAGE)
    target_compile_definitions(${TARGET} PRIVATE "DAWN_EMIT_COVERAGE")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(${TARGET} PRIVATE "--coverage")
        target_link_options(${TARGET} PRIVATE "gcov")
    elseif(COMPILER_IS_CLANG OR COMPILER_IS_CLANG_CL)
        target_compile_options(${TARGET} PRIVATE "-fprofile-instr-generate" "-fcoverage-mapping")
        target_link_options(${TARGET} PRIVATE "-fprofile-instr-generate" "-fcoverage-mapping")
    else()
        message(FATAL_ERROR "Coverage generation not supported for the ${CMAKE_CXX_COMPILER_ID} toolchain")
    endif()
  endif(DAWN_EMIT_COVERAGE)
endfunction()


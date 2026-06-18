FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
  SRCS ${app_sources}
  INCLUDE_DIRS "." "generated"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-complain-wrong-lang)

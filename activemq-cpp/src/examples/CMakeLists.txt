# Build simple examples found under src/examples

file(GLOB_RECURSE EXAMPLE_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cxx
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cc
)

foreach(_src IN LISTS EXAMPLE_SRCS)
  # Compute a target name based on the path relative to examples dir
  file(RELATIVE_PATH _rel ${CMAKE_CURRENT_SOURCE_DIR} ${_src})
  get_filename_component(_name ${_rel} NAME_WE)
  get_filename_component(_dir ${_rel} PATH)
  if(_dir STREQUAL "")
    set(_target example_${_name})
  else()
    string(REPLACE "/" "_" _dirkey ${_dir})
    string(REPLACE "\\" "_" _dirkey ${_dirkey})
    set(_target example_${_dirkey}_${_name})
  endif()
  # Only create standalone example executables for files that contain a Main
  # entry point in their filename (e.g. AdvisoryConsumerMain.cpp). This
  # prevents attempting to link class-only sources that do not define main().
  string(FIND "${_name}" "Main" _hasMain)
  if(NOT _hasMain EQUAL -1)
    # Collect all source files in the same examples directory so helper
    # implementation files (e.g. AdvisoryConsumer.cpp) are linked into the
    # executable that contains the Main entry point.
    if(_dir STREQUAL "")
      set(_src_dir ${CMAKE_CURRENT_SOURCE_DIR})
    else()
      set(_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/${_dir})
    endif()
    # Gather sources in this example directory, but only include the
    # specific Main file and any helper sources that do NOT themselves
    # contain a Main (to avoid multiple main() definitions in one
    # executable when a directory contains multiple sample mains).
    if(_dir STREQUAL "")
      set(_src_dir ${CMAKE_CURRENT_SOURCE_DIR})
    else()
      set(_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/${_dir})
    endif()

    file(GLOB _dir_sources_full
      ${_src_dir}/*.cpp
      ${_src_dir}/*.cxx
      ${_src_dir}/*.cc
    )

    set(_exe_sources)
    foreach(_candidate IN LISTS _dir_sources_full)
      file(RELATIVE_PATH _rel_candidate ${CMAKE_CURRENT_SOURCE_DIR} ${_candidate})
      get_filename_component(_cand_name ${_rel_candidate} NAME_WE)
      # Include the Main file that triggered this target
      if(_cand_name STREQUAL ${_name})
        list(APPEND _exe_sources ${_candidate})
      else()
        # Include helper sources only if their name does NOT contain "Main"
        string(FIND "${_cand_name}" "Main" _cand_hasMain)
        if(_cand_hasMain EQUAL -1)
          list(APPEND _exe_sources ${_candidate})
        endif()
      endif()
    endforeach()

    add_executable(${_target} ${_exe_sources})
    target_include_directories(${_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../main)
    target_link_libraries(${_target} PRIVATE activemq_cms activemq_core decaf)
    
    # Set output directory for examples
    set_target_properties(${_target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples
    )
  endif()
endforeach()

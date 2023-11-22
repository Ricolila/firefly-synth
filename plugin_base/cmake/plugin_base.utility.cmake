function(declare_local_target LIBRARY_TYPE PROJECT_NAME SOURCE_DIR)
  set("${PROJECT_NAME}_SRC_DIR" "${CMAKE_CURRENT_SOURCE_DIR}/src/${SOURCE_DIR}")
  file(GLOB_RECURSE "${PROJECT_NAME}_SRC" "${${PROJECT_NAME}_SRC_DIR}/*.*")
  add_library(${PROJECT_NAME} ${LIBRARY_TYPE} "${${PROJECT_NAME}_SRC}")
  target_include_directories(${PROJECT_NAME} PRIVATE "${${PROJECT_NAME}_SRC_DIR}")
  target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/lib/JUCE/modules)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    file(GLOB_RECURSE grouping_source "${${PROJECT_NAME}_SRC_DIR}/*.*")
    source_group(TREE ${${PROJECT_NAME}_SRC_DIR} FILES ${grouping_source})
  endif()
endfunction()

function(declare_vst3_target WRAPPER_NAME WRAPPER_SRC PLUGIN_NAME IS_FX)
  set(TARGET_NAME "${WRAPPER_NAME}_${PLUGIN_VERSION}.vst3")
  set(public_sdk_SOURCE_DIR "${CMAKE_SOURCE_DIR}/lib/vst3/public.sdk")
  declare_local_target(SHARED ${TARGET_NAME} ${WRAPPER_SRC})
  smtg_target_add_library_main(${TARGET_NAME})
  set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "${WRAPPER_NAME}_${PLUGIN_VERSION}")
  target_compile_definitions(${TARGET_NAME} PRIVATE "INF_IS_FX=${IS_FX}")
  target_include_directories(${TARGET_NAME} SYSTEM PRIVATE lib/vst3)
  target_include_directories(${TARGET_NAME} PRIVATE "src/${PLUGIN_NAME}" "${CMAKE_SOURCE_DIR}/plugin_base/src/plugin_base" "${CMAKE_SOURCE_DIR}/plugin_base/src/plugin_base.vst3")
  target_link_libraries(${TARGET_NAME} ${PLUGIN_NAME} plugin_base.vst3 plugin_base plugin_base.juce pluginterfaces sdk sdk_common)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set_target_properties(${TARGET_NAME} PROPERTIES SUFFIX ".vst3")
    set(BUILD_TYPE "$<$<CONFIG:Debug>:Debug>$<$<CONFIG:Release>:Release>")
    set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist/${BUILD_TYPE}/win/${TARGET_NAME}/Contents/x86_64-win")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources "${CMAKE_SOURCE_DIR}/dist/${BUILD_TYPE}/win/${TARGET_NAME}/Contents/Resources/ui")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/presets "${CMAKE_SOURCE_DIR}/dist/${BUILD_TYPE}/win/${TARGET_NAME}/Contents/Resources/presets")
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "")
    set_target_properties(${TARGET_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist/${CMAKE_BUILD_TYPE}/linux/${TARGET_NAME}/Contents/x86_64-linux")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources "${CMAKE_SOURCE_DIR}/dist/${BUILD_TYPE}/linux/${TARGET_NAME}/Contents/Resources/ui")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/presets "${CMAKE_SOURCE_DIR}/dist/${BUILD_TYPE}/linux/${TARGET_NAME}/Contents/Resources/presets")
  else()
    message(FATAL_ERROR)
  endif()
endfunction()

function(declare_clap_target WRAPPER_NAME WRAPPER_SRC PLUGIN_NAME IS_FX)
  set(TARGET_NAME "${WRAPPER_NAME}_${PLUGIN_VERSION}.clap")
  declare_local_target(SHARED ${TARGET_NAME} ${WRAPPER_SRC})
  set_target_properties(${TARGET_NAME} PROPERTIES SUFFIX ".clap")
  set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "${WRAPPER_NAME}_${PLUGIN_VERSION}")
  target_compile_definitions(${TARGET_NAME} PRIVATE "INF_IS_FX=${IS_FX}")
  target_include_directories(${TARGET_NAME} SYSTEM PRIVATE lib/clap/include lib/clap-helpers/include)
  target_include_directories(${TARGET_NAME} PRIVATE "src/${PLUGIN_NAME}" "${CMAKE_SOURCE_DIR}/plugin_base/src/plugin_base" "${CMAKE_SOURCE_DIR}/plugin_base/src/plugin_base.clap" "${CMAKE_SOURCE_DIR}/lib/readerwriterqueue")
  target_link_libraries(${TARGET_NAME} ${PLUGIN_NAME} plugin_base.clap plugin_base plugin_base.juce)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(BUILD_TYPE "$<$<CONFIG:Debug>:Debug>$<$<CONFIG:Release>:Release>")
    set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist/${BUILD_TYPE}/win/${TARGET_NAME}")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources "${CMAKE_SOURCE_DIR}/dist/${BUILD_TYPE}/win/${TARGET_NAME}/ui")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/presets "${CMAKE_SOURCE_DIR}/dist/${BUILD_TYPE}/win/${TARGET_NAME}/presets")
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "")
    set_target_properties(${TARGET_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist/${CMAKE_BUILD_TYPE}/linux/${TARGET_NAME}")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources "${CMAKE_SOURCE_DIR}/dist/${BUILD_TYPE}/linux/${TARGET_NAME}/ui")
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/presets "${CMAKE_SOURCE_DIR}/dist/${BUILD_TYPE}/linux/${TARGET_NAME}/presets")
  else()
    message(FATAL_ERROR)
  endif()
endfunction()
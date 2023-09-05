function(declare_local_target LIBRARY_TYPE PROJECT_NAME SOURCE_DIR)
  set("${PROJECT_NAME}_SRC_DIR" "src/${SOURCE_DIR}")
  file(GLOB_RECURSE "${PROJECT_NAME}_SRC" "${${PROJECT_NAME}_SRC_DIR}/*.*")
  add_library(${PROJECT_NAME} ${LIBRARY_TYPE} "${${PROJECT_NAME}_SRC}")
  target_include_directories(${PROJECT_NAME} PRIVATE "${${PROJECT_NAME}_SRC_DIR}")
  source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/${${PROJECT_NAME}_SRC_DIR}" FILES "${${PROJECT_NAME}_SRC}")
endfunction()

function(declare_vst3_target VST3_NAME VST3_SOURCE PLUG_NAME IS_FX)
  set(TARGET_NAME "${VST3_NAME}.${INFERNAL_VERSION}.vst3")
  declare_local_target(SHARED ${TARGET_NAME} ${VST3_SOURCE})
  target_compile_definitions(${TARGET_NAME} PRIVATE "INF_IS_FX=${IS_FX}")
  target_include_directories(${TARGET_NAME} SYSTEM PRIVATE lib/vst3)
  target_include_directories(${TARGET_NAME} PRIVATE "src/${PLUG_NAME}" "${CMAKE_SOURCE_DIR}/base/src/infernal.base" "${CMAKE_SOURCE_DIR}/base/src/infernal.base.vst3")
  target_link_libraries(${TARGET_NAME} ${PLUG_NAME} infernal.base infernal.base.vst3 pluginterfaces sdk sdk_common)
  if (CMAKE_HOST_WIN32)
    set_target_properties(${TARGET_NAME} PROPERTIES SUFFIX "")
    set(BUILD_TYPE "$<$<CONFIG:Debug>:Debug>$<$<CONFIG:Release>:Release>")
    set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist/${BUILD_TYPE}/win/${TARGET_NAME}/Contents/x86_64-win")
  endif()
  if(CMAKE_HOST_LINUX)
    set_target_properties(${TARGET_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist/${CMAKE_BUILD_TYPE}/linux/${TARGET_NAME}/Contents/x86_64-linux")
  endif()
endfunction()
# The changes of OpenGothic in Tempest (an upstream submodule) are kept as a patch: patches/tempest-opengothic.patch.
# A clean clone gets them here, before Tempest is built. A tree that has them already (the patch can be reversed) is left alone.
find_package(Git QUIET)
set(OG_TEMPEST_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/lib/Tempest")
set(OG_TEMPEST_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/patches/tempest-opengothic.patch")

# CMake runs again when the patch changes (a git pull with a new version of it), so the new one is put in
if(EXISTS "${OG_TEMPEST_PATCH}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${OG_TEMPEST_PATCH}")
endif()

if(GIT_FOUND AND EXISTS "${OG_TEMPEST_PATCH}" AND EXISTS "${OG_TEMPEST_DIR}/Engine/CMakeLists.txt")
  # The submodule is a repository of its own (.git is a file or a folder): the paths of the patch start there.
  # Without it (a copy of the sources) git would take the repository around it, then the directory is given.
  if(EXISTS "${OG_TEMPEST_DIR}/.git")
    set(OG_TEMPEST_APPLY_DIR  "${OG_TEMPEST_DIR}")
    set(OG_TEMPEST_APPLY_ARGS apply --ignore-whitespace)
  else()
    set(OG_TEMPEST_APPLY_DIR  "${CMAKE_CURRENT_SOURCE_DIR}")
    set(OG_TEMPEST_APPLY_ARGS apply --ignore-whitespace --directory=lib/Tempest)
  endif()

  execute_process(
    COMMAND ${GIT_EXECUTABLE} ${OG_TEMPEST_APPLY_ARGS} --check --reverse "${OG_TEMPEST_PATCH}"
    WORKING_DIRECTORY "${OG_TEMPEST_APPLY_DIR}"
    RESULT_VARIABLE OG_TEMPEST_PATCHED
    OUTPUT_QUIET ERROR_QUIET)
  if(NOT OG_TEMPEST_PATCHED EQUAL 0)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} ${OG_TEMPEST_APPLY_ARGS} "${OG_TEMPEST_PATCH}"
      WORKING_DIRECTORY "${OG_TEMPEST_APPLY_DIR}"
      RESULT_VARIABLE OG_TEMPEST_APPLIED
      ERROR_VARIABLE  OG_TEMPEST_APPLY_ERROR)
    # an older version of the patch is in the submodule (the patch has changed since): the submodule goes back to its
    # commit and the new patch is put in. Only for a submodule that is a repository of its own, the files of it are not edited by hand.
    if(NOT OG_TEMPEST_APPLIED EQUAL 0 AND EXISTS "${OG_TEMPEST_DIR}/.git")
      execute_process(
        COMMAND ${GIT_EXECUTABLE} checkout -- .
        WORKING_DIRECTORY "${OG_TEMPEST_DIR}"
        RESULT_VARIABLE OG_TEMPEST_RESET
        OUTPUT_QUIET ERROR_QUIET)
      if(OG_TEMPEST_RESET EQUAL 0)
        execute_process(
          COMMAND ${GIT_EXECUTABLE} ${OG_TEMPEST_APPLY_ARGS} "${OG_TEMPEST_PATCH}"
          WORKING_DIRECTORY "${OG_TEMPEST_APPLY_DIR}"
          RESULT_VARIABLE OG_TEMPEST_APPLIED
          ERROR_VARIABLE  OG_TEMPEST_APPLY_ERROR)
        if(OG_TEMPEST_APPLIED EQUAL 0)
          message(STATUS "Tempest: lib/Tempest was reset, the new patches/tempest-opengothic.patch applied")
        endif()
      endif()
    endif()
    if(OG_TEMPEST_APPLIED EQUAL 0)
      message(STATUS "Tempest: patches/tempest-opengothic.patch applied")
    else()
      message(FATAL_ERROR "Tempest: patches/tempest-opengothic.patch does not apply to lib/Tempest:\n${OG_TEMPEST_APPLY_ERROR}\n"
                          "Use the commit of Tempest that the submodule points to, see patches/README.md")
    endif()
  endif()
endif()

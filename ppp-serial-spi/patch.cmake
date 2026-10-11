find_package(Git REQUIRED)

execute_process(
    COMMAND ${GIT_EXECUTABLE} apply --reverse --check ${PATCH}
    WORKING_DIRECTORY ${REPO}
    RESULT_VARIABLE already_applied
    OUTPUT_QUIET ERROR_QUIET
)

if(already_applied EQUAL 0)
    message(STATUS "Patch already applied: ${PATCH}")
    return()
endif()

execute_process(
    COMMAND ${GIT_EXECUTABLE} apply ${PATCH}
    WORKING_DIRECTORY ${REPO}
    RESULT_VARIABLE rc
    ERROR_VARIABLE  err
)

if(NOT rc EQUAL 0)
    message(FATAL_ERROR "Failed to apply ${PATCH}:\n${err}")
endif()

message(STATUS "Patch applied: ${PATCH}")

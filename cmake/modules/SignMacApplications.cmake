# This script re-signs the app after CPack packages them. This is necessary because CPack modifies
# the library references to App relative paths, invalidating the code signature.

# Obviously, we only need to run this on Apple targets.
if (APPLE)
    set(app_name "ecapplog")
    set(FULL_APP_PATH "${CPACK_TEMPORARY_INSTALL_DIRECTORY}/ALL_IN_ONE/${app_name}.app")
    message(STATUS "Re-signing ${app_name}.app")
    execute_process(COMMAND "codesign" "--force" "--deep" "-s" "-" "${FULL_APP_PATH}")
endif (APPLE)

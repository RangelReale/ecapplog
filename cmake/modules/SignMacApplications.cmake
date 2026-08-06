# This script re-signs the app after CPack stages it. This is necessary because the install step
# rewrites the library references to App relative paths, invalidating the code signature.
#
# It must re-sign with the real "Developer ID Application" identity: an ad-hoc signature ("-s -")
# carries no Team ID and no chain of trust, so Gatekeeper rejects the result on every machine
# other than the one that built it, and the notary service refuses the submission outright.

# Obviously, we only need to run this on Apple targets.
if (APPLE)
    if(NOT CPACK_MAC_SIGN_IDENTITY)
        message(FATAL_ERROR "CPACK_MAC_SIGN_IDENTITY is not set, refusing to ship an unsigned app")
    endif()

    # The bundle name depends on the generator ("ecapplog.app" with Makefiles/Ninja,
    # "ECAppLog.app" with Xcode via XCODE_ATTRIBUTE_PRODUCT_NAME), and the staging layout
    # depends on whether components are in use. Discover it instead of hardcoding a path.
    # Deliberately not GLOB_RECURSE: that would also descend into the bundle itself.
    file(GLOB _apps LIST_DIRECTORIES true
        "${CPACK_TEMPORARY_INSTALL_DIRECTORY}/*.app"
        "${CPACK_TEMPORARY_INSTALL_DIRECTORY}/*/*.app")
    list(FILTER _apps INCLUDE REGEX "\\.app$")
    if(NOT _apps)
        message(FATAL_ERROR "No .app found under ${CPACK_TEMPORARY_INSTALL_DIRECTORY}")
    endif()
    list(GET _apps 0 FULL_APP_PATH)

    function(_ecapplog_sign path)
        execute_process(
            COMMAND codesign --force --options runtime --timestamp
                    --sign "${CPACK_MAC_SIGN_IDENTITY}" "${path}"
            RESULT_VARIABLE _rc
            ERROR_VARIABLE _err)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "codesign failed for ${path}: ${_err}")
        endif()
    endfunction()

    message(STATUS "Re-signing ${FULL_APP_PATH}")

    # Sign inside-out: nested code first, the bundle last. Apple discourages --deep, which does
    # not apply signing options correctly to nested code.
    file(GLOB_RECURSE _nested
        "${FULL_APP_PATH}/Contents/PlugIns/*.dylib"
        "${FULL_APP_PATH}/Contents/Frameworks/*.dylib")
    foreach(_n IN LISTS _nested)
        _ecapplog_sign("${_n}")
    endforeach()

    file(GLOB _frameworks LIST_DIRECTORIES true "${FULL_APP_PATH}/Contents/Frameworks/*.framework")
    foreach(_f IN LISTS _frameworks)
        _ecapplog_sign("${_f}")
    endforeach()

    _ecapplog_sign("${FULL_APP_PATH}")

    execute_process(
        COMMAND codesign --verify --strict --verbose=2 "${FULL_APP_PATH}"
        RESULT_VARIABLE _rc
        ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "signature verification failed for ${FULL_APP_PATH}: ${_err}")
    endif()
endif (APPLE)

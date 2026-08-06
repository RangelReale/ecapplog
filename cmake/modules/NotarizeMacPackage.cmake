# This script runs after CPack has produced the DMG. A Developer ID signature alone is not
# enough for the package to open on another machine: since macOS 10.15 Gatekeeper also requires
# a notarization ticket. Without one, anything that arrives with a com.apple.quarantine attribute
# (downloaded, AirDropped, copied from a share) is blocked. A locally built artifact never gets
# that attribute, which is why an un-notarized build appears to work on the build machine.
#
# Credentials are never stored here. Create a reusable keychain profile once with:
#   xcrun notarytool store-credentials "ecapplog-notary" \
#       --apple-id <email> --team-id <TEAMID> --password <app-specific-password>

if(APPLE AND CPACK_MAC_NOTARIZE)
    if(NOT CPACK_MAC_SIGN_IDENTITY)
        message(FATAL_ERROR "CPACK_MAC_SIGN_IDENTITY is not set, cannot notarize")
    endif()
    if(NOT CPACK_MAC_NOTARY_PROFILE)
        message(FATAL_ERROR "CPACK_MAC_NOTARY_PROFILE is not set, cannot notarize")
    endif()

    set(_found_dmg FALSE)
    foreach(_pkg IN LISTS CPACK_PACKAGE_FILES)
        if(NOT _pkg MATCHES "\\.dmg$")
            continue()
        endif()
        set(_found_dmg TRUE)

        message(STATUS "Signing ${_pkg}")
        execute_process(
            COMMAND codesign --force --timestamp --sign "${CPACK_MAC_SIGN_IDENTITY}" "${_pkg}"
            RESULT_VARIABLE _rc
            ERROR_VARIABLE _err)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "failed to sign ${_pkg}: ${_err}")
        endif()

        # --wait blocks until the notary service returns a verdict; this usually takes a few
        # minutes. On rejection, inspect the per-file reasons with:
        #   xcrun notarytool log <submission-id> --keychain-profile "${CPACK_MAC_NOTARY_PROFILE}"
        message(STATUS "Submitting ${_pkg} to the notary service (this can take several minutes)")
        execute_process(
            COMMAND xcrun notarytool submit "${_pkg}"
                    --keychain-profile "${CPACK_MAC_NOTARY_PROFILE}" --wait
            RESULT_VARIABLE _rc)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "notarization failed for ${_pkg}")
        endif()

        # Staple the ticket into the DMG so it validates without a network round trip.
        message(STATUS "Stapling ${_pkg}")
        execute_process(
            COMMAND xcrun stapler staple "${_pkg}"
            RESULT_VARIABLE _rc
            ERROR_VARIABLE _err)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "stapling failed for ${_pkg}: ${_err}")
        endif()

        execute_process(
            COMMAND spctl -a -vvv -t install "${_pkg}"
            RESULT_VARIABLE _rc
            ERROR_VARIABLE _err)
        if(NOT _rc EQUAL 0)
            message(FATAL_ERROR "Gatekeeper rejected ${_pkg}: ${_err}")
        endif()
        message(STATUS "Notarized and stapled: ${_pkg}")
    endforeach()

    if(NOT _found_dmg)
        message(FATAL_ERROR "ECAPPLOG_NOTARIZE is ON but no .dmg was found in CPACK_PACKAGE_FILES")
    endif()
endif()

# Removes Qt plugins that macdeployqt bundles but that this application never uses and cannot
# deploy correctly, then re-signs the bundle.
#
# The webp image format plugin depends on libwebp, which in turn links @rpath/libsharpyuv.
# macdeployqt cannot resolve that transitive dependency (its -libpath option does not feed into
# rpath resolution), so it copies libwebp in while leaving a dangling reference. The plugin then
# fails to load on any machine that does not already have libsharpyuv installed. ECAppLog only
# ever renders its own PNG/ICNS resources, whose handlers are built into QtGui, so the whole webp
# chain is dead weight.
#
# Run via: cmake -DBUNDLE=<path.app> -DSIGN_IDENTITY=<ident> -P PruneMacBundle.cmake

if(NOT BUNDLE)
    message(FATAL_ERROR "BUNDLE must be set")
endif()
if(NOT SIGN_IDENTITY)
    message(FATAL_ERROR "SIGN_IDENTITY must be set")
endif()

# Globbed rather than named: the version suffixes (libwebp.7, libwebpmux.3, ...) change between
# upstream releases.
file(GLOB _prune
    "${BUNDLE}/Contents/PlugIns/imageformats/libqwebp.dylib"
    "${BUNDLE}/Contents/Frameworks/libwebp*.dylib"
    "${BUNDLE}/Contents/Frameworks/libsharpyuv*.dylib")

if(_prune)
    foreach(_p IN LISTS _prune)
        message(STATUS "Pruning unused ${_p}")
        file(REMOVE "${_p}")
    endforeach()

    # Removing sealed resources invalidates the signature macdeployqt just applied, so re-seal
    # the bundle. Nested code is untouched and keeps its own valid signature.
    execute_process(
        COMMAND codesign --force --options runtime --timestamp
                --sign "${SIGN_IDENTITY}" "${BUNDLE}"
        RESULT_VARIABLE _rc
        ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "re-signing ${BUNDLE} after pruning failed: ${_err}")
    endif()
endif()

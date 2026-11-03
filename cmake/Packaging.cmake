include(GNUInstallDirs)

set(INSTRUMENT_INSTALL_COMPONENTS)

foreach(format IN LISTS INSTRUMENT_PLUGIN_FORMATS)
    string(TOLOWER "${format}" component)
    set(plugin_format_target "instrument_${format}")

    if(format STREQUAL "VST3")
        if(WIN32)
            set(plugin_destination "VST3")
        elseif(APPLE)
            set(plugin_destination "Library/Audio/Plug-Ins/VST3")
        else()
            set(plugin_destination "${CMAKE_INSTALL_LIBDIR}/vst3")
        endif()
    elseif(format STREQUAL "AU")
        set(plugin_destination "Library/Audio/Plug-Ins/Components")
    elseif(format STREQUAL "AAX")
        if(WIN32)
            set(plugin_destination "Avid/Audio/Plug-Ins")
        else()
            set(plugin_destination "Library/Application Support/Avid/Audio/Plug-Ins")
        endif()
    endif()

    if(NOT TARGET "${plugin_format_target}")
        message(FATAL_ERROR "JUCE did not create the expected ${format} target.")
    endif()

    install(TARGETS "${plugin_format_target}"
        BUNDLE DESTINATION "${plugin_destination}" COMPONENT "${component}"
        LIBRARY DESTINATION "${plugin_destination}" COMPONENT "${component}"
        RUNTIME DESTINATION "${plugin_destination}" COMPONENT "${component}"
    )
    list(APPEND INSTRUMENT_INSTALL_COMPONENTS "${component}")
endforeach()

set(CPACK_PACKAGE_NAME "Pete Beat Synth Guitar")
set(CPACK_PACKAGE_VENDOR "Petebeat Software")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Audio passthrough and MIDI transposition plugin")
set(CPACK_PACKAGE_CONTACT "Petebeat Software")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Pete Beat Synth Guitar")
set(CPACK_COMPONENTS_ALL ${INSTRUMENT_INSTALL_COMPONENTS})
set(CPACK_COMPONENTS_GROUPING IGNORE)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)

if(WIN32)
    set(INSTRUMENT_MAKENSIS_HINTS
        "$ENV{ProgramFiles}/NSIS"
        "$ENV{ProgramFiles\(x86\)}/NSIS"
        "C:/ProgramData/chocolatey/bin"
    )
    find_program(INSTRUMENT_MAKENSIS
        NAMES makensis makensis.exe
        HINTS ${INSTRUMENT_MAKENSIS_HINTS}
    )
    if(INSTRUMENT_MAKENSIS)
        set(CPACK_GENERATOR NSIS)
        set(CPACK_NSIS_EXECUTABLE "${INSTRUMENT_MAKENSIS}")
        set(CPACK_NSIS_PACKAGE_NAME "Pete Beat Synth Guitar")
        set(CPACK_NSIS_DISPLAY_NAME "Pete Beat Synth Guitar")
        set(CPACK_NSIS_COMPONENT_INSTALL ON)
        set(CPACK_NSIS_INSTALL_ROOT "$COMMONFILES64")
        set(CPACK_PACKAGE_INSTALL_DIRECTORY "VST3")
        set(CPACK_NSIS_MODIFY_PATH OFF)
        set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    else()
        message(STATUS "NSIS was not found; generating a component ZIP package instead.")
        set(CPACK_GENERATOR ZIP)
    endif()
elseif(APPLE)
    set(CPACK_GENERATOR productbuild)
    set(CPACK_PRODUCTBUILD_COMPONENT_INSTALL ON)
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")
else()
    set(CPACK_GENERATOR DEB)
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Petebeat Software")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
endif()

include(CPack)

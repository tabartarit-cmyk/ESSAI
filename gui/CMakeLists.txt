cmake_minimum_required(VERSION 3.20)

project(PicoASHAGui VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)

if(ANDROID)
    # Android : la liaison USB passe par l'API USB Host du système (voir android/ et seriallink.cpp)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
else()
    find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort)
endif()

set(ASHA_COMM_INCLUDE_PATH_REL "${PROJECT_SOURCE_DIR}/../include")
cmake_path(NATIVE_PATH ASHA_COMM_INCLUDE_PATH_REL NORMALIZE ASHA_COMM_INCLUDE_PATH)

set(ASHA_COMM_LIB_PATH_REL "${PROJECT_SOURCE_DIR}/../lib")
cmake_path(NATIVE_PATH ASHA_COMM_LIB_PATH_REL NORMALIZE ASHA_COMM_LIB_PATH)

qt_add_executable(PicoASHAGui
    main.cpp
    picoashacomm.h
    picoashacomm.cpp
    remotedevice.h
    remotedevice.cpp
    ../include/asha_comms.hpp
    ../include/bt_status_err.hpp
    ../lib/nanocobs/cobs.c
    picoashamainwindow.h 
    picoashamainwindow.cpp
    pairdialog.h 
    pairdialog.cpp
    i18n.h
    i18n.cpp
    seriallink.h
    seriallink.cpp
)

# Traductions de l'interface : un fichier JSON par langue (code ISO 639-1),
# embarqué dans l'exécutable. Ajouter une langue = ajouter un fichier dans translations/.
file(GLOB PICOASHA_I18N_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/translations/*.json")
qt_add_resources(PicoASHAGui "i18n"
    PREFIX "/i18n"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/translations"
    FILES ${PICOASHA_I18N_FILES}
)

# Icône globe du menu de langue : flags/auto.png
file(GLOB PICOASHA_FLAG_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/flags/*.png")
qt_add_resources(PicoASHAGui "flags"
    PREFIX "/flags"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/flags"
    FILES ${PICOASHA_FLAG_FILES}
)

target_include_directories(PicoASHAGui PUBLIC "${ASHA_COMM_INCLUDE_PATH}" "${ASHA_COMM_LIB_PATH}")

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(PicoASHAGui PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appPicoASHAGui
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
)

target_link_libraries(PicoASHAGui PRIVATE Qt6::Widgets)
if(NOT ANDROID)
    target_link_libraries(PicoASHAGui PRIVATE Qt6::SerialPort)
endif()

if(ANDROID)
    # Le dossier android/ contient le code Java (src/org/picoasha/UsbSerialBridge.java) ajouté à l'APK
    set_target_properties(PicoASHAGui PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE 1
    )
endif()

include(GNUInstallDirs)
install(TARGETS PicoASHAGui
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(UNIX AND NOT APPLE AND NOT ANDROID)
    # Linux : entrée de menu, icône et règle udev (voir PLATFORMS.md)
    install(FILES linux/pico-asha-gui.desktop DESTINATION share/applications)
    install(FILES linux/pico-asha-gui.png DESTINATION share/icons/hicolor/256x256/apps)
    install(FILES linux/99-pico-asha.rules DESTINATION share/pico-asha-gui)
endif()

if(WIN32)
    qt_generate_deploy_app_script(
        TARGET PicoASHAGui
        OUTPUT_SCRIPT deploy_script
        NO_COMPILER_RUNTIME
        NO_UNSUPPORTED_PLATFORM_ERROR
        DEPLOY_TOOL_OPTIONS
            "--no-system-d3d-compiler"
            "--no-opengl-sw"
            "--no-opengl"
            "--no-svg"
            "--no-network"
    )
    install(SCRIPT ${deploy_script})
endif()

set(CPACK_VERBATIM_VARIABLES TRUE)
set(CPACK_STRIP_FILES TRUE)
set(CPACK_PACKAGE_VENDOR "shermp")
set(CPACK_PACKAGE_DESCRIPTION "Simple GUI application for Pico-ASHA")
set(CPACK_GENERATOR ZIP)
include(CPack)

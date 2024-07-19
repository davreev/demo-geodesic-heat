if(TARGET stb::image)
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    stb-image
    URL https://raw.githubusercontent.com/nothings/stb/f75e8d1cad7d90d72ef7a4661f1b994ef78b4e31/stb_image.h
    DOWNLOAD_NO_EXTRACT TRUE
)

FetchContent_GetProperties(stb-image)
if(NOT ${stb-image_POPULATED})
    FetchContent_Populate(stb-image)
endif()

add_library(stb-image INTERFACE)
add_library(stb::image ALIAS stb-image)

target_include_directories(
    stb-image
    SYSTEM # Ignore warnings
    INTERFACE 
        "${stb-image_SOURCE_DIR}"
)

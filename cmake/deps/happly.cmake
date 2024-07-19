if(TARGET happly::happly)
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    happly
    URL https://raw.githubusercontent.com/nmwsharp/happly/8a606309daaa680eee495c8279feb0b704148f4a/happly.h
    DOWNLOAD_NO_EXTRACT TRUE
)

FetchContent_GetProperties(happly)
if(NOT ${happly_POPULATED})
    FetchContent_Populate(happly)
endif()

add_library(happly INTERFACE)
add_library(happly::happly ALIAS happly)

target_include_directories(
    happly
    SYSTEM # Ignore warnings
    INTERFACE
        "${happly_SOURCE_DIR}"
)

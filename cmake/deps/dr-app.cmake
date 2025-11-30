if(TARGET dr::app)
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    dr-app
    GIT_REPOSITORY https://github.com/davreev/dr-app.git
    GIT_TAG 01431c7266dcb825da46e8afed3e37b3d460d292
)

FetchContent_MakeAvailable(dr-app)

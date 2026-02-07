if(TARGET dr::app)
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    dr-app
    GIT_REPOSITORY https://github.com/davreev/dr-app.git
    GIT_TAG 6b99dd197163c36516848f42675e65bfd61ab87d
)

FetchContent_MakeAvailable(dr-app)

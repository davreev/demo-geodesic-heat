if(TARGET dr::app)
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    dr-app
    GIT_REPOSITORY https://github.com/davreev/dr-app.git
    GIT_TAG b80c3f22181956baf37bb089ae04f0fbfd0e651e
)

FetchContent_MakeAvailable(dr-app)

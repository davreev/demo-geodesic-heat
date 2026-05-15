if(TARGET dr::app)
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    dr-app
    GIT_REPOSITORY https://github.com/davreev/dr-app.git
    GIT_TAG af97b4898b965db664ec0db41b0ddac78feb20a4
)

FetchContent_MakeAvailable(dr-app)


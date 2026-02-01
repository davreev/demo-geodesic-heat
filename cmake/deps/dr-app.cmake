if(TARGET dr::app)
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    dr-app
    GIT_REPOSITORY https://github.com/davreev/dr-app.git
    GIT_TAG b21b17dfed30bed92e64c6deafb2a51ed6a7c1f1
)

FetchContent_MakeAvailable(dr-app)

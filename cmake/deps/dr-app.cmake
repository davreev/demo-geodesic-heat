if(TARGET dr::app)
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    dr-app
    GIT_REPOSITORY https://github.com/davreev/dr-app.git
    GIT_TAG 75d500d866dc46a5dd90549f2871eebb7c5a9b9f
)

FetchContent_MakeAvailable(dr-app)

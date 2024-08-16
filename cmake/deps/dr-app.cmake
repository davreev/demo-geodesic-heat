if(TARGET dr::app)
    return()
endif()

include(FetchContent)

# NOTE(dr): Pinned to specific revision until 0.3.0
#[[
FetchContent_Declare(
    dr-app
    URL https://github.com/davreev/dr-app/archive/refs/tags/0.2.0.zip
)
]]
FetchContent_Declare(
    dr-app
    GIT_REPOSITORY https://github.com/davreev/dr-app.git
    GIT_TAG 064a141a5ce5811ffef9a596b99bc092735bf1b3
)

FetchContent_MakeAvailable(dr-app)

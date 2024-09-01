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
    GIT_TAG 2c06c77d37402f1bddddee50da46462dd30f0976
)

FetchContent_MakeAvailable(dr-app)

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
    GIT_REPOSITORY git@github.com:davreev/dr-app.git
    GIT_TAG ea5bd70b3f45e36f00ac513f2fc5a5b98b8074f8
)

FetchContent_MakeAvailable(dr-app)

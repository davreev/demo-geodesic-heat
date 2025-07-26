if(TARGET dr::app)
    return()
endif()

include(FetchContent)

#[[
FetchContent_Declare(
    dr-app
    URL https://github.com/davreev/dr-app/archive/refs/tags/0.4.0.zip
)
]]

# Using latest for camera updates
FetchContent_Declare(
    dr-app
    GIT_REPOSITORY https://github.com/davreev/dr-app.git
    GIT_TAG 8ed0892bab551d3b5d5f88d56e089b23ca62ea62
)

FetchContent_MakeAvailable(dr-app)

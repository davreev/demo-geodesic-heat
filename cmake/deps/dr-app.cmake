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
    GIT_TAG cad2f20
)

FetchContent_MakeAvailable(dr-app)

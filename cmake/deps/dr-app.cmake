if(TARGET dr::app)
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    dr-app
    URL https://github.com/davreev/dr-app/archive/refs/tags/0.4.0.zip
)

FetchContent_MakeAvailable(dr-app)

# Every .cpp under src/ is the application; headers are included by their path from src/ (code-guide.md §2).
file(GLOB_RECURSE app_sources ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer
)

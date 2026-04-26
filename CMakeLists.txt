cmake_minimum_required(VERSION 3.15)
project(chemtui LANGUAGES CXX)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


FetchContent_Declare(
  chemfiles
  GIT_REPOSITORY https://github.com/chemfiles/chemfiles.git
  GIT_TAG 0.10.4
)
FetchContent_MakeAvailable(chemfiles)
find_package(spdlog CONFIG REQUIRED)
find_package(ftxui CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

add_executable(chemtui src/main.cpp)


target_link_libraries(chemtui PRIVATE
  spdlog::spdlog
  ftxui::screen
  ftxui::dom
  ftxui::component
  glm::glm
  chemfiles
)

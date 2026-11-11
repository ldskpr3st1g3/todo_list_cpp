cmake_minimum_required(VERSION 3.14)

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

project(petty_plusik CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
check_include_file_cxx(coroutine HAS_COROUTINE)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_CXX_EXTENSIONS OFF)


find_package(Drogon CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(PostgreSQL REQUIRED)  # <-- ДОБАВИТЬ ЭТУ СТРОКУ


pkg_check_modules(SODIUM REQUIRED libsodium)

include(FetchContent)
FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG        v0.7.0  
)
FetchContent_MakeAvailable(jwt-cpp)


add_executable(${PROJECT_NAME} main.cc CpuThreadPool.cc)

# CONFIGURE_DEPENDS заставляет CMake проверять появление новых файлов при каждой сборке
file(GLOB_RECURSE CTL_SRC CONFIGURE_DEPENDS "controllers/*.cc" "controllers/*.cpp")
file(GLOB_RECURSE FILTER_SRC CONFIGURE_DEPENDS "filters/*.cc" "filters/*.cpp")
file(GLOB_RECURSE PLUGIN_SRC CONFIGURE_DEPENDS "plugins/*.cc" "plugins/*.cpp")
file(GLOB_RECURSE MODEL_SRC CONFIGURE_DEPENDS "models/*.cc" "models/*.cpp")
file(GLOB_RECURSE SERVICE_SRC CONFIGURE_DEPENDS "services/*.cc" "services/*.cpp")
file(GLOB_RECURSE REPO_SRC CONFIGURE_DEPENDS "repositories/*.cc" "repositories/*.cpp")
file(GLOB_RECURSE DTO_SRC CONFIGURE_DEPENDS "dto/*.cc" "dto/*.cpp")

drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
                    ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(${PROJECT_NAME}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                   ${CMAKE_CURRENT_SOURCE_DIR}/models
                                   ${CMAKE_CURRENT_SOURCE_DIR}/services
                                   ${CMAKE_CURRENT_SOURCE_DIR}/dto
                                   ${CMAKE_CURRENT_SOURCE_DIR}/reposirories
                                   ${SODIUM_INCLUDE_DIRS}) 

target_sources(${PROJECT_NAME}
               PRIVATE
               ${CTL_SRC}
               ${FILTER_SRC}
               ${PLUGIN_SRC}
               ${MODEL_SRC}
               ${SERVICE_SRC}
               ${REPO_SRC}
               ${DTO_SRC})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Drogon::Drogon
    jwt-cpp::jwt-cpp
    OpenSSL::SSL
    OpenSSL::Crypto
    PostgreSQL::PostgreSQL
    ${SODIUM_LIBRARIES} 
)

# add_subdirectory(test)
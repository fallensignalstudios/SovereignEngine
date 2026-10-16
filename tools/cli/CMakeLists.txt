add_executable(sovereign-cli main.cpp)
sov_configure_target(sovereign-cli)
target_link_libraries(sovereign-cli PRIVATE sov::commands sov::world sov::core)

include(FetchContent)
set(HANDLECORE_WITH_PCAP ON)
if(NOT TARGET Lidar::HandleCore)
    FetchContent_Declare(lidar_handle_core
        GIT_REPOSITORY https://github.com/Lateef-ZHOU/HandleCore.git
        GIT_TAG 5afbfc6b2ee5b9c8a4e1138e8c2ab7a2d02b0e56)
    FetchContent_MakeAvailable(lidar_handle_core)
endif()
if(NOT TARGET Lidar::Pcap)
    message(FATAL_ERROR "Enable HANDLECORE_WITH_PCAP before the first HandleCore configuration")
endif()
set(TASKCORE_WITH_QML ON)
set(TASKCORE_WITH_PCAP_FORMS ON)
if(NOT TARGET Lidar::TaskUI)
    FetchContent_Declare(lidar_taskcore
        GIT_REPOSITORY https://github.com/Lateef-ZHOU/TaskCore.git
        GIT_TAG 840c67b5c25da0a21e083b953486962593621e8b)
    FetchContent_MakeAvailable(lidar_taskcore)
endif()
if(NOT TARGET Lidar::PcapForms)
    message(FATAL_ERROR "Enable TASKCORE_WITH_PCAP_FORMS before the first TaskCore configuration")
endif()

include(FetchContent)
set(HANDLECORE_WITH_PCAP ON)
if(NOT TARGET Lidar::HandleCore)
    FetchContent_Declare(lidar_handle_core
        GIT_REPOSITORY https://github.com/Lateef-ZHOU/HandleCore.git
        GIT_TAG 7ad9b380fe8d58cdb00a97107e06a96f6f4ba9b0)
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
        GIT_TAG d2555b638e80278b831540ef90efe922db38101f)
    FetchContent_MakeAvailable(lidar_taskcore)
endif()
if(NOT TARGET Lidar::PcapForms)
    message(FATAL_ERROR "Enable TASKCORE_WITH_PCAP_FORMS before the first TaskCore configuration")
endif()

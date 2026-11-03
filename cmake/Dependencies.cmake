include(FetchContent)
set(HANDLECORE_WITH_PCAP ON)
if(NOT TARGET Lidar::HandleCore)
    FetchContent_Declare(lidar_handle_core
        GIT_REPOSITORY https://github.com/Lateef-ZHOU/HandleCore.git
        GIT_TAG a7278031d226a96748a9646a5399b4f8be1ce9d7)
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
        GIT_TAG cdc46f67b84e37f77e954131ac56270e798b64a5)
    FetchContent_MakeAvailable(lidar_taskcore)
endif()
if(NOT TARGET Lidar::PcapForms)
    message(FATAL_ERROR "Enable TASKCORE_WITH_PCAP_FORMS before the first TaskCore configuration")
endif()

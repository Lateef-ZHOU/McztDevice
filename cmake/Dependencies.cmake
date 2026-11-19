include(FetchContent)
set(HANDLECORE_WITH_PCAP ON)
if(NOT TARGET Lidar::HandleCore)
    FetchContent_Declare(lidar_handle_core
        GIT_REPOSITORY https://github.com/Lateef-ZHOU/HandleCore.git
        GIT_TAG 73d5bec896aeba4b445163df279fc15a3157ca4b)
    FetchContent_MakeAvailable(lidar_handle_core)
endif()
if(NOT TARGET Lidar::Pcap)
    message(FATAL_ERROR "Enable HANDLECORE_WITH_PCAP before the first HandleCore configuration")
endif()
set(FAMILYCORE_WITH_QML ON)
set(FAMILYCORE_WITH_PCAP_FORMS ON)
if(NOT TARGET Lidar::TaskUI)
    FetchContent_Declare(lidar_familycore
        GIT_REPOSITORY https://github.com/Lateef-ZHOU/TaskCore.git
        GIT_TAG 5f9e9dc8600112f5b68270ec021af2a6239bd47a)
    FetchContent_MakeAvailable(lidar_familycore)
endif()
if(NOT TARGET Lidar::PcapForms)
    message(FATAL_ERROR "Enable FAMILYCORE_WITH_PCAP_FORMS before the first FamilyCore configuration")
endif()

set(HEADER_FILES
        )
set(SOURCE_FILES
        QTAtom.cpp
        QTAtom_dref.cpp
        QTAtom_elst.cpp
        QTAtom_hinf.cpp
        QTAtom_mdhd.cpp
        QTAtom_mvhd.cpp
        QTAtom_stco.cpp
        QTAtom_stsc.cpp
        QTAtom_stsd.cpp
        QTAtom_stss.cpp
        QTAtom_stsz.cpp
        QTAtom_stts.cpp
        QTAtom_tkhd.cpp
        QTAtom_tref.cpp
        QTFile.cpp
        QTFile_FileControlBlock.cpp
        QTHintTrack.cpp
        QTRTPFile.cpp
        QTTrack.cpp)

add_library(QTFile STATIC ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(QTFile
        PUBLIC include)
target_link_libraries(QTFile
        PUBLIC SafeStd)
target_compile_definitions(QTFile
        PRIVATE -DDSS_USE_API_CALLBACKS)

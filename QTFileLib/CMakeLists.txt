set(HEADER_FILES
        include/QTAtom.h
        include/QTFile.h
        include/QTFile_FileControlBlock.h
        QTAtom_dref.h
        QTAtom_elst.h
        QTAtom_hinf.h
        QTAtom_mdhd.h
        QTAtom_mvhd.h
        QTAtom_stco.h
        QTAtom_stsc.h
        QTAtom_stsd.h
        QTAtom_stsz.h
        QTAtom_stts.h
        QTAtom_tkhd.h
        QTAtom_tref.h
        QTHintTrack.h
        QTRTPFile.h
        QTTrack.h)

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

add_library(QTFile STATIC
        ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(QTFile
        PUBLIC include)
target_link_libraries(QTFile
        PUBLIC APIStub
        PUBLIC StreamingBase)
target_compile_definitions(QTFile
        PRIVATE -DDSS_USE_API_CALLBACKS)

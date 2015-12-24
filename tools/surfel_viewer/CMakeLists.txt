project(surfel_viewer)

find_package(OpenGL)

find_package(PCL 1.7 REQUIRED)
include_directories(${PCL_INCLUDE_DIRS})
link_directories(${PCL_LIBRARY_DIRS})
add_definitions(${PCL_DEFINITIONS})

if(QT_FOUND AND OPENGL_FOUND)
	include_directories(${AQSIS_OPENEXR_INCLUDE_DIR} "${AQSIS_OPENEXR_INCLUDE_DIR}/OpenEXR" ${QT_INCLUDES}
						${OPENGL_INCLUDE_DIR})

	include_subproject(pointrender)

	qt4_wrap_cpp(moc_srcs
		surfel_viewer.h
		${aqsis_all_SOURCE_DIR}/include/aqsis/util/interactivecamera.h
	)
	set(srcs
		${pointrender_srcs}
		${moc_srcs}
		surfel_viewer.cpp
	)

	if(WIN32 AND NOT MINGW)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
	endif()

	set(surfel_viewer_link_libraries ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY}
			${QT_QTOPENGL_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY}
			${OPENGL_gl_LIBRARY} ${pointrender_libs} aqsis_util ${PCL_LIBRARIES})

	if(MINGW)
		list(APPEND surfel_viewer_link_libraries pthread)
	endif()
	if(WIN32)
		list(APPEND surfel_viewer_link_libraries ${QT_QTMAIN_LIBRARY})
	endif()

	aqsis_add_executable(surfel_viewer ${srcs} GUIAPP
		LINK_LIBRARIES  ${surfel_viewer_link_libraries})

	aqsis_install_targets(surfel_viewer)
endif()

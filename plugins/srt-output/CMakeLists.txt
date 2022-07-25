project(srt-output)

set(srt-output_HEADERS
	srt/access_control.h
	srt/logging_api.h
	srt/platform_sys.h
	srt/srt.h
	srt/udt.h
	srt/version.h
	srt/win/syslog_defs.h
	srt/win/unistd.h
	srt-helpers.h
	amf.h
	bytes.h
	flv-mux.h)

set(srt-output_SOURCES
	srt-plugins.c
	srt-output.c
	amf.c
	flv-mux.c)

add_library(srt-output MODULE
	${srt-output_HEADERS}
	${srt-output_SOURCES})
target_link_libraries(srt-output
	libobs
	ws2_32
	w32-pthreads
	crypt32
	"D:/vcpkg/installed/x64-windows-static/lib/libcrypto.lib"
	"D:/vcpkg/installed/x64-windows-static/lib/libssl.lib"
	"D:/vcpkg/installed/x64-windows-static/lib/srt_static.lib")
set_target_properties(srt-output PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(srt-output data)

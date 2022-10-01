project(obs-filters)

find_package(Libspeexdsp REQUIRED)

find_package(Librnnoise QUIET)

add_library(obs-rnnoise INTERFACE)
add_library(Librnnoise::Librnnoise ALIAS obs-rnnoise)

set(_RNNOISE_SOURCES
	rnnoise/src/arch.h
	rnnoise/src/celt_lpc.c
	rnnoise/src/celt_lpc.h
	rnnoise/src/common.h
	rnnoise/src/denoise.c
	rnnoise/src/kiss_fft.c
	rnnoise/src/kiss_fft.h
	rnnoise/src/opus_types.h
	rnnoise/src/pitch.c
	rnnoise/src/pitch.h
	rnnoise/src/rnn_data.c
	rnnoise/src/rnn_data.h
	rnnoise/src/rnn_reader.c
	rnnoise/src/rnn.c
	rnnoise/src/rnn.h
	rnnoise/src/tansig_table.h
	rnnoise/src/_kiss_fft_guts.h
	rnnoise/include/rnnoise.h)

target_sources(obs-rnnoise INTERFACE ${_RNNOISE_SOURCES})

target_include_directories(
	obs-rnnoise INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/rnnoise/include")

target_compile_definitions(obs-rnnoise INTERFACE COMPILE_OPUS)

source_group("rnnoise" FILES ${_RNNOISE_SOURCES})

set(obs-filters_LIBSPEEXDSP_SOURCES noise-suppress-filter.c)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/obs-filters-config.h.in"
	"${CMAKE_BINARY_DIR}/plugins/obs-filters/config/obs-filters-config.h")

set(obs-filters_config_HEADERS
	"${CMAKE_BINARY_DIR}/plugins/obs-filters/config/obs-filters-config.h")
include_directories(${LIBSPEEXDSP_INCLUDE_DIRS}
	"${CMAKE_BINARY_DIR}/plugins/obs-filters/config")

if(MSVC)
	set(obs-filters_PLATFORM_DEPS
		w32-pthreads)
endif()

set(obs-filters_SOURCES
	obs-filters.c
	color-correction-filter.c
	async-delay-filter.c
	gpu-delay.c
	crop-filter.c
	scale-filter.c
	scroll-filter.c
	chroma-key-filter.c
	color-key-filter.c
	color-grade-filter.c
	sharpness-filter.c
	gain-filter.c
	noise-gate-filter.c
	mask-filter.c
	invert-audio-polarity.c
	compressor-filter.c
	limiter-filter.c
	expander-filter.c
	luma-key-filter.c
	obs-shader-filter.c
	)

add_library(obs-filters MODULE
	${obs-filters_SOURCES}
	${obs-filters_config_HEADERS}
	${obs-filters_LIBSPEEXDSP_SOURCES})

target_compile_definitions(obs-filters PRIVATE LIBRNNOISE_ENABLED LIBSPEEXDSP_ENABLED)

target_link_libraries(obs-filters PRIVATE libobs)
target_link_libraries(obs-filters PRIVATE ${obs-filters_PLATFORM_DEPS})
target_link_libraries(obs-filters PRIVATE ${obs-filters_LIBSPEEXDSP_LIBRARIES})
target_link_libraries(obs-filters PRIVATE ${LIBSPEEXDSP_LIBRARIES})
target_link_libraries(obs-filters PRIVATE Librnnoise::Librnnoise)

install_obs_plugin_with_data(obs-filters data)

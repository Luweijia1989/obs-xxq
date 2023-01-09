project(obs-shaderfilter)

set(obs-shaderfilter_SOURCES obs-shaderfilter.c
)

add_library(obs-shaderfilter MODULE
	${obs-shaderfilter_SOURCES})

target_link_libraries(obs-shaderfilter
	libobs
	)
set_target_properties(obs-shaderfilter PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(obs-shaderfilter data)

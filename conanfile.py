import os
from conans import ConanFile, MSBuild


class ObsXXQConan(ConanFile):
    license = "MIT"
    name = "obs-xxq"
    url = "git@git.yupaopao.com:pc-media/obs-xxq.git"
    settings = "os", "compiler", "build_type", "arch"
    generators = "visual_studio"

    def package(self):
    
        self.copy("*.h", dst="include/libobs", src="libobs", keep_path=True)
        self.copy("*.hpp", dst="include/libobs", src="libobs", keep_path=True)
        self.copy("*.dll", dst="bin", src="../dependencies2017/win32/bin", keep_path=False)
        
        if self.settings.build_type=="Debug":
            self.copy("*.lib", dst="lib", src="build32/libobs/Debug", keep_path=True)
        else:
            self.copy("*.lib", dst="lib", src="build32/libobs/RelWithDebInfo", keep_path=True)
            
            
        if self.settings.build_type=="Debug":
            self.copy("*.dll", dst="bin", src="build32/libobs/Debug", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/libobs/Debug", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/libobs-d3d11/Debug", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/libobs-d3d11/Debug", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/libobs-opengl/Debug", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/libobs-opengl/Debug", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/deps/obs-scripting/Debug", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/deps/obs-scripting/Debug", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/deps/w32-pthreads/Debug", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/deps/w32-pthreads/Debug", keep_path=False)
            self.copy("*.exe", dst="bin", src="build32/plugins/obs-ffmpeg/ffmpeg-mux/Debug", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/plugins/obs-ffmpeg/ffmpeg-mux/Debug", keep_path=False)
            self.copy("*.exe", dst="bin", src="build32/plugins/win-airplay/airplay2/Debug", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/plugins/win-airplay/airplay2/Debug", keep_path=False)
            self.copy("*.exe", dst="bin", src="build32/plugins/win-airplay/androidusbmirror/Debug", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/plugins/win-airplay/androidusbmirror/Debug", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/plugins/win-airplay/androidusbmirror/Debug", keep_path=False)
            self.copy("scrcpy-server", dst="bin", src="build32/plugins/win-airplay/androidusbmirror/Debug", keep_path=False)
            self.copy("*.exe", dst="bin", src="build32/plugins/win-airplay/drivertool/Debug", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/plugins/win-airplay/drivertool/Debug", keep_path=False)
            self.copy("*.exe", dst="bin", src="build32/plugins/win-airplay/iosusbmirror/Debug", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/plugins/win-airplay/iosusbmirror/Debug", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/plugins/win-airplay/iosusbmirror/Debug", keep_path=False)
           
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/coreaudio-encoder/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/coreaudio-encoder/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/decklink/win/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/decklink/win/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/image-source/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/image-source/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-ffmpeg/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-ffmpeg/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-filters/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-filters/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-outputs/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-outputs/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-qmlview/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-qmlview/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-qsv11/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-qsv11/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-text/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-text/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-transitions/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-transitions/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-vst/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-vst/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-x264/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-x264/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/rtmp-services/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/rtmp-services/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/text-freetype2/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/text-freetype2/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-capture/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-capture/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-dshow/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-dshow/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-mf/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-mf/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-wasapi/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-wasapi/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/enc-amf/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/enc-amf/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-airplay/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-airplay/Debug", keep_path=False)
        else:
            self.copy("*.dll", dst="bin", src="build32/libobs/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/libobs/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/libobs-d3d11/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/libobs-d3d11/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/libobs-opengl/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/libobs-opengl/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/deps/obs-scripting/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/deps/obs-scripting/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/deps/w32-pthreads/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/deps/w32-pthreads/RelWithDebInfo", keep_path=False)
            self.copy("*.exe", dst="bin", src="build32/plugins/obs-ffmpeg/ffmpeg-mux/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/plugins/obs-ffmpeg/ffmpeg-mux/RelWithDebInfo", keep_path=False)
            self.copy("*.exe", dst="bin", src="build32/plugins/win-airplay/airplay2/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/plugins/win-airplay/airplay2/RelWithDebInfo", keep_path=False)
            self.copy("*.exe", dst="bin", src="build32/plugins/win-airplay/androidusbmirror/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/plugins/win-airplay/androidusbmirror/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/plugins/win-airplay/androidusbmirror/RelWithDebInfo", keep_path=False)
            self.copy("scrcpy-server", dst="bin", src="build32/plugins/win-airplay/androidusbmirror/RelWithDebInfo", keep_path=False)
            self.copy("*.exe", dst="bin", src="build32/plugins/win-airplay/drivertool/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/plugins/win-airplay/drivertool/RelWithDebInfo", keep_path=False)
            self.copy("*.exe", dst="bin", src="build32/plugins/win-airplay/iosusbmirror/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/plugins/win-airplay/iosusbmirror/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/plugins/win-airplay/iosusbmirror/RelWithDebInfo", keep_path=False)
           
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/coreaudio-encoder/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/coreaudio-encoder/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/decklink/win/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/decklink/win/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/image-source/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/image-source/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-ffmpeg/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-ffmpeg/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-filters/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-filters/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-outputs/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-outputs/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-qmlview/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-qmlview/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-qsv11/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-qsv11/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-text/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-text/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-transitions/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-transitions/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-vst/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-vst/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-x264/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-x264/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/rtmp-services/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/rtmp-services/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/text-freetype2/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/text-freetype2/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-capture/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-capture/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-dshow/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-dshow/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-mf/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-mf/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-wasapi/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-wasapi/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/enc-amf/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/enc-amf/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-airplay/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-airplay/RelWithDebInfo", keep_path=False)
            
            
        self.copy("*.*", dst="plugins/data/libobs", src="libobs/data", keep_path=True)
        
        self.copy("*.*", dst="plugins/data/obs-plugins/coreaudio-encoder", src="plugins/coreaudio-encoder/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/decklink", src="plugins/decklink/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/image-source", src="plugins/image-source/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/obs-ffmpeg", src="plugins/obs-ffmpeg/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/obs-filters", src="plugins/obs-filters/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/obs-outputs", src="plugins/obs-outputs/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/obs-qmlview", src="plugins/obs-qmlview/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/obs-qsv11", src="plugins/obs-qsv11/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/obs-text", src="plugins/obs-text/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/obs-transitions", src="plugins/obs-transitions/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/obs-vst", src="plugins/obs-vst/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/obs-x264", src="plugins/obs-x264/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/rtmp-services", src="plugins/rtmp-services/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/text-freetype2", src="plugins/text-freetype2/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/win-capture", src="plugins/win-capture/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/win-dshow", src="plugins/win-dshow/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/win-mf", src="plugins/win-mf/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/win-wasapi", src="plugins/win-wasapi/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/win-capture", src="win-capture-data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/enc-amf", src="build32/plugins/enc-amf/data", keep_path=True)
        
        if self.settings.build_type!="Debug":
            os.system("cd signtool && python sign.py " + self.package_folder)

    def package_info(self):
        self.cpp_info.libs = ["obs"]

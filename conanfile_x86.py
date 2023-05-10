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
            self.copy("*.dll", dst="bin", src="build32/libobs-winrt/Debug", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/libobs-winrt/Debug", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/deps/obs-scripting/Debug", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/deps/obs-scripting/Debug", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/deps/w32-pthreads/Debug", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/deps/w32-pthreads/Debug", keep_path=False)
            self.copy("*.exe", dst="bin", src="build32/plugins/obs-ffmpeg/ffmpeg-mux/Debug", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/plugins/obs-ffmpeg/ffmpeg-mux/Debug", keep_path=False)
            
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-ffmpeg/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-ffmpeg/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-outputs/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-outputs/Debug", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-capture/Debug", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-capture/Debug", keep_path=False)
        else:
            self.copy("*.dll", dst="bin", src="build32/libobs/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/libobs/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/libobs-d3d11/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/libobs-d3d11/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/libobs-opengl/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/libobs-opengl/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/libobs-winrt/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/libobs-winrt/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/deps/obs-scripting/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/deps/obs-scripting/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="bin", src="build32/deps/w32-pthreads/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/deps/w32-pthreads/RelWithDebInfo", keep_path=False)
            self.copy("*.exe", dst="bin", src="build32/plugins/obs-ffmpeg/ffmpeg-mux/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="bin", src="build32/plugins/obs-ffmpeg/ffmpeg-mux/RelWithDebInfo", keep_path=False)
            
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-ffmpeg/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-ffmpeg/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-outputs/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/obs-outputs/RelWithDebInfo", keep_path=False)
            self.copy("*.dll", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-capture/RelWithDebInfo", keep_path=False)
            self.copy("*.pdb", dst="plugins/obs-plugins/32bit", src="build32/plugins/win-capture/RelWithDebInfo", keep_path=False)
            
            
        self.copy("*.*", dst="plugins/data/libobs", src="libobs/data", keep_path=True)
        
        self.copy("*.*", dst="plugins/data/obs-plugins/obs-ffmpeg", src="plugins/obs-ffmpeg/data", keep_path=True)
        self.copy("*.*", dst="plugins/data/obs-plugins/obs-outputs", src="plugins/obs-outputs/data", keep_path=True)
        
        if self.settings.build_type!="Debug":
            os.system("cd signtool && python sign.py " + self.package_folder)

    def package_info(self):
        self.cpp_info.libs = ["obs"]

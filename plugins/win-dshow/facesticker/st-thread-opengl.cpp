#include "st-thread.h"
#include <sstream>
#include <string>
#include <map>
#include <vector>

template <typename T>
bool StringTo(const std::string str, T& value) {
	std::istringstream iss(str);
	T tmp;
	iss >> tmp;
	if (iss.fail())
		return false;
	value = tmp;
	return true;
}

bool opengl_check() {
	char exe[] = "sensetime/opengl_check.exe ";
	PROCESS_INFORMATION pi;
	STARTUPINFOA si;
	memset(&pi, 0, sizeof(PROCESS_INFORMATION));
	memset(&si, 0, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags |= STARTF_USESTDHANDLES;	// STARTF_USESTDHANDLES is Required.
	si.dwFlags |= STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	HANDLE hChildStdoutRd = NULL; // Read-side, used in calls to ReadFile() to get child's stdout output.
	HANDLE hChildStdoutWr = NULL; // Write-side, given to child process using si struct.

	// Create a pipe to get results from child's stdout.
	if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) {
		printf("cannot create pipe\n");
		return false;
	}
	// Ensure the read handle to the pipe for STDOUT is not inherited.
	if (!SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0)) {
		printf("Stdout SetHandleInformation");
		return false;
	}

	si.hStdOutput = hChildStdoutWr;		// Requires STARTF_USESTDHANDLES in dwFlags.
	si.hStdError = hChildStdoutWr;		// Requires STARTF_USESTDHANDLES in dwFlags.

	char *cmd = new char[strlen(exe) + 1];
	strcpy(cmd, exe);
	BOOL bSuccess = CreateProcessA(
		NULL,	// No module name (use command line)
		cmd,	// Command line
		NULL,	// Process handle not inheritable
		NULL,	// Thread handle not inheritable
		TRUE,	// Set handle inheritance to TRUE
		0,		// No creation flags
		NULL,	// Use parent's environment block
		NULL,	// Use parent's starting directory
		&si,	// Pointer to STARTUPINFO structure
		&pi);	// Pointer to PROCESS_INFORMATION structure

	delete[] cmd;

	bool bResult = true;
	if (bSuccess) {
		DWORD               exitCode = 0;
		WaitForSingleObject(pi.hProcess, INFINITE);
		if (GetExitCodeProcess(pi.hProcess, &exitCode))
		{
			printf("Exit code = %d\n", exitCode);
		}
		else
		{
			printf("GetExitCodeProcess() failed: %ld\n", GetLastError());
		}

		// Close the write end of the pipe before reading from the read end of the pipe.
		if (!CloseHandle(hChildStdoutWr)) {
			printf("cannot close handle");
			bResult = false;
		}

		if (exitCode != 0)
			bResult = false;
	}
	else {
		printf("CreateProcess failed or cannot find opengl_check.exe: %d\n", GetLastError());
		bResult = true;
	}

	CloseHandle(hChildStdoutRd);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return bResult;
}

int InitGlfw()
{
	int err = 0;
#ifdef WIN32
	if (opengl_check()) {
#endif
		err = glfwInit();
		if (!err) {
			throw "OpenGL: fail to create window context!";
		}

#ifdef WIN32
	}
#endif

	return err;
}

void UninitGlfw()
{
	glfwTerminate();
}

GLboolean STThread::BindTexture(unsigned char *buffer, int width, int height, GLuint& texId) {
	if (!glIsTexture(texId) || (gTextures.find(texId) == gTextures.end())) {
		glGenTextures(1, &texId);
		std::vector<int> tmp(2);
		tmp[0] = width;
		tmp[1] = height;
		gTextures[texId] = tmp;
	}
	int &OldW = gTextures[texId][0];
	int &OldH = gTextures[texId][1];
	if (!glIsTexture(texId) || (OldW != width) || (OldH != height)) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texId);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, buffer);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		OldW = width;
		OldH = height;
	}
	else if (buffer) {
		glBindTexture(GL_TEXTURE_2D, texId);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA,
			GL_UNSIGNED_BYTE, buffer);
	}
	bool tmp = glIsTexture(texId);

	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		printf("error : %d\n", error);
		//	result = RESULT_TEXTURE_ERR;
		error = GL_NO_ERROR;
		return false;
	}
	return true;
}

static void PrintglfwError(int id, const char* name) {
	printf("glfwerror %d %s\n", id, name);
}

void STThread::unInitGL() {
	for (auto it = gTextures.begin(); it != gTextures.end(); ++it) {
		if (glIsTexture(it->first))
			glDeleteTextures(1, &it->first);
	}
	if (window)
		glfwDestroyWindow(window);
	window = nullptr;
	//glfwTerminate();
}

#ifdef WIN32
void STThread::MesaOpenGL() {
	unInitGL();

	char work_path[_MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, work_path, _MAX_PATH);
	std::string str_work_path(work_path);
	str_work_path = str_work_path.substr(0, str_work_path.find_last_of('\\') + 1);
#ifdef _WIN64
	str_work_path = str_work_path + "..\\external\\libs\\windows-x86_64";
#else
	str_work_path = str_work_path + "..\\external\\libs\\windows-x86";
#endif

	SetDllDirectoryA(str_work_path.data());
	GLenum err = false;
	err = glfwInit();
	if (!err) {
		throw "MESA OpenGL: fail to init glfw";
	}

	// Create a offscream mode window and its OpenGL context
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	window = glfwCreateWindow(10, 10, "hello", NULL, NULL);
	if (!window) {
		unInitGL();
		throw "MESA OpenGL: fail to create window context!";
	}
	// Make the window's context current
	glfwMakeContextCurrent(window);

	str_work_path = str_work_path + "\\opengl32.dll";
	err = gl3wInitWithPath(str_work_path.data());
	if (err != GL3W_OK) {
		fprintf(stderr, "gl3wInit error: %d\n", err);
		throw "MESA OpenGL: fail to init gl3w\n";
	}
}
#endif

bool STThread::InitGL() {
	glfwSetErrorCallback(PrintglfwError);
	bool bGLInited = false;
	if (!window) {
		try {
			int err = 0;
			//#ifdef WIN32
			//			if (opengl_check()) {
			//#endif
			//				err = glfwInit();
			//				if (!err) {
			//					throw "OpenGL: fail to create window context!";
			//				}

							// Create a offscream mode window and its OpenGL context
			glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
			window = glfwCreateWindow(10, 10, "hello", NULL, NULL);
			//#ifdef WIN32
			//			}
			//#endif
			if (!window) {
#ifdef WIN32
				MesaOpenGL();
#else
				throw "OpenGL: fail to init glfw";
#endif
			}
			else {
				// Make the window's context current
				glfwMakeContextCurrent(window);
#ifdef WIN32
				err = gl3wInit();
				if (err != GL3W_OK) {
					MesaOpenGL();
				}
#else
				glewExperimental = GL_TRUE;
				err = glewInit();
				if (err != GLEW_OK) {
					throw "OpenGL: fail to init glew\n";
				}
#endif
			}

			const unsigned char* glVer = glGetString(GL_VERSION);
			const unsigned char* glslVer = glGetString(GL_SHADING_LANGUAGE_VERSION);
			float OpenglVersion = 0.f, GLSLVersion = 0.f;
			StringTo((char*)glVer, OpenglVersion);
			StringTo((char*)glslVer, GLSLVersion);

			fprintf(stdout, "Renderer: %s\n", glGetString(GL_RENDERER));
			fprintf(stdout, "OpenGL version supported %s\n", glGetString(GL_VERSION));
			fprintf(stdout, "OpenGLSL version supported %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
			if (OpenglVersion < 2.1f || GLSLVersion < 1.2f) {
				throw "OpenGL version is too old, we only support above opengl2.1 and glsl1.2.0";
			}
			bGLInited = true;
		}
		catch (std::exception &e) {
			fprintf(stderr, "%s\n", e.what());
		}
		catch (const char* e) {
			fprintf(stderr, "%s\n", e);
		}
		catch (...) {
			return false;
		}

	}
	else {
		GLFWwindow* curCtx = glfwGetCurrentContext();
		if (!curCtx)
			glfwMakeContextCurrent(window);
		bGLInited = true;
	}
	return bGLInited;
}

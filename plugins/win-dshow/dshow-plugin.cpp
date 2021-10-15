#include <obs-module.h>
#include "system_utils.h"
#include "egl_loader_autogen.h"
#include "gles_loader_autogen.h"
#include "bef_effect_ai_load_library.h"
#include "bef_effect_ai_api.h"
#include "be_resource_context.h"
#include <QApplication>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("win-dshow", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Windows DirectShow source/encoder";
}

//extern bool g_st_checkpass;
extern void RegisterDShowSource();
extern void RegisterDShowEncoders();
//extern int check_license();


static angle::Library *eglLibrary = nullptr;
static angle::Library *glesLibrary = nullptr;

static angle::GenericProc KHRONOS_APIENTRY getEGLProcAddress(const char *symbol)
{
	return reinterpret_cast<angle::GenericProc>(
		eglLibrary->getSymbol(symbol));
}

static angle::GenericProc KHRONOS_APIENTRY getGLESProcAddress(const char *symbol)
{
	return reinterpret_cast<angle::GenericProc>(
		glesLibrary->getSymbol(symbol));
}

static void loadEGLLibrary(std::string appPath)
{
	std::string eglLibraryPath;
	std::string glesLibraryPath;

#ifdef DEBUG
	eglLibraryPath = appPath + "libEGLd.dll";
	glesLibraryPath = appPath + "libGLESv2d.dll";
#else
	eglLibraryPath = appPath + "libEGL.dll";
	glesLibraryPath = appPath + "libGLESv2.dll";
#endif

	eglLibrary = angle::OpenSharedLibrary(
		eglLibraryPath.c_str(), angle::SearchType::ApplicationDir);
	DWORD err = GetLastError();
	if (eglLibrary) {
		angle::LoadEGL(getEGLProcAddress);
	}

	glesLibrary = angle::OpenSharedLibrary(
		glesLibraryPath.c_str(), angle::SearchType::ApplicationDir);
	if (glesLibrary) {
		angle::LoadGLES(getGLESProcAddress);
	}

	auto ret= bef_effect_ai_load_egl_library_with_func(getEGLProcAddress);
	ret = bef_effect_ai_load_glesv2_library_with_func(getGLESProcAddress);
}

static int logFuncForEffect(int logLevel, const char *msg)
{
	if (msg != nullptr) {
		blog(LOG_DEBUG, msg);
	}
	return 0;
}

static void initBDResource(std::string appPath)
{
	bef_effect_ai_set_log_to_local_func(logFuncForEffect);

	beResourceContext = new BEResourceContext;
	beResourceContext->setApplicationDir(appPath);
	beResourceContext->initializeContext();

	blog(LOG_INFO, "EffectDemoVersion %s\n",
	       beResourceContext->getVersion().c_str());
}

bool obs_module_load(void)
{
	QString appPathQstr = QApplication::applicationDirPath();
	std::string appPath = "";
	appPath = appPathQstr.toUtf8().toStdString();
	if (!appPath.rfind("/") != appPath.length() - 1) {
		appPath.append("/");
	}

	loadEGLLibrary(appPath);
	initBDResource(appPath);

	RegisterDShowSource();
	RegisterDShowEncoders();

	//g_st_checkpass = check_license() == 0;

	return true;
}

void obs_module_unload(void)
{
	delete beResourceContext;
}

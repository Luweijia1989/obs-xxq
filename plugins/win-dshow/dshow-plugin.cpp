#include <obs-module.h>
#include "system_utils.h"
#include "egl_loader_autogen.h"
#include "gles_loader_autogen.h"
#include "bef_effect_ai_load_library.h"
#include "bef_effect_ai_api.h"
#include "bef_effect_ai_auth_msg.h"
#include "be_resource_context.h"
#include <QApplication>
#include <QMessageAuthenticationCode>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

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
#ifdef DEBUG
	bef_effect_ai_set_log_to_local_func(logFuncForEffect);
#endif // DEBUG

	beResourceContext = new BEResourceContext;
	beResourceContext->setApplicationDir(appPath);
	beResourceContext->initializeContext();

	if (!QFile::exists(getResourceContext()->getFeatureContext()->getLicensePath().c_str())) {
		char *authMsg = nullptr;
		int authMsgLen = 0;
		bef_effect_ai_get_auth_msg(&authMsg, &authMsgLen);

		QString key = "biz_license_tool_test_keycdda5d158b4d4254ba5c4f3bdd8a1127";
		QString screct = "39da2a8d5cf5054e77a15e33bbb60883";
		auto nonce = rand();
		auto timestamp = QDateTime::currentSecsSinceEpoch();
		QJsonObject obj;
		obj["key"] = key;
		obj["authMsg"] = authMsg;
		obj["nonce"] = nonce;
		obj["timestamp"] = timestamp;
		auto func = [](QByteArray data, const char* const digits = "0123456789ABCDEF")
		{
		    QString str;

		    for (unsigned char gap = 0; gap < data.size();) {
			unsigned char beg = data[gap];
			++gap;
			str.append(digits[beg >> 4]);
			str.append(digits[beg & 15]);
		    }
		    return str;
		};

		QString temp = QString("%1%2%3%4").arg(key).arg(nonce).arg(timestamp).arg(authMsg);
		auto data = QMessageAuthenticationCode::hash(temp.toUtf8(), screct.toUtf8(), QCryptographicHash::Sha256);
		obj["digest"] = func(data);

		QNetworkAccessManager *manager = new QNetworkAccessManager;
		QNetworkRequest req;
		req.setUrl(QUrl("https://cv-tob.bytedance.com/v1/api/sdk/tob_license/getlicense"));
		req.setRawHeader("Content-Type", "application/json");
		req.setRawHeader("cache-control", "no-cache");
		QJsonDocument jd(obj);
		qDebug() << jd;
		auto reply = manager->post(req, jd.toJson());
		QObject::connect(reply, &QNetworkReply::finished, [=](){
			QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
			auto o = doc.object();
			if (!o.isEmpty()) {
				QDir dir(QString::fromStdString(getResourceContext()->getFeatureContext()->getResourceDir()));
				if (!dir.exists("license"))
					dir.mkdir("license");
				QString base64Str = o["data"].toString();
				auto towrite = QByteArray::fromBase64(base64Str.toUtf8());
				QFile f(getResourceContext()->getFeatureContext()->getLicensePath().c_str());
				f.open(QFile::ReadWrite | QFile::Truncate);
				f.write(towrite);
				f.close();
			}
			reply->deleteLater();
			manager->deleteLater();
		});
	}

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

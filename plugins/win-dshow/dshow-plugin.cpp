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
#include <QDebug>

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


static const std::string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

static inline bool is_base64(BYTE c)
{
	return (isalnum(c) || (c == '+') || (c == '/'));
}

std::vector<BYTE> base64_decode(std::string const &encoded_string)
{
	int in_len = encoded_string.size();
	int i = 0;
	int j = 0;
	int in_ = 0;
	BYTE char_array_4[4], char_array_3[3];
	std::vector<BYTE> ret;

	while (in_len-- && (encoded_string[in_] != '=') &&
	       is_base64(encoded_string[in_])) {
		char_array_4[i++] = encoded_string[in_];
		in_++;
		if (i == 4) {
			for (i = 0; i < 4; i++)
				char_array_4[i] =
					base64_chars.find(char_array_4[i]);

			char_array_3[0] = (char_array_4[0] << 2) +
					  ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) +
					  ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) +
					  char_array_4[3];

			for (i = 0; (i < 3); i++)
				ret.push_back(char_array_3[i]);
			i = 0;
		}
	}

	if (i) {
		for (j = i; j < 4; j++)
			char_array_4[j] = 0;

		for (j = 0; j < 4; j++)
			char_array_4[j] = base64_chars.find(char_array_4[j]);

		char_array_3[0] = (char_array_4[0] << 2) +
				  ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) +
				  ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] =
			((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for (j = 0; (j < i - 1); j++)
			ret.push_back(char_array_3[j]);
	}

	return ret;
}

static void initBDResource(std::string appPath)
{
#ifdef DEBUG
	bef_effect_ai_set_log_to_local_func(logFuncForEffect);
#endif // DEBUG

	beResourceContext = new BEResourceContext;
	beResourceContext->setApplicationDir(appPath);
	beResourceContext->initializeContext();
	QFileInfo finfo(getResourceContext()->getFeatureContext()->getLicensePath().c_str());
	if (!finfo.exists() || finfo.size() == 0) {
		char *authMsg = nullptr;
		int authMsgLen = 0;
		bef_effect_ai_get_auth_msg(&authMsg, &authMsgLen);

		QString key = "biz_license_tool_test_key1dc40f68d29c421e977b01d61b13f5e0";
		QString screct = "9a671942ec36a5e7d3ff21f96aca2b79";
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
		auto reply = manager->post(req, jd.toJson());
		QObject::connect(reply, &QNetworkReply::finished, [=](){
			QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
			qDebug() << "license doc " << doc;
			auto o = doc.object();
			if (!o.isEmpty()) {
				QDir dir(QString::fromStdString(getResourceContext()->getFeatureContext()->getResourceDir()));
				if (!dir.exists("license"))
					dir.mkdir("license");
				QString base64Str = o["data"].toString();
				std::vector<uint8_t> lic_b = base64_decode(base64Str.toStdString());
				QFile f(getResourceContext()->getFeatureContext()->getLicensePath().c_str());
				f.open(QFile::ReadWrite | QFile::Truncate);
				f.write((const char *)lic_b.data(), lic_b.size());
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
